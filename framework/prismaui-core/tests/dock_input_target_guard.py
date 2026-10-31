from pathlib import Path


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


source = Path("src/PrismaUI/WebRuntimeInput.cpp").read_text(encoding="utf-8")
show = function_body(source, "void ShowDock()")
focused = show.index("const auto focused = st.focusedView.load")
inspector = show.index("const auto inspectorOwner = st.localInspectorOwner.load")
view = show.index("const auto view = st.dockView.load")
invalid = show.index("if (!view || !IsValid(view))")
failed_visible = show.index("st.dockVisible.store(false", invalid)
failed_target = show.index("SetInputTargetView(0)", invalid)
failed_capture = show.index("WebInput::SetCaptureActive(false, false)", invalid)
failed_return = show.index("return;", invalid)
show_view = show.index("Show(view);", failed_return)
visible = show.index("st.dockVisible.store(true", show_view)
guard = show.index("if (focused == 0 && inspectorOwner == 0)", visible)
target = show.index("SetInputTargetView(view)", guard)
capture = show.index("WebInput::SetCaptureActive(true, false)", target)
assert focused < inspector < view < invalid
assert invalid < failed_visible < failed_target < failed_capture < failed_return < show_view
assert show_view < visible < guard < target < capture

hide = function_body(source, "void HideDock()")
hide_guard = hide.index(
    "if (st.focusedView.load() == 0 && st.localInspectorOwner.load(std::memory_order_acquire) == 0)"
)
hide_target = hide.index("SetInputTargetView(0)")
hide_capture = hide.index("WebInput::SetCaptureActive(false, false)")
assert hide_guard < hide_target < hide_capture

print("dock input target guard passed")
