#include <cstddef>

#include "Engine/ControllerMenuKey.h"

#include <array>
#include <cassert>
#include <cstring>

int main() {
    std::array<std::byte, 12> event{};
    const std::uint32_t down = 5;
    const std::uint32_t up = 6;
    const std::uint32_t arrowUp = 38;
    std::memcpy(event.data(), &down, sizeof(down));
    std::memcpy(event.data() + 8, &arrowUp, sizeof(arrowUp));
    const auto pressed = PrismaUI::Engine::ReadControllerMenuKey(event.data());
    assert(pressed && pressed->key == arrowUp && pressed->pressed);
    std::memcpy(event.data(), &up, sizeof(up));
    const auto released = PrismaUI::Engine::ReadControllerMenuKey(event.data());
    assert(released && released->key == arrowUp && !released->pressed);
    const std::uint32_t invalidType = 4;
    std::memcpy(event.data(), &invalidType, sizeof(invalidType));
    assert(!PrismaUI::Engine::ReadControllerMenuKey(event.data()));
    assert(!PrismaUI::Engine::ReadControllerMenuKey(nullptr));
}
