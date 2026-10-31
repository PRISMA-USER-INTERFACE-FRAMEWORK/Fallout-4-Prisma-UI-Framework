#include "API.h"
#include "PrismaUI/WebRuntime.h"

bool PluginAPI::PrismaUIInterface::HasFocus(PrismaView view) noexcept
{
    if (!view) {
        return false;
    }
    return PrismaUI::WebRuntime::HasFocus(view);
}

bool PluginAPI::PrismaUIInterface::Focus(PrismaView view, bool pauseGame, bool disableFocusMenu) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return false;
    }
    return PrismaUI::WebRuntime::Focus(view, pauseGame, disableFocusMenu);
}

bool PluginAPI::PrismaUIInterface::FocusOverlay(PrismaView view, bool pauseGame, bool disableFocusMenu) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return false;
    }
    return PrismaUI::WebRuntime::FocusOverlay(view, pauseGame, disableFocusMenu);
}

void PluginAPI::PrismaUIInterface::Unfocus(PrismaView view) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return;
    }
    PrismaUI::WebRuntime::Unfocus(view);
}

void PluginAPI::PrismaUIInterface::Show(PrismaView view) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
		return;
	}
	PrismaUI::WebRuntime::Show(view);
}

void PluginAPI::PrismaUIInterface::Hide(PrismaView view) noexcept
{
	if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
		return;
	}
	PrismaUI::WebRuntime::Hide(view);
}

bool PluginAPI::PrismaUIInterface::IsHidden(PrismaView view) noexcept
{
	if (!view) {
		return true;
	}
	return PrismaUI::WebRuntime::IsHidden(view);
}
