#pragma once

#include <cstdint>
#include <string_view>

namespace PrismaUI::Engine {

enum class DockCursorState : std::uint8_t {
    kInvalid,
    kInactive,
    kActive
};

constexpr DockCursorState ParseDockCursorState(std::string_view payload) noexcept {
    if (payload == "0") return DockCursorState::kInactive;
    if (payload == "1") return DockCursorState::kActive;
    return DockCursorState::kInvalid;
}

}
