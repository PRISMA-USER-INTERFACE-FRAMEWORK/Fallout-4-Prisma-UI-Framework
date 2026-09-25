#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace PrismaUI::Web {

struct GamepadInput {
    enum class Type { Button, Stick, Connection };
    Type type = Type::Connection;
    unsigned index = 0;
    float x = 0;
    float y = 0;
    bool freshPress = false;
};

struct GamepadState {
    std::array<double, 4> axes{};
    std::array<double, 16> buttons{};
    std::array<bool, 2> centered{};
    bool connected = false;

    void Apply(const GamepadInput& input) noexcept {
        if (input.type == GamepadInput::Type::Connection) {
            const bool next = input.x != 0;
            if (next != connected) {
                *this = {};
                connected = next;
            }
            return;
        }
        if (!std::isfinite(input.x) || !std::isfinite(input.y)) return;
        if (input.type == GamepadInput::Type::Button) {
            if (input.index >= buttons.size() || input.x < 0 || input.x > 1) return;
            connected = true;
            auto& value = buttons[input.index];
            if (input.x == 0 || value != 0 || input.freshPress) value = input.x;
            return;
        }
        if (input.index >= centered.size() || std::abs(input.x) > 1 || std::abs(input.y) > 1) return;
        connected = true;
        if (input.x == 0 && input.y == 0) centered[input.index] = true;
        if (!centered[input.index]) return;
        axes[input.index * 2] = input.x;
        axes[input.index * 2 + 1] = input.y;
    }
};

}
