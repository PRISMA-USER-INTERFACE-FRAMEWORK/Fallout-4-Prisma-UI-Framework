#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace PrismaUI::PrivateNetworkGuard {

namespace detail {

inline bool ParseIPv4Number(std::string_view text, std::uint32_t max, std::uint32_t& out) {
    if (text.empty()) return false;
    unsigned base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2);
    } else if (text.size() > 1 && text[0] == '0') {
        base = 8;
    }
    if (text.empty()) return false;

    std::uint64_t value = 0;
    for (const unsigned char c : text) {
        unsigned digit = 0;
        if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
        else return false;
        if (digit >= base) return false;
        value = value * base + digit;
        if (value > max) return false;
    }
    out = static_cast<std::uint32_t>(value);
    return true;
}

inline bool ParseIPv4(const std::string& host, std::array<std::uint8_t, 4>& out) {
    if (host.empty()) return false;

    std::vector<std::string_view> components;
    std::size_t pos = 0;
    while (pos <= host.size()) {
        const std::size_t dot = host.find('.', pos);
        components.emplace_back(host.data() + pos,
                                (dot == std::string::npos ? host.size() : dot) - pos);
        if (dot == std::string::npos) break;
        pos = dot + 1;
    }
    if (components.empty() || components.size() > 4) return false;

    std::uint64_t packed = 0;
    if (components.size() == 1) {
        std::uint32_t value = 0;
        if (!ParseIPv4Number(components[0], std::numeric_limits<std::uint32_t>::max(), value)) {
            return false;
        }
        packed = value;
    } else {

        for (std::size_t i = 0; i + 1 < components.size(); ++i) {
            std::uint32_t byte = 0;
            if (!ParseIPv4Number(components[i], 255, byte)) return false;
            packed = (packed << 8) | byte;
        }
        const unsigned remainingBytes = static_cast<unsigned>(5 - components.size());
        const std::uint64_t finalMax = (std::uint64_t{1} << (remainingBytes * 8)) - 1;
        std::uint32_t final = 0;
        if (!ParseIPv4Number(components.back(), static_cast<std::uint32_t>(finalMax), final)) {
            return false;
        }
        packed = (packed << (remainingBytes * 8)) | final;
    }

    out = {static_cast<std::uint8_t>((packed >> 24) & 0xff),
           static_cast<std::uint8_t>((packed >> 16) & 0xff),
           static_cast<std::uint8_t>((packed >> 8) & 0xff),
           static_cast<std::uint8_t>(packed & 0xff)};
    return true;
}

inline bool IsNonPublicIPv4(const std::array<std::uint8_t, 4>& ip) {
    const unsigned a = ip[0], b = ip[1], c = ip[2];
    if (a == 0 || a == 10 || a == 127) return true;
    if (a == 100 && b >= 64 && b <= 127) return true;
    if (a == 169 && b == 254) return true;
    if (a == 172 && b >= 16 && b <= 31) return true;
    if (a == 192 && b == 168) return true;
    if (a == 192 && b == 0 && c == 0) return true;
    if (a == 192 && b == 0 && c == 2) return true;
    if (a == 198 && (b == 18 || b == 19)) return true;
    if (a == 198 && b == 51 && c == 100) return true;
    if (a == 203 && b == 0 && c == 113) return true;
    if (a >= 224) return true;
    return false;
}

inline bool ParseHex16(std::string_view text, std::uint16_t& out) {
    if (text.empty() || text.size() > 4) return false;
    unsigned value = 0;
    for (const unsigned char c : text) {
        value <<= 4;
        if (c >= '0' && c <= '9') value |= c - '0';
        else if (c >= 'a' && c <= 'f') value |= c - 'a' + 10;
        else return false;
    }
    out = static_cast<std::uint16_t>(value);
    return true;
}

inline bool ParseIPv6(const std::string& host, std::array<std::uint8_t, 16>& out) {
    if (host.empty()) return false;

    const std::size_t compression = host.find("::");
    if (compression != std::string::npos && host.find("::", compression + 2) != std::string::npos) {
        return false;
    }

    auto parseSide = [](std::string_view side, std::vector<std::uint16_t>& words,
                        std::array<std::uint8_t, 4>* ipv4Tail) -> bool {
        if (side.empty()) return true;
        std::size_t pos = 0;
        while (pos <= side.size()) {
            const std::size_t colon = side.find(':', pos);
            const std::string_view part = side.substr(pos,
                (colon == std::string_view::npos ? side.size() : colon) - pos);
            if (part.empty()) return false;
            if (part.find('.') != std::string_view::npos) {
                if (colon != std::string_view::npos || !ipv4Tail) return false;
                std::array<std::uint8_t, 4> parsed{};
                if (!ParseIPv4(std::string(part), parsed)) return false;
                *ipv4Tail = parsed;
                return true;
            }
            std::uint16_t word = 0;
            if (!ParseHex16(part, word)) return false;
            words.push_back(word);
            if (colon == std::string_view::npos) break;
            pos = colon + 1;
        }
        return true;
    };

    std::vector<std::uint16_t> left;
    std::vector<std::uint16_t> right;
    std::array<std::uint8_t, 4> ipv4{};
    bool hasIpv4 = false;

    auto parseWithTail = [&](std::string_view side, std::vector<std::uint16_t>& words) -> bool {
        std::array<std::uint8_t, 4> tail{};
        if (!parseSide(side, words, &tail)) return false;
        if (side.find('.') != std::string_view::npos) {
            if (hasIpv4) return false;
            ipv4 = tail;
            hasIpv4 = true;
        }
        return true;
    };

    if (compression == std::string::npos) {
        if (!parseWithTail(host, left)) return false;
        const std::size_t totalWords = left.size() + (hasIpv4 ? 2 : 0);
        if (totalWords != 8) return false;
    } else {
        if (!parseWithTail(std::string_view(host).substr(0, compression), left) ||
            !parseWithTail(std::string_view(host).substr(compression + 2), right)) {
            return false;
        }
        const std::size_t totalWords = left.size() + right.size() + (hasIpv4 ? 2 : 0);
        if (totalWords >= 8) return false;
    }

    std::vector<std::uint16_t> words;
    words.reserve(8);
    words.insert(words.end(), left.begin(), left.end());
    const std::size_t explicitWords = left.size() + right.size() + (hasIpv4 ? 2 : 0);
    if (compression != std::string::npos) words.insert(words.end(), 8 - explicitWords, 0);
    words.insert(words.end(), right.begin(), right.end());
    if (hasIpv4) {
        words.push_back(static_cast<std::uint16_t>((ipv4[0] << 8) | ipv4[1]));
        words.push_back(static_cast<std::uint16_t>((ipv4[2] << 8) | ipv4[3]));
    }
    if (words.size() != 8) return false;

    for (std::size_t i = 0; i < words.size(); ++i) {
        out[i * 2] = static_cast<std::uint8_t>(words[i] >> 8);
        out[i * 2 + 1] = static_cast<std::uint8_t>(words[i] & 0xff);
    }
    return true;
}

inline bool IsNonPublicIPv6(const std::string& host) {
    std::array<std::uint8_t, 16> ip{};
    if (!ParseIPv6(host, ip)) return false;

    const bool first15Zero = std::all_of(ip.begin(), ip.end() - 1, [](const std::uint8_t b) { return b == 0; });
    if (first15Zero && (ip[15] == 0 || ip[15] == 1)) return true;

    const bool first10Zero = std::all_of(ip.begin(), ip.begin() + 10,
                                         [](const std::uint8_t b) { return b == 0; });
    const bool first12Zero = first10Zero && ip[10] == 0 && ip[11] == 0;
    const bool mapped = first10Zero && ip[10] == 0xff && ip[11] == 0xff;
    if (first12Zero || mapped) {
        return IsNonPublicIPv4({ip[12], ip[13], ip[14], ip[15]});
    }

    if ((ip[0] & 0xfeu) == 0xfcu) return true;
    if (ip[0] == 0xfeu && (ip[1] & 0xc0u) == 0x80u) return true;
    if (ip[0] == 0xfeu && (ip[1] & 0xc0u) == 0xc0u) return true;
    if (ip[0] == 0xffu) return true;
    if (ip[0] == 0x20u && ip[1] == 0x01u && ip[2] == 0x0du && ip[3] == 0xb8u) return true;
    return false;
}

inline bool HasSuffix(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

}

inline bool IsPrivateOrLoopbackHost(const std::string& rawHost) {
    std::string host;
    host.reserve(rawHost.size());
    for (const unsigned char c : rawHost) {
        host.push_back(static_cast<char>(std::tolower(c)));
    }

    while (!host.empty() && host.back() == '.') host.pop_back();
    if (host.empty()) return true;

    std::string bare = host;
    if (bare.size() >= 2 && bare.front() == '[' && bare.back() == ']') {
        bare = bare.substr(1, bare.size() - 2);
    }
    if (const std::size_t zone = bare.find('%'); zone != std::string::npos) {
        bare.resize(zone);
    }

    if (bare == "localhost" || detail::HasSuffix(bare, ".localhost") ||
        detail::HasSuffix(bare, ".local") || detail::HasSuffix(bare, ".localdomain") ||
        detail::HasSuffix(bare, ".internal") || detail::HasSuffix(bare, ".lan") ||
        detail::HasSuffix(bare, ".home")) {
        return true;
    }

    if (bare.find('.') == std::string::npos && bare.find(':') == std::string::npos) return true;

    std::array<std::uint8_t, 4> ipv4{};
    if (detail::ParseIPv4(bare, ipv4)) return detail::IsNonPublicIPv4(ipv4);
    if (bare.find(':') != std::string::npos) return detail::IsNonPublicIPv6(bare);
    return false;
}

}
