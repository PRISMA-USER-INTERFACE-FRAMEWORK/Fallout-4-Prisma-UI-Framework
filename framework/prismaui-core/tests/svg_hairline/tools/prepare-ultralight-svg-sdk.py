#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

ARCHIVE_NAME = "ultralight-free-sdk-1.4.1-dev-win-x64.7z"
ARCHIVE_SIZE = 18467246
ARCHIVE_SHA256 = "4fa7aadd1e4ba4a7dc04d17b1d82b37b141c6e4e7196501150486fa6ac1635c5"
REQUIRED_DIRS = ("include", "bin", "lib", "resources", "inspector")
REQUIRED_FILES = (
    "include/AppCore/Platform.h",
    "include/Ultralight/Ultralight.h",
    "bin/AppCore.dll",
    "bin/Ultralight.dll",
    "bin/UltralightCore.dll",
    "bin/WebCore.dll",
    "lib/AppCore.lib",
    "lib/Ultralight.lib",
    "lib/UltralightCore.lib",
    "lib/WebCore.lib",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def archive_tool() -> str:
    for name in ("7z", "7zz", "7zr"):
        resolved = shutil.which(name)
        if resolved:
            return resolved
    windows = Path(r"C:\Program Files\7-Zip\7z.exe")
    if windows.is_file():
        return str(windows)
    raise RuntimeError("7z/7zz/7zr is required to extract the Ultralight SDK archive")


def locate_sdk_root(extracted: Path) -> Path:
    candidates = [extracted]
    candidates.extend(path for path in extracted.iterdir() if path.is_dir())
    matches = [path for path in candidates if all((path / name).is_dir() for name in REQUIRED_DIRS)]
    if len(matches) != 1:
        raise RuntimeError(f"expected exactly one Ultralight SDK root, found {len(matches)}")
    return matches[0]


def validate_sdk(root: Path) -> None:
    missing_dirs = [name for name in REQUIRED_DIRS if not (root / name).is_dir()]
    if missing_dirs:
        raise RuntimeError("Ultralight SDK is missing directories: " + ", ".join(missing_dirs))
    missing_files = [name for name in REQUIRED_FILES if not (root / name).is_file()]
    if missing_files:
        raise RuntimeError("Ultralight SDK is missing required files: " + ", ".join(missing_files))
    if not any((root / "resources").glob("icudt*.dat")):
        raise RuntimeError("Ultralight SDK resources contain no icudt*.dat payload")
    if not (root / "resources" / "cacert.pem").is_file():
        raise RuntimeError("Ultralight SDK resources are missing cacert.pem")


def inventory(root: Path) -> dict[str, dict[str, int | str]]:
    result: dict[str, dict[str, int | str]] = {}
    for directory in ("bin", "lib", "resources"):
        for path in sorted((root / directory).rglob("*")):
            if not path.is_file():
                continue
            relative = path.relative_to(root).as_posix()
            result[relative] = {"size": path.stat().st_size, "sha256": sha256(path)}
    for relative in ("include/AppCore/Platform.h", "include/Ultralight/Ultralight.h"):
        path = root / relative
        result[relative] = {"size": path.stat().st_size, "sha256": sha256(path)}
    return result


def copy_sdk(source: Path, destination: Path) -> None:
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(source, destination, symlinks=False)


def main() -> int:
    parser = argparse.ArgumentParser(description="Hash, extract, and validate the exact Ultralight 1.4.1-dev SDK used by the SVG raster comparison")
    parser.add_argument("archive", type=Path)
    parser.add_argument("--output", type=Path, required=True, help="validated SDK destination")
    parser.add_argument("--provenance", type=Path, required=True, help="JSON provenance output")
    parser.add_argument("--allow-renamed-archive", action="store_true")
    args = parser.parse_args()

    archive = args.archive.resolve()
    if not archive.is_file():
        raise RuntimeError(f"SDK archive not found: {archive}")
    if not args.allow_renamed_archive and archive.name != ARCHIVE_NAME:
        raise RuntimeError(f"expected archive name {ARCHIVE_NAME}, got {archive.name}")
    if archive.stat().st_size != ARCHIVE_SIZE:
        raise RuntimeError(f"candidate archive size mismatch: expected {ARCHIVE_SIZE}, got {archive.stat().st_size}")

    archive_digest = sha256(archive)
    if archive_digest != ARCHIVE_SHA256:
        raise RuntimeError(f"candidate archive SHA-256 mismatch: expected {ARCHIVE_SHA256}, got {archive_digest}")

    with tempfile.TemporaryDirectory(prefix="prisma-ultralight-svg-sdk-") as temporary:
        extracted = Path(temporary)
        subprocess.run([archive_tool(), "x", str(archive), f"-o{extracted}", "-y"], check=True)
        root = locate_sdk_root(extracted)
        validate_sdk(root)
        files = inventory(root)
        copy_sdk(root, args.output.resolve())

    payload = {
        "schema": 1,
        "sdk": "ultralight-free-sdk",
        "version": "1.4.1-dev",
        "archive": {
            "name": archive.name,
            "size": archive.stat().st_size,
            "sha256": archive_digest,
        },
        "files": files,
    }
    args.provenance.parent.mkdir(parents=True, exist_ok=True)
    args.provenance.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(payload["archive"], sort_keys=True))
    print(f"validated SDK: {args.output.resolve()}")
    print(f"provenance: {args.provenance.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
