from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WEB_INPUT = (ROOT / "src/PrismaUI/WebInput.cpp").read_text(encoding="utf-8")
RUNTIME_INPUT = (ROOT / "src/PrismaUI/WebRuntimeInput.cpp").read_text(encoding="utf-8")
RUNTIME_FOCUS = (ROOT / "src/PrismaUI/WebRuntimeFocus.cpp").read_text(encoding="utf-8")
RUNTIME_LIFECYCLE = (ROOT / "src/PrismaUI/WebRuntimeInspector.cpp").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unclosed function: {signature}")


set_capture = function_body(WEB_INPUT, "void SetCaptureActive(bool active, bool swallow)")
capture_end = set_capture.find("WebRuntime::EndInspectorGesture();")
capture_reset = set_capture.find("g_mouseCaptureMask.store(0")
assert -1 not in (capture_end, capture_reset) and capture_end < capture_reset, (
    "capture ownership changes must end inspector drag/pointer capture before resetting the logical mouse state"
)

end_gesture = function_body(RUNTIME_LIFECYCLE, "void EndInspectorGesture()")
pointer_target_clear = end_gesture.find("st.inspectorPointerCaptureTarget.store(0")
backend_end = end_gesture.find("st.backend->EndInspectorGesture()")
assert -1 not in (pointer_target_clear, backend_end) and pointer_target_clear < backend_end, (
    "ending an inspector gesture must clear Prisma's captured inspector id before backend gesture teardown"
)

mouse_move = function_body(RUNTIME_INPUT, "void SendMouseMove(")
pointer_clear = mouse_move.find("if (!pointerCaptured) st.inspectorPointerCaptureTarget.store(0")
gesture_update = mouse_move.find("const ViewId capturedInspector = UpdateInspectorGesture")
assert -1 not in (pointer_clear, gesture_update) and pointer_clear < gesture_update, (
    "mouse movement must expose the stale-gesture hazard this handoff guard protects"
)

unfocus = function_body(RUNTIME_FOCUS, "void Unfocus(ViewId view)")
inspector_branch = unfocus.find(
    "if (st.focusedView.load() == 0 && st.localInspectorOwner.load(std::memory_order_acquire))"
)
assert inspector_branch != -1, "Unfocus lost the inspector-owner handoff branch"
unfocus_end = unfocus.find("EndInspectorGesture();", inspector_branch)
unfocus_capture = unfocus.find("WebInput::SetCaptureActive(true, true);", inspector_branch)
assert -1 not in (unfocus_end, unfocus_capture) and unfocus_end < unfocus_capture, (
    "unfocusing an inspected owner must end its active drag before capture state is reset"
)

focus = function_body(
    RUNTIME_FOCUS,
    "bool Internal::FocusImpl(ViewId view, bool pauseGame, bool disableFocusMenu,",
)
focus_end = focus.find("if (inspectorOwnsBackendFocus) EndInspectorGesture();")
focus_capture = focus.find("WebInput::SetCaptureActive(true, true);")
assert -1 not in (focus_end, focus_capture) and focus_end < focus_capture, (
    "logical focus handoff under DevTools must end an active inspector drag before capture state is reset"
)

lifecycle = function_body(
    RUNTIME_LIFECYCLE,
    "void ApplyInspectorBackendStateChanged(ViewId owner, bool visible, bool fallback)",
)
stale_reopen = lifecycle.find("if (!fallback &&")
release_gate = lifecycle.find("if (!releasedOwner && !releasedPending) return;")
focused_capture = lifecycle.find("WebInput::SetCaptureActive(true, true);", release_gate)
passive_capture = lifecycle.find("WebInput::SetCaptureActive(captureOwner !=", release_gate)
assert -1 not in (stale_reopen, release_gate, focused_capture, passive_capture), (
    "inspector close must preserve the stale-reopen guard and confirmed ownership handoff paths"
)
assert stale_reopen < release_gate < focused_capture < passive_capture, (
    "backend-confirmed inspector close must reject stale reopen callbacks before any capture reset"
)

print("inspector capture handoff guard passed")
