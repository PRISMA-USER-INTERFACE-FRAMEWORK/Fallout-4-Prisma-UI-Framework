#pragma once

#include <cstdint>

namespace PrismaUI::Hooks {
    enum class HookInstallStrategy { DirectMinHook, SwapchainChain, Reject };

    struct HookInstallObservation {
        bool readable = false;
        bool ownerAllowed = false;
        bool clean = false;
        bool chainable = false;
    };

    inline HookInstallStrategy ChooseHookInstallStrategy(const HookInstallObservation& present,
                                                          const HookInstallObservation& resize) {
        if (present.readable && resize.readable && present.ownerAllowed && resize.ownerAllowed &&
            present.clean && resize.clean)
            return HookInstallStrategy::DirectMinHook;
        if (present.readable && resize.readable && present.chainable && resize.chainable)
            return HookInstallStrategy::SwapchainChain;
        return HookInstallStrategy::Reject;
    }

    constexpr bool IsChainEnbPairCompatible(bool presentKnownEnbProxy, bool resizeKnownEnbProxy,
                                             bool sameModuleIdentity) {
        return !(presentKnownEnbProxy && resizeKnownEnbProxy) || sameModuleIdentity;
    }

    template <class Fn>
    constexpr Fn SelectHookForwarder(HookInstallStrategy strategy, Fn chainPrevious, Fn minHookTrampoline) {
        return strategy == HookInstallStrategy::SwapchainChain ? chainPrevious : minHookTrampoline;
    }

    enum class ReattachAction { None, DiagnoseOnly, Reattach };

    struct HookCoverage {
        uintptr_t ourDetourA   = 0;
        uintptr_t ourDetourB   = 0;
        uintptr_t ourResizeDetourA = 0;
        uintptr_t ourResizeDetourB = 0;
        uintptr_t activePresentDetour = 0;
        uintptr_t activeResizeDetour = 0;
        uintptr_t hookedTarget = 0;
        uint32_t  generation   = 0;
        uintptr_t coveredSwapChain = 0;
        HookInstallStrategy strategy = HookInstallStrategy::Reject;
        bool      chainOwned = false;
        bool      activePresentForwardable = false;
        bool      activeResizeForwardable = false;
    };

    struct HookDetourPair {
        uintptr_t primary = 0;
        uintptr_t alternate = 0;
    };

    constexpr HookDetourPair SelfDetours(const HookCoverage& cov, bool resize) {
        return resize ? HookDetourPair{cov.ourResizeDetourA, cov.ourResizeDetourB}
                      : HookDetourPair{cov.ourDetourA, cov.ourDetourB};
    }

    constexpr bool RelaySlotReusable(bool isForwardRelay) { return !isForwardRelay; }

    inline bool CanUseActiveForwarder(const HookCoverage& cov, bool resize, uintptr_t liveSwapChain,
                                      uintptr_t target, uintptr_t jumpTarget) {
        const auto activeDetour = resize ? cov.activeResizeDetour : cov.activePresentDetour;
        const bool forwardable = resize ? cov.activeResizeForwardable : cov.activePresentForwardable;
        if (!activeDetour || !forwardable || (target != activeDetour && jumpTarget != activeDetour)) return false;
        return cov.strategy != HookInstallStrategy::SwapchainChain || liveSwapChain == cov.coveredSwapChain;
    }

    struct ReattachInputs {
        bool      presentStale       = false;
        bool      minimized          = false;
        bool      liveTargetReadable = false;
        bool      resizeTargetSafe   = false;
        uintptr_t liveTarget         = 0;
        uintptr_t liveSwapChain      = 0;
        bool      liveTargetStable   = false;

        bool      liveTargetJumpsToUs = false;
        int64_t   nowMs              = 0;
        int64_t   lastAttemptMs      = 0;
    };

    struct ReattachDecision {
        ReattachAction action;
        const char* reason;
    };

    ReattachDecision ShouldReattach(const ReattachInputs& in, const HookCoverage& cov, int64_t rateLimitMs);

    struct StabilityConfig {
        int64_t staleMs      = 3000;
        int64_t minSpacingMs = 500;
    };

    struct StabilityState {
        int64_t   lastProbeMs      = 0;
        uintptr_t lastTarget       = 0;
        uintptr_t lastSwapChain    = 0;
        uint32_t  lastGeneration   = 0;
        uint64_t  lastPresentCount = 0;
    };

    struct StabilityInputs {
        int64_t   nowMs         = 0;
        int64_t   lastPresentMs = 0;
        int64_t   startMs       = 0;
        uint64_t  presentCount  = 0;
        uintptr_t liveTarget    = 0;
        uintptr_t liveSwapChain = 0;
        uint32_t  generation    = 0;
    };

    struct StabilityVerdict {
        bool presentStale = false;
        bool stable       = false;
    };

    StabilityVerdict EvaluatePresentStability(const StabilityInputs& in, StabilityState& state,
                                              const StabilityConfig& cfg);
}
