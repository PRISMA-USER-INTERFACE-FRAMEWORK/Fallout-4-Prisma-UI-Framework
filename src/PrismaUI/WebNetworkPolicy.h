#pragma once

#include <array>
#include <string_view>

#include "URLWhitelist.h"

namespace PrismaUI::WebNetworkPolicy {

    enum class FrameClass { kTrustedLocal, kOwnedRemoteNested, kUnattributed };

    inline bool IsTrustedLocalDocumentUrl(std::string_view url, std::string_view expected) {
        if (url.empty() || expected.empty()) return false;
        const auto withoutFragmentOrQuery = [](std::string_view value) {
            const auto suffix = value.find_first_of("?#");
            return value.substr(0, suffix);
        };
        return withoutFragmentOrQuery(url) == withoutFragmentOrQuery(expected);
    }

    inline constexpr auto kRemoteEmbedFrameAndMedia = std::array<std::string_view, 3>{
        "youtube.com",
        "youtu.be",
        "googlevideo.com",
    };
    inline constexpr auto kRemoteEmbedScript = std::array<std::string_view, 4>{
        "youtube.com",
        "youtu.be",
        "ytimg.com",
        "gstatic.com",
    };
    inline constexpr auto kRemoteEmbedConnect = std::array<std::string_view, 4>{
        "youtube.com",
        "googlevideo.com",
        "ytimg.com",
        "gstatic.com",
    };
    inline constexpr auto kRemoteEmbedImage = std::array<std::string_view, 5>{
        "youtube.com", "ytimg.com", "ggpht.com", "googleusercontent.com", "gstatic.com",
    };
    inline constexpr auto kRemoteEmbedStyleAndFont = std::array<std::string_view, 2>{
        "youtube.com",
        "gstatic.com",
    };

    inline const char* FrameClassName(FrameClass frameClass) {
        switch (frameClass) {
            case FrameClass::kTrustedLocal:
                return "trusted-local";
            case FrameClass::kOwnedRemoteNested:
                return "remote-embed";
            case FrameClass::kUnattributed:
                return "unattributed";
        }
        return "unknown";
    }

    inline bool IsAllowedRemoteEmbed(URLWhitelist::Capability capability, std::string_view host) {
        using URLWhitelist::Capability;
        using URLWhitelist::detail::MatchesAny;
        switch (capability) {
            case Capability::kScript:
                return MatchesAny(kRemoteEmbedScript, host);
            case Capability::kConnect:
                return MatchesAny(kRemoteEmbedConnect, host);
            case Capability::kImage:
                return MatchesAny(kRemoteEmbedImage, host);
            case Capability::kMedia:
            case Capability::kFrame:
                return MatchesAny(kRemoteEmbedFrameAndMedia, host);
            case Capability::kStylesheet:
            case Capability::kFont:
                return MatchesAny(kRemoteEmbedStyleAndFont, host);
            case Capability::kDocument:
            case Capability::kUnknown:
                return false;
        }
        return false;
    }

    inline bool DenyTrustedLocalRemoteScript(FrameClass frameClass, URLWhitelist::Capability capability) {
        return frameClass == FrameClass::kTrustedLocal && capability == URLWhitelist::Capability::kScript;
    }

    inline bool AllowPublicHttpsAfterCoreChecks(FrameClass frameClass, URLWhitelist::Capability capability,
                                                std::string_view host) {
        switch (frameClass) {
            case FrameClass::kTrustedLocal:
                return URLWhitelist::IsAllowedFor(capability, host);
            case FrameClass::kOwnedRemoteNested:
                return IsAllowedRemoteEmbed(capability, host);
            case FrameClass::kUnattributed:
                return false;
        }
        return false;
    }

}
