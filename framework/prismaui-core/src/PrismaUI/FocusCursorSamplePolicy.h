#pragma once

#include <atomic>
#include <cstdint>

namespace PrismaUI::FocusCursorSamplePolicy {

class Episode
{
public:
    void Begin(std::uint64_t view, std::uint64_t generation) noexcept
    {
        generation_.store(0, std::memory_order_release);
        view_.store(view, std::memory_order_relaxed);
        afterRegistration_.store(false, std::memory_order_relaxed);
        movementSample_.store(false, std::memory_order_relaxed);
        generation_.store(view && generation ? generation : 0, std::memory_order_release);
    }

    void End() noexcept
    {
        generation_.store(0, std::memory_order_release);
        view_.store(0, std::memory_order_relaxed);
        afterRegistration_.store(false, std::memory_order_relaxed);
        movementSample_.store(false, std::memory_order_relaxed);
    }

    [[nodiscard]] bool IsCurrent(std::uint64_t view, std::uint64_t generation) const noexcept
    {
        return generation != 0 && generation_.load(std::memory_order_acquire) == generation &&
               view_.load(std::memory_order_relaxed) == view;
    }

    [[nodiscard]] bool ClaimAfterRegistration(std::uint64_t view, std::uint64_t generation) noexcept
    {
        if (!IsCurrent(view, generation)) return false;
        bool claimed = false;
        return afterRegistration_.compare_exchange_strong(claimed, true, std::memory_order_acq_rel,
                                                          std::memory_order_relaxed);
    }

    [[nodiscard]] bool ClaimMovementSample(std::uint64_t view, std::uint64_t generation) noexcept
    {
        if (!IsCurrent(view, generation)) return false;
        if (!afterRegistration_.load(std::memory_order_acquire)) return false;
        bool claimed = false;
        return movementSample_.compare_exchange_strong(claimed, true, std::memory_order_acq_rel,
                                                       std::memory_order_relaxed);
    }

    void ReleaseMovementSample(std::uint64_t view, std::uint64_t generation) noexcept
    {
        if (!IsCurrent(view, generation)) return;
        movementSample_.store(false, std::memory_order_release);
    }

    [[nodiscard]] std::uint64_t Generation() const noexcept
    {
        return generation_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::uint64_t View() const noexcept { return view_.load(std::memory_order_relaxed); }

private:
    std::atomic<std::uint64_t> generation_{0};
    std::atomic<std::uint64_t> view_{0};
    std::atomic<bool> afterRegistration_{false};
    std::atomic<bool> movementSample_{false};
};

}
