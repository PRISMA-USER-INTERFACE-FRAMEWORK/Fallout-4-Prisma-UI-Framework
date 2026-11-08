from pathlib import Path


source = Path("src/main.cpp").read_text(encoding="utf-8")
opening = source.index("if (a_event.opening)")
closing = source.index("} else {", opening)
body = source[opening:closing]
release = body.index("WebRuntime::Unfocus(focused)")
dock_show = body.index("WebRuntime::ShowDock()")
assert "WebRuntime::GetFocusedView()" in body
assert release < dock_show, "PauseMenu opening must release focused view before showing the dock"
input_source = Path("src/PrismaUI/WebInput.cpp").read_text(encoding="utf-8")
capture = input_source[input_source.index("void SetCaptureActive"):]
assert "const auto focused = active && swallow ? WebRuntime::GetFocusedView()" in capture
assert "if (!focused) g_escapeOwned.store(false);" in capture
callbacks = Path("src/PrismaUI/WebRuntimeCallbacks.cpp").read_text(encoding="utf-8")
listener = callbacks[callbacks.index("void RegisterJSListener"):]
assert listener.index("st.backend->RegisterListener") < listener.index(
    'if (fnName == "requestClose") SetViewOwnsEscape(view, true);'
), "requestClose registration must claim Escape ownership after listener installation"
print("pause dock focus release guard passed")
