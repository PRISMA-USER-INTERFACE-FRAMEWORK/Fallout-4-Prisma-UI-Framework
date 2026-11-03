#include "PCH.h"
#include "API/API.h"

PrismaView PluginAPI::VerifiedPrismaUIInterface::CreateView(
    const char* htmlPath,
    PRISMA_UI_API::OnDomReadyCallback onDomReadyCallback) noexcept
{
    return PrismaUIInterface::CreateView(htmlPath, onDomReadyCallback);
}

PrismaView PluginAPI::VerifiedPrismaUIInterface::CreateViewEx(
    const char* htmlPath,
    PRISMA_UI_FLAT_API::OnDomReadyCallbackEx onDomReady,
    void* userdata) noexcept
{
    return PrismaUIInterface::CreateViewEx(htmlPath, onDomReady, userdata);
}

bool PluginAPI::PrismaUIInterface::DispatchToGameThread(
    PRISMA_UI_API::GameThreadTaskCallback callback,
    void* userdata) noexcept
{
    (void)callback;
    (void)userdata;
    logger::warn("[V11] DispatchToGameThread is not exposed by the FO4VR provider");
    return false;
}

bool PluginAPI::PrismaUIInterface::IsGameThread() noexcept
{
    return false;
}

bool PluginAPI::PrismaUIInterface::BindGameThreadUIEvent(
    PrismaView view,
    const char* functionName,
    PRISMA_UI_API::GameThreadUIEventCallback callback,
    void* userdata) noexcept
{
    (void)view;
    (void)functionName;
    (void)callback;
    (void)userdata;
    logger::warn("[V11] BindGameThreadUIEvent is not exposed by the FO4VR provider");
    return false;
}

void PluginAPI::VerifiedPrismaUIInterface::BindUIEvent(
    PrismaView view,
    const char* functionName,
    PRISMA_UI_API::JSListenerCallback callback) noexcept
{
    PrismaUIInterface::BindUIEvent(view, functionName, callback);
}
