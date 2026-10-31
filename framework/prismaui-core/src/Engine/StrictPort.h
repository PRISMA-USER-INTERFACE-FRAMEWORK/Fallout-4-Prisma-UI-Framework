#pragma once

#include <charconv>
#include <string_view>
#include <system_error>

namespace PrismaUI::Engine {

inline int ParseStrictPort(std::string_view text) {
    if (text.empty()) return 0;
    int port = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), port);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || port < 1 || port > 65535) {
        return 0;
    }
    return port;
}

}
