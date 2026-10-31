#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
stager_path = ROOT / "scripts" / "stage-ultralight-runtime.py"
spec = importlib.util.spec_from_file_location("ultralight_stager_rollback", stager_path)
assert spec and spec.loader
stager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(stager)

with tempfile.TemporaryDirectory(prefix="prisma-ultralight-deploy-") as temp:
    root = Path(temp)
    dist = root / "dist"
    mods = root / "mods"
    destination = mods / "PrismaUI_F4"
    staging = mods / ".PrismaUI_F4.staging"

    (dist / "PrismaUI_F4").mkdir(parents=True)
    (dist / "PrismaUI_F4" / "new.txt").write_text("new", encoding="utf-8")
    destination.mkdir(parents=True)
    (destination / "old.txt").write_text("old", encoding="utf-8")

    original_validate = stager.validate
    original_replace = stager.os.replace
    stager.validate = lambda *_args, **_kwargs: None

    def failing_replace(source, target):
        if Path(source) == staging and Path(target) == destination:
            raise OSError("simulated staging replacement failure")
        return original_replace(source, target)

    stager.os.replace = failing_replace
    try:
        try:
            stager.deploy(dist, mods, root / "manifest.json")
        except OSError as exc:
            assert "simulated staging replacement failure" in str(exc)
        else:
            raise AssertionError("failed deployment must propagate the replacement error")
    finally:
        stager.os.replace = original_replace
        stager.validate = original_validate

    assert destination.is_dir()
    assert (destination / "old.txt").read_text(encoding="utf-8") == "old"
    assert not (destination / "new.txt").exists()
    assert not (mods / ".PrismaUI_F4.previous").exists()

print("ultralight full deploy rollback test passed")
