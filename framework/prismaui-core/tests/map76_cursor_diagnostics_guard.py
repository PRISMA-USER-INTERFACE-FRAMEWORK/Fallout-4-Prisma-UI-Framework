from pathlib import Path

root = Path(__file__).resolve().parents[1]
diag = (root / "src" / "PrismaUI" / "MenuCursorDiagnostics.cpp").read_text(encoding="utf-8")
header = (root / "src" / "PrismaUI" / "MenuCursorDiagnostics.h").read_text(encoding="utf-8")
engine = (root / "src" / "Engine" / "EngineMenuCursor.h").read_text(encoding="utf-8")
web_input = (root / "src" / "PrismaUI" / "WebInput.cpp").read_text(encoding="utf-8")
web_runtime = (root / "src" / "PrismaUI" / "WebRuntimeFocus.cpp").read_text(encoding="utf-8")
ini = (root / "release-assets" / "PrismaUI_F4.ini").read_text(encoding="utf-8")

assert 'static_assert(sizeof(RE::MenuCursor) == 0x58' in engine
assert '#include "RE/M/MenuCursor.h"' in engine
assert "RE::MenuCursor::GetSingleton()" in engine
assert "RE::" not in diag
assert '#include "Engine/EngineMenuCursor.h"' in diag
assert "Engine::SnapshotMenuCursor()" in diag
assert 'GetPrivateProfileIntA("Diagnostics", "bMap76Cursor", 0' in diag
assert "bMap76CursorApplyLegacyClear" not in diag
assert "bMap76CursorApplyLegacyClear" not in header
assert "bMap76CursorApplyLegacyClear" not in ini

for field in (
    "cursorPosX",
    "cursorPosY",
    "minCursorX",
    "minCursorY",
    "maxCursorX",
    "maxCursorY",
    "leftConstraintPct",
    "rightConstraintPct",
    "topConstraintPct",
    "bottomConstraintPct",
    "registeredCursors",
    "forceOSCursorPos",
    "allowGamepadCursorOverride",
):
    assert f"snapshot.{field} = cursor->{field};" in engine, f"missing Engine MenuCursor field: {field}"
    assert f"cursor.{field}" in diag, f"missing diagnostic snapshot field: {field}"

for stage in ("focus-pre-constraint-policy", "focus-post-focus-task", "focus-next-present"):
    assert f'Dump("{stage}"' in diag, f"missing diagnostic stage: {stage}"

assert "void OnFocusedCaptureReleased();" in header
assert "MenuCursorDiagnostics::OnFocusedCaptureReleased();" in web_input
assert "g_pendingView.exchange(0" in diag
assert "g_pendingPresentGeneration.exchange(0" in diag

assert "ShouldApplyLegacyConstraintClear" not in header
assert "ShouldApplyLegacyConstraintClear" not in diag
assert "ShouldApplyLegacyConstraintClear" not in web_runtime
assert web_runtime.count("menuCursor->ClearConstraints();") == 1
focus_start = web_runtime.index("bool Internal::FocusImpl(")
clear_call = web_runtime.index("menuCursor->ClearConstraints();", focus_start)
session_start = web_runtime.index("if (!st.focusSession.exchange(true))", focus_start)
assert focus_start < clear_call < session_start, "Focus must clear inherited cursor constraints on every focus"

captured = diag.index("Engine::CaptureCursorInputSample(input)")
revalidated = diag.index("g_pendingPresentGeneration.load", captured)
assert revalidated > captured, "cursor samples must revalidate focus identity after heavy reads"

assert '#include "MenuCursorDiagnostics.h"' in web_input
assert "MenuCursorDiagnostics::OnFocusedCaptureArmed(focused);" in web_input
assert "MenuCursorDiagnostics::OnPresentCursorSample();" in web_input

print("MAP76 MenuCursor diagnostics guard passed")
