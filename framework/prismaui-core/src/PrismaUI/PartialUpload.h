#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace PrismaUI::PartialUpload {

    // Pixels are BGRA, 4 bytes each.
    inline constexpr std::uint32_t kBytesPerPixel = 4;

    struct Box {
        std::uint32_t left = 0;
        std::uint32_t top = 0;
        std::uint32_t right = 0;
        std::uint32_t bottom = 0;

        [[nodiscard]] constexpr bool Empty() const noexcept { return right <= left || bottom <= top; }
        [[nodiscard]] constexpr bool operator==(const Box&) const noexcept = default;
    };

    [[nodiscard]] constexpr Box Full(std::uint32_t width, std::uint32_t height) noexcept {
        return {0, 0, width, height};
    }

    [[nodiscard]] constexpr Box Clamp(int left, int top, int right, int bottom, std::uint32_t width,
                                      std::uint32_t height) noexcept {
        const auto clampTo = [](int value, std::uint32_t limit) {
            return value <= 0 ? 0u : std::min(static_cast<std::uint32_t>(value), limit);
        };
        Box box{clampTo(left, width), clampTo(top, height), clampTo(right, width), clampTo(bottom, height)};
        return box.Empty() ? Box{} : box;
    }

    [[nodiscard]] constexpr Box Union(const Box& a, const Box& b) noexcept {
        if (a.Empty()) return b;
        if (b.Empty()) return a;
        return {std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right),
                std::max(a.bottom, b.bottom)};
    }

    [[nodiscard]] constexpr std::size_t Offset(const Box& box, std::uint32_t rowBytes) noexcept {
        return static_cast<std::size_t>(box.top) * rowBytes + static_cast<std::size_t>(box.left) * kBytesPerPixel;
    }

    // dst and src share width, height and rowBytes; only the rows and columns inside box are written.
    inline void CopyBox(std::uint8_t* dst, const std::uint8_t* src, std::uint32_t rowBytes, const Box& box) noexcept {
        if (box.Empty()) return;
        const std::size_t bytes = static_cast<std::size_t>(box.right - box.left) * kBytesPerPixel;
        for (std::uint32_t y = box.top; y < box.bottom; ++y) {
            const std::size_t at = static_cast<std::size_t>(y) * rowBytes + static_cast<std::size_t>(box.left) * kBytesPerPixel;
            std::memcpy(dst + at, src + at, bytes);
        }
    }

    // Returns false, touching nothing, when either buffer is not rowBytes * height.
    [[nodiscard]] inline bool Merge(std::vector<std::uint8_t>& published, Box& pending,
                                    const std::vector<std::uint8_t>& shadow, std::uint32_t rowBytes,
                                    std::uint32_t height, const Box& dirty) noexcept {
        const std::size_t size = static_cast<std::size_t>(rowBytes) * height;
        if (published.size() != size || shadow.size() != size) return false;
        CopyBox(published.data(), shadow.data(), rowBytes, dirty);
        pending = Union(pending, dirty);
        return true;
    }

    [[nodiscard]] constexpr Box UploadBox(bool freshTexture, const Box& pending, std::uint32_t width,
                                          std::uint32_t height) noexcept {
        return freshTexture || pending.Empty() ? Full(width, height) : pending;
    }
}
