#include "PCH.h"
#include "PapyrusVM.h"

#include "API/API.h"
#include "API/MeshBinding.h"
#include "PendingBindIntake.h"

#include "RE/B/BSScriptUtil.h"
#include "RE/G/GameScript.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESObjectREFR.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

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

        // A retry worker must drive attempts independently of the composite path; source:docs/code-notes/prismaui-core.md.
        constexpr MeshBinding::Readiness::Policy kBindRetryPolicy{};

        struct PendingBind {
            std::string                   name;
            std::string                   target;
            const char*                   what{};
            RE::TESFormID                 formID{};
            bool                          firstPerson{};
            bool                          byTexture{};
            bool                          attemptQueued{};
            MeshBinding::Readiness::State state{};
        };

        std::mutex                                     g_pendingMutex;
        std::condition_variable                        g_pendingCv;
        std::unordered_map<std::uint64_t, PendingBind> g_pendingBinds;
        std::uint64_t                                  g_nextPendingBind{ 1 };
        bool                                           g_retryWorkerStarted{};

        bool PushGameTask(PendingBindIntake::Task a_task) {
            auto* tasks = F4SE::GetTaskInterface();
            if (!tasks) return false;
            tasks->AddTask(std::move(a_task));
            return true;
        }

        PendingBindIntake::Intake g_bindIntake{ &PushGameTask };

        void AttemptPendingBind(std::uint64_t a_id);
        void SchedulePendingBind(std::uint64_t a_id);
        void RetryWorkerMain();

        void FinishPendingBind(std::uint64_t a_id) {
            {
                std::lock_guard lk(g_pendingMutex);
                g_pendingBinds.erase(a_id);
            }
            g_pendingCv.notify_all();
        }

        void CancelPendingBindsForName(const std::string& a_name) {
            {
                std::lock_guard lk(g_pendingMutex);
                for (auto it = g_pendingBinds.begin(); it != g_pendingBinds.end();) {
                    if (it->second.name == a_name) {
                        it = g_pendingBinds.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            g_pendingCv.notify_all();
        }

        void EnsureRetryWorker() {
            std::lock_guard lk(g_pendingMutex);
            if (g_retryWorkerStarted) return;
            g_retryWorkerStarted = true;
            std::thread(RetryWorkerMain).detach();
        }

        // The worker must live for the process lifetime; source:docs/code-notes/prismaui-core.md.
        void RetryWorkerMain() {
            std::unique_lock lk(g_pendingMutex);
            for (;;) {
                if (g_pendingBinds.empty()) {
                    g_pendingCv.wait(lk);
                    continue;
                }
                auto earliest = MeshBinding::Readiness::Clock::time_point::max();
                for (const auto& [id, pending] : g_pendingBinds) {
                    if (!pending.attemptQueued && pending.state.nextAttempt < earliest) {
                        earliest = pending.state.nextAttempt;
                    }
                }
                if (earliest == MeshBinding::Readiness::Clock::time_point::max()) {
                    g_pendingCv.wait(lk);
                    continue;
                }
                const auto now = MeshBinding::Readiness::Clock::now();
                if (earliest > now) {
                    g_pendingCv.wait_until(lk, earliest);
                    continue;
                }
                std::vector<std::uint64_t> due;
                due.reserve(g_pendingBinds.size());
                for (auto& [id, pending] : g_pendingBinds) {
                    if (!pending.attemptQueued && pending.state.nextAttempt <= now) {
                        pending.attemptQueued = true;
                        due.push_back(id);
                    }
                }
                lk.unlock();
                for (const auto id : due) {
                    SchedulePendingBind(id);
                }
                lk.lock();
            }
        }

        void SchedulePendingBind(std::uint64_t a_id) {
            if (auto* tasks = F4SE::GetTaskInterface()) {
                tasks->AddTask([a_id] { AttemptPendingBind(a_id); });
                return;
            }
            FinishPendingBind(a_id);
            logger::warn("[PapyrusVM] pending mesh binding dropped: no game-thread task interface");
        }

        void AttemptPendingBind(std::uint64_t a_id) {
            PendingBind pending;
            {
                std::lock_guard lk(g_pendingMutex);
                const auto it = g_pendingBinds.find(a_id);
                if (it == g_pendingBinds.end()) return;
                pending = it->second;
            }

            auto* api = API();
            const auto now = MeshBinding::Readiness::Clock::now();
            const auto availability = MeshBinding::GetAvailability();
            const PrismaView view = Lookup(pending.name);
            const bool viewLive = api && view && api->IsValid(view);
            auto* live = RE::TESForm::GetFormByID<RE::TESObjectREFR>(pending.formID);
            const bool referenceLive = live != nullptr;

            switch (MeshBinding::Readiness::Classify(pending.state, now, kBindRetryPolicy, availability,
                                                     viewLive, referenceLive)) {
            case MeshBinding::Readiness::Decision::Wait:
            case MeshBinding::Readiness::Decision::Unavailable:
                FinishPendingBind(a_id);
                return;
            case MeshBinding::Readiness::Decision::ViewGone:
                FinishPendingBind(a_id);
                logger::warn("[PapyrusVM] {} '{}' abandoned: view is no longer valid", pending.what,
                             pending.name);
                return;
            case MeshBinding::Readiness::Decision::ReferenceGone:
                FinishPendingBind(a_id);
                logger::warn("[PapyrusVM] {} '{}' abandoned: ref {:#010x} is no longer loaded", pending.what,
                             pending.name, pending.formID);
                return;
            case MeshBinding::Readiness::Decision::Expired: {
                const auto elapsed = MeshBinding::Readiness::Elapsed(now, pending.state.started).count();
                const auto attempts = pending.state.attempts;
                FinishPendingBind(a_id);
                logger::warn("[PapyrusVM] {} '{}' timed out after {}ms ({} attempts) waiting for '{}'; the "
                             "feature is available, so the target geometry/texture or the view frame was not ready",
                             pending.what, pending.name, elapsed, attempts, pending.target);
                return;
            }
            case MeshBinding::Readiness::Decision::AttemptNow:
                break;
            }

            RE::NiAVObject* root = live->Get3D(pending.firstPerson);
            if (!root) root = live->Get3D();
            const bool bound = root && (pending.byTexture
                ? api->BindViewToScreenTexture(view, root, pending.target.c_str())
                : api->BindViewToGeometry(view, root, pending.target.c_str()));
            if (bound) {
                FinishPendingBind(a_id);
                logger::info("[PapyrusVM] {} '{}' bound to '{}'", pending.what, pending.name, pending.target);
                return;
            }

            {
                std::lock_guard lk(g_pendingMutex);
                const auto it = g_pendingBinds.find(a_id);
                if (it != g_pendingBinds.end()) {
                    it->second.attemptQueued = false;
                    MeshBinding::Readiness::NoteAttempt(it->second.state, now, kBindRetryPolicy);
                }
            }
            g_pendingCv.notify_all();
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
            const bool queued = g_bindIntake.Cancel([name]() {
                auto* api = API(); if (!api) return;
                CancelPendingBindsForName(name);
                PrismaView v = 0;
                { std::lock_guard lk(g_mutex); auto it = g_byName.find(name);
                  if (it != g_byName.end()) { v = it->second; g_byName.erase(it); } }
                if (v) { api->UnbindViewFromGeometry(v); api->Destroy(v); }
            });
            if (!queued) {
                logger::warn("[PapyrusVM] Destroy '{}' dropped: no game-thread task interface", name);
            }
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

        // source:docs/code-notes/prismaui-core.md (pending-bind ordering; object identity captured at call time)
        void StartPendingBind(std::string name, RE::TESFormID formID, std::string target,
                              bool firstPerson, bool byTexture) {
            const char* const what = byTexture ? "BindToTexture" : "BindToObject";
            // Permanent unavailability must fail before any retry; source:docs/code-notes/prismaui-core.md.
            const auto availability = MeshBinding::GetAvailability();
            if (availability != MeshBinding::Availability::Available) {
                logger::error("[PapyrusVM] {} '{}' unavailable: mesh binding is {}; no retries scheduled",
                              what, name, MeshBinding::Readiness::AvailabilityReason(availability));
                return;
            }

            const auto now = MeshBinding::Readiness::Clock::now();
            std::uint64_t id = 0;
            {
                CancelPendingBindsForName(name);
                std::lock_guard lk(g_pendingMutex);
                id = g_nextPendingBind++;
                PendingBind pending;
                pending.name = std::move(name);
                pending.target = std::move(target);
                pending.what = what;
                pending.formID = formID;
                pending.firstPerson = firstPerson;
                pending.byTexture = byTexture;
                pending.attemptQueued = true;
                pending.state = MeshBinding::Readiness::Begin(now);
                g_pendingBinds.emplace(id, std::move(pending));
            }
            EnsureRetryWorker();
            SchedulePendingBind(id);
            g_pendingCv.notify_all();
        }

        bool BindToObject(std::monostate, RE::BSFixedString a_name, RE::TESObjectREFR* a_ref,
                          RE::BSFixedString a_node, bool a_firstPerson) {
            if (!a_ref || a_name.empty() || a_node.empty()) return false;
            const std::string name = a_name.c_str();
            const std::string target = a_node.c_str();
            const RE::TESFormID formID = a_ref->GetFormID();
            const bool queued = g_bindIntake.Bind([name, formID, target, a_firstPerson] {
                StartPendingBind(name, formID, target, a_firstPerson, false);
            });
            if (!queued) {
                logger::warn("[PapyrusVM] BindToObject '{}' dropped: no game-thread task interface", name);
            }
            return queued;
        }

        bool BindToTexture(std::monostate, RE::BSFixedString a_name, RE::TESObjectREFR* a_ref,
                           RE::BSFixedString a_textureSubstring, bool a_firstPerson) {
            if (!a_ref || a_name.empty() || a_textureSubstring.empty()) return false;
            const std::string name = a_name.c_str();
            const std::string target = a_textureSubstring.c_str();
            const RE::TESFormID formID = a_ref->GetFormID();
            const bool queued = g_bindIntake.Bind([name, formID, target, a_firstPerson] {
                StartPendingBind(name, formID, target, a_firstPerson, true);
            });
            if (!queued) {
                logger::warn("[PapyrusVM] BindToTexture '{}' dropped: no game-thread task interface", name);
            }
            return queued;
        }

        void Unbind(std::monostate, RE::BSFixedString a_name) {
            std::string name = a_name.c_str();
            const bool queued = g_bindIntake.Cancel([name]() {
                auto* api = API(); if (!api) return;
                CancelPendingBindsForName(name);
                PrismaView v = Lookup(name); if (v) api->UnbindViewFromGeometry(v);
            });
            if (!queued) {
                logger::warn("[PapyrusVM] Unbind '{}' dropped: no game-thread task interface", name);
            }
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
