#include "API.h"

#include "PrismaUI/ControllerActions.h"
#include "PrismaUI/ControllerGlyphs.h"
#include "PrismaUI/WebRuntime.h"

namespace {
bool ControllerViewReady(PrismaView view) noexcept {
    if (PrismaUI::WebRuntime::IsFrameworkSystemView(view)) return false;
    const auto health = static_cast<PrismaUI::Web::ViewHealth>(PrismaUI::WebRuntime::GetViewHealth(view));
    return health == PrismaUI::Web::ViewHealth::DomReady || health == PrismaUI::Web::ViewHealth::Live;
}

bool NativeButton(const char* canonicalButton, std::uint32_t& code) noexcept {
    return canonicalButton && PrismaUI::ControllerGlyphs::CodeFromCanonical(canonicalButton, code) &&
           PrismaUI::ControllerGlyphs::IsControllerButton(code);
}
}

bool PluginAPI::PrismaUIInterface::BindControllerAction(
    PrismaView view, const char* canonicalButton, const char* action) noexcept {
    std::uint32_t code = 0;
    if (!ControllerViewReady(view) || !NativeButton(canonicalButton, code) ||
        !PrismaUI::ControllerActions::Bind(view, code, canonicalButton, action)) {
        return false;
    }
    if (!ControllerViewReady(view)) {
        PrismaUI::ControllerActions::Clear(view);
        return false;
    }
    PrismaUI::ControllerActions::InstallBridge(view);
    return true;
}

bool PluginAPI::PrismaUIInterface::UnbindControllerAction(
    PrismaView view, const char* canonicalButton) noexcept {
    std::uint32_t code = 0;
    if (PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !NativeButton(canonicalButton, code)) return false;
    return PrismaUI::ControllerActions::Unbind(view, code);
}

void PluginAPI::PrismaUIInterface::ClearControllerActions(PrismaView view) noexcept {
    if (PrismaUI::WebRuntime::IsFrameworkSystemView(view)) return;
    PrismaUI::ControllerActions::Clear(view);
}

PRISMA_UI_API::ControllerActionBridgeState PluginAPI::PrismaUIInterface::GetControllerActionBridgeState(
    PrismaView view) noexcept {
    return static_cast<PRISMA_UI_API::ControllerActionBridgeState>(
        static_cast<std::uint8_t>(PrismaUI::ControllerActions::GetBridgeState(view)));
}

bool PluginAPI::PrismaUIInterface::BindControllerFocusEntry(
    PrismaView view, const char* canonicalButton,
    PRISMA_UI_API::GameThreadTaskCallback callback, void* userdata) noexcept {
    std::uint32_t code = 0;
    if (!ControllerViewReady(view) || !NativeButton(canonicalButton, code) || !callback) return false;
    return PrismaUI::ControllerActions::BindFocusEntry(view, code, callback, userdata);
}

bool PluginAPI::PrismaUIInterface::UnbindControllerFocusEntry(
    PrismaView view, const char* canonicalButton) noexcept {
    std::uint32_t code = 0;
    if (PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !NativeButton(canonicalButton, code)) return false;
    return PrismaUI::ControllerActions::UnbindFocusEntry(view, code);
}

bool PluginAPI::PrismaUIInterface::SetNativeGamepad(PrismaView view, bool enabled) noexcept {
    return PrismaUI::ControllerGlyphs::SetNativeGamepad(view, enabled);
}

bool PluginAPI::VerifiedPrismaUIInterface::Focus(
    PrismaView view, bool pauseGame, bool disableFocusMenu) noexcept {
    const bool accepted = PrismaUIInterface::Focus(view, pauseGame, disableFocusMenu);
    if (accepted) PrismaUI::ControllerActions::OnFocusAccepted(view);
    return accepted;
}

bool PluginAPI::VerifiedPrismaUIInterface::FocusOverlay(
    PrismaView view, bool pauseGame, bool disableFocusMenu) noexcept {
    const bool accepted = PrismaUIInterface::FocusOverlay(view, pauseGame, disableFocusMenu);
    if (accepted) PrismaUI::ControllerActions::OnFocusAccepted(view);
    return accepted;
}

void PluginAPI::VerifiedPrismaUIInterface::Destroy(PrismaView view) noexcept {
    if (PrismaUI::WebRuntime::IsFrameworkSystemView(view)) return;
    PrismaUI::ControllerActions::Clear(view);
    PrismaUIInterface::Destroy(view);
}
