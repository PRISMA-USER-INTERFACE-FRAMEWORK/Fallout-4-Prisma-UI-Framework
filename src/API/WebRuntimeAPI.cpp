#include "API.h"

#include "PrismaUI/InvokeResult.h"
#include "PrismaUI/ModelPreview.h"
#include "PrismaUI/WebRuntime.h"
#include "Utils/ConflictChecker.h"
#include "Utils/Encoding.h"

#include <intrin.h>

#pragma intrinsic(_ReturnAddress)

#include <functional>

PrismaView PluginAPI::PrismaUIInterface::CreateView(const char* htmlPath, PRISMA_UI_API::OnDomReadyCallback onDomReadyCallback) noexcept
{
    if (!htmlPath) return 0;

    const std::string owner = PrismaUI::ConflictChecker::OwnerOf(_ReturnAddress());
    if (PrismaUI::WebRuntime::EnsureLoaded()) {
        std::function<void(uint64_t)> webDomReady;
        if (onDomReadyCallback) {
            webDomReady = [onDomReadyCallback](uint64_t id) {
                (void)DispatchApiCallback([onDomReadyCallback, id]() { onDomReadyCallback(id); }, id);
            };
        }
        const auto id = PrismaUI::WebRuntime::CreateView(htmlPath, webDomReady, owner.c_str());
        logger::info("CreateView (Ultralight): path='{}' -> view [{}] owner='{}'", htmlPath, id, owner);
        PrismaUI::ModelPreview::NoteViewCreated();
        return id;
    }

    logger::critical("CreateView: PrismaUI_F4's Host backend failed to initialize -- cannot create view '{}'", htmlPath);
    return 0;
}

void PluginAPI::PrismaUIInterface::Invoke(PrismaView view, const char* script, PRISMA_UI_API::JSCallback callback) noexcept
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
