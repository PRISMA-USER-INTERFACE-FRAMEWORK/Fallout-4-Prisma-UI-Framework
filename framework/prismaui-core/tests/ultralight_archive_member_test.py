#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
stager_path = ROOT / "scripts" / "stage-ultralight-runtime.py"
spec = importlib.util.spec_from_file_location("ultralight_stager_members", stager_path)
assert spec and spec.loader
stager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stager)


def rejected(archive: Path, expected: str) -> None:
    with zipfile.ZipFile(archive) as release:
        try:
            stager.normalized_archive_members(release)
        except ValueError as exc:
            assert expected in str(exc)
        else:
            raise AssertionError(f"archive member policy should reject {archive.name}")


with tempfile.TemporaryDirectory(prefix="prisma-ultralight-archive-") as temp:
    root = Path(temp)

    duplicate = root / "duplicate-paths.zip"
    with zipfile.ZipFile(duplicate, "w") as release:
        release.writestr("Data/PrismaUI_F4/test.txt", b"first")
        release.writestr("Data\\PrismaUI_F4\\test.txt", b"second")
    rejected(duplicate, "duplicate normalized path")

    case_collision = root / "case-collision.zip"
    with zipfile.ZipFile(case_collision, "w") as release:
        release.writestr("Data/PrismaUI_F4/test.txt", b"first")
        release.writestr("data/prismaui_f4/TEST.txt", b"second")
    rejected(case_collision, "Windows case-colliding paths")

    file_directory_collision = root / "file-directory-collision.zip"
    with zipfile.ZipFile(file_directory_collision, "w") as release:
        release.writestr("Data/", b"")
        release.writestr("data", b"file")
    rejected(file_directory_collision, "Windows case-colliding paths")

    unsafe_paths = (
        "../outside.txt",
        "/absolute.txt",
        "C:/drive.txt",
        "Data/./alias.txt",
        "Data/../outside.txt",
        "Data//alias.txt",
        "\\\\server\\share\\file.txt",
    )
    for index, member in enumerate(unsafe_paths):
        archive = root / f"unsafe-{index}.zip"
        with zipfile.ZipFile(archive, "w") as release:
            release.writestr(member, b"unsafe")
        rejected(archive, "unsafe path")

    unsafe_windows_paths = (
        "Data/file.txt:stream",
        "Data/trailing-dot.",
        "Data/trailing-space ",
        "Data/CON",
        "Data/nul.txt",
        "Data/COM1.bin",
        "Data/LPT9",
        "Data/bad?.txt",
    )
    for index, member in enumerate(unsafe_windows_paths):
        archive = root / f"unsafe-windows-{index}.zip"
        with zipfile.ZipFile(archive, "w") as release:
            release.writestr(member, b"unsafe")
        rejected(archive, "unsafe Windows path")

    valid = root / "valid.zip"
    with zipfile.ZipFile(valid, "w") as release:
        release.writestr("Data/", b"")
        release.writestr("Data/PrismaUI_F4/test.txt", b"ok")
    with zipfile.ZipFile(valid) as release:
        members = stager.normalized_archive_members(release)
        assert members["Data/"] == "Data/"
        assert members["Data/PrismaUI_F4/test.txt"] == "Data/PrismaUI_F4/test.txt"

stager_source = stager_path.read_text(encoding="utf-8")
assert "windows_key = path_text.casefold()" in stager_source
assert "validate_windows_archive_component(component, original)" in stager_source
assert "PurePosixPath(name).name.casefold()" in stager_source
assert '"prismaui_f4_cef/" in name.casefold()' in stager_source
assert 'name.casefold().startswith("data/prismaui_f4/views/inspector/")' in stager_source

print("ultralight archive member test passed")
