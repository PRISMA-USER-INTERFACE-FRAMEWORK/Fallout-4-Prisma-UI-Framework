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

    struct ChainSlotOriginals {
        void* present = nullptr;
        void* resize = nullptr;
    };

    constexpr ChainSlotOriginals SelectChainSlotOriginals(const void* liveVtable, const void* shadowVtable,
                                                          void* originalPresent, void* originalResize) {
        if (!liveVtable || liveVtable != shadowVtable || !originalPresent || !originalResize)
            return ChainSlotOriginals{};
        return ChainSlotOriginals{originalPresent, originalResize};
    }

    struct QuarantineEntry {
        uintptr_t target = 0;
        uintptr_t trampoline = 0;
        int       slot = -1;
        bool      enabled = false;
        bool      drained = false;
    };

    constexpr int kQuarantineCapacity = 8;

    enum class QuarantineAdd { Recorded, AlreadyRecorded, Full };

    class TrampolineQuarantine {
    public:
        QuarantineAdd Record(uintptr_t target, uintptr_t trampoline, int slot) {
            if (!target) return QuarantineAdd::Full;
            if (Find(target) >= 0) return QuarantineAdd::AlreadyRecorded;
            if (size_ >= kQuarantineCapacity) return QuarantineAdd::Full;
            records_[size_++] = QuarantineEntry{target, trampoline, slot, false};
            return QuarantineAdd::Recorded;
        }

        int Find(uintptr_t target) const {
            if (!target) return -1;
            for (int i = 0; i < size_; ++i) {
                if (records_[i].target == target) return i;
            }
            return -1;
        }

        const QuarantineEntry* At(int index) const {
            if (index < 0 || index >= size_) return nullptr;
            return &records_[index];
        }

        bool MarkEnabled(uintptr_t target, bool enabled) {
            const int index = Find(target);
            if (index < 0) return false;
            records_[index].enabled = enabled;
            if (enabled) records_[index].drained = false;
            return true;
        }

        bool MarkDrained(uintptr_t target, bool drained) {
            const int index = Find(target);
            if (index < 0) return false;
            records_[index].drained = drained;
            return true;
        }

        bool Forget(uintptr_t target) {
            const int index = Find(target);
            if (index < 0) return false;
            records_[index] = records_[--size_];
            records_[size_] = QuarantineEntry{};
            return true;
        }

        int Size() const { return size_; }
        bool Full() const { return size_ >= kQuarantineCapacity; }
        bool Empty() const { return size_ == 0; }

    private:
        QuarantineEntry records_[kQuarantineCapacity]{};
        int size_ = 0;
    };

    enum class HookEntryAction { Create, Reenable, Recreate, Refuse };

    inline HookEntryAction DecideHookEntry(const TrampolineQuarantine& quarantine, uintptr_t target, int slot) {
        const QuarantineEntry* entry = quarantine.At(quarantine.Find(target));
        if (entry) {
            if (entry->slot == slot) return HookEntryAction::Reenable;
            return !entry->enabled && entry->drained ? HookEntryAction::Recreate : HookEntryAction::Refuse;
        }
        return quarantine.Full() ? HookEntryAction::Refuse : HookEntryAction::Create;
    }

    inline const char* DescribeHookEntryRefusal(const TrampolineQuarantine& quarantine, uintptr_t target,
                                                int slot) {
        const QuarantineEntry* entry = quarantine.At(quarantine.Find(target));
        if (!entry) return "the trampoline quarantine is full";
        if (entry->slot == slot) return "the target is already owned by this slot";
        return "MinHook still owns a live or undrained entry at this target for the other slot";
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
