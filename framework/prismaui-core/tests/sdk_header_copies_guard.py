#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CANONICAL = ROOT / "src" / "PrismaUI_F4_API.h"
COPIES = (
    ROOT / "example-f4se-plugin" / "src" / "PrismaUI_F4_API.h",
)

expected = CANONICAL.read_bytes()
checked = 0
for copy in COPIES:
    if not copy.is_file():
        continue
    if copy.read_bytes() != expected:
        raise SystemExit(f"FAIL: {copy} differs from src/PrismaUI_F4_API.h; copy the canonical header over it")
    checked += 1
print(f"sdk_header_copies_guard ok ({checked} shipped copies match src/PrismaUI_F4_API.h)")
