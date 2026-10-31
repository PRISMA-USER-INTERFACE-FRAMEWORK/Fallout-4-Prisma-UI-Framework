#!/usr/bin/env python3
"""Validate the complete pinned Ultralight runtime tree.

The package root is the directory containing ``libs`` and ``resources`` (normally
``Data/PrismaUI_F4``).  The same command is used by release packaging, local deploy,
and deterministic tests so a build cannot silently pick up a stale loose DLL.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path


DEFAULT_MANIFEST = Path(__file__).resolve().parent.parent / "runtime" / "ultralight-1.4.0.manifest.json"


def _fail(message: str) -> int:
    print(f"ultralight runtime validation failed: {message}", file=sys.stderr)
    return 1


def validate(package_root: Path, manifest_path: Path) -> list[str]:
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read manifest {manifest_path}: {exc}") from exc

    if manifest.get("runtime") != "ultralight" or manifest.get("version") != "1.4.0":
        raise ValueError("manifest is not the pinned Ultralight 1.4.0 manifest")

    root = package_root.resolve()
    for directory in manifest.get("required_directories", []):
        path = root / directory
        if not path.is_dir():
            raise ValueError(f"required directory is missing: {path}")

    files = manifest.get("files")
    if not isinstance(files, dict) or not files:
        raise ValueError("manifest has no runtime files")

    expected = {Path(name).as_posix() for name in files}
    for name, metadata in files.items():
        relative = Path(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"manifest path escapes package root: {name}")
        path = root / relative
        if not path.is_file():
            raise ValueError(f"required runtime file is missing: {path}")
        size = metadata.get("size")
        if path.stat().st_size != size:
            raise ValueError(f"size mismatch for {name}: expected {size}, got {path.stat().st_size}")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest.lower() != str(metadata.get("sha256", "")).lower():
            raise ValueError(f"SHA-256 mismatch for {name}: expected {metadata.get('sha256')}, got {digest}")

    # A DLL beside the provider or an unlisted DLL in libs can defeat the pinned set through
    # Windows DLL search order.  Reject it rather than allowing a stale mixed-version install.
    for path in (root / "libs").glob("*.dll"):
        if path.relative_to(root).as_posix() not in expected:
            raise ValueError(f"unlisted DLL in runtime libs: {path}")

    return sorted(expected)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package_root", type=Path, help="directory containing libs/ and resources/")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    args = parser.parse_args()
    try:
        files = validate(args.package_root, args.manifest)
    except ValueError as exc:
        return _fail(str(exc))
    print(f"ultralight runtime validation passed: {args.package_root} ({len(files)} pinned files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
