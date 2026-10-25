#include "Hooks/ReattachPolicy.h"

namespace PrismaUI::Hooks {
    ReattachDecision ShouldReattach(const ReattachInputs& in, const HookCoverage& cov, int64_t rateLimitMs) {
        if (in.minimized) return {ReattachAction::None, "minimized"};
        if (!in.presentStale) return {ReattachAction::None, "present-fresh"};
        if (!in.liveTargetReadable || in.liveTarget == 0)
            return {ReattachAction::DiagnoseOnly, "live-target-unreadable"};
        if (in.liveTarget == cov.ourDetourA || in.liveTarget == cov.ourDetourB)
            return {ReattachAction::None, "still-covered-by-our-detour"};
        if (cov.strategy == HookInstallStrategy::SwapchainChain &&
            in.liveSwapChain != cov.coveredSwapChain) {
            if (in.liveSwapChain == 0)
                return {ReattachAction::DiagnoseOnly, "live-swapchain-unreadable"};
            if (!in.liveTargetStable)
                return {ReattachAction::DiagnoseOnly, "swapchain-replaced-unstable"};
            if (!in.resizeTargetSafe)
                return {ReattachAction::DiagnoseOnly, "swapchain-replaced-resize-unsafe"};
            if (rateLimitMs > 0 && in.lastAttemptMs > 0 &&
                in.nowMs >= in.lastAttemptMs && in.nowMs - in.lastAttemptMs < rateLimitMs)
                return {ReattachAction::None, "rate-limited"};
            return {ReattachAction::Reattach, "swapchain-replaced"};
        }
        if (cov.strategy == HookInstallStrategy::SwapchainChain && !cov.chainOwned) {
            if (in.liveSwapChain == 0)
                return {ReattachAction::DiagnoseOnly, "live-swapchain-unreadable"};
            if (!in.liveTargetStable)
                return {ReattachAction::DiagnoseOnly, "chain-lost-unstable"};
            if (!in.resizeTargetSafe && !cov.activeResizeForwardable)
                return {ReattachAction::DiagnoseOnly, "chain-lost-resize-unsafe"};
            if (rateLimitMs > 0 && in.lastAttemptMs > 0 &&
                in.nowMs >= in.lastAttemptMs && in.nowMs - in.lastAttemptMs < rateLimitMs)
                return {ReattachAction::None, "rate-limited"};
            return {ReattachAction::Reattach, "chain-lost"};
        }
        if (in.liveTarget == cov.hookedTarget) {

            if (in.liveTargetJumpsToUs)
                return {ReattachAction::DiagnoseOnly, "same-target-our-jmp"};
            if (!in.liveTargetStable)
                return {ReattachAction::DiagnoseOnly, "same-target-unstable"};
            if (rateLimitMs > 0 && in.lastAttemptMs > 0 &&
                in.nowMs >= in.lastAttemptMs && in.nowMs - in.lastAttemptMs < rateLimitMs)
                return {ReattachAction::None, "rate-limited"};
            return {ReattachAction::DiagnoseOnly, "same-target-foreign-prologue"};
        }
        if (!in.liveTargetStable)
            return {ReattachAction::DiagnoseOnly, "new-target-unstable"};
        if (!in.resizeTargetSafe && !cov.activeResizeForwardable)
            return {ReattachAction::DiagnoseOnly, "resize-target-unsafe"};

        if (rateLimitMs > 0 && in.lastAttemptMs > 0 &&
            in.nowMs >= in.lastAttemptMs && in.nowMs - in.lastAttemptMs < rateLimitMs)
            return {ReattachAction::None, "rate-limited"};
        return {ReattachAction::Reattach, "live-target-moved"};
    }

    StabilityVerdict EvaluatePresentStability(const StabilityInputs& in, StabilityState& st,
                                              const StabilityConfig& cfg) {

        const int64_t reference = in.lastPresentMs != 0 ? in.lastPresentMs : in.startMs;
        const bool stale = reference != 0 && in.nowMs - reference > cfg.staleMs;

        if (!stale) {
            st = StabilityState{};
            return {false, false};
        }

        const bool presentSincePrev = st.lastProbeMs != 0 && in.presentCount != st.lastPresentCount;
        const bool stable = st.lastProbeMs != 0 &&
                            in.liveTarget == st.lastTarget &&
                            in.liveSwapChain == st.lastSwapChain &&
                            in.generation == st.lastGeneration &&
                            !presentSincePrev &&
                            in.nowMs - st.lastProbeMs >= cfg.minSpacingMs;

        st.lastProbeMs      = in.nowMs;
        st.lastTarget       = in.liveTarget;
        st.lastSwapChain    = in.liveSwapChain;
        st.lastGeneration   = in.generation;
        st.lastPresentCount = in.presentCount;
        return {true, stable};
    }
}
