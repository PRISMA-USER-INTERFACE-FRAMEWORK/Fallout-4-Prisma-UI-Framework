#pragma once

#include <cstdint>

namespace PrismaUI::UltralightDevToolsPolicy {

inline constexpr int kDefaultPort = 9222;

constexpr std::uint16_t NormalizePort(int requestedPort) noexcept {
    return static_cast<std::uint16_t>(requestedPort > 0 && requestedPort <= 65535 ? requestedPort : kDefaultPort);
}

}
