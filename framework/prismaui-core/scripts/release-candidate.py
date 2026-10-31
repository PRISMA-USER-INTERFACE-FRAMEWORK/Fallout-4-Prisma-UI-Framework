from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path

DLL_MEMBERS = {
    "PrismaUI_F4.dll": "Data/F4SE/plugins/PrismaUI_F4.dll",
    "PrismaUI_F4VR.dll": "Data/F4SE/plugins/PrismaUI_F4VR.dll",
}
PDB_MEMBERS = {
    "PrismaUI_F4.pdb": "PrismaUI_F4.pdb",
    "PrismaUI_F4VR.pdb": "PrismaUI_F4VR.pdb",
}
ACCEPTANCE_MEMBER = "LocalizationTestMod/F4SE/Plugins/PrismaLocalizationAcceptance.dll"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def names(version: str) -> dict[str, str]:
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ValueError(f"version must be x.y.z: {version}")
    return {
        "desktop_zip": f"PrismaUI_F4-{version}-Fallout4-OG-AE.zip",
        "vr_zip": f"PrismaUI_F4-{version}-Fallout4-VR.zip",
        "symbols_zip": f"PrismaUI_F4-{version}-symbols.zip",
    }


def measured_hashes(directory: Path, version: str, require_vr: bool = True) -> dict[str, str]:
    archive_names = names(version)
    desktop = directory / archive_names["desktop_zip"]
    vr = directory / archive_names["vr_zip"]
    symbols = directory / archive_names["symbols_zip"]
    with zipfile.ZipFile(desktop) as archive:
        desktop_dll = sha256(archive.read(DLL_MEMBERS["PrismaUI_F4.dll"]))
        acceptance_dll = sha256(archive.read(ACCEPTANCE_MEMBER))
    with zipfile.ZipFile(symbols) as archive:
        flat_pdb = sha256(archive.read(PDB_MEMBERS["PrismaUI_F4.pdb"]))
        vr_pdb = sha256(archive.read(PDB_MEMBERS["PrismaUI_F4VR.pdb"]))
    result = {
        "PrismaUI_F4.dll": desktop_dll,
        "PrismaLocalizationAcceptance.dll": acceptance_dll,
        archive_names["desktop_zip"]: file_sha256(desktop),
        "PrismaUI_F4.pdb": flat_pdb,
        "PrismaUI_F4VR.pdb": vr_pdb,
        archive_names["symbols_zip"]: file_sha256(symbols),
    }
    if require_vr or vr.exists():
        with zipfile.ZipFile(vr) as archive:
            result["PrismaUI_F4VR.dll"] = sha256(archive.read(DLL_MEMBERS["PrismaUI_F4VR.dll"]))
        result[archive_names["vr_zip"]] = file_sha256(vr)
    return result


def read_sums(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text(encoding="ascii").splitlines():
        digest, separator, filename = line.partition("  ")
        if not separator or not re.fullmatch(r"[0-9A-Fa-f]{64}", digest) or not filename:
            raise ValueError(f"invalid checksum line: {line!r}")
        if filename in result:
            raise ValueError(f"duplicate checksum entry: {filename}")
        result[filename] = digest.upper()
    return result


def validate_commit(commit: str) -> None:
    if not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
        raise ValueError("candidate commit must be a full Git SHA")


def create(directory: Path, version: str, commit: str, workflow_url: str) -> None:
    validate_commit(commit)
    hashes = measured_hashes(directory, version)
    (directory / "SHA256SUMS.txt").write_text(
        "".join(f"{digest}  {filename}\n" for filename, digest in hashes.items()),
        encoding="ascii",
    )
    record = {
        "schema_version": 2,
        "version": version,
        "commit": commit.lower(),
        "workflow_url": workflow_url,
        "sha256": hashes,
    }
    (directory / "RELEASE-CANDIDATE.json").write_text(
        json.dumps(record, indent=2) + "\n", encoding="utf-8"
    )


def verify(
    directory: Path,
    version: str,
    commit: str,
    accepted_dll: str | None,
    accepted_zip: str | None,
) -> None:
    validate_commit(commit)
    for accepted in (accepted_dll, accepted_zip):
        if accepted is not None and not re.fullmatch(r"[0-9a-fA-F]{64}", accepted):
            raise ValueError("supplied acceptance hash must be a full SHA-256")
    measured = measured_hashes(directory, version, require_vr=False)
    sums = read_sums(directory / "SHA256SUMS.txt")
    record = json.loads((directory / "RELEASE-CANDIDATE.json").read_text(encoding="utf-8"))
    if record.get("schema_version") != 2:
        raise ValueError("unsupported release-candidate schema")
    if record.get("version") != version or record.get("commit", "").lower() != commit.lower():
        raise ValueError("release-candidate version or commit does not match")
    expected_names = {
        "PrismaUI_F4.dll",
        "PrismaLocalizationAcceptance.dll",
        names(version)["desktop_zip"],
        "PrismaUI_F4VR.dll",
        names(version)["vr_zip"],
        "PrismaUI_F4.pdb",
        "PrismaUI_F4VR.pdb",
        names(version)["symbols_zip"],
    }
    if set(sums) != expected_names or record.get("sha256") != sums:
        raise ValueError("release-candidate checksum records are incomplete or inconsistent")
    if any(sums[filename] != digest for filename, digest in measured.items()):
        raise ValueError("release-candidate hashes do not match the downloaded bytes")
    if accepted_dll and sums["PrismaUI_F4.dll"] != accepted_dll.upper():
        raise ValueError("runtime-accepted desktop DLL hash does not match the candidate")
    desktop_name = names(version)["desktop_zip"]
    if accepted_zip and sums[desktop_name] != accepted_zip.upper():
        raise ValueError("runtime-accepted desktop ZIP hash does not match the candidate")


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    for command in ("create", "verify"):
        sub = subparsers.add_parser(command)
        sub.add_argument("--directory", type=Path, required=True)
        sub.add_argument("--version", required=True)
        sub.add_argument("--commit", required=True)
        if command == "create":
            sub.add_argument("--workflow-url", required=True)
        else:
            sub.add_argument("--accepted-dll-sha256")
            sub.add_argument("--accepted-zip-sha256")
    args = parser.parse_args()
    try:
        if args.command == "create":
            create(args.directory, args.version, args.commit, args.workflow_url)
        else:
            verify(
                args.directory,
                args.version,
                args.commit,
                args.accepted_dll_sha256,
                args.accepted_zip_sha256,
            )
    except (FileNotFoundError, KeyError, ValueError, zipfile.BadZipFile) as error:
        print(f"release candidate verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
