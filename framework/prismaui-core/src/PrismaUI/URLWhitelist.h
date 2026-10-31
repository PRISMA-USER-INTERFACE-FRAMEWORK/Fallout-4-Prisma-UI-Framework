#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace PrismaUI::URLWhitelist {

    enum class Capability {
        kScript,
        kStylesheet,
        kFont,
        kImage,
        kMedia,
        kConnect,
        kFrame,
        kDocument,
        kUnknown,
    };

    enum class CspTransport {
        kLegacyCompatibility,
        kCefCompatibility = kLegacyCompatibility,
        kUltralight,
    };

    inline constexpr std::array<std::string_view, 1> kImageDomains{
        "static.wikia.nocookie.net",
    };

    inline constexpr std::array<std::string_view, 0> kStylesheetDomains{};

    inline constexpr std::array<std::string_view, 0> kFontDomains{};

    inline constexpr std::array<std::string_view, 3> kFrameDomains{
        "youtube.com",
        "youtu.be",
        "googlevideo.com",
    };

    inline constexpr std::array<std::string_view, 3> kMediaDomains{
        "youtube.com",
        "youtu.be",
        "googlevideo.com",
    };

    inline constexpr std::array<std::string_view, 0> kConnectDomains{};

    inline constexpr std::array<std::string_view, 1> kExternalNavigationDomains{
        "nexusmods.com",
    };

    namespace detail {

        template <std::size_t N>
        inline bool MatchesAny(const std::array<std::string_view, N>& domains, std::string_view host) {
            if (host.empty()) return false;
            return std::any_of(domains.begin(), domains.end(), [host](std::string_view allowed) {
                if (host == allowed) return true;
                return host.size() > allowed.size() && host.substr(host.size() - allowed.size()) == allowed &&
                       host[host.size() - allowed.size() - 1] == '.';
            });
        }

    }

    inline bool IsAllowedFor(Capability capability, std::string_view host) {
        switch (capability) {
            case Capability::kScript:
                return false;
            case Capability::kDocument:
                return false;
            case Capability::kUnknown:
                return false;
            case Capability::kConnect:
                return detail::MatchesAny(kConnectDomains, host);
            case Capability::kStylesheet:
                return detail::MatchesAny(kStylesheetDomains, host);
            case Capability::kFont:
                return detail::MatchesAny(kFontDomains, host);
            case Capability::kImage:
                return detail::MatchesAny(kImageDomains, host);
            case Capability::kMedia:
                return detail::MatchesAny(kMediaDomains, host);
            case Capability::kFrame:
                return detail::MatchesAny(kFrameDomains, host);
        }
        return false;
    }

    inline bool IsAllowedExternalNavigation(std::string_view host) {
        return detail::MatchesAny(kExternalNavigationDomains, host);
    }

    inline bool IsKnownResourceHost(std::string_view host) {
        return detail::MatchesAny(kStylesheetDomains, host) || detail::MatchesAny(kFontDomains, host) ||
               detail::MatchesAny(kImageDomains, host) || detail::MatchesAny(kMediaDomains, host) ||
               detail::MatchesAny(kFrameDomains, host) || detail::MatchesAny(kConnectDomains, host);
    }

    namespace detail {

        template <std::size_t N>
        inline std::string Directive(std::string_view name, std::string_view localSources,
                                     const std::array<std::string_view, N>& domains, CspTransport transport) {
            std::string directive{name};
            if (!localSources.empty()) {
                directive += ' ';
                directive += localSources;
            }
            if (transport == CspTransport::kLegacyCompatibility) {
                for (const auto domain : domains) {
                    directive += " https://";
                    directive += domain;
                }
            }
            directive += ';';
            return directive;
        }

    }

    inline std::string LocalCspSources(CspTransport transport) {
        return transport == CspTransport::kLegacyCompatibility ? "'self' prisma:" : "'self'";
    }

    inline std::string GenerateScriptSrcDirective(CspTransport transport = CspTransport::kLegacyCompatibility) {
        return "script-src " + LocalCspSources(transport) + " 'unsafe-inline';";
    }

    inline std::string GenerateStyleSrcDirective(CspTransport transport = CspTransport::kLegacyCompatibility) {
        return detail::Directive("style-src",
                                 LocalCspSources(transport) + " 'unsafe-inline' data:", kStylesheetDomains, transport);
    }

    inline std::string GenerateFontSrcDirective(CspTransport transport = CspTransport::kLegacyCompatibility) {
        return detail::Directive("font-src", LocalCspSources(transport) + " data:", kFontDomains, transport);
    }

    inline std::string GenerateImgSrcDirective(CspTransport transport = CspTransport::kLegacyCompatibility) {
        return detail::Directive("img-src", LocalCspSources(transport) + " data: blob:", kImageDomains, transport);
    }

    inline std::string GenerateMediaSrcDirective(CspTransport transport = CspTransport::kLegacyCompatibility) {
        return detail::Directive("media-src", LocalCspSources(transport) + " data: blob:", kMediaDomains, transport);
    }

    inline constexpr std::string_view kLoopbackVideoProxyCspFrameSource = "http://127.0.0.1:*";

    inline std::string GenerateConnectSrcDirective(CspTransport transport = CspTransport::kLegacyCompatibility) {
        return detail::Directive("connect-src", LocalCspSources(transport), kConnectDomains, transport);
    }

    inline std::string GenerateFrameSrcDirective(CspTransport transport = CspTransport::kLegacyCompatibility) {
        std::string localSources = LocalCspSources(transport);
        if (transport == CspTransport::kLegacyCompatibility) {
            localSources += ' ';
            localSources += kLoopbackVideoProxyCspFrameSource;
        }
        return detail::Directive("frame-src", localSources, kFrameDomains, transport);
    }

    inline std::string GenerateWorkerSrcDirective() { return std::string{"worker-src 'none';"}; }

    inline std::string GenerateObjectSrcDirective() { return std::string{"object-src 'none';"}; }

    inline std::string GenerateDefaultSrcDirective(CspTransport transport = CspTransport::kLegacyCompatibility) {
        return "default-src " + LocalCspSources(transport) + ";";
    }

    inline std::string GenerateNavigationDirectives(CspTransport transport = CspTransport::kLegacyCompatibility) {
        return "base-uri 'self'; form-action 'none'; frame-ancestors " + LocalCspSources(transport) + ";";
    }

    inline std::string GenerateContentSecurityPolicy(CspTransport transport = CspTransport::kLegacyCompatibility) {
        return GenerateDefaultSrcDirective(transport) + " " + GenerateScriptSrcDirective(transport) + " " +
               GenerateStyleSrcDirective(transport) + " " + GenerateFontSrcDirective(transport) + " " +
               GenerateImgSrcDirective(transport) + " " + GenerateMediaSrcDirective(transport) + " " +
               GenerateConnectSrcDirective(transport) + " " + GenerateFrameSrcDirective(transport) + " " +
               GenerateWorkerSrcDirective() + " " + GenerateObjectSrcDirective() + " " +
               GenerateNavigationDirectives(transport);
    }

    inline std::string GenerateJsConnectWhitelistArray() {
        std::string arr = "[";
        for (std::size_t i = 0; i < kConnectDomains.size(); ++i) {
            if (i) arr += ",";
            arr += "\"";
            arr += kConnectDomains[i];
            arr += "\"";
        }
        arr += "]";
        return arr;
    }

}
