#include "WebRuntime.h"
#include "WebRuntimeInternal.h"
#include <Windows.h>
#include "GameThreadDispatcher.h"
#include "WebInput.h"
#include "InputMask.h"
#include "Menus/PauseHold/PauseHold.h"
#include "PresentationHeartbeatPolicy.h"
#ifndef PRISMAUI_FO4VR
    #include "Hooks/Hooks.h"
    #include "Hooks/InlineHookClassifier.h"
    #include "Utils/ConflictChecker.h"
#endif
#include <chrono>
#include <thread>
namespace PrismaUI::WebRuntime {
    namespace {
        auto& st = Internal::RT();
        using namespace Internal;
        constexpr int64_t kUnresponsiveGraceMs = 3000;
        constexpr int64_t kPresentationStaleMs = 2000;
        constexpr int64_t kPresentAliveMs = 500;
        void DiscoverInputWindow() {
            HWND h = st.inputHwnd.load();
            if (!h) {
                if (auto* rendererData = RE::BSGraphics::GetRendererData()) {
                    h = reinterpret_cast<HWND>(rendererData->renderWindow[0].hwnd);
                    if (h) st.inputHwnd.store(h);
                }
            }
            if (h && !WebInput::IsInstalled()) (void)WebInput::QueueInstall(h);
        }
#ifndef PRISMAUI_FO4VR
        constexpr int64_t kPresentStaleReattachMs = 3000;
        constexpr int64_t kReattachRateLimitMs = 5000;
        constexpr int64_t kProbeStuckTimeoutMs = 5000;
        constexpr int64_t kMinProbeSpacingMs = 500;
        bool ClassifyTarget(void* target, const PrismaUI::Hooks::HookCoverage& cov, bool resize,
                            PrismaUI::Hooks::HookClassification* out = nullptr) {
            PrismaUI::Hooks::HookClassification classificationStorage;
            auto* classificationOut = out ? out : &classificationStorage;
            const auto selfDetours = PrismaUI::Hooks::SelfDetours(cov, resize);
            const auto decision = PrismaUI::ConflictChecker::ClassifyHookTargetForInstall(
                target, classificationOut, reinterpret_cast<void*>(selfDetours.primary),
                reinterpret_cast<void*>(selfDetours.alternate));
            return decision.safe || decision.chainAllowed;
        }
        void ProbeAndMaybeReattachPresentHook(int64_t nowMs) {
            const HWND hwnd = st.inputHwnd.load(std::memory_order_acquire);
            if (hwnd && IsIconic(hwnd)) return;
            auto* rd = RE::BSGraphics::GetRendererData();
            auto* swap = (rd && rd->renderWindow[0].swapChain) ? rd->renderWindow[0].swapChain : nullptr;
            if (!swap) return;
            void** vtable = *reinterpret_cast<void***>(swap);
            void* liveTarget = vtable[8];
            void* liveResize = vtable[13];
            const PrismaUI::Hooks::HookCoverage cov = ::Hooks::D3DHooks::CurrentCoverage();
            PrismaUI::Hooks::StabilityInputs sin;
            sin.nowMs = nowMs;
            sin.lastPresentMs = st.lastPresentMs.load(std::memory_order_acquire);
            sin.startMs = st.recoveryStartMs.load(std::memory_order_acquire);
            sin.presentCount = st.presentCount.load(std::memory_order_acquire);
            sin.liveTarget = reinterpret_cast<uintptr_t>(liveTarget);
            sin.liveSwapChain = reinterpret_cast<uintptr_t>(swap);
            sin.generation = cov.generation;
            static PrismaUI::Hooks::StabilityState s_stability;
            const PrismaUI::Hooks::StabilityVerdict verdict = PrismaUI::Hooks::EvaluatePresentStability(
                sin, s_stability, {kPresentStaleReattachMs, kMinProbeSpacingMs});
            if (!verdict.presentStale) return;
            PrismaUI::Hooks::HookClassification presentClass;
            const bool presentSafeToHook = ClassifyTarget(liveTarget, cov, false, &presentClass);
            const bool resizeSafeToHook = ClassifyTarget(liveResize, cov, true);
            PrismaUI::Hooks::ReattachInputs in;
            in.presentStale = true;
            in.minimized = false;
            in.liveTargetReadable = presentSafeToHook;
            in.resizeTargetSafe = resizeSafeToHook;
            in.liveTarget = reinterpret_cast<uintptr_t>(liveTarget);
            in.liveSwapChain = reinterpret_cast<uintptr_t>(swap);
            in.liveTargetStable = verdict.stable;
            in.liveTargetJumpsToUs = presentClass.isInlineJmp &&
                                     (presentClass.jmpTarget == cov.ourDetourA || presentClass.jmpTarget == cov.ourDetourB);
            in.nowMs = nowMs;
            in.lastAttemptMs = ::Hooks::D3DHooks::s_lastReattachAttemptMs.load(std::memory_order_acquire);
            const PrismaUI::Hooks::ReattachDecision decision =
                PrismaUI::Hooks::ShouldReattach(in, cov, kReattachRateLimitMs);
            if (decision.action == PrismaUI::Hooks::ReattachAction::Reattach) {
                logger::warn("[Reattach] recovery=start reason={} Present owner='{}' eligible={}", decision.reason,
                             presentClass.ownerModule.empty() ? "?" : presentClass.ownerModule.c_str(),
                             presentSafeToHook ? "yes" : "no");
                const bool reattached = ::Hooks::D3DHooks::Reattach(liveTarget, liveResize, nowMs, cov.generation);
                logger::warn("[Reattach] recovery={} reason={}", reattached ? "succeeded" : "failed", decision.reason);
            }
        }
        void MaybeReattachPresentHook(int64_t nowMs) { ProbeAndMaybeReattachPresentHook(nowMs); }
        void SchedulePresentHookProbeIfStale(int64_t nowMs) {
            const int64_t lastPresent = st.lastPresentMs.load(std::memory_order_acquire);
            const int64_t reference = lastPresent != 0 ? lastPresent : st.recoveryStartMs.load(std::memory_order_acquire);
            if (reference == 0 || nowMs - reference <= kPresentStaleReattachMs) return;
            if (const HWND hwnd = st.inputHwnd.load(std::memory_order_acquire); hwnd && IsIconic(hwnd)) return;
            static std::atomic<int64_t> s_probeInFlightSinceMs{0};
            const int64_t pendingSince = s_probeInFlightSinceMs.load(std::memory_order_acquire);
            if (pendingSince != 0 && nowMs - pendingSince < kProbeStuckTimeoutMs) return;
            int64_t expected = pendingSince;
            if (!s_probeInFlightSinceMs.compare_exchange_strong(expected, nowMs, std::memory_order_acq_rel)) return;
            const int64_t myToken = nowMs;
            if (auto* task = F4SE::GetTaskInterface()) {
                task->AddTask([myToken] {
                    if (s_probeInFlightSinceMs.load(std::memory_order_acquire) != myToken) return;
                    MaybeReattachPresentHook(HealthNowMs());
                    int64_t owned = myToken;
                    s_probeInFlightSinceMs.compare_exchange_strong(owned, 0, std::memory_order_acq_rel);
                });
            } else {
                int64_t owned = myToken;
                s_probeInFlightSinceMs.compare_exchange_strong(owned, 0, std::memory_order_acq_rel);
            }
        }
#endif
        void PresentIndependentWatchdog() {
            constexpr int64_t kPresentDeadMs = 3000;
            uint32_t handledGen = UINT32_MAX;
            while (st.watchdogRunning.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                RetryInspectorTeardown();
                TryInstallGameInput();
#ifndef PRISMAUI_FO4VR
                SchedulePresentHookProbeIfStale(HealthNowMs());
#endif
                const ViewId focused = st.focusedView.load();
                if (focused == 0 || !st.focusSession.load()) continue;
                if (const HWND hwnd = st.inputHwnd.load(); hwnd && IsIconic(hwnd)) continue;
                const uint32_t gen = st.focusGeneration.load();
                const int64_t now = HealthNowMs();
                const int64_t episodeStart = st.focusEpisodeStartMs.load();
                const int64_t lastPresent = st.lastPresentMs.load();
                const bool heldLongEnough = episodeStart && (now - episodeStart) > kPresentDeadMs;
                const bool presentSilent = (lastPresent == 0) || (now - lastPresent > kPresentDeadMs);
                if (heldLongEnough && presentSilent && handledGen != gen) {
                    logger::warn(
                        "[SAFETY] Present-independent watchdog: focus held {}ms but Present silent "
                        "{}ms (view={}) -- presentation path may be gone; scheduling focus release",
                        now - episodeStart, lastPresent ? (now - lastPresent) : -1, focused);
                    if (ScheduleFocusRelease(focused)) handledGen = gen;
                }
            }
        }
        void MaybeRecoverDeadFocus() {
            const ViewId focused = st.focusedView.load();
            static uint32_t s_episodeGen = UINT32_MAX;
            static bool s_handled = false;
            static int64_t s_unresponsiveSinceMs = 0;
            const uint32_t gen = st.focusGeneration.load();
            if (gen != s_episodeGen) {
                s_episodeGen = gen;
                s_handled = false;
                s_unresponsiveSinceMs = 0;
            }
            if (focused == 0 || !st.backend || s_handled) return;
            const bool invalid = !st.backend->IsViewValid(focused);
            const bool hidden = !IsRuntimeViewVisible(focused);
            const int health = static_cast<int>(st.backend->GetViewHealth(focused));
            const bool terminal = invalid || hidden || health == static_cast<int>(PrismaUI::Web::ViewHealth::LoadFailed) ||
                                  health == static_cast<int>(PrismaUI::Web::ViewHealth::DomReadyTimeout);
            const int64_t now = HealthNowMs();
            bool recover = terminal;
            if (!terminal) {
                if (health == static_cast<int>(PrismaUI::Web::ViewHealth::Unresponsive)) {
                    if (s_unresponsiveSinceMs == 0) s_unresponsiveSinceMs = now;
                    if (now - s_unresponsiveSinceMs >= kUnresponsiveGraceMs) recover = true;
                } else {
                    s_unresponsiveSinceMs = 0;
                }
            }
            if (recover) {
                const char* why = invalid  ? "invalid (destroyed)"
                                  : hidden ? "hidden"
                                  : health == static_cast<int>(PrismaUI::Web::ViewHealth::LoadFailed) ? "load-failed"
                                  : health == static_cast<int>(PrismaUI::Web::ViewHealth::DomReadyTimeout)
                                      ? "dom-ready-timeout"
                                      : "unresponsive";
                logger::warn(
                    "[WebRuntime] pause fail-safe: focused view {} is {} (health={}) -- scheduling focus "
                    "release so input and pause recover",
                    focused, why, health);
                s_handled = ScheduleFocusRelease(focused);
                return;
            }
            const int64_t lastPresent = st.lastPresentMs.load(std::memory_order_acquire);
            const int64_t presentAge = lastPresent ? now - lastPresent : -1;
            const int64_t focusStart = st.focusEpisodeStartMs.load(std::memory_order_acquire);
            const int64_t lastSuccessfulPresentation =
                st.lastSuccessfulPresentationMs.load(std::memory_order_acquire);
            const bool isLive = health == static_cast<int>(PrismaUI::Web::ViewHealth::Live);
            const PrismaUI::PresentationHeartbeatPolicy::Inputs presentationInputs{
                isLive,
                st.focusSession.load(std::memory_order_acquire),
                lastPresent != 0 && now >= lastPresent && presentAge < kPresentAliveMs,
                now,
                focusStart,
                lastSuccessfulPresentation,
            };
            if (PrismaUI::PresentationHeartbeatPolicy::ShouldEmergencyReleaseFocus(presentationInputs, kPresentationStaleMs)) {
                const int64_t reference = PrismaUI::PresentationHeartbeatPolicy::ReferenceMs(presentationInputs);
                const int64_t presentationAge = now - reference;
                logger::warn(
                    "[SAFETY] Emergency focus release: kLive view {} had no successful presentation for {}ms "
                    "this focus episode (Present alive {}ms ago, last fresh composite {}ms ago) -- scheduling "
                    "pause/input/cursor/focus release",
                    focused, presentationAge, presentAge,
                    st.lastCompositeMs.load() ? now - st.lastCompositeMs.load() : -1);
                s_handled = ScheduleFocusRelease(focused);
            }
        }
        void LogPrismaHealthPeriodic() {
            const ViewId focused = st.focusedView.load();
            if (focused == 0) return;
            static int64_t s_lastLogMs = 0;
            const int64_t now = HealthNowMs();
            if (s_lastLogMs != 0 && now - s_lastLogMs < 5000) return;
            s_lastLogMs = now;
            const int64_t presentAge = st.lastPresentMs.load() ? now - st.lastPresentMs.load() : -1;
            const int64_t compositeAge = st.lastCompositeMs.load() ? now - st.lastCompositeMs.load() : -1;
            const int64_t presentationAge =
                st.lastSuccessfulPresentationMs.load() ? now - st.lastSuccessfulPresentationMs.load() : -1;
            const int64_t focusAge = st.focusEpisodeStartMs.load() ? now - st.focusEpisodeStartMs.load() : -1;
            const int64_t hostPaintCbAge = st.lastPaintChangeMs.load() ? now - st.lastPaintChangeMs.load() : -1;
            logger::info(
                "[PrismaHealth] view={} pause={} capture={} mask={} cursor={} srvPresent={} "
                "hostPaintCbAge={}ms presentAge={}ms compositeAge={}ms presentationAge={}ms focusAge={}ms "
                "composites={} ctxDropStreak={} swapchain={}",
                focused, PauseHold::IsRequested() ? 1 : 0, WebInput::IsCaptureActive() ? 1 : 0,
                InputMask::IsApplied() ? 1 : 0, WebInput::IsCursorOwned() ? 1 : 0, st.srvPresent.load() ? 1 : 0,
                hostPaintCbAge, presentAge, compositeAge, presentationAge, focusAge, st.compositeCount.load(),
                st.contextDropStreak.load(), static_cast<const void*>(st.lastSwapChain.load()));
        }
    }
    bool IsActive() { return st.active.load(); }
    bool WaitUntilActive(int timeoutMs) {
        if (st.active.load()) return true;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (st.active.load()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        return st.active.load();
    }
    bool IsHostLoaded() { return st.backendReady.load(); }
    bool WaitUntilHostLoaded(int timeoutMs) {
        if (st.backendReady.load()) return true;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            if (st.backendReady.load()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
        return st.backendReady.load();
    }
    bool EnsureLoaded() {
        if (st.active.load(std::memory_order_acquire)) return true;
        std::lock_guard<std::mutex> lock(st.loadMutex);
        if (st.active.load(std::memory_order_acquire)) return true;
        if (st.initFailed.load(std::memory_order_acquire)) return false;
        if (st.startupAccepted.load(std::memory_order_acquire) && st.backendReady.load(std::memory_order_acquire)) {
            return true;
        }
        return DoLoadAndInit();
    }
    bool LoadAndInit() { return EnsureLoaded(); }
    void TryInstallGameInput() {
        if (WebInput::IsInstalled()) {
#ifndef PRISMAUI_FO4VR
            if (!GameThreadDispatcher::IsReady()) (void)WebInput::QueueDispatcherReattach();
#endif
            return;
        }
        const HWND h = st.inputHwnd.load();
        if (h) {
            (void)WebInput::QueueInstall(h);
            return;
        }
        if (st.inputInstallPosted.exchange(true)) return;
        if (auto* task = F4SE::GetTaskInterface()) {
            task->AddTask([] {
                DiscoverInputWindow();
                st.inputInstallPosted.store(false);
            });
        } else {
            st.inputInstallPosted.store(false);
        }
    }
    void StartRecoveryWatchdog() {
        if (st.watchdogRunning.exchange(true)) return;
        st.recoveryStartMs.store(HealthNowMs(), std::memory_order_release);
        std::thread(PresentIndependentWatchdog).detach();
        logger::info("[WebRuntime] Present-independent recovery watchdog started (500ms poll)");
    }
}
namespace PrismaUI::WebRuntime::Internal {
    namespace {
        auto& st = Internal::RT();
    }
    bool ScheduleFocusRelease(ViewId focused) {
#ifndef PRISMAUI_FO4VR
        if (GameThreadDispatcher::DispatchSafety([focused] { Unfocus(focused); })) return true;
        logger::critical(
            "[WebRuntime] window-thread dispatcher could not schedule Unfocus(view={}); "
            "leaving focus state unchanged because no verified execution path remains",
            focused);
        return false;
#else
        (void)focused;
        return false;
#endif
    }
    void TickPauseMaintenance() {
        RetryInspectorTeardown();
        PauseHold::Tick();
        MaybeRecoverDeadFocus();
        LogPrismaHealthPeriodic();
        static std::atomic<unsigned> watchdogFrames{0};
        if (watchdogFrames.fetch_add(1, std::memory_order_relaxed) + 1 >= 120) {
            watchdogFrames.store(0, std::memory_order_relaxed);
            if (st.focusedView.load() == 0) PauseHold::Set(false);
        }
    }
}
