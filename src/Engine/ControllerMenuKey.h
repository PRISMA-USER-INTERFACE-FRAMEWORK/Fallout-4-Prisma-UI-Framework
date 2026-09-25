#pragma once

#include <cstdint>
#include <optional>
#ifndef PRISMAUI_FO4VR
#include "RE/M/ScaleformMenuEvent.h"
#endif

namespace PrismaUI::Engine {
struct ControllerMenuKey {
    std::uint32_t key;
    bool pressed;
};

inline std::optional<ControllerMenuKey> ReadControllerMenuKey(const void* event) {
#ifdef PRISMAUI_FO4VR
    (void)event;
    return std::nullopt;
#else
    if (!event) return std::nullopt;
    const auto* menuEvent = static_cast<const RE::ScaleformMenuEvent*>(event);
    const auto type = menuEvent->GetType();
    if (type != 5 && type != 6) return std::nullopt;
    const auto key = menuEvent->GetKey();
    if (!key || key > 255) return std::nullopt;
    return ControllerMenuKey{key, type == 5};
#endif
}
}
