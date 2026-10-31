#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
probe = (root / "src" / "Engine" / "EngineMenuCursorInputProbe.h").read_text(encoding="utf-8")
diag = (root / "src" / "PrismaUI" / "MenuCursorDiagnostics.cpp").read_text(encoding="utf-8")

# CommonLib-authoritative event split: raw mouse deltas and absolute cursor positions are distinct
# engine events. The MAP76 probe must observe both or it cannot locate where AE/Proton clamps.
assert '#include "RE/M/MouseMoveEvent.h"' in probe
assert '#include "RE/C/CursorMoveEvent.h"' in probe
assert "RE::INPUT_EVENT_TYPE::kMouseMove" in probe
assert "RE::INPUT_EVENT_TYPE::kCursorMove" in probe
assert "mouse->mouseInputX" in probe and "mouse->mouseInputY" in probe
assert "cursor->cursorPosX" in probe and "cursor->cursorPosY" in probe

# Observe both engine chains Prisma already uses instead of adding a speculative MenuCursor vtable hook.
assert '#include "RE/M/MenuControls.h"' in probe
assert '#include "RE/P/PlayerControls.h"' in probe
assert "controls->RegisterHandler(sink);" in probe
assert "CursorInputChain::Menu" in probe
assert "CursorInputChain::Gameplay" in probe

# CommonLib exposes PipboyMenu::pipboyCursorEnabled separately from MenuCursor's four constraints.
# Heavy engine state is captured only through the explicit post-throttle snapshot helper.
assert '#include "RE/P/PipboyMenu.h"' in probe
assert '#include "RE/U/UI.h"' in probe
assert "CaptureCursorInputSample" in probe
assert "sample.cursor = SnapshotMenuCursor();" in probe
assert "ui->GetMenu<RE::PipboyMenu>()" in probe
assert "menu->pipboyCursorEnabled" in probe
assert "sample.pipboyMenuOnStack = menu->OnStack();" in probe
assert "pipboyCursorEnabled" in diag

# The hot input sinks only decode cheap event metadata and invoke the callback. They do not snapshot
# MenuCursor or take the UI menu-map lock themselves.
emit = probe[probe.index("inline void EmitCursorInput"):probe.index("class MenuCursorInputSink")]
assert "callback(input);" in emit
assert "SnapshotMenuCursor" not in emit
assert "GetMenu<RE::PipboyMenu>" not in emit

# This probe is diagnostic only. It must never consume or mutate Fallout's input stream.
menu_sink = probe[probe.index("class MenuCursorInputSink"):probe.index("class GameplayCursorInputSink")]
gameplay_sink = probe[probe.index("class GameplayCursorInputSink"):probe.index("inline bool InstallMenuCursorInputProbe")]
assert "return false;" in menu_sink and "return false;" in gameplay_sink
assert "handled =" not in probe
assert "const_cast<RE::InputEvent" not in probe

# Rate limiting is per (chain, kind), happens before any heavy snapshot/lock, and preserves all four
# menu/gameplay x delta/position comparisons independently.
assert "std::array<std::atomic<std::uint64_t>, 4> g_lastInputLogMs" in diag
assert "InputLogSlotIndex" in diag
assert "static_cast<std::size_t>(chain) * 2u + static_cast<std::size_t>(kind)" in diag
claim = diag.index("ClaimInputLogSlot(input.chain, input.kind)")
capture = diag.index("Engine::CaptureCursorInputSample(input)")
assert claim < capture, "rate limiting must happen before MenuCursor/UI snapshots"
assert "kIntervalMs = 50" in diag

# Persistent sinks must not keep attributing input to a stale MAP76 focus after release.
assert "void OnFocusedCaptureReleased()" in diag
assert "g_pendingView.exchange(0" in diag
assert "g_pendingPresentGeneration.exchange(0" in diag
assert "g_focusGeneration.fetch_add(1" in diag

print("MAP76 CommonLib mouse-input probe guard passed")
