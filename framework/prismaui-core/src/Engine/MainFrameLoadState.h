#pragma once

#include <atomic>
#include <cstdint>

namespace PrismaUI::Engine {

class MainFrameLoadState {
public:
    void OnLoadStart(bool isMain) noexcept {
        if (!isMain) return;
        State expected = State::Failed;
        (void)m_state.compare_exchange_strong(
            expected, State::Recovering, std::memory_order_acq_rel, std::memory_order_acquire);
    }

    void OnLoadError(bool isMain) noexcept {
        if (isMain) m_state.store(State::Failed, std::memory_order_release);
    }

    void OnLoadEnd(bool isMain, bool navigationSucceeded) noexcept {
        if (!isMain || !navigationSucceeded) return;
        State current = m_state.load(std::memory_order_acquire);
        while (current != State::Failed &&
               !m_state.compare_exchange_weak(
                   current, State::Loaded, std::memory_order_acq_rel, std::memory_order_acquire)) {
        }
    }

    [[nodiscard]] bool IsLoaded() const noexcept {
        return m_state.load(std::memory_order_acquire) == State::Loaded;
    }

private:
    enum class State : std::uint8_t {
        NeverLoaded,
        Loaded,
        Failed,
        Recovering,
    };

    std::atomic<State> m_state{State::NeverLoaded};
};

}
