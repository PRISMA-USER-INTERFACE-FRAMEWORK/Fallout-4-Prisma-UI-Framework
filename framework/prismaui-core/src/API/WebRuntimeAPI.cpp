#include "API.h"

#include "PrismaUI/InvokeResult.h"
#include "PrismaUI/ModelPreview.h"
#include "PrismaUI/WebRuntime.h"
#include "Utils/ConflictChecker.h"
#include "Utils/Encoding.h"

#include <intrin.h>

#pragma intrinsic(_ReturnAddress)

#include <functional>

PrismaView PluginAPI::PrismaUIInterface::CreateViewShared(const char* htmlPath, const std::string& owner, std::function<void(PrismaView)> onDomReady, DomReadyDispatcher dispatch)
{
    if (!PrismaUI::WebRuntime::EnsureLoaded()) {
        logger::critical("CreateView: PrismaUI_F4's Host backend failed to initialize -- cannot create view '{}'", htmlPath);
        return 0;
    }

    std::function<void(uint64_t)> webDomReady;
    if (onDomReady) {
        webDomReady = [onDomReady, dispatch](uint64_t id) {
            (void)dispatch([onDomReady, id]() { onDomReady(id); }, id);
        };
    }
    const auto id = PrismaUI::WebRuntime::CreateView(htmlPath, webDomReady, owner.c_str());
    logger::info("CreateView (Ultralight): path='{}' -> view [{}] owner='{}'", htmlPath, id, owner);
    PrismaUI::ModelPreview::NoteViewCreated();
    return id;
}

PrismaView PluginAPI::PrismaUIInterface::CreateView(const char* htmlPath, PRISMA_UI_API::OnDomReadyCallback onDomReadyCallback) noexcept
{
    if (!htmlPath) return 0;

    const std::string owner = PrismaUI::ConflictChecker::OwnerOf(_ReturnAddress());
    std::function<void(PrismaView)> onDomReady;
    if (onDomReadyCallback) {
        onDomReady = [onDomReadyCallback](PrismaView id) { onDomReadyCallback(id); };
    }
    return CreateViewShared(htmlPath, owner, onDomReady, DispatchApiCallback);
}

PrismaView PluginAPI::PrismaUIInterface::CreateViewEx(const char* htmlPath, PRISMA_UI_FLAT_API::OnDomReadyCallbackEx onDomReady, void* userdata) noexcept
{
    if (!htmlPath) return 0;

    const std::string owner = PrismaUI::ConflictChecker::OwnerOf(_ReturnAddress());
    std::function<void(PrismaView)> task;
    if (onDomReady) {
        task = [onDomReady, userdata](PrismaView id) { onDomReady(id, userdata); };
    }
    return CreateViewShared(htmlPath, owner, task, DispatchApiCallback);
}

void PluginAPI::PrismaUIInterface::Invoke(PrismaView view, const char* script, PRISMA_UI_API::JSCallback callback) noexcept
{
    std::function<void(const char*)> task;
    if (callback) {
        task = [callback](const char* result) { callback(result); };
    }
    InvokeShared(view, script, task);
}

void PluginAPI::PrismaUIInterface::InvokeEx(PrismaView view, const char* script, PRISMA_UI_FLAT_API::JSCallbackEx callback, void* userdata) noexcept
{
    std::function<void(const char*)> task;
    if (callback) {
        task = [callback, userdata](const char* result) { callback(result, userdata); };
    }
    InvokeShared(view, script, task);
}

void PluginAPI::PrismaUIInterface::InvokeShared(PrismaView view, const char* script, std::function<void(const char*)> callback)
{
    const auto completeError = [callback](const char* message) {
        if (!callback) return;
        const std::string result = PrismaUI::InvokeResult::Error(message);
        (void)DispatchApiCallback([callback, result]() { callback(result.c_str()); });
    };

    if (!view) {
        completeError("Invoke target view is not live");
        return;
    }
    if (PrismaUI::WebRuntime::IsFrameworkSystemView(view)) {
        completeError("Invoke target view is framework-owned");
        return;
    }
    if (!script) {
        completeError("Invoke script is null");
        return;
    }

    std::string processedScript;
    if (isValidUTF8(script)) {
        processedScript = script;
    } else {
        processedScript = convertFromANSIToUTF8(script);
        if (processedScript.empty()) {
            completeError("Invoke script is not valid text");
            return;
        }
    }

    if (!PrismaUI::WebRuntime::IsActive()) {
        logger::warn("Invoke: Ultralight backend not active -- completing view [{}] with an error", view);
        completeError("Ultralight backend is not active");
        return;
    }

    std::function<void(std::string)> webResult;
    if (callback) {
        webResult = [callback](const std::string& result) {
            (void)DispatchApiCallback([callback, result]() { callback(result.c_str()); });
        };
    }
    PrismaUI::WebRuntime::Invoke(view, processedScript, webResult);
}

void PluginAPI::PrismaUIInterface::InteropCall(PrismaView view, const char* functionName, const char* argument) noexcept
{
    if (!view || PrismaUI::WebRuntime::IsFrameworkSystemView(view) || !functionName || !argument) return;

    std::string processedArgument;
    if (isValidUTF8(argument)) {
        processedArgument = argument;
    } else {
        processedArgument = convertFromANSIToUTF8(argument);
        if (processedArgument.empty()) return;
    }

    if (!PrismaUI::WebRuntime::IsActive()) {
        logger::warn("InteropCall: Ultralight backend not active -- view [{}] call '{}' dropped", view, functionName);
        return;
    }
    PrismaUI::WebRuntime::InteropCall(view, functionName, processedArgument);
}
