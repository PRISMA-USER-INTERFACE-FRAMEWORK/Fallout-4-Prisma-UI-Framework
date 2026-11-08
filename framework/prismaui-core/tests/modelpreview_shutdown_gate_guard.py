#!/usr/bin/env python3
import re
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
GATE = ROOT / "src/PrismaUI/ModelPreviewShutdownGate.h"
SRC = ROOT / "src/PrismaUI/ModelPreview.cpp"
GATE_TEST = ROOT / "tests/modelpreview_shutdown_gate_tests.cpp"
REGISTRY = ROOT / "tests/ci_windows_gate.py"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


gate = GATE.read_text(encoding="utf-8")
text = SRC.read_text(encoding="utf-8")

for token in ("<d3d", "<windows.h>", "Engine/", "RE/", "REL/", "F4SE", "CommonLib", "PCH.h"):
    if token in gate:
        fail(f"ModelPreviewShutdownGate.h must stay host-testable; it references {token}")

enter = gate.find("EnterTick()")
begin = gate.find("BeginShutdown()")
first_check = gate.find("if (started_.load()) return {};", enter)
lock_taken = gate.find("std::unique_lock lock(mutex_, std::try_to_lock);", enter)
recheck = gate.find("if (lock.owns_lock() && started_.load()) lock.unlock();", enter)
set_flag = gate.find("started_.store(true);", begin)
shutdown_lock = gate.find("std::unique_lock<std::mutex>(mutex_)", begin)
if min(enter, begin, first_check, lock_taken, recheck, set_flag, shutdown_lock) < 0:
    fail("ShutdownGate must define EnterTick and BeginShutdown with the flag checks and mutex")
if not enter < first_check < lock_taken < recheck:
    fail("EnterTick must check the shutdown flag, lock, then recheck under the lock")
if not begin < set_flag < shutdown_lock:
    fail("BeginShutdown must set the shutdown flag before taking the gate mutex")

if '#include "ModelPreviewShutdownGate.h"' not in text:
    fail("ModelPreview.cpp must include the shutdown gate header")
tick = text.find("void TickCore(")
shutdown = text.find("void Shutdown() {")
if min(tick, shutdown) < 0 or not tick < shutdown:
    fail("TickCore must precede Shutdown in ModelPreview.cpp")
tick_body = text[tick:shutdown]
tick_enter_match = re.search(r"const auto tick = g_shutdownGate\.EnterTick\(\);\s*if \(!tick\.owns_lock\(\) \|\|", tick_body)
tick_enter = tick_enter_match.start() if tick_enter_match else -1
tick_work = tick_body.find("MarkTickCoreSeen();")
if tick_enter < 0 or tick_work < 0 or not tick_enter < tick_work:
    fail("TickCore must enter the shutdown gate and skip when it is not owned before any tick work")
if "TickCoreImpl(dev, ctx);" not in tick_body[tick_enter:]:
    fail("TickCore must run TickCoreImpl only inside the shutdown gate")

done = text.find("if (done.exchange(true)) return;", shutdown)
gate_begin = text.find("const auto shutdownLock = g_shutdownGate.BeginShutdown();", shutdown)
read_shutdown = text.find("BeginReadShutdown();", shutdown)
join_workers = text.find("ShutdownLoadWorkers();", shutdown)
if min(done, gate_begin, read_shutdown, join_workers) < 0 or not done < gate_begin < read_shutdown < join_workers:
    fail("Shutdown must keep its done latch, then take the shutdown gate before any teardown work")
shutdown_end = text.find("\n}", gate_begin)
if text.count("shutdownLock", shutdown, shutdown_end) != 1:
    fail("Shutdown must hold shutdownLock to the end of its body without touching it again")

registry = REGISTRY.read_text(encoding="utf-8")
if '("modelpreview_shutdown_gate_tests", "-pthread", "-Isrc", "tests/modelpreview_shutdown_gate_tests.cpp"),' not in registry:
    fail("modelpreview_shutdown_gate_tests must be registered in ci_windows_gate.py TESTS")
if not GATE_TEST.is_file():
    fail("modelpreview_shutdown_gate_tests.cpp is missing")

print("ok    ModelPreview shutdown gate wraps TickCore and Shutdown; header stays host-testable")
