#pragma once

#include <bitset>
#include <cstdint>

namespace PrismaUI::Web {
struct ControllerKeyState {
    std::bitset<256> down;

    bool Apply(std::uint32_t key, bool pressed, bool fresh) {
        if (!key || key >= down.size()) return false;
        if (pressed) {
            if (down[key]) return !fresh;
            if (!fresh) return false;
            down.set(key);
            return true;
        }
        const bool owned = down[key];
        down.reset(key);
        return owned;
    }

    template <class Emit>
    void Release(Emit&& emit) {
        const auto previous = down;
        down.reset();
        for (std::uint32_t key = 0; key < previous.size(); ++key) {
            if (previous[key] && !emit(key)) break;
        }
    }
};
}
