from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
API = (
    (ROOT / "src/API/API.cpp").read_text(encoding="utf-8")
    + (ROOT / "src/API/WebRuntimeAPI.cpp").read_text(encoding="utf-8")
    + (ROOT / "src/API/FocusAPI.cpp").read_text(encoding="utf-8")
    + (ROOT / "src/API/ListenerInspectorAPI.cpp").read_text(encoding="utf-8")
)
GAME_THREAD_API = (ROOT / "src/API/GameThreadAPI.cpp").read_text(encoding="utf-8")
CONTROLLER_API = (ROOT / "src/API/ControllerActionAPI.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/PrismaUI/WebRuntime.h").read_text(encoding="utf-8")
SYSTEM_VIEWS = (ROOT / "src/PrismaUI/WebRuntimeViews.cpp").read_text(encoding="utf-8")
LIFECYCLE = (ROOT / "src/PrismaUI/WebRuntimeViews.cpp").read_text(encoding="utf-8")


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


def requires_system_guard(source: str, signature: str) -> None:
    body = function_body(source, signature)
    assert "IsFrameworkSystemView(view)" in body, f"{signature} must reject framework system views"


assert "bool IsFrameworkSystemView(ViewId view);" in HEADER
system_predicate = function_body(SYSTEM_VIEWS, "bool IsFrameworkSystemView(ViewId view)")
assert "st.bootView.load(std::memory_order_acquire)" in system_predicate
assert "st.dockView.load(std::memory_order_acquire)" in system_predicate
fast_accept = system_predicate.find("if (view == boot || view == dock) return true;")
steady_state = system_predicate.find("if ((boot && dock) || !st.backend) return false;")
metadata_lookup = system_predicate.find("st.backend->EnumerateViews(")
owner_match = system_predicate.find('owner == "PrismaUI"')
boot_path = system_predicate.find('path == "system/boot.html"')
dock_path = system_predicate.find('path == "system/dock.html"')
assert -1 not in (fast_accept, steady_state, metadata_lookup, owner_match, boot_path, dock_path)
assert fast_accept < steady_state < metadata_lookup < owner_match < boot_path < dock_path, (
    "system-view detection must fall back to published backend metadata while system IDs are incomplete"
)

api_destroy = function_body(API, "void PluginAPI::PrismaUIInterface::Destroy(PrismaView view)")
assert "PrismaUI::WebRuntime::Destroy(view);" in api_destroy, (
    "public Destroy must continue to use the runtime lifecycle boundary"
)

enumerate_views = function_body(API, "void PluginAPI::PrismaUIInterface::EnumerateViews(")
assert "callback(id, htmlPath.c_str(), userdata);" in enumerate_views, (
    "public enumeration must continue to report every runtime view"
)
enumerate_ex = function_body(API, "void PluginAPI::PrismaUIInterface::EnumerateViewsEx(")
assert "callback(id, htmlPath.c_str(), owner.c_str(), userdata);" in enumerate_ex, (
    "public enumeration must keep exposing system-view owner metadata"
)

for signature in (
    "void PluginAPI::PrismaUIInterface::InvokeShared(",
    "void PluginAPI::PrismaUIInterface::InteropCall(",
    "void PluginAPI::PrismaUIInterface::RegisterJSListener(",
    "bool PluginAPI::PrismaUIInterface::Focus(",
    "bool PluginAPI::PrismaUIInterface::FocusOverlay(",
    "void PluginAPI::PrismaUIInterface::Unfocus(",
    "void PluginAPI::PrismaUIInterface::Show(",
    "void PluginAPI::PrismaUIInterface::Hide(",
    "void PluginAPI::PrismaUIInterface::Destroy(",
    "void PluginAPI::PrismaUIInterface::SetOrder(",
    "void PluginAPI::PrismaUIInterface::CreateInspectorView(",
    "void PluginAPI::PrismaUIInterface::SetInspectorVisibility(",
    "void PluginAPI::PrismaUIInterface::SetInspectorBounds(",
    "void PluginAPI::PrismaUIInterface::RegisterConsoleCallback(",
    "void PluginAPI::PrismaUIInterface::RegisterConsoleCallbackEx(",
    "void PluginAPI::PrismaUIInterface::RegisterTranslations(",
    "void PluginAPI::PrismaUIInterface::BindUIEvent(",
    "void PluginAPI::PrismaUIInterface::SetViewOffscreen(",
    "void PluginAPI::PrismaUIInterface::SetViewOffscreenSize(",
    "void PluginAPI::PrismaUIInterface::SetViewOffscreenBackground(",
    "void PluginAPI::PrismaUIInterface::SetViewOwnsEscape(",
    "void PluginAPI::PrismaUIInterface::SetViewRole(",
    "bool PluginAPI::PrismaUIInterface::SetInputRegions(",
    "bool PluginAPI::PrismaUIInterface::BindViewToGeometry(",
    "bool PluginAPI::PrismaUIInterface::BindViewToScreenTexture(",
    "void PluginAPI::PrismaUIInterface::UnbindViewFromGeometry(",
):
    requires_system_guard(API, signature)

requires_system_guard(GAME_THREAD_API, "bool PluginAPI::PrismaUIInterface::BindGameThreadUIEvent(")
requires_system_guard(GAME_THREAD_API, "void PluginAPI::VerifiedPrismaUIInterface::BindUIEvent(")

controller_ready = function_body(CONTROLLER_API, "bool ControllerViewReady(PrismaView view)")
assert "IsFrameworkSystemView(view)" in controller_ready
for signature in (
    "bool PluginAPI::PrismaUIInterface::UnbindControllerAction(",
    "void PluginAPI::PrismaUIInterface::ClearControllerActions(",
    "bool PluginAPI::PrismaUIInterface::UnbindControllerFocusEntry(",
    "void PluginAPI::VerifiedPrismaUIInterface::Destroy(",
):
    requires_system_guard(CONTROLLER_API, signature)

verified_destroy = function_body(CONTROLLER_API, "void PluginAPI::VerifiedPrismaUIInterface::Destroy(")
system_guard = verified_destroy.find("IsFrameworkSystemView(view)")
controller_clear = verified_destroy.find("ControllerActions::Clear(view)")
base_destroy = verified_destroy.find("PrismaUIInterface::Destroy(view)")
assert -1 not in (system_guard, controller_clear, base_destroy)
assert system_guard < controller_clear < base_destroy, (
    "verified Destroy must reject system views before clearing controller state"
)

destroy = function_body(LIFECYCLE, "void Destroy(ViewId view)")
zero_guard = destroy.find("if (!view) return;")
boot = destroy.find("const ViewId boot = st.bootView.load(std::memory_order_acquire);")
dock = destroy.find("const ViewId dock = st.dockView.load(std::memory_order_acquire);")
runtime_guard = destroy.find("if (view == boot || view == dock)")
reject = destroy.find("refusing to destroy framework system view")
drop = destroy.find("GameThreadDispatcher::DropView(view)")
backend_destroy = destroy.find("st.backend->DestroyView(view)")
forget = destroy.find("ForgetViewShell(view)")
assert -1 not in (zero_guard, boot, dock, runtime_guard, reject, drop, backend_destroy, forget)
assert zero_guard < boot < dock < runtime_guard < reject < drop < backend_destroy < forget, (
    "framework boot/Dock views must be rejected before dispatcher, backend, or shell teardown"
)

shutdown = function_body(LIFECYCLE, "void ShutdownFrameworkOnWindowThread()")
assert "st.backend->Shutdown();" in shutdown, (
    "framework shutdown must keep its direct backend teardown path independent of public Destroy"
)
preview_shutdown = shutdown.find("ModelPreview::Shutdown();")
backend_shutdown = shutdown.find("st.backend->Shutdown();")
assert -1 not in (preview_shutdown, backend_shutdown)
assert preview_shutdown < backend_shutdown, (
    "framework shutdown must join ModelPreview before backend teardown"
)

print("system view lifecycle guard passed")
