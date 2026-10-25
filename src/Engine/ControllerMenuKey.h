#pragma once

#include <cstdint>
#include <optional>

namespace PrismaUI::Engine {
struct ControllerMenuKey {
    std::uint32_t key;
    bool pressed;
};
}

#ifdef PRISMAUI_FO4VR
namespace PrismaUI::Engine {
inline std::optional<ControllerMenuKey> ReadControllerMenuKey(const void* event) {
    (void)event;
    return std::nullopt;
}
}
#else
#include "RE/M/ScaleformMenuEvent.h"

namespace PrismaUI::Engine {
inline std::optional<ControllerMenuKey> ReadControllerMenuKey(const void* event) {
    if (!event) return std::nullopt;
    const auto* menuEvent = static_cast<const RE::ScaleformMenuEvent*>(event);
    const auto type = menuEvent->GetType();
    if (type != 5 && type != 6) return std::nullopt;
    const auto key = menuEvent->GetKey();
    if (!key || key > 255) return std::nullopt;
    return ControllerMenuKey{key, type == 5};
}
}
#endif
