#!/usr/bin/env python3
"""Pin the virtual-pointer lifecycle rules that keep an accepted click owned.

An accepted down owns {view, generation}. Nothing may clear it except a transition that genuinely
ends that ownership. These are the rules the live audit found broken:

  * destroying/forgetting an unrelated view must not discard another view's hold
  * focus admission must cancel the hold with its owner before the backend focus mutation that
    changes the controller generation -- never silently reset an accepted down
  * opening the inspector must cancel before the backend transitions to inspector-visible, because
    that transition changes the generation before the visible callback runs
  * a normal release is not a session loss and must not reset the movement clock while hasSample
    stays true
  * an accepted click must stay releasable while the compositor cursor policy hides the cursor
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src" / "PrismaUI"


def read(name: str) -> str:
    return (SRC / name).read_text(encoding="utf-8", errors="replace")


def body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unclosed function: {signature}")


state_header = read("VirtualPointerClickState.h")
pointer_header = read("VirtualPointer.h")
pointer = read("VirtualPointer.cpp")
views = read("WebRuntimeViews.cpp")
core = read("WebRuntime.cpp")
focus = read("WebRuntimeFocus.cpp")
inspector = read("WebRuntimeInspector.cpp")
web_input_h = read("WebInput.h")
web_input = read("WebInput.cpp")


def fail(message: str) -> None:
    print(f"FAIL: {message}")
    raise SystemExit(1)


if "DiscardIfOwnerView(State& state, std::uint64_t view)" not in state_header:
    fail("the pure owner-view discard helper DiscardIfOwnerView is missing")
if "inline bool DiscardIfOwnerView" not in state_header:
    fail("DiscardIfOwnerView must stay header-inline so both pointer TUs share one definition")

if "void DiscardHeldClick(std::uint64_t view) noexcept;" not in pointer_header:
    fail("DiscardHeldClick must take the destroyed view")
if "void DiscardHeldClick() noexcept;" in pointer_header:
    fail("the global unrelated-view DiscardHeldClick overload came back")

discard = body(pointer, "void DiscardHeldClick(std::uint64_t view) noexcept")
if "DiscardIfOwnerView(g_click, view)" not in discard:
    fail("DiscardHeldClick must go through the owner-view helper")
if "CancelClick(g_click, {})" in discard:
    fail("DiscardHeldClick must not clear an owner it does not own")

if "VirtualPointer::DiscardHeldClick(view);" not in body(views, "void Destroy(ViewId view)"):
    fail("Destroy must discard only the view it is destroying")
if "VirtualPointer::DiscardHeldClick(view);" not in body(core, "void Internal::ForgetViewShell(ViewId view)"):
    fail("ForgetViewShell must discard only the view it is forgetting")
for name, source in (("WebRuntimeViews.cpp", views), ("WebRuntime.cpp", core)):
    if "VirtualPointer::DiscardHeldClick()" in source:
        fail(f"{name} still uses the global DiscardHeldClick()")

focus_impl = body(focus, "bool Internal::FocusImpl(")
if "VirtualPointer::CancelHeldClick();" not in focus_impl:
    fail("FocusImpl must cancel a held click with its owner before focus mutation")
if "VirtualPointer::ResetSession();" in focus_impl:
    fail("FocusImpl must not silently reset an accepted down")
cancel_at = focus_impl.index("VirtualPointer::CancelHeldClick();")
if focus_impl.index("st.backend->FocusView(view);") < cancel_at:
    fail("FocusImpl must cancel before the backend FocusView that changes generation")
if focus_impl.index("st.backend->UnfocusView(prevFocused);") < cancel_at:
    fail("FocusImpl must cancel before the backend UnfocusView that changes generation")
if focus_impl.index("GameThreadDispatcher::IsReady()") > cancel_at:
    fail("FocusImpl must finish fail-fast validation before cancelling")

inspector_visible = "st.backend->SetInspectorVisibility("
transitions = [i for i in range(len(inspector)) if inspector.startswith("st.backend->SetInspectorVisibility(", i)
               and inspector[i:inspector.index("\n", i)].rstrip().endswith(", true);")]
if not transitions:
    fail("no backend inspector-visible transition found in WebRuntimeInspector.cpp")
for at in transitions:
    preceding = inspector[max(0, at - 400):at]
    if "VirtualPointer::CancelHeldClick();" not in preceding:
        fail("every inspector-visible backend transition must cancel the held click before it")

on_button = body(pointer, "bool OnButton(")
if "if (!pressed) return g_click.held;" not in on_button:
    fail("a held sample must stay owned once an A-down was admitted")
if "!= VirtualPointerClick::Begin::kAdmitted)" not in on_button or "return g_click.held;" not in on_button:
    fail("a rejected A-down must report current ownership so held/release stay unowned")
if "if (!g_click.held) return false;" not in on_button:
    fail("a release with no owned A-down must not be consumed")
if on_button.count("Dispatch(route, false)") != 1:
    fail("an owned release must dispatch exactly one mouse-up")

released_branch = on_button[on_button.index("if (released) {"):]
released_branch = released_branch[:released_branch.index("return true;")]
if "ResetTiming()" in released_branch or "ResetSession()" in released_branch:
    fail("a normal release is not a session loss; it must not reset the movement clock")

reset = body(pointer, "void ResetSession() noexcept")
if "VirtualPointerClick::ResetSession(g_click)" not in reset or "ResetTiming()" not in reset:
    fail("ResetSession must clear the click state and the movement clock together")

if "int GetTrackedCursorX();" not in web_input_h or "int GetTrackedCursorY();" not in web_input_h:
    fail("WebInput must expose tracked cursor coordinates independent of the compositor policy")
if "int GetTrackedCursorX() { return g_lastCursorX.load(); }" not in web_input:
    fail("GetTrackedCursorX must not consult CompositorCursorHidden")
if "int GetLastCursorX()" not in web_input or "CompositorCursorHidden() ? -1" not in web_input:
    fail("GetLastCursorX must keep the presentation-hidden sentinel")

owner_route = body(pointer, "Route EvaluateOwnerRoute() noexcept")
if "WebInput::GetTrackedCursorX()" not in owner_route or "WebInput::GetTrackedCursorY()" not in owner_route:
    fail("the accepted-click route must use tracked coordinates, not the presentation-cursor getters")
if "GetLastCursorX()" in owner_route or "GetLastCursorY()" in owner_route:
    fail("the accepted-click route must not depend on compositor cursor visibility")

begin_route = body(pointer, "Route EvaluateRoute() noexcept")
if "WebInput::GetLastCursorX()" not in begin_route:
    fail("click BEGIN must keep the presentation-aware coordinates")

offscreen = body(views, "void SetViewOffscreen(ViewId view, bool offscreen)")
if "st.focusedView.load(std::memory_order_acquire) == view" not in offscreen:
    fail("SetViewOffscreen must scope the offscreen cancel to the focused view")
if "VirtualPointer::CancelHeldClick();" not in offscreen:
    fail("SetViewOffscreen must cancel a held click before the page-space transition")
if offscreen.index("VirtualPointer::CancelHeldClick();") > offscreen.index("OffscreenFrames::Track(view);"):
    fail("the offscreen transition must cancel before the view becomes page-space")

if "WebRuntime::IsOffscreen(focused)" not in begin_route:
    fail("virtual-pointer BEGIN must require an onscreen focused view")
if "WebRuntime::IsOffscreen(owner.view)" not in body(pointer, "void OnStick("):
    fail("the right stick must not drive the OS cursor for an offscreen-focused view")

window_state = read("WebInputWindow.h")
if window_state.count("std::atomic<") != 1:
    fail("WindowState must keep exactly one atomic; a second flag reintroduces a straddling read")
if "installed_" in window_state:
    fail("WindowState must not carry a separate installed flag beside the handle")
if "void* Get() const noexcept { return handle_.load(std::memory_order_acquire); }" not in window_state:
    fail("WindowState::Get must be a single atomic load of the published handle")

print("virtual pointer lifecycle guard passed")
