#pragma once

#include <cstdint>

namespace PrismaUI::FreezeDiagnostics {

    struct GameLoadWindow {
        static constexpr bool Starts(bool wasActive, bool nowActive) noexcept {
            return nowActive && !wasActive;
        }

        static constexpr bool BackingOff(bool active, std::int64_t sinceMs, std::int64_t nowMs,
                                         std::int64_t capMs) noexcept {
            return active && nowMs - sinceMs < capMs;
        }
    };

}
