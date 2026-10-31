#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/PrismaUI/WebCompositorOverlays.cpp").read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def body_of(signature: str) -> str:
    start = SOURCE.find(signature)
    if start == -1:
        fail(f"{signature} is missing from WebCompositorOverlays.cpp")
    end = SOURCE.find("\n    void WebCompositor::", start + 1)
    return SOURCE[start:] if end == -1 else SOURCE[start:end]


for name in ("DrawModelOverlays", "DrawCursor"):
    body = body_of(f"void WebCompositor::{name}(")
    begin_at = body.find("m_spriteBatch->Begin(")
    try_at = body.rfind("try {", 0, begin_at)
    end_at = body.find("m_spriteBatch->End();", begin_at)
    catch_at = body.find("} catch (const std::exception& e) {", end_at)
    if min(try_at, begin_at, end_at, catch_at) < 0 or not (try_at < begin_at < end_at < catch_at):
        fail(f"{name}: Begin..End must sit inside try, followed by a catch (std::exception)")
    handler = body[catch_at:]
    if "m_spriteBatch.reset();" not in handler:
        fail(f"{name}: the catch must reset m_spriteBatch so a poisoned batch is discarded")
    if "logger::error(" not in handler:
        fail(f"{name}: the catch must log the failure with logger::error")
    if name == "DrawModelOverlays" and handler.find("pruneCache();") < 0:
        fail("DrawModelOverlays: pruneCache() must still run after a failed draw")

if "__except" not in SOURCE or "RunSehGuarded(" not in SOURCE:
    fail("the draw must run under an SEH guard (SehInvoke/RunSehGuarded)")
if SOURCE.count("RunSehGuarded(draw") != 2 or SOURCE.count("g_spriteBatchFaulted = true;") != 2:
    fail("both draw paths must run under RunSehGuarded and set g_spriteBatchFaulted on an access violation")
if "EXCEPTION_ACCESS_VIOLATION" not in SOURCE or "__except (MemoryFaultFilter(GetExceptionCode()))" not in SOURCE:
    fail("the SEH filter must handle only memory faults, so C++ exceptions reach the std::exception catch")
if "if (g_spriteBatchFaulted)" not in SOURCE or "g_spriteBatchFaulted) return;" not in SOURCE:
    fail("both draw paths must return early once g_spriteBatchFaulted is set")
if "ShouldLogDrawError(" not in SOURCE:
    fail("repeated draw errors must be rate limited")

print("ok   spritebatch draw guard: Begin..End guarded by a catch that resets m_spriteBatch")
