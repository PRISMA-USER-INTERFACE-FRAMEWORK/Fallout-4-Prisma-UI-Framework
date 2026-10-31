#include "API.h"
#include "PrismaUI/WebRuntime.h"

void PluginAPI::PrismaUIInterface::RegisterJSListener(PrismaView view, const char* fnName, PRISMA_UI_API::JSListenerCallback callback) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !fnName || !callback) {
        logger::warn("RegisterJSListener: invalid args — view={} fn={}", view, fnName ? fnName : "null");
        return;
    }

    logger::info("RegisterJSListener: View [{}] registering '{}'", view, fnName);

    std::string name = fnName;
    std::function<void(std::string)> callbackWrapper = [callback, view, name](const std::string& arg) {
        (void)DispatchApiCallback([targetCallback = callback, data = arg, view, name]() {

            logger::debug("RegisterJSListener fired: View [{}] '{}' — data: '{}'", view, name, data);
            targetCallback(data.c_str());
        }, view);
    };

    if (!PrismaUI::WebRuntime::IsActive()) {
        logger::warn("RegisterJSListener: Ultralight backend not active -- view [{}] fn '{}' not registered", view, fnName);
        return;
    }
    PrismaUI::WebRuntime::RegisterJSListener(view, fnName, callbackWrapper);
}

void PluginAPI::PrismaUIInterface::CreateInspectorView(PrismaView view) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return;
    }
    PrismaUI::WebRuntime::CreateInspectorView(view);
}

void PluginAPI::PrismaUIInterface::SetInspectorVisibility(PrismaView view, bool visible) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return;
    }
    PrismaUI::WebRuntime::SetInspectorVisibility(view, visible);
}

bool PluginAPI::PrismaUIInterface::IsInspectorVisible(PrismaView view) noexcept
{
    if (!view) {
        return false;
    }

    return PrismaUI::WebRuntime::IsInspectorVisible(view);
}

void PluginAPI::PrismaUIInterface::SetInspectorBounds(PrismaView view, float topLeftX, float topLeftY, unsigned int width, unsigned int height) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        return;
    }

    PrismaUI::WebRuntime::SetInspectorBounds(view, topLeftX, topLeftY, width, height);
}
