#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/PrismaUI/WebInputWndProc.cpp").read_text(encoding="utf-8")
runtime = (ROOT / "src/PrismaUI/WebRuntimeInput.cpp").read_text(encoding="utf-8")
resize = (ROOT / "src/PrismaUI/WebRuntimeFrame.cpp").read_text(encoding="utf-8")
present = (ROOT / "src/PrismaUI/WebRuntimePresent.cpp").read_text(encoding="utf-8")
lifecycle = (ROOT / "src/PrismaUI/WebRuntimeViews.cpp").read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


activation_start = runtime.find("bool OnWindowActivation(bool active)")
activation_end = runtime.find("bool HasAnyActiveFocus()", activation_start)
if activation_start < 0 or activation_end < 0:
    fail("could not locate inspector window-activation handling")
activation_body = runtime[activation_start:activation_end]
for token in (
    "st.localInspectorOwner.load(std::memory_order_acquire)",
    "st.backend->IsInspectorVisible(owner)",
    "EndInspectorGesture();",
    "WebInput::SetCaptureActive(active, active);",
    "SyncVanillaCursorVisibility();",
):
    if token not in activation_body:
        fail(f"inspector window activation lost required capture handoff: {token}")

if "void QueueActivationRestore(ViewId view, bool pauseGame, bool disableFocusMenu," not in runtime:
    fail("activation restore lost its deferred restore helper")
restore_start = runtime.find("void QueueActivationRestore(")
restore_end = runtime.find("void MapClientPointToBrowser(", restore_start)
restore_body = runtime[restore_start:restore_end]
for token in (
    "GameThreadDispatcher::Dispatch(restore, view)",
    "st.focusedView.load(std::memory_order_acquire) != 0",
    "!IsRuntimeViewVisible(view)",
    "IsAnyPanelVisible(view)",
    "SetInputRegions(view, inputState.regions.data()",
):
    if token not in restore_body:
        fail(f"activation restore lost required deferred guard: {token}")
if "tasks->AddTask(std::move(restore))" in restore_body:
    fail("activation restore must fail closed when the verified dispatcher rejects it")

destroy_start = lifecycle.find("void Destroy(ViewId view)")
destroy_end = lifecycle.find("void ShutdownFrameworkOnWindowThread()", destroy_start)
if destroy_start < 0 or destroy_end < 0:
    fail("could not locate view destruction cleanup")
destroy_body = lifecycle[destroy_start:destroy_end]
restore_clear = destroy_body.find("st.activationRestoreView.compare_exchange_strong(expectedRestore, 0")
state_clear = destroy_body.find("st.activationRestoreInputState = {}")
backend_destroy = destroy_body.find("st.backend->DestroyView(view)")
if restore_clear < 0 or state_clear < restore_clear or backend_destroy < state_clear:
    fail("Destroy must clear a pending activation restore before destroying the view")


mouse_start = source.find("bool IsMouseMessage(UINT msg)")
mouse_end = source.find("uint32_t MouseButtonBit", mouse_start)
if mouse_start < 0 or mouse_end < 0:
    fail("could not locate IsMouseMessage")
mouse_body = source[mouse_start:mouse_end]
if "case WM_MOUSEHWHEEL:" not in mouse_body:
    fail("WM_MOUSEHWHEEL must remain classified as a mouse message")

switch_start = source.find("switch (uMsg)")
if switch_start < 0:
    fail("could not locate SubclassProc dispatch switch")
switch_body = source[switch_start:]
horizontal_case = re.search(
    r"case\s+WM_MOUSEHWHEEL\s*:(?P<body>.*?)(?=case\s+WM_|default\s*:)",
    switch_body,
    re.DOTALL,
)
if horizontal_case is None:
    fail("could not locate WM_MOUSEHWHEEL dispatch case")
horizontal_dispatch = re.search(
    r"WebRuntime::SendMouseWheel\s*\(\s*x\s*,\s*y\s*,\s*"
    r"GET_WHEEL_DELTA_WPARAM\s*\(\s*wParam\s*\)\s*,\s*0\s*,\s*"
    r"mouseModifiers\s*,\s*rescale\s*\)",
    horizontal_case.group("body"),
)
if horizontal_dispatch is None:
    fail("WM_MOUSEHWHEEL must route its wheel delta through WebRuntime deltaX")

if "if (uMsg == WM_MOUSEWHEEL || uMsg == WM_MOUSEHWHEEL)" not in source:
    fail("vertical and horizontal wheel messages must both convert screen coordinates to client space")

subclass_start = source.find("LRESULT CALLBACK SubclassProc(")
capture_start = source.find("if (g_capture.load())", subclass_start)
deactivation = source.find("if (uMsg == WM_ACTIVATEAPP && !wParam)", subclass_start)
if subclass_start < 0 or capture_start < 0 or deactivation < 0:
    fail("SubclassProc must release focus on WM_ACTIVATEAPP deactivation")
if not deactivation < capture_start:
    fail("window deactivation must release focus before the capture/input path")
if "WebRuntime::Unfocus(focused)" not in source[deactivation:capture_start]:
    fail("window deactivation must release the focused view")
if "CancelMouseCapture(hwnd);" not in source[deactivation:capture_start]:
    fail("window deactivation must cancel Prisma native mouse capture before releasing focus")
if "const bool inspectorOwnsWindow = WebRuntime::OnWindowActivation(false);" not in source[deactivation:capture_start]:
    fail("window deactivation must suspend inspector capture before releasing ordinary focus")
if "if (focused && !inspectorOwnsWindow) WebRuntime::Unfocus(focused);" not in source[deactivation:capture_start]:
    fail("window deactivation must preserve an active inspector owner")
if "WebRuntime::OnWindowActivation(true);" not in source[deactivation:capture_start]:
    fail("window activation must resume an active inspector owner")

acquire_start = source.find("void AcquireMouseCapture(HWND hwnd, uint32_t buttonBit)")
release_start = source.find("void ReleaseMouseCapture(HWND hwnd, uint32_t buttonBit)", acquire_start)
cancel_start = source.find("void CancelMouseCapture(HWND hwnd)", release_start)
modifiers_start = source.find("uint32_t Modifiers()", cancel_start)
if min(acquire_start, release_start, cancel_start, modifiers_start) < 0:
    fail("could not locate Prisma native mouse capture helpers")
acquire_body = source[acquire_start:release_start]
release_body = source[release_start:cancel_start]
cancel_body = source[cancel_start:modifiers_start]
for token in (
    "g_mouseCaptureMask.fetch_or(buttonBit, std::memory_order_acq_rel)",
    "SetCapture(hwnd);",
    "GetCapture() == hwnd",
    "g_nativeMouseCapture.store",
):
    if token not in acquire_body:
        fail(f"first Prisma mouse button must acquire Win32 capture: {token}")
for token in (
    "g_mouseCaptureMask.fetch_and(~buttonBit, std::memory_order_acq_rel)",
    "previous & ~buttonBit",
    "g_nativeMouseCapture.exchange(false, std::memory_order_acq_rel)",
    "GetCapture() == hwnd",
    "ReleaseCapture();",
):
    if token not in release_body:
        fail(f"final Prisma mouse button release must release Win32 capture: {token}")
for token in (
    "g_mouseCaptureMask.store(0, std::memory_order_release)",
    "g_nativeMouseCapture.exchange(false, std::memory_order_acq_rel)",
    "GetCapture() == hwnd",
    "ReleaseCapture();",
    "WebRuntime::EndInspectorGesture();",
):
    if token not in cancel_body:
        fail(f"capture cancellation must clear logical/native drag ownership: {token}")

capture_changed = source.find("if (uMsg == WM_CAPTURECHANGED)", subclass_start)
if capture_changed < 0 or capture_changed > deactivation:
    fail("WM_CAPTURECHANGED must clear Prisma drag state before deactivation/input routing")
capture_changed_body = source[capture_changed:deactivation]
for token in (
    "g_nativeMouseCapture.store(false, std::memory_order_release)",
    "g_mouseCaptureMask.store(0, std::memory_order_release)",
    "WebRuntime::EndInspectorGesture();",
):
    if token not in capture_changed_body:
        fail(f"WM_CAPTURECHANGED must clear stale Prisma drag state: {token}")

if "g_pendingSurrogate.Reset();" not in capture_changed_body:
    fail("WM_CAPTURECHANGED must drop a pending high surrogate with the other capture state")
input_source = (ROOT / "src/PrismaUI/WebInput.cpp").read_text(encoding="utf-8")
set_capture_start = input_source.find("void SetCaptureActive(bool active, bool swallow)")
set_capture_end = input_source.find("bool IsCaptureActive()", set_capture_start)
if set_capture_start < 0 or set_capture_end < 0 or "g_pendingSurrogate.Reset();" not in input_source[set_capture_start:set_capture_end]:
    fail("SetCaptureActive must drop a pending high surrogate on every focus, view, or capture change")

inactive_release = source.find(
    "if (!g_capture.load(std::memory_order_acquire) && IsMouseButtonUp(uMsg)", deactivation
)
if inactive_release < 0 or inactive_release > capture_start:
    fail("button-up must release native capture even after Prisma routing is disabled mid-drag")
inactive_release_body = source[inactive_release:capture_start]
for token in (
    "g_nativeMouseCapture.load(std::memory_order_acquire)",
    "CancelMouseCapture(hwnd);",
):
    if token not in inactive_release_body:
        fail(f"mid-drag capture disable must retain a cleanup path: {token}")

for token in (
    "EF_PRISMA_POINTER_CAPTURE = 1u << 30",
    "EF_PRISMA_POINTER_CAPTURE_CONTINUES = 1u << 31",
    "if (captureMask != 0) mouseModifiers |= EF_PRISMA_POINTER_CAPTURE;",
    "captureMask & ~buttonBit",
):
    if token not in source:
        fail(f"native mouse capture no longer tags inspector routing with {token}")

capture_routing = "captureMask != 0 && (uMsg == WM_MOUSEMOVE || buttonBit != 0)"
if capture_routing not in source:
    fail("an active Prisma mouse capture must retain every button event, including secondary buttons")

for button_name in ("kMouseLeft", "kMouseRight", "kMouseMiddle"):
    if f"AcquireMouseCapture(hwnd, {button_name});" not in source:
        fail(f"{button_name} press must acquire/extend native mouse capture")
    if f"ReleaseMouseCapture(hwnd, {button_name});" not in source:
        fail(f"{button_name} release must shrink/release native mouse capture")

for stale_pattern in (
    "g_mouseCaptureMask.fetch_or(kMouseLeft)",
    "g_mouseCaptureMask.fetch_or(kMouseRight)",
    "g_mouseCaptureMask.fetch_or(kMouseMiddle)",
    "g_mouseCaptureMask.fetch_and(~kMouseLeft)",
    "g_mouseCaptureMask.fetch_and(~kMouseRight)",
    "g_mouseCaptureMask.fetch_and(~kMouseMiddle)",
):
    if stale_pattern in source:
        fail(f"button dispatch bypasses the native mouse capture helpers: {stale_pattern}")

route_start = runtime.find("bool ShouldRouteFocusedMouse(")
move_start = runtime.find("void SendMouseMove(")
click_start = runtime.find("void SendMouseClick(", move_start)
wheel_start = runtime.find("void SendMouseWheel(", click_start)
key_start = runtime.find("void SendKey(", wheel_start)
if route_start < 0 or move_start < 0 or click_start < 0 or wheel_start < 0 or key_start < 0:
    fail("could not locate WebRuntime mouse routing functions")
route_body = runtime[route_start:move_start]
move_body = runtime[move_start:click_start]
click_body = runtime[click_start:wheel_start]
wheel_body = runtime[wheel_start:key_start]

if "st.inspectorPointerCaptureTarget" not in runtime:
    fail("inspector drag target must survive pointer movement outside inspector bounds")
if "st.inspectorPointerCaptureTarget.load(std::memory_order_acquire) != 0" not in route_body:
    fail("captured inspector input must be admitted before overlay-region hit testing")
if route_body.find("st.inspectorPointerCaptureTarget.load") > route_body.find("st.inputRegionMutex"):
    fail("inspector capture admission must precede the focused input-region policy")
if "(modifiers & kPrismaPointerCaptureModifier) != 0" not in move_body:
    fail("inspector move routing must use Prisma's native capture tag, not physical button state")
if "if (pointerCaptured && pointerCapture)" not in move_body:
    fail("captured inspector drag must route before ordinary hover hit-testing")
if "st.backend->VisibleInspectorView() == pointerCapture" not in move_body:
    fail("captured drag must validate the inspector is still live before dispatch")
if move_body.find("if (pointerCaptured && pointerCapture)") > move_body.find(
    "const ViewId inspector = st.backend->InspectorTargetAt"
):
    fail("captured inspector drag must take precedence over current-pointer hit-testing")

for token in (
    "(modifiers & kPrismaPointerCaptureModifier) != 0",
    "(modifiers & kPrismaPointerCaptureContinuesModifier) != 0",
    "if (!captureLive)",
    "if (up && !captureContinues)",
):
    if token not in click_body:
        fail(f"inspector click capture lost required release behavior: {token}")
stale_start = click_body.find("if (!captureLive)")
stale_end = click_body.find("inspector = pointerCapture", stale_start)
if stale_start < 0 or stale_end < 0 or "return;" not in click_body[stale_start:stale_end]:
    fail("a release for an inspector that closed mid-drag must be swallowed, not handed to Dock/focus")
if "st.inspectorPointerCaptureTarget.store(0, std::memory_order_release);" not in click_body:
    fail("final inspector button release must clear pointer capture")

for token in (
    "(modifiers & kPrismaPointerCaptureModifier) != 0",
    "st.inspectorPointerCaptureTarget.load(std::memory_order_acquire)",
    "st.backend->VisibleInspectorView() == pointerCapture",
    "InputRouting::DispatchScroll(*st.backend, pointerCapture, {deltaX, deltaY});",
):
    if token not in wheel_body:
        fail(f"captured inspector wheel routing lost required behavior: {token}")
if wheel_body.find("if (pointerCaptured && pointerCapture)") > wheel_body.find(
    "const ViewId inspector = st.backend->InspectorTargetAt"
):
    fail("captured inspector wheel must route before ordinary hover hit-testing")
if "return;" not in wheel_body[wheel_body.find("if (pointerCaptured && pointerCapture)"):wheel_body.find("int localX")]:
    fail("wheel input for a closed captured inspector must be swallowed rather than leaked")

for token in (
    "kCtrlModifier = 1u << 2",
    "kInspectorTitleHeight = 24",
    "kInspectorResizeGrip = 14",
    "kInspectorMinimumSize = 32",
):
    if token not in runtime:
        fail(f"native inspector gesture geometry lost required constant: {token}")

for token in (
    "st.backend->InspectorTargetAt(x + kInspectorResizeGrip, y, probeX, probeY) != inspector",
    "st.backend->InspectorTargetAt(x, y + kInspectorResizeGrip, probeX, probeY) != inspector",
    "originX + kInspectorMinimumSize - 1",
    "originY + kInspectorMinimumSize - 1",
    "if (!minimumWidth || !minimumHeight) return;",
    "st.backend->BeginInspectorResize(inspector, localX, localY);",
    "st.backend->BeginInspectorMove(inspector, localX, localY);",
    "(modifiers & kCtrlModifier) == 0",
):
    if token not in click_body:
        fail(f"native inspector gesture start lost required behavior: {token}")

gesture_start = click_body.find("if (inspector && !up && button == 0 && !pointerCapture")
capture_store = click_body.find("st.inspectorPointerCaptureTarget.store(inspector, std::memory_order_release);")
if gesture_start < 0 or capture_store < 0 or gesture_start > capture_store:
    fail("native inspector gesture must start before pointer capture is armed")

resize_begin = resize.find("void OnResizeBegin(")
resize_complete = resize.find("void OnResizeComplete(", resize_begin)
resize_direct = resize.find("void OnResize(", resize_complete)
if resize_begin < 0 or resize_complete < 0 or resize_direct < 0:
    fail("could not locate WebRuntime resize boundaries")
if "EndInspectorGesture();" not in resize[resize_begin:resize_complete]:
    fail("swap-chain resize must end an active inspector gesture before backend resize")
if "EndInspectorGesture();" not in resize[resize_direct:]:
    fail("direct resize must end an active inspector gesture before backend resize")

present_resize = present.find("if (bw != s_pushedW || bh != s_pushedH)")
present_backend_resize = present.find("st.backend->Resize(bw, bh);", present_resize)
if present_resize < 0 or present_backend_resize < 0:
    fail("could not locate present-time backend resize boundary")
if "EndInspectorGesture();" not in present[present_resize:present_backend_resize]:
    fail("present-time backbuffer resize must end an active inspector gesture before backend resize")

ncdestroy_start = source.find("if (uMsg == WM_NCDESTROY) {", subclass_start)
ncdestroy_end = source.find("}\n", ncdestroy_start)
if ncdestroy_start < 0 or ncdestroy_end < 0:
    fail("could not locate the WM_NCDESTROY branch in SubclassProc")
if "WebRuntime::ShutdownFrameworkOnWindowThread();" not in source[ncdestroy_start:ncdestroy_end]:
    fail("WM_NCDESTROY must call WebRuntime::ShutdownFrameworkOnWindowThread, the production path to ModelPreview::Shutdown")

print("ok    WebInput routes wheel and inspector pointer-capture events safely")
