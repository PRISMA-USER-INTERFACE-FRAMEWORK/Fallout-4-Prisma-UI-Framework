#pragma once

#include <cstddef>
#include <string_view>

namespace PrismaUI::Engine {

inline constexpr std::size_t kMaxNativeMessageChannelBytes = 128;
inline constexpr std::size_t kMaxNativeMessagePayloadBytes = 1024 * 1024;

inline bool IsNativeMessageWithinLimits(std::string_view channel, std::string_view payload) {
    return !channel.empty() && channel.size() <= kMaxNativeMessageChannelBytes &&
           payload.size() <= kMaxNativeMessagePayloadBytes;
}

}
