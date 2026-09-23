#include "UltralightDocumentCallbacks.h"

#include "NetworkSandbox.h"
#include "UltralightString.h"
#include "WebNetworkPolicy.h"

#include <AppCore/JSHelpers.h>
#include <JavaScriptCore/JavaScript.h>

#include <chrono>
#include <utility>
#include <vector>

namespace PrismaUI::WebRuntimeUltralight {

    namespace {

        std::string JavaScriptExceptionText(JSContextRef context, JSValueRef exception) {
            if (!context || !exception) return {};
            JSStringRef text = JSValueToStringCopy(context, exception, nullptr);
            if (!text) return {};
            const auto size = JSStringGetMaximumUTF8CStringSize(text);
            std::vector<char> buffer(size ? size : 1);
            const auto written = JSStringGetUTF8CString(text, buffer.data(), buffer.size());
            JSStringRelease(text);
            if (!written) return {};
            return std::string(buffer.data(), written - 1);
        }

    }

    UltralightDocumentCallbacks::UltralightDocumentCallbacks(IUltralightDocumentCallbackHost& host) :
        host_(host) {}

    void UltralightDocumentCallbacks::InstallListener(ViewRecord& record, const std::string& name) {
        if (!record.view) return;
        auto context = record.view->LockJSContext();
        if (!context) return;
        ultralight::SetJSContext(context->ctx());
        auto global = ultralight::JSGlobalObject();
        const auto listenerIt = record.listeners.find(name);
        if (listenerIt == record.listeners.end()) {
            global[ultralight::JSString(name.c_str())] = ultralight::JSValue(ultralight::JSValueUndefinedTag{});
            return;
        }
        const auto callback = listenerIt->second;
        global[ultralight::JSString(name.c_str())] = ultralight::JSCallback([callback](const ultralight::JSObject&, const ultralight::JSArgs& args) {
            std::string value;
            if (args.size()) value = ToUtf8(args[0].ToString());
            callback(std::move(value));
        });
    }

    void UltralightDocumentCallbacks::InstallListeners(ViewRecord& record) {
        for (const auto& [name, _] : record.listeners) InstallListener(record, name);
    }

    void UltralightDocumentCallbacks::ReportError(
        PrismaUI::Web::ViewId id, PrismaUI::Web::ViewHealth health,
        std::string message, std::string detail) {
        host_.ReportDocumentError(id, health, std::move(message), std::move(detail));
    }

    bool UltralightDocumentCallbacks::IsTrustedDocument(
        const ViewRecord& record, std::string_view url) const {
        return WebNetworkPolicy::IsTrustedLocalDocumentUrl(
            url, host_.DocumentFileUrl(record.source.relativePath));
    }

    void UltralightDocumentCallbacks::BlockNavigation(ViewRecord& record, std::string url) {
        const auto id = record.id;
        record.navigationBlocked = true;
        record.blockedNavigationUrl = url;
        record.trustedDocumentReady = false;
        host_.SetDocumentHealth(record, PrismaUI::Web::ViewHealth::LoadFailed);
        if (record.view) {
            const auto fallback = NetworkSandbox::InjectContentSecurityPolicyIntoHtml("<body></body>");
            record.view->LoadHTML(fallback.c_str(), host_.DocumentFileUrl(record.source.relativePath).c_str());
        }
        ReportError(id, PrismaUI::Web::ViewHealth::LoadFailed,
                    "Ultralight remote document navigation rejected", std::move(url));
    }

    void UltralightDocumentCallbacks::OnConsole(
        PrismaUI::Web::ViewId id, const ultralight::ConsoleMessage& message) {
        auto* record = host_.FindDocumentView(id);
        if (!record || !record->console) return;
        record->console(static_cast<int>(message.level()), ToUtf8(message.message()),
                        ToUtf8(message.source_id()), static_cast<int>(message.line_number()));
    }

    void UltralightDocumentCallbacks::OnChangeURL(PrismaUI::Web::ViewId id, std::string url) {
        auto* record = host_.FindDocumentView(id);
        if (!record || !record->trustedDocumentReady || IsTrustedDocument(*record, url)) return;
        BlockNavigation(*record, std::move(url));
    }

    void UltralightDocumentCallbacks::OnBeginLoading(
        PrismaUI::Web::ViewId id, bool mainFrame, std::string url) {
        auto* record = host_.FindDocumentView(id);
        if (!record || !mainFrame) return;
        ++record->documentGeneration;
        host_.OnDocumentBeginLoading(id);
        if (!IsTrustedDocument(*record, url)) {
            BlockNavigation(*record, std::move(url));
            return;
        }
        record->navigationBlocked = false;
        record->trustedDocumentReady = false;
        host_.SetDocumentHealth(*record, PrismaUI::Web::ViewHealth::Creating);
    }

    void UltralightDocumentCallbacks::OnFinishLoading(
        PrismaUI::Web::ViewId id, bool mainFrame, std::string url) {
        auto* record = host_.FindDocumentView(id);
        if (record && mainFrame && url == record->blockedNavigationUrl) return;
        if (!record || !mainFrame || record->navigationBlocked || !IsTrustedDocument(*record, url)) {
            if (record && mainFrame && !IsTrustedDocument(*record, url))
                BlockNavigation(*record, std::move(url));
            return;
        }
        if (record->domReadyFired && record->health != PrismaUI::Web::ViewHealth::LoadFailed)
            host_.SetDocumentHealth(*record, PrismaUI::Web::ViewHealth::Live);
    }

    void UltralightDocumentCallbacks::OnFailLoading(
        PrismaUI::Web::ViewId id, bool mainFrame, std::string url,
        std::string description, std::string domain, int code) {
        auto* record = host_.FindDocumentView(id);
        if (!record) return;
        if (!mainFrame) {
            logger::warn("[WebRuntime] subframe load failed for view {}: {} {}({}):{}", id, description,
                         domain, code, url);
            return;
        }
        if (url == record->blockedNavigationUrl) return;
        host_.SetDocumentHealth(*record, PrismaUI::Web::ViewHealth::LoadFailed);
        ReportError(id, PrismaUI::Web::ViewHealth::LoadFailed, std::move(description),
                    domain + "(" + std::to_string(code) + "):" + url);
    }

    void UltralightDocumentCallbacks::OnWindowObjectReady(
        PrismaUI::Web::ViewId id, bool mainFrame, std::string url) {
        auto* record = host_.FindDocumentView(id);
        if (record && mainFrame && url == record->blockedNavigationUrl) return;
        if (!record || !mainFrame || record->navigationBlocked || !IsTrustedDocument(*record, url)) {
            if (record && mainFrame && !IsTrustedDocument(*record, url))
                BlockNavigation(*record, std::move(url));
            return;
        }
        record->trustedDocumentReady = true;
        if (!record->localizationScript.empty() && record->view) {
            ultralight::String exception;
            record->view->EvaluateScript(ultralight::String(record->localizationScript.c_str()), &exception);
            if (!exception.empty())
                ReportError(id, PrismaUI::Web::ViewHealth::ScriptError,
                            "JavaScript evaluation failed", ToUtf8(exception));
        }
        if (record->isInspector && record->view) {
            host_.InstallInspectorHost(*record->view, id);
            return;
        }
        if (record->view)
            record->view->EvaluateScript(
                "addEventListener('gamepadconnected',function(){}); navigator.getGamepads();");
        InstallListeners(*record);
    }

    void UltralightDocumentCallbacks::FlushPendingCalls(PrismaUI::Web::ViewId id) {
        auto* stored = host_.FindDocumentView(id);
        if (!stored) return;
        auto& record = *stored;
        const auto start = std::chrono::steady_clock::now();
        for (int count = 0; count < 8 && !record.pendingCalls.empty(); ++count) {
            auto pending = std::move(record.pendingCalls.front());
            record.pendingCalls.pop_front();
            record.pendingCallBytes -= pending.name.size() + pending.argumentJson.size();
            InvokeFunction(record, pending.name, pending.argumentJson);
            if (std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(2)) break;
        }
        if (!record.pendingCalls.empty()) {
            if (!host_.DispatchDocument([this, id] { FlushPendingCalls(id); })) {
                record.pendingCalls.clear();
                record.pendingCallBytes = 0;
                record.replayingPendingCalls = false;
            }
            return;
        }
        record.replayingPendingCalls = false;
    }

    void UltralightDocumentCallbacks::OnDomReady(
        PrismaUI::Web::ViewId id, bool mainFrame, std::string url) {
        auto* record = host_.FindDocumentView(id);
        if (record && mainFrame && url == record->blockedNavigationUrl) return;
        if (!record || !mainFrame || record->navigationBlocked || !IsTrustedDocument(*record, url)) {
            if (record && mainFrame && !IsTrustedDocument(*record, url))
                BlockNavigation(*record, std::move(url));
            return;
        }
        record->trustedDocumentReady = true;
        host_.OnTrustedDocumentReady(id);
        if (record->domReadyFired) return;
        if (record->health != PrismaUI::Web::ViewHealth::Live)
            host_.SetDocumentHealth(*record, PrismaUI::Web::ViewHealth::DomReady);
        record->domReadyFired = true;
        record->replayingPendingCalls = true;
        const auto callback = std::move(record->domReady);
        if (callback) callback(id);
        FlushPendingCalls(id);
    }

    void UltralightDocumentCallbacks::InvokeFunction(
        ViewRecord& record, const std::string& name, const std::string& argumentJson) {
        auto context = record.view->LockJSContext();
        if (!context) {
            logger::error("[WebRuntime] CallFunction '{}' failed for view {}: JS context unavailable", name, record.id);
            return;
        }

        JSContextRef jsContext = context->ctx();
        JSValueRef exception = nullptr;
        JSObjectRef global = JSContextGetGlobalObject(jsContext);
        JSStringRef functionName = JSStringCreateWithUTF8CString(name.c_str());
        if (!functionName) {
            logger::error("[WebRuntime] CallFunction '{}' failed for view {}: could not create function name", name, record.id);
            return;
        }
        JSValueRef functionValue = JSObjectGetProperty(jsContext, global, functionName, &exception);
        JSStringRelease(functionName);
        if (exception) {
            logger::error("[WebRuntime] CallFunction '{}' failed for view {} while resolving function: {}",
                          name, record.id, JavaScriptExceptionText(jsContext, exception));
            return;
        }
        if (!functionValue || !JSValueIsObject(jsContext, functionValue)) {
            logger::warn("[WebRuntime] CallFunction '{}' skipped for view {}: global property is not an object", name, record.id);
            return;
        }

        JSObjectRef function = JSValueToObject(jsContext, functionValue, &exception);
        if (exception || !function || !JSObjectIsFunction(jsContext, function)) {
            logger::warn("[WebRuntime] CallFunction '{}' skipped for view {}: global property is not a function", name, record.id);
            return;
        }

        JSStringRef argument = JSStringCreateWithUTF8CString(argumentJson.c_str());
        if (!argument) {
            logger::error("[WebRuntime] CallFunction '{}' failed for view {}: could not create argument string", name, record.id);
            return;
        }
        const JSValueRef arguments[] = {JSValueMakeString(jsContext, argument)};
        JSObjectCallAsFunction(jsContext, function, global, 1, arguments, &exception);
        JSStringRelease(argument);
        if (exception)
            logger::error("[WebRuntime] CallFunction '{}' failed for view {}: {}",
                          name, record.id, JavaScriptExceptionText(jsContext, exception));
    }

}
