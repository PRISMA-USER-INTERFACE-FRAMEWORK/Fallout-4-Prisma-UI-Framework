#include "PapyrusBridge.h"

#include "WebRuntime.h"
#include "PapyrusJson.h"
#include "PapyrusVM.h"

#include <mutex>
#include <unordered_map>

namespace PrismaUI::PapyrusBridge {

static constexpr const char* kBridgeScript = R"js(
(function() {
    'use strict';
    var _pending = {};
    var _nextId = 1;

    window.__prisma_resolve = function(id, value) {
        var cb = _pending[id];
        if (cb) { delete _pending[id]; cb.resolve(value); }
    };

    window.__prisma_reject = function(id, msg) {
        var cb = _pending[id];
        if (cb) { delete _pending[id]; cb.reject(new Error(String(msg))); }
    };

    function _read(op, params) {
        return new Promise(function(resolve, reject) {
            var id = String(_nextId++);
            _pending[id] = { resolve: resolve, reject: reject };
            try {
                window.__prisma_request(JSON.stringify(
                    Object.assign({ op: op, id: id }, params)
                ));
            } catch(e) { delete _pending[id]; reject(e); }
        });
    }

    function _write(op, params) {
        try {
            window.__prisma_request(JSON.stringify(
                Object.assign({ op: op, id: '' }, params)
            ));
        } catch(e) {}
    }

    window.prisma = Object.freeze({
        getGlobal: function(esp, formId) {
            return _read('getGlobal', { esp: String(esp), formId: String(formId) });
        },
        setGlobal: function(esp, formId, value) {
            _write('setGlobal', { esp: String(esp), formId: String(formId), value: +value });
        },
        getProperty: function(esp, formId, scriptName, propName) {
            return _read('getProperty', {
                esp: String(esp), formId: String(formId),
                scriptName: String(scriptName), propName: String(propName)
            });
        },
        setProperty: function(esp, formId, scriptName, propName, value) {
            _write('setProperty', {
                esp: String(esp), formId: String(formId),
                scriptName: String(scriptName), propName: String(propName),
                value: value
            });
        },

        emit: function(eventName, data) {
            _write('emit', {
                event: String(eventName),
                data: (typeof data === 'string') ? data : JSON.stringify(data || {})
            });
        }
    });
})();
)js";

static std::mutex g_ownerMutex;
static std::unordered_map<WebRuntime::ViewId, std::string> g_viewOwners;

static std::string OwnerFor(WebRuntime::ViewId viewId) {
    {
        std::lock_guard lock(g_ownerMutex);
        if (auto it = g_viewOwners.find(viewId); it != g_viewOwners.end()) return it->second;
    }
    std::string owner;
    WebRuntime::EnumerateViews([&](WebRuntime::ViewId id, const std::string&, const std::string& o) {
        if (id == viewId) owner = o;
    });
    std::lock_guard lock(g_ownerMutex);
    g_viewOwners[viewId] = owner;
    return owner;
}

static std::string BuildResolve(const std::string& id, bool found, bool isBool, bool boolVal, double numVal) {
    const std::string quotedId = PapyrusJson::JsStringLiteral(id);
    if (!found) return "__prisma_resolve(" + quotedId + ",null)";
    if (isBool) return "__prisma_resolve(" + quotedId + "," + (boolVal ? "true" : "false") + ")";
    return "__prisma_resolve(" + quotedId + "," + std::to_string(numVal) + ")";
}

static std::string BuildReject(const std::string& id, std::string_view msg) {
    return "__prisma_reject(" + PapyrusJson::JsStringLiteral(id) + "," +
           PapyrusJson::JsStringLiteral(msg) + ")";
}

static RE::TESForm* LookupFormByPlugin(const std::string& esp, const std::string& formIdHex) {
    uint32_t localId = 0;
    try {
        localId = std::stoul(formIdHex, nullptr, 16);
    } catch (...) {
        return nullptr;
    }

    auto* handler = RE::TESDataHandler::GetSingleton();
    if (!handler) return nullptr;

    const RE::TESFile* mod = handler->LookupModByName(esp.c_str());
    if (!mod) return nullptr;

    uint32_t fullId;
    const auto compile = static_cast<uint32_t>(mod->GetCompileIndex());
    if (mod->IsLight() || compile == 0xFEu) {
        fullId = 0xFE000000u | (static_cast<uint32_t>(mod->GetSmallFileCompileIndex()) << 12u) |
                 (localId & 0xFFFu);
    } else {

        fullId = (compile << 24u) | (localId & 0x00FFFFFFu);
    }
    return RE::TESForm::GetFormByID(fullId);
}

struct PropResult {
    bool found = false;
    bool isBool = false;
    bool boolVal = false;
    double numVal = 0.0;
};

static PropResult GetPapyrusProperty(RE::TESForm* form, const std::string& scriptName,
                                     const std::string& propName) {
    if (!form || scriptName.empty() || propName.empty()) return {};
    auto* gameVM = RE::GameVM::GetSingleton();
    if (!gameVM) return {};
    auto* vm = gameVM->GetVM().get();
    if (!vm) return {};
    auto* concreteVM = static_cast<RE::BSScript::Internal::VirtualMachine*>(vm);

    RE::BSAutoLock lock(concreteVM->attachedScriptsLock);

    auto handle = vm->GetObjectHandlePolicy().GetHandleForObject(
        static_cast<uint32_t>(form->GetFormType()), form);
    auto it = concreteVM->attachedScripts.find(handle);
    if (it == concreteVM->attachedScripts.end()) return {};

    for (auto& attached : it->second) {
        auto* obj = attached.get();
        if (!obj) continue;
        if (_stricmp(obj->GetTypeInfo()->GetName(), scriptName.c_str()) != 0) continue;

        auto* prop = obj->GetProperty(RE::BSFixedString(propName.c_str()));
        if (!prop) continue;

        PropResult r;
        r.found = true;
        if (prop->is<bool>()) {
            r.isBool = true;
            r.boolVal = RE::BSScript::get<bool>(*prop);
        } else if (prop->is<float>()) {
            r.numVal = static_cast<double>(RE::BSScript::get<float>(*prop));
        } else if (prop->is<std::int32_t>()) {
            r.numVal = static_cast<double>(RE::BSScript::get<std::int32_t>(*prop));
        }
        return r;
    }
    return {};
}

static bool SetPapyrusProperty(RE::TESForm* form, const std::string& scriptName,
                               const std::string& propName, double value) {
    if (!form || scriptName.empty() || propName.empty()) return false;
    auto* gameVM = RE::GameVM::GetSingleton();
    if (!gameVM) return false;
    auto* vm = gameVM->GetVM().get();
    if (!vm) return false;
    auto* concreteVM = static_cast<RE::BSScript::Internal::VirtualMachine*>(vm);

    RE::BSAutoLock lock(concreteVM->attachedScriptsLock);

    auto handle = vm->GetObjectHandlePolicy().GetHandleForObject(
        static_cast<uint32_t>(form->GetFormType()), form);
    auto it = concreteVM->attachedScripts.find(handle);
    if (it == concreteVM->attachedScripts.end()) return false;

    for (auto& attached : it->second) {
        auto* obj = attached.get();
        if (!obj) continue;
        if (_stricmp(obj->GetTypeInfo()->GetName(), scriptName.c_str()) != 0) continue;

        auto* prop = obj->GetProperty(RE::BSFixedString(propName.c_str()));
        if (!prop) continue;

        if (prop->is<float>())
            *prop = static_cast<float>(value);
        else if (prop->is<std::int32_t>())
            *prop = static_cast<std::int32_t>(value);
        else if (prop->is<bool>())
            *prop = (value != 0.0);
        return true;
    }
    return false;
}

static void HandlePrismaRequest(WebRuntime::ViewId viewId, const std::string& req) {
    PapyrusJson::Object json;
    if (!PapyrusJson::ParseFlatObject(req, json)) {
        logger::warn("PapyrusBridge: rejected malformed JSON");
        return;
    }

    const std::string op = PapyrusJson::GetString(json, "op");
    const std::string callbackId = PapyrusJson::GetString(json, "id");
    const bool hasCallback = !callbackId.empty();
    if (hasCallback && !PapyrusJson::IsSafeCallbackId(callbackId)) {
        logger::warn("PapyrusBridge: rejected unsafe callback id");
        return;
    }

    const std::string owner = OwnerFor(viewId);
    auto deny = [&](std::string_view why) {
        logger::warn("PapyrusBridge [{}]: {} (owner='{}')", viewId, why, owner);
        if (hasCallback && WebRuntime::IsValid(viewId)) {
            WebRuntime::Invoke(viewId, BuildReject(callbackId, why), nullptr);
        }
    };

    if (op == "getGlobal") {
        const std::string esp = PapyrusJson::GetString(json, "esp");
        const std::string formId = PapyrusJson::GetString(json, "formId");
        if (!PapyrusJson::EspAllowedForView(owner, esp, false)) {
            deny("getGlobal blocked: ESP is outside this view's owning plugin");
            return;
        }

        F4SE::GetTaskInterface()->AddTask([viewId, callbackId, esp, formId]() {
            if (!WebRuntime::IsValid(viewId)) return;
            auto* form = LookupFormByPlugin(esp, formId);
            auto* global = form ? form->As<RE::TESGlobal>() : nullptr;
            std::string script =
                global ? BuildResolve(callbackId, true, false, false, static_cast<double>(global->value))
                       : BuildResolve(callbackId, false, false, false, 0.0);
            WebRuntime::Invoke(viewId, script, nullptr);
        });

    } else if (op == "setGlobal") {
        const std::string esp = PapyrusJson::GetString(json, "esp");
        const std::string formId = PapyrusJson::GetString(json, "formId");
        const double value = PapyrusJson::GetNumber(json, "value");
        if (!PapyrusJson::EspAllowedForView(owner, esp, true)) {
            deny("setGlobal blocked: ESP is outside this view's owning plugin");
            return;
        }

        F4SE::GetTaskInterface()->AddTask([esp, formId, value]() {
            auto* form = LookupFormByPlugin(esp, formId);
            auto* global = form ? form->As<RE::TESGlobal>() : nullptr;
            if (global) global->value = static_cast<float>(value);
        });

    } else if (op == "getProperty") {
        const std::string esp = PapyrusJson::GetString(json, "esp");
        const std::string formId = PapyrusJson::GetString(json, "formId");
        const std::string scriptName = PapyrusJson::GetString(json, "scriptName");
        const std::string propName = PapyrusJson::GetString(json, "propName");
        if (scriptName.empty() || propName.empty()) {
            deny("getProperty requires scriptName and propName");
            return;
        }
        if (!PapyrusJson::EspAllowedForView(owner, esp, false)) {
            deny("getProperty blocked: ESP is outside this view's owning plugin");
            return;
        }

        F4SE::GetTaskInterface()->AddTask([viewId, callbackId, esp, formId, scriptName, propName]() {
            if (!WebRuntime::IsValid(viewId)) return;
            auto* form = LookupFormByPlugin(esp, formId);
            auto result = GetPapyrusProperty(form, scriptName, propName);
            WebRuntime::Invoke(viewId,
                              BuildResolve(callbackId, result.found, result.isBool, result.boolVal,
                                           result.numVal),
                              nullptr);
        });

    } else if (op == "setProperty") {
        const std::string esp = PapyrusJson::GetString(json, "esp");
        const std::string formId = PapyrusJson::GetString(json, "formId");
        const std::string scriptName = PapyrusJson::GetString(json, "scriptName");
        const std::string propName = PapyrusJson::GetString(json, "propName");
        const double value = PapyrusJson::GetNumber(json, "value");
        if (scriptName.empty() || propName.empty()) {
            deny("setProperty requires scriptName and propName");
            return;
        }
        if (!PapyrusJson::EspAllowedForView(owner, esp, true)) {
            deny("setProperty blocked: ESP is outside this view's owning plugin");
            return;
        }

        F4SE::GetTaskInterface()->AddTask([esp, formId, scriptName, propName, value]() {
            auto* form = LookupFormByPlugin(esp, formId);
            SetPapyrusProperty(form, scriptName, propName, value);
        });

    } else if (op == "emit") {
        const std::string event = PapyrusJson::GetString(json, "event");
        const std::string data = PapyrusJson::GetString(json, "data");
        if (event.empty() || event.size() > 128) {
            deny("emit rejected: invalid event name");
            return;
        }

        F4SE::GetTaskInterface()->AddTask([event, data]() {
            PrismaUI::PapyrusVM::DispatchEvent(event, data);
        });

    } else {
        logger::warn("PapyrusBridge: unknown op '{}'", op);
    }
}

void InjectBridge(WebRuntime::ViewId viewId) {
    std::string owner;
    WebRuntime::EnumerateViews([&](WebRuntime::ViewId id, const std::string&, const std::string& o) {
        if (id == viewId) owner = o;
    });
    {
        std::lock_guard lock(g_ownerMutex);
        g_viewOwners[viewId] = owner;
    }

    WebRuntime::RegisterJSListener(viewId, "__prisma_request",
                                  [viewId](std::string req) { HandlePrismaRequest(viewId, req); });

    WebRuntime::Invoke(viewId, kBridgeScript, nullptr);
    logger::info("PapyrusBridge [{}]: window.prisma injected (owner='{}')", viewId, owner);
}

}
