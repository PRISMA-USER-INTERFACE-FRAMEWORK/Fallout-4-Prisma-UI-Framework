#pragma once

#include <atomic>

namespace PrismaUI::PauseHoldPolicy {

class State final {
public:

    static constexpr int kVerifyAfterCompleteFrames = 8;

    void SetTarget(bool paused) noexcept {
        m_targetPaused.store(paused, std::memory_order_release);
    }

    [[nodiscard]] bool TargetPaused() const noexcept {
        return m_targetPaused.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool ShouldSendMessage(bool& outPaused) const noexcept {
        outPaused = TargetPaused();
        if (!m_haveSent.load(std::memory_order_acquire)) return true;
        return m_sentValue.load(std::memory_order_acquire) != outPaused;
    }

    void MarkMessageSent(bool paused) noexcept {
        m_sentValue.store(paused, std::memory_order_relaxed);
        m_haveSent.store(true, std::memory_order_release);
        m_verifyFrames.store(0, std::memory_order_release);
    }

    void InvalidateMessageState() noexcept {
        m_haveSent.store(false, std::memory_order_release);
        m_verifyFrames.store(-1, std::memory_order_release);
    }

    [[nodiscard]] bool AdvanceVerificationFrame() noexcept {
        int frames = m_verifyFrames.load(std::memory_order_acquire);
        for (;;) {
            if (frames < 0) return false;
            if (frames < kVerifyAfterCompleteFrames) {
                if (m_verifyFrames.compare_exchange_weak(
                        frames, frames + 1, std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    return false;
                }
                continue;
            }
            if (m_verifyFrames.compare_exchange_weak(
                    frames, -1, std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
        }
    }

private:
    std::atomic<bool> m_targetPaused{false};
    std::atomic<bool> m_sentValue{false};
    std::atomic<bool> m_haveSent{false};
    std::atomic<int> m_verifyFrames{-1};
};

}
