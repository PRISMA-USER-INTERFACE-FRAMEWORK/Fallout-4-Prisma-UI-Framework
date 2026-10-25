#include "PCH.h"
#include "PapyrusVM.h"

#include "API/API.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameScript.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESObjectREFR.h"

#include <mutex>
#include <memory>
#include <unordered_map>
#include <variant>

namespace PrismaUI::PapyrusVM
{
    using PrismaView = std::uint64_t;

    namespace
    {
        constexpr const char* kScript     = "PrismaUI";
        constexpr const char* kEventName  = "PrismaUI_Event";

        std::mutex                                  g_mutex;
        std::unordered_map<std::string, PrismaView> g_byName;

        PluginAPI::PrismaUIInterface* API() { return PluginAPI::PrismaUIInterface::GetSingleton(); }

        PrismaView Lookup(const std::string& name) {
            std::lock_guard lk(g_mutex);
            auto it = g_byName.find(name);
            return it != g_byName.end() ? it->second : 0;
        }

        bool CreateView(std::monostate, RE::BSFixedString a_name, RE::BSFixedString a_html) {
            if (a_name.empty() || a_html.empty()) return false;
            std::string name = a_name.c_str();
            std::string html = a_html.c_str();
            F4SE::GetTaskInterface()->AddTask([name, html]() {
                auto* api = API();
                if (!api) return;
                {
                    std::lock_guard lk(g_mutex);
                    auto it = g_byName.find(name);
                    if (it != g_byName.end() && api->IsValid(it->second)) return;
                }
                PrismaView v = api->CreateView(html.c_str(), nullptr);
                if (!v) { logger::warn("[PapyrusVM] CreateView('{}') failed", name); return; }
                { std::lock_guard lk(g_mutex); g_byName[name] = v; }
                logger::info("[PapyrusVM] CreateView '{}' -> [{}]", name, v);
            });
            return true;
        }

        void Push(std::monostate, RE::BSFixedString a_name, RE::BSFixedString a_fn, RE::BSFixedString a_json) {
            std::string name = a_name.c_str(), fn = a_fn.c_str(), json = a_json.c_str();
            F4SE::GetTaskInterface()->AddTask([name, fn, json]() {
                auto* api = API(); if (!api) return;
                PrismaView v = Lookup(name);
                if (v && api->IsValid(v)) api->InteropCall(v, fn.c_str(), json.c_str());
            });
        }

        void Show(std::monostate, RE::BSFixedString a_name) {
            std::string name = a_name.c_str();
            F4SE::GetTaskInterface()->AddTask([name]() {
                auto* api = API(); if (!api) return;
                PrismaView v = Lookup(name); if (v) api->Show(v);
            });
        }

        void Hide(std::monostate, RE::BSFixedString a_name) {
            std::string name = a_name.c_str();
            F4SE::GetTaskInterface()->AddTask([name]() {
                auto* api = API(); if (!api) return;
                PrismaView v = Lookup(name); if (v) api->Hide(v);
            });
        }

        void Destroy(std::monostate, RE::BSFixedString a_name) {
            std::string name = a_name.c_str();
            F4SE::GetTaskInterface()->AddTask([name]() {
                auto* api = API(); if (!api) return;
                PrismaView v = 0;
                { std::lock_guard lk(g_mutex); auto it = g_byName.find(name);
                  if (it != g_byName.end()) { v = it->second; g_byName.erase(it); } }
                if (v) { api->UnbindViewFromGeometry(v); api->Destroy(v); }
            });
        }

        bool IsValid(std::monostate, RE::BSFixedString a_name) {
            auto* api = API(); if (!api) return false;
            PrismaView v = Lookup(a_name.c_str());
            return v != 0 && api->IsValid(v);
        }

        bool Focus(std::monostate, RE::BSFixedString a_name, bool a_pauseGame, bool a_disableFocusMenu) {
            auto* api = API();
            if (!api || a_name.empty()) return false;
            const std::string name = a_name.c_str();
            const PrismaView view = Lookup(name);
            if (!view || !api->IsValid(view)) return false;

            auto* tasks = F4SE::GetTaskInterface();
            if (!tasks) return false;
            tasks->AddTask([name, a_pauseGame, a_disableFocusMenu]() {
                auto* api = API(); if (!api) return;
                const PrismaView view = Lookup(name);
                if (!view || !api->IsValid(view)) return;
                if (!api->Focus(view, a_pauseGame, a_disableFocusMenu)) {
                    logger::warn("[PapyrusVM] Focus('{}') was rejected by PrismaUI", name);
                }
            });
            return true;
        }

        void Unfocus(std::monostate, RE::BSFixedString a_name) {
            if (a_name.empty()) return;
            const std::string name = a_name.c_str();
            if (auto* tasks = F4SE::GetTaskInterface()) {
                tasks->AddTask([name]() {
                    auto* api = API(); if (!api) return;
                    const PrismaView view = Lookup(name);
                    if (view) api->Unfocus(view);
                });
            }
        }

        bool HasFocus(std::monostate, RE::BSFixedString a_name) {
            auto* api = API(); if (!api || a_name.empty()) return false;
            const PrismaView view = Lookup(a_name.c_str());
            return view != 0 && api->HasFocus(view);
        }

        void SetOffscreen(std::monostate, RE::BSFixedString a_name, bool a_offscreen) {
            std::string name = a_name.c_str();
            F4SE::GetTaskInterface()->AddTask([name, a_offscreen]() {
                auto* api = API(); if (!api) return;
                PrismaView v = Lookup(name); if (v) api->SetViewOffscreen(v, a_offscreen);
            });
        }

        void SetOffscreenSize(std::monostate, RE::BSFixedString a_name, std::int32_t a_width,
                              std::int32_t a_height) {
            std::string name = a_name.c_str();
            if (a_width <= 0 || a_height <= 0) {
                logger::warn("[PapyrusVM] SetOffscreenSize('{}', {}, {}) ignored: both must be > 0",
                             name, a_width, a_height);
                return;
            }
            F4SE::GetTaskInterface()->AddTask([name, a_width, a_height]() {
                auto* api = API(); if (!api) return;
                PrismaView v = Lookup(name); if (v) api->SetViewOffscreenSize(v, a_width, a_height);
            });
        }

        void QueueBind(std::string name, RE::TESObjectREFR* ref, std::string target,
                       bool firstPerson, bool byTexture) {
            if (!ref) return;
            const RE::TESFormID formID = ref->GetFormID();
            const char* const what = byTexture ? "BindToTexture" : "BindToObject";

            auto retry = std::make_shared<std::function<void(int)>>();
            std::weak_ptr<std::function<void(int)>> weak = retry;
            *retry = [name = std::move(name), formID, target = std::move(target), firstPerson,
                      byTexture, what, weak](int left) {
                auto* api = API();
                PrismaView view = Lookup(name);

                auto* live = RE::TESForm::GetFormByID<RE::TESObjectREFR>(formID);
                if (!live) {
                    logger::warn("[PapyrusVM] {} '{}' abandoned: ref {:#010x} is no longer loaded",
                                 what, name, formID);
                    return;
                }

                RE::NiAVObject* root = live->Get3D(firstPerson);
                if (!root) root = live->Get3D();
                const bool bound = api && view && root &&
                    (byTexture ? api->BindViewToScreenTexture(view, root, target.c_str())
                               : api->BindViewToGeometry(view, root, target.c_str()));
                if (bound) {
                    logger::info("[PapyrusVM] {} '{}' bound to '{}'", what, name, target);
                } else if (left > 0) {
                    if (auto* tasks = F4SE::GetTaskInterface()) {
                        if (auto strong = weak.lock()) {
                            tasks->AddTask([strong, left] { (*strong)(left - 1); });
                        }
                    }
                } else {
                    logger::warn("[PapyrusVM] {} '{}' timed out waiting for '{}'", what, name, target);
                }
            };
            if (auto* tasks = F4SE::GetTaskInterface()) tasks->AddTask([retry] { (*retry)(90); });
        }

        bool BindToObject(std::monostate, RE::BSFixedString a_name, RE::TESObjectREFR* a_ref,
                          RE::BSFixedString a_node, bool a_firstPerson) {
            if (!a_ref || a_name.empty() || a_node.empty()) return false;
            QueueBind(a_name.c_str(), a_ref, a_node.c_str(), a_firstPerson, false);
            return true;
        }

        bool BindToTexture(std::monostate, RE::BSFixedString a_name, RE::TESObjectREFR* a_ref,
                           RE::BSFixedString a_textureSubstring, bool a_firstPerson) {
            if (!a_ref || a_name.empty() || a_textureSubstring.empty()) return false;
            QueueBind(a_name.c_str(), a_ref, a_textureSubstring.c_str(), a_firstPerson, true);
            return true;
        }

        void Unbind(std::monostate, RE::BSFixedString a_name) {
            std::string name = a_name.c_str();
            F4SE::GetTaskInterface()->AddTask([name]() {
                auto* api = API(); if (!api) return;
                PrismaView v = Lookup(name); if (v) api->UnbindViewFromGeometry(v);
            });
        }

        struct EventCtx { std::string event; std::string data; };

        void F4SEAPI EventRegistrant(std::uint64_t a_handle, const char* a_scriptName,
                                     const char* a_callbackName, void* a_data) {
            auto* ctx = static_cast<EventCtx*>(a_data);
            if (!ctx) return;
            auto* gameVM = RE::GameVM::GetSingleton(); if (!gameVM) return;
            auto vm = gameVM->GetVM(); if (!vm) return;
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> nullCb;
            vm->DispatchMethodCall(a_handle, a_scriptName, a_callbackName, nullCb,
                RE::BSFixedString(ctx->event.c_str()), RE::BSFixedString(ctx->data.c_str()));
        }
    }

    void DispatchEvent(const std::string& a_event, const std::string& a_data) {
        const auto* papyrus = F4SE::GetPapyrusInterface();
        if (!papyrus) return;
        EventCtx ctx{ a_event, a_data };
        papyrus->GetExternalEventRegistrations(kEventName, &ctx, EventRegistrant);
    }

    bool Register(RE::BSScript::IVirtualMachine* a_vm) {
        if (!a_vm) return false;
        a_vm->BindNativeMethod(kScript, "CreateView",   CreateView);
        a_vm->BindNativeMethod(kScript, "Push",         Push);
        a_vm->BindNativeMethod(kScript, "Show",         Show);
        a_vm->BindNativeMethod(kScript, "Hide",         Hide);
        a_vm->BindNativeMethod(kScript, "Destroy",      Destroy);
        a_vm->BindNativeMethod(kScript, "IsValid",      IsValid);
        a_vm->BindNativeMethod(kScript, "Focus",        Focus);
        a_vm->BindNativeMethod(kScript, "Unfocus",      Unfocus);
        a_vm->BindNativeMethod(kScript, "HasFocus",     HasFocus);
        a_vm->BindNativeMethod(kScript, "SetOffscreen", SetOffscreen);
        a_vm->BindNativeMethod(kScript, "SetOffscreenSize", SetOffscreenSize);
        a_vm->BindNativeMethod(kScript, "BindToObject", BindToObject);
        a_vm->BindNativeMethod(kScript, "BindToTexture", BindToTexture);
        a_vm->BindNativeMethod(kScript, "Unbind",       Unbind);
        logger::info("[PapyrusVM] PrismaUI Papyrus natives registered");
        return true;
    }
}
