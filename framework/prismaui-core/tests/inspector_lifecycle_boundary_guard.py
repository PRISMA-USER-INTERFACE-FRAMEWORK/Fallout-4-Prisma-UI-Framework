from pathlib import Path

root = Path(__file__).resolve().parent.parent
lifecycle = (root / "src/PrismaUI/WebRuntimeInspector.cpp").read_text(encoding="utf-8")
views = (root / "src/PrismaUI/WebRuntimeViews.cpp").read_text(encoding="utf-8")
focus_source = (root / "src/PrismaUI/WebRuntimeFocus.cpp").read_text(encoding="utf-8")
runtime = (root / "src/PrismaUI/WebRuntimeInspector.cpp").read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
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


destroy = function_body(views, "void Destroy(ViewId view)")
hide_request = destroy.find("SetInspectorVisibility(view, false);")
backend_destroy = destroy.find("st.backend->DestroyView(view)")
assert hide_request != -1 and backend_destroy != -1 and hide_request < backend_destroy, (
    "Destroy must request a real backend inspector hide before destroying the owner view"
)
assert "OnInspectorBackendStateChanged(view, false)" not in destroy, (
    "Destroy must not fabricate backend-hidden confirmation and release input early"
)

bounds = function_body(lifecycle, "void SetInspectorBounds(ViewId view, float x, float y")
for token in (
    "std::isfinite(safeX)",
    "std::isfinite(safeY)",
    "std::numeric_limits<int>::lowest()",
    "std::numeric_limits<int>::max()",
):
    assert token in bounds, f"inspector bounds lost coordinate validation: {token}"
assert '#include <limits>' in runtime, "WebRuntime must include numeric_limits for inspector coordinate validation"

focus = function_body(
    focus_source,
    "bool Internal::FocusImpl(ViewId view, bool pauseGame, bool disableFocusMenu,",
)
assert "inspectorOwnsBackendFocus" in focus, (
    "logical focus handoff must detect active, pending, or tearing-down inspector ownership"
)
for token in (
    "st.localInspectorOwner.load(std::memory_order_acquire) != 0",
    "st.localInspectorPending.load(std::memory_order_acquire) != 0",
    "st.inspectorTeardownPending.load(std::memory_order_acquire) != 0",
):
    assert token in focus, f"focus handoff lost inspector ownership state: {token}"
focus_gate = focus.find("if (!inspectorOwnsBackendFocus)")
backend_focus = focus.find("st.backend->FocusView(view);")
logical_focus = focus.find("st.focusedView.store(view);")
assert -1 not in (focus_gate, backend_focus, logical_focus) and focus_gate < backend_focus < logical_focus, (
    "backend focus must remain with DevTools while logical focus changes underneath it"
)

state_change = function_body(lifecycle, "void ApplyInspectorBackendStateChanged(ViewId owner, bool visible, bool fallback)")
mask_release = state_change.find("InputMask::Release(InputMask::Owner::kInspector)")
focused = state_change.find("const ViewId focused = st.focusedView.load(std::memory_order_acquire);")
unfocus_owner = state_change.find("if (owner != focused) st.backend->UnfocusView(owner);")
refocus = state_change.find("if (focused) st.backend->FocusView(focused);")
assert -1 not in (mask_release, focused, unfocus_owner, refocus), (
    "inspector close must resynchronize backend focus with the runtime focus owner"
)
assert mask_release < focused < unfocus_owner < refocus, (
    "backend focus resynchronization must happen after confirmed inspector release and before input handoff"
)

print("inspector lifecycle boundary guard passed")
