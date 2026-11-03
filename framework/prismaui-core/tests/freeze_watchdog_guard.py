#!/usr/bin/env python3
"""Source-contract guard for the FreezeDiagnostics freeze watchdog.

The watchdog is diagnostic-only and must stay that way: it observes lock-free atomics and writes
a log block / minidump, and it must NEVER call into Ultralight, D3D, or RE/game objects, take a
foreign lock, or attempt any recovery. The pure detection core must stay free of Windows/RE deps
so it keeps compiling in the Linux g++ gate. This guard enforces those invariants structurally and
checks the instrumentation is actually wired in.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"

failures = []


def fail(msg):
    failures.append(msg)


def read(rel):
    p = SRC / rel
    if not p.exists():
        fail(f"missing expected file: src/{rel}")
        return ""
    return p.read_text(encoding="utf-8", errors="replace")


def strip_comments(text):
    # Drop // line comments and /* */ block comments so matches are on real code, not prose.
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    return "\n".join(re.sub(r"//.*$", "", line) for line in text.splitlines())


# --- 1. The watchdog implementation must call nothing forbidden -----------------------------------
watchdog = strip_comments(read("PrismaUI/FreezeDiagnostics.cpp"))

FORBIDDEN_CALLS = {
    r"\bRE::": "touches an RE/game object",
    r"\bultralight::": "calls an Ultralight API",
    r"\bID3D11": "references a D3D11 interface",
    r"->Render\s*\(": "drives a renderer",
    r"->Update\s*\(": "drives a renderer",
    r"->RefreshDisplay\s*\(": "drives a renderer",
    r"DrainPendingGeneration": "drives the GPU driver",
    r"\bUnfocus\s*\(": "attempts focus recovery",
    r"\bReattach": "attempts hook recovery",
    r"AddTask\s*\(": "marshals game-thread work (recovery)",
    r"DestroyView": "mutates view lifetime",
    r"TerminateThread": "terminates a thread",
    r"SuspendThread": "suspends a thread directly",
    r"ResumeThread": "resumes a thread directly",
}
for pattern, why in FORBIDDEN_CALLS.items():
    if re.search(pattern, watchdog):
        fail(f"FreezeDiagnostics.cpp {why} (matched /{pattern}/) -- the watchdog must be diagnostic-only")

# The watchdog must not take a lock: it reads lock-free published atomics only.
for pattern in (r"std::mutex", r"std::lock_guard", r"std::unique_lock", r"\.lock\s*\(\s*\)"):
    if re.search(pattern, watchdog):
        fail(f"FreezeDiagnostics.cpp uses a lock (/{pattern}/) -- watchdog instrumentation must be lock-free")

# The watchdog must not pull in renderer/game headers.
for bad_inc in ("UltralightBackend.h", "IWebBackend.h", "WebRuntime.h", "ModelPreview.h", "d3d11.h",
                "Ultralight/Ultralight.h"):
    if re.search(r'#\s*include\s*[<"].*' + re.escape(bad_inc), watchdog):
        fail(f"FreezeDiagnostics.cpp includes {bad_inc} -- keep the watchdog decoupled from render/game code")

# It must actually write the diagnostic + a live-process minidump.
if "MiniDumpWriteDump" not in watchdog:
    fail("FreezeDiagnostics.cpp no longer calls MiniDumpWriteDump -- the freeze minidump was removed")
if "std::thread" not in watchdog:
    fail("FreezeDiagnostics.cpp no longer spawns the watchdog thread")

# --- 2. The pure core must stay Linux/g++-buildable (no Windows/RE) -------------------------------
for core_file in ("PrismaUI/FreezeDiagnosticsCore.h", "PrismaUI/FreezeDiagnosticsCore.cpp"):
    core = strip_comments(read(core_file))
    for pattern in (r"#\s*include\s*<Windows.h>", r"#\s*include\s*<windows.h>", r"\bRE::",
                    r"\bultralight::", r"\bID3D11", r"DbgHelp", r"std::atomic"):
        if re.search(pattern, core):
            fail(f"{core_file} references /{pattern}/ -- the core must stay pure so the Linux gate keeps building it")

# --- 3. The watchdog must be started, and boundaries instrumented --------------------------------
def requires(rel, needle, why):
    text = read(rel)
    if needle not in text:
        fail(f"src/{rel} is missing `{needle}` -- {why}")


requires("main.cpp", "FreezeDiagnostics::StartWatchdog(", "the watchdog is never started in the flat build")
requires("VR/VRMain.cpp", "FreezeDiagnostics::StartWatchdog(", "the watchdog is never started in the VR build")

for rel, why in (
    ("PrismaUI/UltralightBackend.cpp", "owner-thread / present stages are uninstrumented"),
    ("PrismaUI/GameThreadDispatcher.cpp", "game-thread callback / DropView wait are uninstrumented"),
    ("PrismaUI/WebRuntimePresent.cpp", "the Present path is uninstrumented"),
    ("PrismaUI/ModelPreview.cpp", "model-preview servicing is uninstrumented"),
):
    if "FreezeDiagnostics::" not in read(rel):
        fail(f"src/{rel} has no FreezeDiagnostics:: instrumentation -- {why}")

requires("PrismaUI/PresentProfiler.h", "FreezeDiagnostics::ScopedStage freezePresent_",
         "OnPresent no longer has an RAII Present-lane scope, so completed Presents can leave a stale active stage")
requires("PrismaUI/FreezeDiagnosticsCore.cpp", "IsActiveStage(present.stage)",
         "a stale Idle Present heartbeat would again be classified as a hard freeze during loading/Bink pauses")

if failures:
    print("freeze watchdog guard FAILED:")
    for f in failures:
        print(f"  - {f}")
    sys.exit(1)

print("freeze watchdog guard passed")
