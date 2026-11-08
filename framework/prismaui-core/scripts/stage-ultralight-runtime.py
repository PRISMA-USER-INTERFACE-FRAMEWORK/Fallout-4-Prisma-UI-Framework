#!/usr/bin/env python3
"""Stage and atomically deploy the pinned Ultralight runtime."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

sys.path.insert(0, str(Path(__file__).resolve().parent))
_validator_spec = importlib.util.spec_from_file_location(
    "prisma_ultralight_validator", Path(__file__).resolve().with_name("validate-ultralight-runtime.py")
)
if _validator_spec is None or _validator_spec.loader is None:
    raise RuntimeError("cannot load the Ultralight runtime validator")
_validator = importlib.util.module_from_spec(_validator_spec)
_validator_spec.loader.exec_module(_validator)
DEFAULT_MANIFEST = _validator.DEFAULT_MANIFEST
validate = _validator.validate


ROOT = Path(__file__).resolve().parent.parent
DLLS = ("AppCore.dll", "Ultralight.dll", "UltralightCore.dll", "WebCore.dll")
RESOURCES = ("cacert.pem", "icudt67l.dat")
LEGAL_FILES = {
    "LICENSE.txt": (
        "Ultralight-LICENSE.txt",
        38877,
        "bff09887bcd34a25bed2804e9c403edc9a281d3aeabc3bda92c56f19247f2c82",
    ),
    "EULA.txt": (
        "Ultralight-EULA.txt",
        5859,
        "5dd75b8bc8fdf12ae170d10443f4a183321f98f68d670658349c7d83752c7870",
    ),
    "NOTICES.md": (
        "Ultralight-NOTICES.md",
        50016,
        "ccf577ba1a07c4a469840e45a6e6c56e506bcbc39d7bc25bd46f410b0d8b2f90",
    ),
}
NOTICE_ALIAS = "NOTICES.txt"
CREDIT_FILE = ROOT / "runtime" / "ultralight-1.4.0-CREDITS.txt"
CREDIT_DESTINATION = "Ultralight-CREDITS.txt"
INSPECTOR_PREFIX = "Data/PrismaUI_F4/inspector/"
INSPECTOR_PROVENANCE = ROOT / "tests" / "ultralight_inspector_provenance.json"
WINDOWS_RESERVED_NAMES = {
    "con",
    "prn",
    "aux",
    "nul",
    *(f"com{index}" for index in range(1, 10)),
    *(f"lpt{index}" for index in range(1, 10)),
}
WINDOWS_INVALID_NAME_CHARACTERS = frozenset('<>:"|?*')


def canonical_text_bytes(path: Path) -> bytes:
    return path.read_bytes().replace(b"\r\n", b"\n")


def validate_windows_archive_component(component: str, original: str) -> None:
    if component[-1] in ". ":
        raise ValueError(f"production archive contains unsafe Windows path: {original}")
    if any(ord(character) < 0x20 or character in WINDOWS_INVALID_NAME_CHARACTERS for character in component):
        raise ValueError(f"production archive contains unsafe Windows path: {original}")
    if component.split(".", 1)[0].casefold() in WINDOWS_RESERVED_NAMES:
        raise ValueError(f"production archive contains unsafe Windows path: {original}")


def normalized_archive_members(release: zipfile.ZipFile) -> dict[str, str]:
    members: dict[str, str] = {}
    windows_members: dict[str, str] = {}
    for original in release.namelist():
        normalized = original.replace("\\", "/")
        is_directory = normalized.endswith("/")
        path_text = normalized[:-1] if is_directory else normalized
        components = path_text.split("/")
        if not path_text or normalized.startswith("/"):
            raise ValueError(f"production archive contains unsafe path: {original}")
        if len(components[0]) >= 2 and components[0][1] == ":":
            raise ValueError(f"production archive contains unsafe path: {original}")
        if any(component in ("", ".", "..") for component in components):
            raise ValueError(f"production archive contains unsafe path: {original}")
        for component in components:
            validate_windows_archive_component(component, original)
        if normalized in members:
            raise ValueError(f"production archive contains duplicate normalized path: {normalized}")
        windows_key = path_text.casefold()
        if windows_key in windows_members:
            raise ValueError(
                "production archive contains Windows case-colliding paths: "
                f"{windows_members[windows_key]} and {original}"
            )
        members[normalized] = original
        windows_members[windows_key] = original
    return members


def locate(source: Path, name: str) -> Path:
    candidates = [source / "libs" / name, source / "bin" / name, source / name]
    candidates.extend(source.rglob(name))
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise RuntimeError(f"runtime source is missing {name}: {source}")


def locate_legal(source: Path, name: str) -> Path:
    destination = LEGAL_FILES[name][0]
    for candidate in (
        source / "license" / name,
        source / "licenses" / name,
        source / "licenses" / destination,
        source / name,
        source / destination,
    ):
        if candidate.is_file():
            return candidate
    raise RuntimeError(f"Ultralight SDK legal payload is missing {name}: {source}")


def has_legal_files(source: Path) -> bool:
    try:
        for name in LEGAL_FILES:
            locate_legal(source, name)
    except RuntimeError:
        return False
    return True


def copy_legal_files(
    source: Path,
    package_root: Path,
    legal_files: dict[str, tuple[str, int, str]] = LEGAL_FILES,
) -> None:
    destination = package_root / "licenses"
    destination.mkdir(parents=True, exist_ok=True)
    for source_name, (destination_name, expected_size, expected_sha256) in legal_files.items():
        source_file = locate_legal(source, source_name)
        payload = source_file.read_bytes()
        sha256 = hashlib.sha256(payload).hexdigest()
        if len(payload) != expected_size or sha256.lower() != expected_sha256.lower():
            raise RuntimeError(
                f"Ultralight SDK legal payload mismatch for {source_name}: "
                f"size={len(payload)}, sha256={sha256}"
            )
        shutil.copy2(source_file, destination / destination_name)
        if source_name == "NOTICES.md":
            (destination / NOTICE_ALIAS).write_bytes(payload)
    (destination / "PrismaUI-LICENSE.md").write_bytes(canonical_text_bytes(ROOT / "LICENSE.md"))
    (destination / CREDIT_DESTINATION).write_bytes(canonical_text_bytes(CREDIT_FILE))


def verify_release_archive(
    archive: Path,
    legal_files: dict[str, tuple[str, int, str]] = LEGAL_FILES,
    project_license: Path = ROOT / "LICENSE.md",
    credit_file: Path = CREDIT_FILE,
) -> None:
    required_legal = {
        f"Data/PrismaUI_F4/licenses/{destination}": (size, sha256)
        for destination, size, sha256 in legal_files.values()
    }
    project_path = "Data/PrismaUI_F4/licenses/PrismaUI-LICENSE.md"
    notice_alias_path = f"Data/PrismaUI_F4/licenses/{NOTICE_ALIAS}"
    credit_path = f"Data/PrismaUI_F4/licenses/{CREDIT_DESTINATION}"
    provenance = json.loads(INSPECTOR_PROVENANCE.read_text(encoding="utf-8"))
    with zipfile.ZipFile(archive) as release:
        members = normalized_archive_members(release)
        for header in ("PrismaUI_F4_API.h", "PrismaUI_F4VR_API.h"):
            member = f"Developer/{header}"
            if member not in members or release.read(members[member]) != (ROOT / "src" / header).read_bytes():
                raise ValueError("production archive has missing or stale SDK header: " + member)
        names = {PurePosixPath(name).name.casefold() for name in members}
        forbidden = {"libcef.dll", "prismaui_f4_cefsubprocess.exe"} & names
        cef_tree = [name for name in members if "prismaui_f4_cef/" in name.casefold()]
        if forbidden or cef_tree:
            raise ValueError(
                "production archive contains retired CEF artifacts: "
                + ", ".join(sorted(forbidden) + cef_tree[:3])
            )
        if any(name.casefold().startswith("data/prismaui_f4/views/inspector/") for name in members):
            raise ValueError("production archive contains the consumer-visible inspector tree")
        inspector_files = sorted(
            [
                (
                    name[len(INSPECTOR_PREFIX) :],
                    original,
                )
                for name, original in members.items()
                if name.startswith(INSPECTOR_PREFIX) and not name.endswith("/")
            ],
            key=lambda item: tuple(PurePosixPath(item[0]).parts),
        )
        if len(inspector_files) != provenance["file_count"]:
            raise ValueError(
                f"production archive has {len(inspector_files)} inspector files; "
                f"expected {provenance['file_count']}"
            )
        tree = hashlib.sha256()
        for relative, original in inspector_files:
            payload = release.read(original)
            tree.update(relative.encode("utf-8"))
            tree.update(b"\0")
            tree.update(str(len(payload)).encode("ascii"))
            tree.update(b"\0")
            tree.update(payload)
        if tree.hexdigest() != provenance["tree_sha256"]:
            raise ValueError("production archive has invalid inspector tree provenance")
        missing = sorted((set(required_legal) | {project_path, notice_alias_path, credit_path}) - set(members))
        if missing:
            raise ValueError("production archive is missing legal files: " + ", ".join(missing))
        for path, (expected_size, expected_sha256) in required_legal.items():
            payload = release.read(members[path])
            if len(payload) != expected_size or hashlib.sha256(payload).hexdigest() != expected_sha256:
                raise ValueError("production archive has invalid legal file: " + path)
        notices_path = f"Data/PrismaUI_F4/licenses/{legal_files['NOTICES.md'][0]}"
        if release.read(members[notice_alias_path]) != release.read(members[notices_path]):
            raise ValueError("production archive has invalid legal file: " + notice_alias_path)
        if release.read(members[project_path]) != canonical_text_bytes(project_license):
            raise ValueError("production archive has invalid legal file: " + project_path)
        if release.read(members[credit_path]) != canonical_text_bytes(credit_file):
            raise ValueError("production archive has invalid legal file: " + credit_path)


def archive_tool() -> str:
    for candidate in ("7z", "7zz", "7zr", r"C:\\Program Files\\7-Zip\\7z.exe"):
        resolved = shutil.which(candidate) if "\\" not in candidate else candidate
        if resolved and Path(resolved).is_file():
            return resolved
    raise RuntimeError("7z/7zz is required to extract the pinned Ultralight SDK archive")


def copy_runtime(
    source: Path,
    package_root: Path,
    manifest: Path,
    legal_source: Path | None = None,
) -> None:
    libs = package_root / "libs"
    resources = package_root / "resources"
    libs.mkdir(parents=True, exist_ok=True)
    resources.mkdir(parents=True, exist_ok=True)
    for name in DLLS:
        source_file = locate(source, name)
        if source_file.resolve() != (libs / name).resolve():
            shutil.copy2(source_file, libs / name)
    for name in RESOURCES:
        source_file = locate(source, name)
        if source_file.resolve() != (resources / name).resolve():
            shutil.copy2(source_file, resources / name)
    copy_legal_files(legal_source or source, package_root)
    validate(package_root, manifest)


def stage_from_source(package_root: Path, source: Path | None, archive: Path | None, manifest: Path) -> None:
    if source is not None:
        if has_legal_files(source):
            copy_runtime(source, package_root, manifest)
            return
        if archive is None or not archive.is_file():
            raise RuntimeError("runtime source has no legal payload and no explicit SDK archive was provided")
        with tempfile.TemporaryDirectory(prefix="prisma-ultralight-") as temp:
            subprocess.run([archive_tool(), "x", str(archive), f"-o{temp}", "-y"], check=True)
            copy_runtime(source, package_root, manifest, Path(temp))
        return
    if archive is None or not archive.is_file():
        raise RuntimeError("ULTRALIGHT_RUNTIME_DIR or an explicit --archive is required")
    with tempfile.TemporaryDirectory(prefix="prisma-ultralight-") as temp:
        subprocess.run([archive_tool(), "x", str(archive), f"-o{temp}", "-y"], check=True)
        copy_runtime(Path(temp), package_root, manifest)


def deploy(dist: Path, mods_path: Path, manifest: Path) -> Path:
    if not dist.is_dir():
        raise RuntimeError(f"distribution directory is missing: {dist}")
    destination = mods_path / "PrismaUI_F4"
    staging = mods_path / ".PrismaUI_F4.staging"
    backup = mods_path / ".PrismaUI_F4.previous"
    if staging.exists():
        shutil.rmtree(staging)
    if backup.exists():
        shutil.rmtree(backup)
    shutil.copytree(dist, staging)
    validate(staging / "PrismaUI_F4", manifest)
    backed_up = False
    try:
        if destination.exists():
            os.replace(destination, backup)
            backed_up = True
        os.replace(staging, destination)
    except Exception:
        if backed_up:
            try:
                if destination.exists():
                    shutil.rmtree(destination, ignore_errors=True)
                if backup.exists():
                    os.replace(backup, destination)
            except OSError:
                pass
        raise
    return destination


def deploy_into_existing_mod(dist: Path, destination: Path, manifest: Path) -> Path:
    """Swap the package-owned directories while preserving MO2 metadata and sibling mods."""
    destination.mkdir(parents=True, exist_ok=True)
    staging = destination.parent / f".{destination.name}.staging"
    if staging.exists():
        shutil.rmtree(staging)
    shutil.copytree(dist, staging)
    validate(staging / "PrismaUI_F4", manifest)
    journal: list[tuple[str, Path, Path | None]] = []
    try:
        for component in ("F4SE", "PrismaUI_F4", "Scripts"):
            staged_component = staging / component
            if not staged_component.exists():
                continue
            live_component = destination / component
            backup_component = destination / f".{component}.previous"
            if backup_component.exists():
                shutil.rmtree(backup_component)
            if live_component.exists():
                os.replace(live_component, backup_component)
                journal.append(("backup", live_component, backup_component))
            os.replace(staged_component, live_component)
            journal.append(("place", live_component, None))
    except Exception:
        for kind, live_component, backup_component in reversed(journal):
            try:
                if kind == "place" and live_component.exists():
                    shutil.rmtree(live_component, ignore_errors=True)
                elif kind == "backup" and backup_component is not None and backup_component.exists():
                    os.replace(backup_component, live_component)
            except OSError:
                pass
        raise
    shutil.rmtree(staging, ignore_errors=True)
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-root", type=Path, help="stage libs/resources under this PrismaUI_F4 root")
    parser.add_argument("--deploy-from", type=Path, help="complete distribution directory to deploy atomically")
    parser.add_argument("--mods-path", type=Path, help="MO2 mods root used with --deploy-from")
    parser.add_argument("--deploy-root", type=Path, help="existing mod directory; package-owned trees are swapped in place")
    parser.add_argument("--source", type=Path, help="directory containing bin/ or libs/ runtime files")
    parser.add_argument("--archive", type=Path)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--verify-release-archive", type=Path)
    args = parser.parse_args()
    try:
        if args.verify_release_archive:
            verify_release_archive(args.verify_release_archive)
            print(f"release archive content check passed: {args.verify_release_archive}")
            return 0
        if args.package_root:
            stage_from_source(args.package_root, args.source, args.archive, args.manifest)
            print(f"staged Ultralight runtime in {args.package_root}")
        if args.deploy_from:
            if args.deploy_root:
                destination = deploy_into_existing_mod(args.deploy_from, args.deploy_root, args.manifest)
            elif args.mods_path:
                destination = deploy(args.deploy_from, args.mods_path, args.manifest)
            else:
                raise RuntimeError("--mods-path or --deploy-root is required with --deploy-from")
            print(f"atomically deployed PrismaUI_F4 to {destination}")
        if not args.package_root and not args.deploy_from:
            raise RuntimeError("one of --package-root or --deploy-from is required")
    except (OSError, RuntimeError, ValueError, zipfile.BadZipFile, subprocess.CalledProcessError) as exc:
        print(f"Ultralight staging failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
