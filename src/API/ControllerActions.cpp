#include "PrismaUI/ControllerActions.h"

#include "PrismaUI/WebRuntime.h"
#ifndef PRISMAUI_FO4VR
#include "PrismaUI/GameThreadDispatcher.h"
#endif

#include <condition_variable>
#include <limits>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace PrismaUI::ControllerActions {
namespace {
Registry g_registry;
BridgeLifecycle g_bridges;

struct FocusEntryRegistration {
    std::uint32_t buttonCode = 0;
    FocusEntryCallback callback = nullptr;
    void* userdata = nullptr;
    FocusEntrySequence sequence;
    bool cancelled = false;
    bool executing = false;
    std::thread::id executingThread{};
    std::condition_variable idle;
};

std::mutex g_focusEntryMutex;
std::unordered_map<ViewId, std::shared_ptr<FocusEntryRegistration>> g_focusEntries;

bool BridgeReady(ViewId view) {
    return g_bridges.Get(view) == BridgeState::kReady;
}

void CancelFocusEntryLocked(std::unique_lock<std::mutex>& lock,
                            const std::shared_ptr<FocusEntryRegistration>& entry) noexcept {
    if (!entry) return;
    entry->cancelled = true;
    const auto current = std::this_thread::get_id();
    while (entry->executing && entry->executingThread != current) {
        entry->idle.wait(lock);
    }
}

void ResetFocusEntrySequences() noexcept {
    std::lock_guard lock(g_focusEntryMutex);
    for (auto& [view, entry] : g_focusEntries) {
        (void)view;
        if (entry) entry->sequence.Reset();
    }
}
}

bool Bind(ViewId view, std::uint32_t buttonCode,
          const char* canonicalButton, const char* action) noexcept {
    if (!canonicalButton || !action) return false;
    try {
        return g_registry.Bind(view, buttonCode, canonicalButton, action);
    } catch (...) {
        return false;
    }
}

bool Unbind(ViewId view, std::uint32_t buttonCode) noexcept {
    return g_registry.Unbind(view, buttonCode);
}

bool BindFocusEntry(ViewId view, std::uint32_t buttonCode,
                    FocusEntryCallback callback, void* userdata) noexcept {
#ifdef PRISMAUI_FO4VR
    (void)view;
    (void)buttonCode;
    (void)callback;
    (void)userdata;
    return false;
#else
    if (!view || !buttonCode || !callback) return false;

    try {
        auto replacement = std::make_shared<FocusEntryRegistration>();
        replacement->buttonCode = buttonCode;
        replacement->callback = callback;
        replacement->userdata = userdata;

        std::unique_lock lock(g_focusEntryMutex);
        if (const auto found = g_focusEntries.find(view); found != g_focusEntries.end()) {
            const auto previous = found->second;
            g_focusEntries.erase(found);
            CancelFocusEntryLocked(lock, previous);
        }
        g_focusEntries.emplace(view, std::move(replacement));
        return true;
    } catch (...) {
        return false;
    }
#endif
}

bool UnbindFocusEntry(ViewId view, std::uint32_t buttonCode) noexcept {
#ifdef PRISMAUI_FO4VR
    (void)view;
    (void)buttonCode;
    return false;
#else
    if (!view || !buttonCode) return false;

    std::unique_lock lock(g_focusEntryMutex);
    const auto found = g_focusEntries.find(view);
    if (found == g_focusEntries.end() || !found->second || found->second->buttonCode != buttonCode) return false;
    const auto entry = found->second;
    g_focusEntries.erase(found);
    CancelFocusEntryLocked(lock, entry);
    return true;
#endif
}

BridgeState GetBridgeState(ViewId view) noexcept {
    return g_bridges.Get(view);
}

bool HasMappings(ViewId view) noexcept {
    return g_registry.HasMappings(view);
}

void InvalidateBridge(ViewId view) noexcept {
    g_bridges.Invalidate(view);
    g_registry.ResetHeld(view);
}

void Clear(ViewId view) noexcept {
    g_registry.Clear(view);
    g_bridges.Clear(view);

    std::unique_lock lock(g_focusEntryMutex);
    const auto found = g_focusEntries.find(view);
    if (found == g_focusEntries.end()) return;
    const auto entry = found->second;
    g_focusEntries.erase(found);
    CancelFocusEntryLocked(lock, entry);
}

void OnFocusAccepted(ViewId view) noexcept {
    g_registry.OnFocusAccepted(view);
    ResetFocusEntrySequences();
    if (g_registry.HasMappings(view)) InstallBridge(view);
}

void ScheduleBridgeRetry(ViewId view) {
    if (!WebRuntime::IsValid(view) || !g_bridges.CanRetry(view)) return;
    WebRuntime::InvokeDeferred(view, "true", [view](std::string result) {
        if (result == "true" && WebRuntime::IsValid(view) && g_bridges.CanRetry(view))
            InstallBridge(view);
    });
}

void InstallBridge(ViewId view) {
    static const std::string script =
        "window.__prismaUI_controllerAction=function(s){try{window.dispatchEvent(new CustomEvent("
        "'prisma-controller-action',{detail:JSON.parse(s)}));}catch(e){}};true";

    BridgeInstall install{};
    try {
        install = g_bridges.Ensure(view);
        if (!install.startInstall) return;
        WebRuntime::Invoke(view, script, [view, ticket = install.ticket](std::string result) {
            const bool success = result == "true";
            if (g_bridges.Complete(view, ticket, success) && !success) ScheduleBridgeRetry(view);
        });
    } catch (...) {
        if (install.startInstall && g_bridges.Complete(view, install.ticket, false)) ScheduleBridgeRetry(view);
    }
}

HandleResult Handle(ViewId view, std::uint32_t buttonCode,
                    bool justPressed, bool released, float heldDownSecs) noexcept {
    if (!WebRuntime::HasFocus(view) || !WebRuntime::IsValid(view)) return {};
    const auto decision = g_registry.Decide(view, buttonCode, justPressed, released, heldDownSecs);
    if (decision.state != State::kNone && BridgeReady(view)) {
        WebRuntime::InteropCall(view, "__prismaUI_controllerAction", decision.Payload());
    }
    return {decision.mappingActive, decision.consumeGameEvent};
}

HandleResult HandleFocusEntry(std::uint32_t buttonCode,
                              bool justPressed, bool released) noexcept {
#ifdef PRISMAUI_FO4VR
    (void)buttonCode;
    (void)justPressed;
    (void)released;
    return {};
#else
    const bool anyFocus = WebRuntime::HasAnyActiveFocus();
    if (anyFocus) {
        ResetFocusEntrySequences();
        return {};
    }

    std::vector<ViewId> candidates;
    {
        std::lock_guard lock(g_focusEntryMutex);
        candidates.reserve(g_focusEntries.size());
        for (const auto& [view, entry] : g_focusEntries) {
            if (entry && entry->buttonCode == buttonCode && !entry->cancelled) candidates.push_back(view);
        }
    }
    if (candidates.empty()) return {};

    ViewId target = 0;
    int bestOrder = std::numeric_limits<int>::min();
    for (const auto view : candidates) {
        if (!WebRuntime::IsValid(view) || WebRuntime::IsHidden(view) || WebRuntime::GetViewRole(view) != 2u) continue;
        const int order = WebRuntime::GetOrder(view);
        if (!target || order > bestOrder || (order == bestOrder && view > target)) {
            target = view;
            bestOrder = order;
        }
    }
    if (!target) return {};

    std::shared_ptr<FocusEntryRegistration> registration;
    bool shouldRequest = false;
    {
        std::lock_guard lock(g_focusEntryMutex);
        const auto found = g_focusEntries.find(target);
        if (found == g_focusEntries.end() || !found->second || found->second->cancelled ||
            found->second->buttonCode != buttonCode) {
            return {};
        }
        registration = found->second;
        shouldRequest = registration->sequence.ShouldRequest(true, false, justPressed);
        if (!shouldRequest) {
            return {true, registration->sequence.Owns(true, false, released)};
        }
    }

    bool admitted = false;
    try {

        admitted = GameThreadDispatcher::Dispatch([registration, target]() {
            bool execute = false;
            {
                std::lock_guard lock(g_focusEntryMutex);
                const auto found = g_focusEntries.find(target);
                if (found != g_focusEntries.end() && found->second == registration &&
                    !registration->cancelled && registration->callback) {
                    registration->executing = true;
                    registration->executingThread = std::this_thread::get_id();
                    execute = true;
                }
            }
            if (!execute) return;

            try {
                registration->callback(registration->userdata);
            } catch (...) {

            }

            {
                std::lock_guard lock(g_focusEntryMutex);
                registration->executing = false;
                registration->executingThread = {};
            }
            registration->idle.notify_all();
        }, target);
    } catch (...) {
        admitted = false;
    }

    const bool ownUntilFocus = admitted && !WebRuntime::HasAnyActiveFocus();
    bool stillCurrent = false;
    {
        std::lock_guard lock(g_focusEntryMutex);
        const auto found = g_focusEntries.find(target);
        stillCurrent = found != g_focusEntries.end() && found->second == registration && !registration->cancelled;
        if (stillCurrent) registration->sequence.OnDispatchResult(ownUntilFocus);
    }

    return {true, admitted && stillCurrent};
#endif
}

}
