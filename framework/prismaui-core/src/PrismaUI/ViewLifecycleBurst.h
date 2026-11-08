#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace PrismaUI::ViewLifecycle {

    struct BurstSummary {
        std::size_t views = 0;
        std::size_t domReady = 0;
        std::size_t abandoned = 0;
        std::size_t outstanding = 0;
        std::size_t createSamples = 0;
        std::uint64_t createTotalUs = 0;
        std::uint64_t createMaxUs = 0;
        std::int64_t firstRequestUs = 0;
        std::int64_t lastDomReadyUs = 0;

        [[nodiscard]] double MeanCreateUs() const noexcept {
            return createSamples ? static_cast<double>(createTotalUs) / static_cast<double>(createSamples) : 0.0;
        }
    };

    class Burst final {
    public:
        static constexpr std::int64_t kIdleRolloverUs = 5'000'000;

        std::size_t Request(std::uint64_t view, std::int64_t nowUs) {
            if (open_ && nowUs - lastUs_ >= kIdleRolloverUs) Reset();
            if (!open_) {
                open_ = true;
                openedUs_ = nowUs;
                summary_ = {};
                summary_.firstRequestUs = nowUs;
            }
            lastUs_ = nowUs;
            outstanding_.insert(view);
            return ++summary_.views;
        }

        void CreateDone(std::uint64_t durationUs) {
            if (!open_) return;
            ++summary_.createSamples;
            summary_.createTotalUs += durationUs;
            if (durationUs > summary_.createMaxUs) summary_.createMaxUs = durationUs;
        }

        void DomReady(std::uint64_t view, std::int64_t nowUs) {
            if (!outstanding_.erase(view) || !open_) return;
            lastUs_ = nowUs;
            ++summary_.domReady;
            summary_.lastDomReadyUs = nowUs;
        }

        void Abandon(std::uint64_t view) {
            if (!outstanding_.erase(view) || !open_) return;
            ++summary_.abandoned;
        }

        [[nodiscard]] bool Open() const noexcept { return open_; }

        [[nodiscard]] bool ReadyToClose(std::int64_t nowUs, std::int64_t maxWaitUs) const {
            if (!open_ || outstanding_.empty()) return true;
            return nowUs - openedUs_ >= maxWaitUs;
        }

        [[nodiscard]] BurstSummary Take() {
            BurstSummary out = summary_;
            out.outstanding = outstanding_.size();
            outstanding_.clear();
            summary_ = {};
            open_ = false;
            return out;
        }

    private:
        void Reset() {
            open_ = false;
            outstanding_.clear();
            summary_ = {};
        }

        bool open_ = false;
        std::int64_t openedUs_ = 0;
        std::int64_t lastUs_ = 0;
        BurstSummary summary_{};
        std::unordered_set<std::uint64_t> outstanding_;
    };

}
