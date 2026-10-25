#pragma once

#include "RuntimeSupportPolicy.h"

namespace PrismaUI::PluginRuntimePolicy {

[[nodiscard]] constexpr bool IsOgOrAeBuild(
    std::uint16_t major, std::uint16_t minor, std::uint16_t patch) noexcept
{
    return RuntimeSupportPolicy::IsSupportedGameVersion(major, minor, patch) &&
           !(major == 1 && minor == 10 && patch != 163);
}

}
