#pragma once

#include <cstdint>

namespace PrismaUI::PresentationScalePolicy {

struct Scale {
    float x = 1.0f;
    float y = 1.0f;
};

// View-space overlay coordinates require this target/view scale before drawing.
[[nodiscard]] constexpr Scale For(std::uint32_t viewWidth, std::uint32_t viewHeight, std::uint32_t targetWidth,
                                  std::uint32_t targetHeight) noexcept
{
    Scale scale{};
    if (viewWidth > 0 && targetWidth > 0 && viewWidth != targetWidth) {
        scale.x = static_cast<float>(targetWidth) / static_cast<float>(viewWidth);
    }
    if (viewHeight > 0 && targetHeight > 0 && viewHeight != targetHeight) {
        scale.y = static_cast<float>(targetHeight) / static_cast<float>(viewHeight);
    }
    return scale;
}

[[nodiscard]] constexpr bool IsIdentity(const Scale& scale) noexcept
{
    return scale.x == 1.0F && scale.y == 1.0F;
}

}
