#include "WebRuntime.h"
#include "WebRuntimeInternal.h"
#include "InvokeResult.h"
#include <utility>
namespace PrismaUI::WebRuntime {
    namespace {
        auto& st = Internal::RT();
        using namespace Internal;
        void CompleteInvokeError(std::function<void(std::string)> onResult, const char* message) {
            if (onResult) onResult(PrismaUI::InvokeResult::Error(message));
        }
    }
    void Invoke(ViewId view, const std::string& script, std::function<void(std::string)> onResult) {
        if (!st.backend) {
            CompleteInvokeError(std::move(onResult), "Host ABI is unavailable");
            return;
        }
        if (!view || !IsValid(view)) {
            CompleteInvokeError(std::move(onResult), "Invoke target view is not live");
            return;
        }
        if (!st.active.load(std::memory_order_acquire)) {
            CompleteInvokeError(std::move(onResult), "web backend is not active");
            return;
        }
        const bool hasCb = static_cast<bool>(onResult);
        uint64_t token = 0;
        if (hasCb) {
            std::lock_guard<std::mutex> lock(st.cbMutex);
            token = st.nextToken++;
            if (token == 0) token = st.nextToken++;
            st.resultByToken[token] = ResultRegistration{0, std::move(onResult)};
        }
        st.backend->EvaluateScript(view, script,
                                   hasCb ? [token](std::string result) { ResultTramp(std::move(result), token); }
                                         : PrismaUI::Web::ScriptResultCallback{});
        if (hasCb && !IsValid(view)) {
            ResultTramp(PrismaUI::InvokeResult::Error("Invoke target view is not live"), token);
        }
    }
    void InvokeDeferred(ViewId view, const std::string& script,
                        std::function<void(std::string)> onResult) {
        if (!st.backend) {
            CompleteInvokeError(std::move(onResult), "Host ABI is unavailable");
            return;
        }
        if (!view || !IsValid(view)) {
            CompleteInvokeError(std::move(onResult), "Invoke target view is not live");
            return;
        }
        if (!st.active.load(std::memory_order_acquire)) {
            CompleteInvokeError(std::move(onResult), "web backend is not active");
            return;
        }
        const bool hasCb = static_cast<bool>(onResult);
        uint64_t token = 0;
        if (hasCb) {
            std::lock_guard<std::mutex> lock(st.cbMutex);
            token = st.nextToken++;
            if (token == 0) token = st.nextToken++;
            st.resultByToken[token] = ResultRegistration{0, std::move(onResult)};
        }
        const bool admitted = st.backend->EvaluateScriptDeferred(
            view, script, hasCb ? [token](std::string result) { ResultTramp(std::move(result), token); }
                              : PrismaUI::Web::ScriptResultCallback{});
        if (!admitted && hasCb)
            ResultTramp(PrismaUI::InvokeResult::Error("deferred invoke was not admitted"), token);
    }
    void RegisterLocalizationScript(ViewId view, const std::string& script) {
        if (!st.backend || !view || script.empty()) return;
        st.backend->RegisterLocalizationScript(view, script);
    }
    void InteropCall(ViewId view, const std::string& functionName, const std::string& argument) {
        if (!st.backend || !view || !IsValid(view)) return;
        st.backend->CallFunction(view, functionName, argument);
    }
    void RegisterJSListener(ViewId view, const std::string& fnName, std::function<void(std::string)> callback) {
        if (!st.backend || !view || fnName.empty() || !callback || !IsValid(view)) return;
        uint64_t token;
        {
            std::lock_guard<std::mutex> lock(st.cbMutex);
            for (auto it = st.listenerByToken.begin(); it != st.listenerByToken.end();) {
                if (it->second.view == view && it->second.name == fnName)
                    it = st.listenerByToken.erase(it);
                else
                    ++it;
            }
            token = st.nextToken++;
            if (token == 0) token = st.nextToken++;
            st.listenerByToken[token] = ListenerRegistration{view, fnName, std::move(callback)};
        }
        st.backend->RegisterListener(view, fnName,
                                     [token](std::string argument) { ListenerTramp(std::move(argument), token); });
        if (fnName == "requestClose") SetViewOwnsEscape(view, true);
    }
    void RegisterConsoleCallback(ViewId view, std::function<void(int, std::string, std::string, int)> callback) {
        if (!st.backend || !view || !IsValid(view)) return;
        if (!callback) {
            {
                std::lock_guard<std::mutex> lock(st.cbMutex);
                for (auto it = st.consoleByToken.begin(); it != st.consoleByToken.end();) {
                    if (it->second.view == view)
                        it = st.consoleByToken.erase(it);
                    else
                        ++it;
                }
            }
            st.backend->RegisterConsole(view, {});
            return;
        }
        uint64_t token;
        {
            std::lock_guard<std::mutex> lock(st.cbMutex);
            for (auto it = st.consoleByToken.begin(); it != st.consoleByToken.end();) {
                if (it->second.view == view)
                    it = st.consoleByToken.erase(it);
                else
                    ++it;
            }
            token = st.nextToken++;
            if (token == 0) token = st.nextToken++;
            st.consoleByToken[token] = ConsoleRegistration{view, std::move(callback)};
        }
        st.backend->RegisterConsole(view, [token](int level, std::string message, std::string source, int line) {
            ConsoleTramp(level, std::move(message), std::move(source), line, token);
        });
    }
}
