#pragma once

#include <cstdint>

namespace PrismaUI::RuntimeSupportPolicy {

[[nodiscard]] constexpr bool IsSupportedGameVersion(
    std::uint16_t major, std::uint16_t minor, std::uint16_t patch) noexcept
{
    return (major == 1 && minor == 10 && (patch == 163 || patch == 984)) ||
           (major == 1 && (minor > 11 || (minor == 11 && patch >= 137)));
}

static_assert(IsSupportedGameVersion(1, 10, 163));
static_assert(IsSupportedGameVersion(1, 10, 984));
static_assert(!IsSupportedGameVersion(1, 11, 136));
static_assert(IsSupportedGameVersion(1, 11, 137));
static_assert(IsSupportedGameVersion(1, 11, 240));

}
