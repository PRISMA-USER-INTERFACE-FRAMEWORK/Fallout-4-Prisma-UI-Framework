#pragma once

#include <cstdint>

namespace PrismaUI::InputMask {

    enum class Owner : std::uint8_t {
        kFocus = 1u << 0,
        kInspector = 1u << 1,
    };

    bool Acquire(Owner owner);

    bool Release(Owner owner);

    bool IsApplied();

}
