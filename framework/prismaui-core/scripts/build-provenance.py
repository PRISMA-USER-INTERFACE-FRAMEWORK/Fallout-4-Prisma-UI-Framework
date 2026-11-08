from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


def measure(flat: Path, vr: Path, version: str, commit: str) -> dict:
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ValueError("build version must be x.y.z")
    if not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
        raise ValueError("build commit must be a full Git SHA")
    hashes = {}
    for directory, stem in ((flat, "PrismaUI_F4"), (vr, "PrismaUI_F4VR")):
        for suffix in (".dll", ".pdb"):
            path = directory / (stem + suffix)
            with path.open("rb") as stream:
                hashes[path.name] = hashlib.file_digest(stream, "sha256").hexdigest()
    for test_plugin in (flat / "PrismaLocalizationAcceptance.dll", flat / "PrismaControllerTest_F4.dll"):
        with test_plugin.open("rb") as stream:
            hashes[test_plugin.name] = hashlib.file_digest(stream, "sha256").hexdigest()
    return {"schema_version": 1, "version": version, "commit": commit.lower(), "sha256": hashes}


def verify(flat: Path, vr: Path, version: str, commit: str) -> None:
    record = json.loads((flat / "BUILD-PROVENANCE.json").read_text(encoding="utf-8"))
    if record != measure(flat, vr, version, commit):
        raise ValueError("build provenance does not match source identity or DLL/PDB bytes")


def check_source(root: Path, commit: str) -> None:
    def git(*args: str) -> str:
        return subprocess.check_output(
            ["git", "-c", f"safe.directory={root}", "-C", str(root), *args], text=True
        ).strip()

    if git("rev-parse", "HEAD").lower() != commit.lower():
        raise ValueError("source commit changed during the build")
    if git("status", "--porcelain", "--untracked-files=no"):
        raise ValueError("build provenance requires clean tracked source and submodules")
    for line in git("submodule", "status", "--recursive").splitlines():
        if line.startswith(("+", "-", "U")):
            raise ValueError("build provenance requires initialized, pinned submodules")
    if git("ls-files", "--others", "--exclude-standard", "--", "framework/prismaui-core/src", "shared"):
        raise ValueError("untracked build source is not covered by the commit")
    if git("submodule", "foreach", "--quiet", "--recursive", "git status --porcelain --untracked-files=all"):
        raise ValueError("submodule source is not clean")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("record", "verify"))
    parser.add_argument("--flat", type=Path, required=True)
    parser.add_argument("--vr", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    check_source(root, args.commit)
    if args.command == "record":
        record = measure(args.flat, args.vr, args.version, args.commit)
        (args.flat / "BUILD-PROVENANCE.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    else:
        verify(args.flat, args.vr, args.version, args.commit)


if __name__ == "__main__":
    main()
