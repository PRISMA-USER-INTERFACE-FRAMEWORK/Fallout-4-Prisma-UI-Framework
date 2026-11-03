#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace PrismaUI {

class ThreadObserver {
public:
    enum class Result { Known, NewThread, Overflow };

    static constexpr std::size_t kSlots = 4;

    Result Observe(std::uint32_t id) noexcept {
        if (id == 0) return Result::Overflow;
        for (auto& slot : slots_) {
            std::uint32_t seen = slot.load(std::memory_order_acquire);
            if (seen == id) return Result::Known;
            if (seen != 0) continue;
            if (slot.compare_exchange_strong(seen, id, std::memory_order_acq_rel)) return Result::NewThread;
            if (seen == id) return Result::Known;
        }
        return Result::Overflow;
    }

    bool Contains(std::uint32_t id) const noexcept {
        if (id == 0) return false;
        for (const auto& slot : slots_) {
            if (slot.load(std::memory_order_acquire) == id) return true;
        }
        return false;
    }

    std::size_t Count() const noexcept {
        std::size_t count = 0;
        for (const auto& slot : slots_) {
            if (slot.load(std::memory_order_acquire) != 0) ++count;
        }
        return count;
    }

private:
    std::array<std::atomic<std::uint32_t>, kSlots> slots_{};
};

inline ThreadObserver& PresentThreads() noexcept {
    static ThreadObserver observer;
    return observer;
}

inline ThreadObserver& ResizeThreads() noexcept {
    static ThreadObserver observer;
    return observer;
}

}
