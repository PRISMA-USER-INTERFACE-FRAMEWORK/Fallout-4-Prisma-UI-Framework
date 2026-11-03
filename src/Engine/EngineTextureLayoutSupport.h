#pragma once

#include <cstdint>

namespace PrismaUI::Engine {

[[nodiscard]] constexpr bool IsVerifiedTextureLayoutBuild(
    std::uint16_t major, std::uint16_t minor, std::uint16_t patch) noexcept
{
    return major == 1 && ((minor == 10 && patch == 163) || (minor == 11 && patch == 240));
}

}
