#!/usr/bin/env python3
"""Fail if the committed runtime header has drifted from the authored JSON manifest."""

from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
gen_path = ROOT / "scripts" / "gen-runtime-manifest-header.py"
spec = importlib.util.spec_from_file_location("gen_runtime_manifest_header", gen_path)
assert spec and spec.loader
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)

expected = gen.render(gen.DEFAULT_MANIFEST)
actual = gen.DEFAULT_HEADER.read_text(encoding="utf-8")

if actual != expected:
    raise SystemExit(
        f"{gen.DEFAULT_HEADER.name} is out of sync with the JSON manifest. "
        "Run: python scripts/gen-runtime-manifest-header.py"
    )

print("ultralight manifest parity test passed")
