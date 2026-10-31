#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace PrismaUI::PresentHold {

    inline constexpr int64_t kDarkLatchMs = 2000;
    inline constexpr int64_t kHeldRefreshMinMs = 500;
    inline constexpr int64_t kOverlayHoldMs = 2000;
    inline constexpr int64_t kStarvationMinMs = 2000;

    enum class Action : uint8_t {
        Blank,
        ComposeCommit,
        BlitCommitted,
        Dark,
    };

    struct Decision {
        Action action = Action::Blank;
    };

    struct ViewSource {
        uint64_t id = 0;
        bool hasFresh = false;
        bool hasHeld = false;
    };

    struct ViewContent {
        uint64_t id = 0;
        bool hasFresh = false;
        uint64_t freshContentGeneration = 0;
    };

    struct Classification {
        bool complete = false;
        bool committedFaithful = false;
    };

    enum class ReuseReason : uint8_t {
        NoRecompose,
        FreshSourceMissing,
        ContentGenerationStalled,
        ViewSetChange,
        OverlayChange,
        ComposeFailure,
        PresentLeaseUnavailable,
    };

    struct ReuseDiagnosticInput {
        bool leaseAvailable = true;
        bool freshSourceMissing = false;
        bool contentGenerationStalled = false;
        bool viewSetChanged = false;
        bool overlayChanged = false;
        bool composeFailed = false;
        bool needsRecompose = false;
        bool committedFaithful = false;
        bool haveCommitted = false;
    };

    struct ReuseDiagnostic {
        ReuseReason reason = ReuseReason::NoRecompose;
        bool starvationCandidate = false;
    };

    [[nodiscard]] constexpr ReuseReason ClassifyReuseReason(const ReuseDiagnosticInput& input) noexcept {
        if (!input.leaseAvailable) return ReuseReason::PresentLeaseUnavailable;
        if (input.composeFailed) return ReuseReason::ComposeFailure;
        if (input.viewSetChanged) return ReuseReason::ViewSetChange;
        if (input.overlayChanged) return ReuseReason::OverlayChange;
        if (input.contentGenerationStalled) return ReuseReason::ContentGenerationStalled;
        if (input.freshSourceMissing) return ReuseReason::FreshSourceMissing;
        return ReuseReason::NoRecompose;
    }

    [[nodiscard]] constexpr bool IsStarvationCandidate(const ReuseDiagnosticInput& input,
                                                       bool sustained) noexcept {
        return sustained && input.leaseAvailable && input.haveCommitted && input.committedFaithful &&
               !input.viewSetChanged && input.needsRecompose &&
               (input.freshSourceMissing || input.contentGenerationStalled) &&
               !input.composeFailed;
    }

    [[nodiscard]] constexpr ReuseDiagnostic ClassifyReuseDiagnostic(const ReuseDiagnosticInput& input,
                                                                     bool sustained) noexcept {
        return {ClassifyReuseReason(input), IsStarvationCandidate(input, sustained)};
    }

    [[nodiscard]] constexpr const char* ReuseReasonName(ReuseReason reason) noexcept {
        switch (reason) {
            case ReuseReason::NoRecompose: return "no-recompose";
            case ReuseReason::FreshSourceMissing: return "fresh-source-missing";
            case ReuseReason::ContentGenerationStalled: return "content-generation-stalled";
            case ReuseReason::ViewSetChange: return "view-set-change";
            case ReuseReason::OverlayChange: return "overlay-change";
            case ReuseReason::ComposeFailure: return "compose-failure";
            case ReuseReason::PresentLeaseUnavailable: return "present-lease-unavailable";
        }
        return "unknown";
    }

    [[nodiscard]] constexpr bool HeldSourceEpochValid(bool presentationStateValid, uint64_t heldEpoch,
                                                       uint64_t currentEpoch) noexcept {
        return presentationStateValid && heldEpoch == currentEpoch;
    }

    [[nodiscard]] inline Classification Classify(const std::vector<ViewSource>& visible,
                                                 const std::vector<uint64_t>& committed) {
        Classification c;
        bool anySource = false;
        bool displayedViewMissingSource = false;
        for (const auto& view : visible) {
            const bool hasSource = view.hasFresh || view.hasHeld;
            anySource = anySource || hasSource;
            if (hasSource) continue;
            for (const auto id : committed) {
                if (id == view.id) {
                    displayedViewMissingSource = true;
                    break;
                }
            }
        }
        c.complete = anySource && !displayedViewMissingSource;
        c.committedFaithful = !committed.empty();
        for (const auto id : committed) {
            bool stillVisible = false;
            for (const auto& view : visible) {
                if (view.id == id) {
                    stillVisible = true;
                    break;
                }
            }
            if (!stillVisible) {
                c.committedFaithful = false;
                break;
            }
        }
        return c;
    }

    [[nodiscard]] constexpr bool OverlayContentClean(uint64_t liveGeneration, uint64_t committedGeneration) noexcept {
        return liveGeneration != 0 && liveGeneration == committedGeneration;
    }

    struct HeldSlotKey {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t texFormat = 0;
        uint32_t mipLevels = 0;
        uint32_t arraySize = 0;
        uint32_t sampleCount = 0;
        uint32_t sampleQuality = 0;
        uint32_t srvFormat = 0;
        uint32_t srvViewDim = 0;
        uint32_t srvMostDetailedMip = 0;
        uint32_t srvMipLevels = 0;
    };

    [[nodiscard]] constexpr bool HeldSlotReusable(const HeldSlotKey& have, const HeldSlotKey& want) noexcept {
        return have.width == want.width && have.height == want.height && have.texFormat == want.texFormat &&
               have.mipLevels == want.mipLevels && have.arraySize == want.arraySize &&
               have.sampleCount == want.sampleCount && have.sampleQuality == want.sampleQuality &&
               have.srvFormat == want.srvFormat && have.srvViewDim == want.srvViewDim &&
               have.srvMostDetailedMip == want.srvMostDetailedMip && have.srvMipLevels == want.srvMipLevels;
    }

    enum class OverlayAction : uint8_t { DrawLive, DrawHeld, Drop };

    struct OverlayInput {
        uint64_t key = 0;
        int order = 0;
        bool live = false;
        bool held = false;
        bool tombstoned = false;
    };

    struct OverlayResolved {
        uint64_t key = 0;
        int order = 0;
        OverlayAction action = OverlayAction::Drop;
    };

    [[nodiscard]] constexpr OverlayAction ReconcileOverlay(bool live, bool held, bool tombstoned) noexcept {
        if (live) return OverlayAction::DrawLive;
        if (tombstoned || !held) return OverlayAction::Drop;
        return OverlayAction::DrawHeld;
    }

    [[nodiscard]] inline std::vector<OverlayResolved> ReconcileOverlays(std::vector<OverlayInput> overlays) {
        std::stable_sort(overlays.begin(), overlays.end(),
                         [](const OverlayInput& a, const OverlayInput& b) { return a.order < b.order; });
        std::vector<OverlayResolved> out;
        out.reserve(overlays.size());
        for (const auto& o : overlays) {
            const OverlayAction action = ReconcileOverlay(o.live, o.held, o.tombstoned);
            if (action == OverlayAction::Drop) continue;
            out.push_back({o.key, o.order, action});
        }
        return out;
    }

    [[nodiscard]] inline std::vector<uint64_t> PlanHeldCapture(const std::vector<uint64_t>& oldHeldOrder,
                                                              const std::vector<uint64_t>& liveOrder,
                                                              const std::vector<bool>& liveCopyOk) {
        for (const bool ok : liveCopyOk)
            if (!ok) return oldHeldOrder;
        std::vector<uint64_t> result = oldHeldOrder;
        for (const auto key : liveOrder) {
            bool placed = false;
            for (const auto have : result)
                if (have == key) { placed = true; break; }
            if (!placed) result.push_back(key);
        }
        return result;
    }

    [[nodiscard]] inline bool CommittedContainsRemoved(const std::vector<uint64_t>& committedOverlayKeys,
                                                       const std::vector<uint64_t>& removedKeys) {
        for (const auto committed : committedOverlayKeys)
            for (const auto removed : removedKeys)
                if (committed == removed) return true;
        return false;
    }

    [[nodiscard]] inline bool AnyContentChanged(const std::vector<ViewContent>& visible,
                                                const std::map<uint64_t, uint64_t>& committedContentGeneration) {
        for (const auto& view : visible) {
            if (!view.hasFresh) continue;
            if (view.freshContentGeneration == 0) return true;
            const auto it = committedContentGeneration.find(view.id);
            if (it == committedContentGeneration.end() || it->second != view.freshContentGeneration) return true;
        }
        return false;
    }

    enum class OverlaySourceChoice : uint8_t { Live, Held, None };

    [[nodiscard]] constexpr OverlaySourceChoice ChooseOverlaySource(bool liveAvailable, bool liveObservable,
                                                                   bool heldAvailable, int64_t nowMs,
                                                                   int64_t lastLiveSeenMs) noexcept {
        if (liveObservable && liveAvailable) return OverlaySourceChoice::Live;
        if (!heldAvailable) return OverlaySourceChoice::None;
        if (!liveObservable) return OverlaySourceChoice::Held;
        return nowMs - lastLiveSeenMs <= kOverlayHoldMs ? OverlaySourceChoice::Held : OverlaySourceChoice::None;
    }

    class Policy {
    public:
        [[nodiscard]] Decision OnPresent(int64_t nowMs, bool logicallyVisible, bool complete, bool committedFaithful,
                                         bool haveCommitted, bool needsRecompose) noexcept {
            if (!logicallyVisible) {
                latchUntilMs_ = 0;
                displayed_ = false;
                heldStreak_ = 0;
                return {Action::Blank};
            }
            if (nowMs < latchUntilMs_) {
                heldStreak_ = 0;
                return {Action::Dark};
            }
            if (complete) {
                if (!needsRecompose && haveCommitted && committedFaithful) {
                    ++heldStreak_;
                    return {Action::BlitCommitted};
                }
                heldStreak_ = 0;
                return {Action::ComposeCommit};
            }
            if (haveCommitted && committedFaithful) {
                ++heldStreak_;
                return {Action::BlitCommitted};
            }
            heldStreak_ = 0;
            return {Action::Dark};
        }

        [[nodiscard]] bool ShouldRefreshHeldSources(int64_t nowMs) const noexcept {
            return nowMs - lastHeldRefreshMs_ >= kHeldRefreshMinMs;
        }

        void OnHeldSourcesRefreshed(int64_t nowMs) noexcept { lastHeldRefreshMs_ = nowMs; }

        [[nodiscard]] bool ObserveFreshContentGenerations(const std::map<uint64_t, uint64_t>& generations,
                                                          int64_t nowMs) {
            const bool changed = generations != observedFreshContentGenerations_;
            if (changed) {
                observedFreshContentGenerations_ = generations;
                contentGenerationStallSinceMs_ = -1;
            } else if (!generations.empty() && contentGenerationStallSinceMs_ < 0) {
                contentGenerationStallSinceMs_ = nowMs;
            }
            return changed;
        }

        [[nodiscard]] bool ContentGenerationStalled(int64_t nowMs) const noexcept {
            return contentGenerationStallSinceMs_ >= 0 && nowMs - contentGenerationStallSinceMs_ >= kStarvationMinMs;
        }

        bool ObserveStarvation(int64_t nowMs, bool requiredFreshContent) noexcept {
            if (!requiredFreshContent) {
                starvationSinceMs_ = -1;
                return false;
            }
            if (starvationSinceMs_ < 0) starvationSinceMs_ = nowMs;
            return nowMs - starvationSinceMs_ >= kStarvationMinMs;
        }

        void ResetDiagnosticObservation() noexcept {
            observedFreshContentGenerations_.clear();
            contentGenerationStallSinceMs_ = -1;
            starvationSinceMs_ = -1;
        }

        [[nodiscard]] Action OnComposeFailed(bool committedFaithful, bool haveCommitted) noexcept {
            ++composeFailures_;
            if (haveCommitted && committedFaithful) return Action::BlitCommitted;
            return Action::Dark;
        }

        void OnCommitSucceeded() noexcept { ++commits_; }

        void OnBlitSucceeded() noexcept {
            displayed_ = true;
            blitFailureStreak_ = 0;
        }

        void OnBlitSkipped(int64_t nowMs) noexcept {
            ++blitSkips_;
            if (displayed_) latchUntilMs_ = nowMs + kDarkLatchMs;
        }

        void OnBlitFailed(int64_t nowMs) noexcept {
            ++blitFailures_;
            ++blitFailureStreak_;
            latchUntilMs_ = nowMs + kDarkLatchMs;
        }

        [[nodiscard]] bool Latched(int64_t nowMs) const noexcept { return nowMs < latchUntilMs_; }
        [[nodiscard]] bool HasDisplayed() const noexcept { return displayed_; }
        [[nodiscard]] uint64_t HeldStreak() const noexcept { return heldStreak_; }
        [[nodiscard]] uint64_t Commits() const noexcept { return commits_; }
        [[nodiscard]] uint64_t ComposeFailures() const noexcept { return composeFailures_; }
        [[nodiscard]] uint64_t BlitFailures() const noexcept { return blitFailures_; }
        [[nodiscard]] uint64_t BlitSkips() const noexcept { return blitSkips_; }
        [[nodiscard]] uint64_t BlitFailureStreak() const noexcept { return blitFailureStreak_; }

    private:
        int64_t latchUntilMs_ = 0;
        int64_t lastHeldRefreshMs_ = 0;
        bool displayed_ = false;
        uint64_t heldStreak_ = 0;
        uint64_t commits_ = 0;
        uint64_t composeFailures_ = 0;
        uint64_t blitFailures_ = 0;
        uint64_t blitSkips_ = 0;
        uint64_t blitFailureStreak_ = 0;
        std::map<uint64_t, uint64_t> observedFreshContentGenerations_;
        int64_t contentGenerationStallSinceMs_ = -1;
        int64_t starvationSinceMs_ = -1;
    };

}
