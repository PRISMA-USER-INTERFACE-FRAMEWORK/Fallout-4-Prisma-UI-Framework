#pragma once

#include <cstdint>
#include <string>

namespace PrismaUI::AdapterLuidPolicy {

struct LuidValue {
    std::uint32_t lowPart = 0;
    std::int32_t highPart = 0;
};

[[nodiscard]] constexpr bool SameAdapter(const LuidValue& a, const LuidValue& b) noexcept {
    return a.lowPart == b.lowPart && a.highPart == b.highPart;
}

[[nodiscard]] constexpr bool IsZero(const LuidValue& v) noexcept {
    return v.lowPart == 0 && v.highPart == 0;
}

[[nodiscard]] inline std::string FormatUseAdapterLuidSwitchValue(const LuidValue& v) {
    return std::to_string(static_cast<std::int32_t>(v.highPart)) + "," +
           std::to_string(static_cast<std::uint32_t>(v.lowPart));
}

[[nodiscard]] inline bool ParseUseAdapterLuidSwitchValue(const std::string& s, LuidValue& out) {
    const auto comma = s.find(',');
    if (comma == std::string::npos || comma == 0 || comma + 1 >= s.size()) return false;
    try {
        const long long high = std::stoll(s.substr(0, comma));
        const unsigned long long low = std::stoull(s.substr(comma + 1));
        if (high < INT32_MIN || high > INT32_MAX) return false;
        if (low > UINT32_MAX) return false;
        out.highPart = static_cast<std::int32_t>(high);
        out.lowPart = static_cast<std::uint32_t>(low);
        return true;
    } catch (...) {
        return false;
    }
}

[[nodiscard]] constexpr std::int64_t EncodeUseAdapterLuidInt64(const LuidValue& v) noexcept {
    const std::uint64_t high = static_cast<std::uint64_t>(static_cast<std::uint32_t>(v.highPart));
    const std::uint64_t low = static_cast<std::uint64_t>(v.lowPart);
    return static_cast<std::int64_t>((high << 32) | low);
}

[[nodiscard]] constexpr bool ShouldRequestPin(const LuidValue& gameLuid) noexcept {
    return !IsZero(gameLuid);
}

}
