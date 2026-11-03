#pragma once

#include <cstddef>
#include <cstdint>

namespace PrismaUI::Engine {

    inline constexpr std::size_t kNoMatch = static_cast<std::size_t>(-1);

    [[nodiscard]] constexpr std::size_t FindUniqueBytes(const std::uint8_t* a_haystack,
                                                        std::size_t        a_haystackLen,
                                                        const std::uint8_t* a_needle,
                                                        std::size_t        a_needleLen) noexcept
    {
        if (!a_haystack || !a_needle || a_needleLen == 0 || a_haystackLen < a_needleLen) {
            return kNoMatch;
        }

        std::size_t found = kNoMatch;
        for (std::size_t i = 0; i + a_needleLen <= a_haystackLen; ++i) {
            std::size_t j = 0;
            for (; j < a_needleLen; ++j) {
                if (a_haystack[i + j] != a_needle[j]) break;
            }
            if (j != a_needleLen) continue;
            if (found != kNoMatch) return kNoMatch;
            found = i;
        }
        return found;
    }

    inline constexpr std::uint8_t kLocalMapSetZoomBody[] = {
        0x48, 0x8B, 0x41, 0x50, 0xF3, 0x0F, 0x11, 0x48, 0x40, 0xC3
    };

    inline constexpr std::size_t kLocalMapSetZoomWindow = 0x200;

}
