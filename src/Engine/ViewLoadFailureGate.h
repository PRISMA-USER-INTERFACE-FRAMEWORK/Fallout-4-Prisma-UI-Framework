#pragma once

#include <charconv>
#include <cstdint>
#include <map>
#include <mutex>
#include <string_view>
#include <system_error>

namespace PrismaUI::Engine {

class ViewLoadFailureGate {
public:
    static std::uint64_t ParseViewId(std::string_view text) noexcept {
        if (text.empty()) return 0;
        std::uint64_t id = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), id);
        return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && id != 0 ? id : 0;
    }

    void OnLoadStart(std::uint64_t view) {
        if (view == 0) return;
        std::lock_guard lock(m_mutex);
        const auto it = m_states.find(view);
        if (it != m_states.end()) {
            it->second = State::kRecovering;
        }
    }

    void OnLoadError(std::uint64_t view) {
        if (view == 0) return;
        std::lock_guard lock(m_mutex);
        m_states[view] = State::kFailed;
    }

    void OnLoadSuccess(std::uint64_t view) {
        if (view == 0) return;
        std::lock_guard lock(m_mutex);
        const auto it = m_states.find(view);

        if (it != m_states.end() && it->second == State::kRecovering) {
            m_states.erase(it);
        }
    }

    [[nodiscard]] bool AllowDomReady(std::uint64_t view) const {
        if (view == 0) return true;
        std::lock_guard lock(m_mutex);
        return !m_states.contains(view);
    }

private:
    enum class State : std::uint8_t {
        kFailed,
        kRecovering,
    };

    mutable std::mutex m_mutex;
    std::map<std::uint64_t, State> m_states;
};

}
