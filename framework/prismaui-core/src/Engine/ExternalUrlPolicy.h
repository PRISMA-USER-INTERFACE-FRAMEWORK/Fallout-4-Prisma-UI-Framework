#pragma once

#include "../PrismaUI/PrivateNetworkGuard.h"
#include "../PrismaUI/URLWhitelist.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace PrismaUI::Engine {

enum class ExternalUrlAction {
    kContinueInternal,
    kOpenExternal,
    kReject,
};

enum class ExternalUrlNavigationKind {
    kNormal,
    kPopup,
};

namespace detail {

inline std::string LowerAscii(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char ch : value) {
        result.push_back(static_cast<char>(std::tolower(ch)));
    }
    return result;
}

inline bool HasSafeUrlCharacters(std::string_view value) {
    if (value.empty() || value.size() > 4096) return false;
    for (const unsigned char ch : value) {
        if (ch <= 0x20 || ch == 0x7f || ch == '\\' || ch == '"') return false;
    }
    return true;
}

inline bool IsValidScheme(std::string_view scheme) {
    if (scheme.empty() || !std::isalpha(static_cast<unsigned char>(scheme.front()))) return false;
    for (const unsigned char ch : scheme) {
        if (!std::isalnum(ch) && ch != '+' && ch != '-' && ch != '.') return false;
    }
    return true;
}

inline bool IsValidPort(std::string_view value) {
    if (value.empty() || value.size() > 5) return false;
    std::uint32_t port = 0;
    for (const unsigned char ch : value) {
        if (!std::isdigit(ch)) return false;
        port = port * 10 + static_cast<std::uint32_t>(ch - '0');
    }
    return port != 0 && port <= 65535;
}

inline bool IsAllowedHttpsPort(std::string_view value) {
    return IsValidPort(value) && value == "443";
}

inline bool IsValidDnsHost(std::string_view value) {
    if (value.empty() || value.size() > 253 || value.front() == '.' || value.back() == '.') return false;
    for (const unsigned char ch : value) {
        if (!std::isalnum(ch) && ch != '-' && ch != '.') return false;
    }
    return true;
}

inline bool ParseHttpsAuthority(std::string_view authority, std::string& host) {
    if (authority.empty() || authority.find('@') != std::string_view::npos) return false;
    if (authority.front() == '[') {
        const std::size_t close = authority.find(']');
        if (close == std::string_view::npos || close == 1) return false;
        const std::string_view suffix = authority.substr(close + 1);
        if (!suffix.empty() && (suffix.front() != ':' || !IsAllowedHttpsPort(suffix.substr(1)))) return false;
        std::array<std::uint8_t, 16> address{};
        if (!PrivateNetworkGuard::detail::ParseIPv6(std::string(authority.substr(1, close - 1)), address)) {
            return false;
        }
        host = LowerAscii(authority.substr(0, close + 1));
        return true;
    }

    const std::size_t colon = authority.find(':');
    if (colon == std::string_view::npos) {
        host = LowerAscii(authority);
        return IsValidDnsHost(host);
    }
    if (authority.find(':', colon + 1) != std::string_view::npos) return false;
    host = LowerAscii(authority.substr(0, colon));
    return IsValidDnsHost(host) && IsAllowedHttpsPort(authority.substr(colon + 1));
}

}

inline bool TryPublicHttpsHost(std::string_view url, std::string& host) {
    if (!detail::HasSafeUrlCharacters(url)) return false;

    const std::size_t colon = url.find(':');
    if (colon == std::string_view::npos || !detail::IsValidScheme(url.substr(0, colon))) {
        return false;
    }
    const std::string scheme = detail::LowerAscii(url.substr(0, colon));
    const std::string_view remainder = url.substr(colon + 1);
    if (scheme != "https" || remainder.rfind("//", 0) != 0) return false;

    const std::string_view authority = remainder.substr(2).substr(0, remainder.substr(2).find_first_of("/?#"));
    return detail::ParseHttpsAuthority(authority, host) &&
           !PrivateNetworkGuard::IsPrivateOrLoopbackHost(host);
}

inline ExternalUrlAction ClassifyExternalUrl(std::string_view url) {
    if (!detail::HasSafeUrlCharacters(url)) return ExternalUrlAction::kReject;

    const std::size_t colon = url.find(':');
    if (colon == std::string_view::npos || !detail::IsValidScheme(url.substr(0, colon))) {
        return ExternalUrlAction::kReject;
    }
    const std::string scheme = detail::LowerAscii(url.substr(0, colon));
    const std::string_view remainder = url.substr(colon + 1);

    if (scheme == "prisma") {
        if (remainder.rfind("//", 0) != 0) return ExternalUrlAction::kReject;
        const std::string_view authority = remainder.substr(2).substr(0, remainder.substr(2).find_first_of("/?#"));
        return authority.empty() ? ExternalUrlAction::kReject : ExternalUrlAction::kContinueInternal;
    }

    std::string host;
    if (!TryPublicHttpsHost(url, host) || !PrismaUI::URLWhitelist::IsAllowedExternalNavigation(host)) {
        return ExternalUrlAction::kReject;
    }
    return ExternalUrlAction::kOpenExternal;
}

inline bool IsAllowedVideoEmbedUrl(std::string_view url) {
    std::string host;
    return TryPublicHttpsHost(url, host) &&
           PrismaUI::URLWhitelist::IsAllowedFor(PrismaUI::URLWhitelist::Capability::kFrame, host);
}

inline ExternalUrlAction ClassifyExternalNavigation(std::string_view url, bool trustedPrismaSource,
                                                     bool userGesture, ExternalUrlNavigationKind) {
    const ExternalUrlAction action = ClassifyExternalUrl(url);
    if (action == ExternalUrlAction::kContinueInternal) return action;
    if (action == ExternalUrlAction::kOpenExternal && trustedPrismaSource && userGesture) return action;
    return ExternalUrlAction::kReject;
}

}