#pragma once

#include <array>
#include <atomic>
#include <optional>

namespace PrismaUI::Utf16Pairing {

constexpr bool IsHighSurrogate(char16_t unit) noexcept { return unit >= 0xD800 && unit <= 0xDBFF; }
constexpr bool IsLowSurrogate(char16_t unit) noexcept { return unit >= 0xDC00 && unit <= 0xDFFF; }

constexpr std::array<char16_t, 2> ToSurrogatePair(char32_t codePoint) noexcept {
    const char32_t offset = codePoint - 0x10000;
    return {static_cast<char16_t>(0xD800 + (offset >> 10)), static_cast<char16_t>(0xDC00 + (offset & 0x3FF))};
}

class Pending {
public:
    std::optional<char32_t> Feed(char16_t unit) noexcept {
        const char16_t high = pending_.exchange(0, std::memory_order_acq_rel);
        if (IsHighSurrogate(unit)) {
            pending_.store(unit, std::memory_order_release);
            return std::nullopt;
        }
        if (IsLowSurrogate(unit)) {
            if (high == 0) return std::nullopt;
            return static_cast<char32_t>(0x10000 + ((high - 0xD800) << 10) + (unit - 0xDC00));
        }
        return static_cast<char32_t>(unit);
    }

    void Reset() noexcept { pending_.store(0, std::memory_order_release); }

private:
    std::atomic<char16_t> pending_{0};
};

}
