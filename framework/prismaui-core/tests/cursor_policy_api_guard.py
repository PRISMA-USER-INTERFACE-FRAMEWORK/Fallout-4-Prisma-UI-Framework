from pathlib import Path

root = Path(__file__).resolve().parents[1]
header = (root / "src" / "PrismaUI_F4_API.h").read_text(encoding="utf-8")
exports = (root / "src" / "API" / "CursorPolicyExports.cpp").read_text(encoding="utf-8")
web_input_h = (root / "src" / "PrismaUI" / "WebInput.h").read_text(encoding="utf-8")
web_input_cpp = (root / "src" / "PrismaUI" / "WebInput.cpp").read_text(encoding="utf-8")

# The extension must live after the frozen V1-V10 region. public_api_abi_guard.py owns the digest;
# this catches accidentally moving the new declarations back above its separator.
separator = header.index("using GameThreadTaskCallback")
cursor_policy = header.index("enum class CursorPolicy")
assert cursor_policy > separator, "CursorPolicy must stay outside the frozen V1-V10 API region"

assert "Default = 0" in header
assert "Hidden = 1" in header
assert 'GetProcAddress(pluginHandle, "PrismaUI_F4_SetViewCursorPolicy")' in header
assert 'GetProcAddress(pluginHandle, "PrismaUI_F4_GetViewCursorPolicy")' in header

assert 'extern "C" __declspec(dllexport) bool PrismaUI_F4_SetViewCursorPolicy' in exports
assert 'extern "C" __declspec(dllexport) PRISMA_UI_API::CursorPolicy PrismaUI_F4_GetViewCursorPolicy' in exports

assert "bool SetViewCursorPolicy(std::uint64_t view, std::uint32_t policy);" in web_input_h
assert "if (!view || !WebRuntime::HasFocus(view)) return false;" in web_input_cpp
assert "ViewCursorPolicy::OnFocusChanged(g_cursorPolicyState, focused);" in web_input_cpp
assert "CompositorCursorHidden() ? -1 : g_lastCursorX.load()" in web_input_cpp
assert "CompositorCursorHidden() ? -1 : g_lastCursorY.load()" in web_input_cpp

# Hiding Prisma's compositor cursor must not redefine cursor ownership. Vanilla cursor suppression
# and input capture continue to use IsCursorOwned() independently.
owned_body = web_input_cpp.split("bool IsCursorOwned()", 1)[1].split("bool SetViewCursorPolicy", 1)[0]
assert "ViewCursorPolicy" not in owned_body

print("cursor policy public API guard passed")
