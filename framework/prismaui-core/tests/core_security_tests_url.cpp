#include <string>

#include "core_security_tests_support.h"
#include "Engine/ExternalUrlPolicy.h"
#include "Engine/ResourceTypePolicy.h"
#include "PrismaUI/NetworkAuditConfig.h"
#include "PrismaUI/URLWhitelist.h"
#include "PrismaUI/WebNetworkPolicy.h"

using CoreSecurityTests::Check;

void RunUrlWhitelistChecks() {
    using PrismaUI::URLWhitelist::Capability;
    using PrismaUI::URLWhitelist::IsAllowedFor;
    namespace RTP = PrismaUI::Engine::ResourceTypePolicy;

    Check(!IsAllowedFor(Capability::kScript, "unpkg.com"), "rejects unpkg.com JavaScript");
    Check(!IsAllowedFor(Capability::kScript, "cdn.tailwindcss.com"),
          "rejects the Tailwind CDN compiler as JavaScript");
    Check(!IsAllowedFor(Capability::kScript, "cdn.jsdelivr.net"),
          "rejects jsdelivr JavaScript (it was the old list's script CDN)");
    Check(!IsAllowedFor(Capability::kScript, "example.com"), "rejects arbitrary HTTPS JavaScript");

    Check(!IsAllowedFor(Capability::kStylesheet, "fonts.googleapis.com"),
          "rejects Google Fonts stylesheets; views must self-host CSS");
    Check(!IsAllowedFor(Capability::kFont, "fonts.gstatic.com"),
          "rejects Google-hosted font payloads; views must self-host fonts");
    Check(!IsAllowedFor(Capability::kStylesheet, "example.com"), "rejects an arbitrary external stylesheet");
    Check(!IsAllowedFor(Capability::kFont, "example.com"), "rejects an arbitrary external font");
    Check(!IsAllowedFor(Capability::kScript, "fonts.googleapis.com"), "rejects Google Fonts JavaScript");
    Check(!IsAllowedFor(Capability::kScript, "fonts.gstatic.com"), "rejects gstatic JavaScript");
    Check(IsAllowedFor(Capability::kImage, "static.wikia.nocookie.net"),
          "allows the Fallout wiki CDN to serve an image");
    Check(!IsAllowedFor(Capability::kScript, "static.wikia.nocookie.net"),
          "rejects wiki-CDN JavaScript even though its images are permitted");

    Check(PrismaUI::URLWhitelist::IsAllowedExternalNavigation("nexusmods.com"),
          "allows Nexus as an external-browser navigation destination");
    Check(!IsAllowedFor(Capability::kScript, "nexusmods.com"),
          "rejects Nexus JavaScript even though Nexus links open externally");
    Check(!IsAllowedFor(Capability::kImage, "nexusmods.com"),
          "opening a Nexus link in the system browser grants no resource capability");
    Check(!PrismaUI::URLWhitelist::IsAllowedExternalNavigation("github.com"),
          "GitHub is not on the external-navigation whitelist");
    Check(!PrismaUI::URLWhitelist::IsAllowedExternalNavigation("fonts.googleapis.com"),
          "a former font origin is not an external navigation destination");
    Check(PrismaUI::Engine::ClassifyExternalUrl("https://fonts.googleapis.com") ==
              PrismaUI::Engine::ExternalUrlAction::kReject,
          "clicking Google Fonts does not open the system browser");

    Check(PrismaUI::Engine::ClassifyExternalNavigation("https://www.nexusmods.com/fallout4/mods/1", true, true,
                                                        PrismaUI::Engine::ExternalUrlNavigationKind::kNormal) ==
              PrismaUI::Engine::ExternalUrlAction::kOpenExternal,
          "a Nexus link from a trusted view still opens in the user's browser");
    Check(PrismaUI::Engine::ClassifyExternalNavigation("https://github.com/PrismaUI", true, true,
                                                        PrismaUI::Engine::ExternalUrlNavigationKind::kNormal) ==
              PrismaUI::Engine::ExternalUrlAction::kReject,
          "a trusted user-gesture GitHub link is still not on the navigation whitelist");
    Check(PrismaUI::Engine::ClassifyExternalNavigation("https://www.nexusmods.com/fallout4/mods/1", false, true,
                                                        PrismaUI::Engine::ExternalUrlNavigationKind::kNormal) ==
              PrismaUI::Engine::ExternalUrlAction::kReject,
          "an untrusted frame cannot open the system browser");
    Check(PrismaUI::Engine::ClassifyExternalNavigation("https://www.nexusmods.com/fallout4/mods/1", true, false,
                                                        PrismaUI::Engine::ExternalUrlNavigationKind::kNormal) ==
              PrismaUI::Engine::ExternalUrlAction::kReject,
          "external navigation requires a user gesture");
    Check(PrismaUI::Engine::ClassifyExternalNavigation("javascript:alert(1)", true, true,
                                                        PrismaUI::Engine::ExternalUrlNavigationKind::kNormal) ==
              PrismaUI::Engine::ExternalUrlAction::kReject,
          "a javascript: URL is never handed to the external browser");

    Check(!IsAllowedFor(Capability::kImage, "fonts.googleapis.com"), "a stylesheet origin may not serve images");
    Check(!IsAllowedFor(Capability::kStylesheet, "static.wikia.nocookie.net"),
          "an image origin may not serve stylesheets");
    Check(IsAllowedFor(Capability::kFrame, "youtube.com") && IsAllowedFor(Capability::kMedia, "googlevideo.com"),
          "allows the video embed origins for frames and media");
    Check(!IsAllowedFor(Capability::kScript, "youtube.com"),
          "rejects YouTube JavaScript inside a Prisma view even though it may be framed");

    Check(!IsAllowedFor(Capability::kConnect, "fonts.googleapis.com") &&
              !IsAllowedFor(Capability::kConnect, "example.com"),
          "no origin may be a fetch/XHR target");
    Check(!IsAllowedFor(Capability::kDocument, "nexusmods.com"),
          "a view may not navigate itself onto a remote document");
    Check(!IsAllowedFor(Capability::kUnknown, "youtube.com"),
          "an unclassifiable request fails closed even for an otherwise-allowed origin");

    Check(IsAllowedFor(Capability::kImage, "images.static.wikia.nocookie.net"),
          "matches a subdomain of an allowed origin");
    Check(!IsAllowedFor(Capability::kImage, "evilstatic.wikia.nocookie.net"),
          "does not match a host that merely ends with the allowed string");
    Check(!IsAllowedFor(Capability::kImage, "static.wikia.nocookie.net.attacker.test"),
          "does not match an allowed origin used as a prefix label");
    Check(!IsAllowedFor(Capability::kImage, ""), "rejects an empty host");

    Check(RTP::Classify(RTP::kScript) == Capability::kScript, "classifies RT_SCRIPT as script");
    Check(RTP::Classify(RTP::kWorker) == Capability::kScript, "classifies RT_WORKER as script");
    Check(RTP::Classify(RTP::kSharedWorker) == Capability::kScript, "classifies RT_SHARED_WORKER as script");
    Check(RTP::Classify(RTP::kServiceWorker) == Capability::kScript, "classifies RT_SERVICE_WORKER as script");
    Check(RTP::Classify(RTP::kObject) == Capability::kScript,
          "classifies RT_OBJECT as executable rather than passive");
    Check(RTP::Classify(RTP::kStylesheet) == Capability::kStylesheet, "classifies RT_STYLESHEET");
    Check(RTP::Classify(RTP::kFontResource) == Capability::kFont, "classifies RT_FONT_RESOURCE");
    Check(RTP::Classify(RTP::kImage) == Capability::kImage, "classifies RT_IMAGE");
    Check(RTP::Classify(RTP::kFavicon) == Capability::kImage, "classifies RT_FAVICON as an image");
    Check(RTP::Classify(RTP::kMedia) == Capability::kMedia, "classifies RT_MEDIA");
    Check(RTP::Classify(RTP::kXhr) == Capability::kConnect,
          "classifies RT_XHR (fetch and streaming WASM) as connect");
    Check(RTP::Classify(RTP::kSubFrame) == Capability::kFrame, "classifies RT_SUB_FRAME");
    Check(RTP::Classify(RTP::kMainFrame) == Capability::kDocument, "classifies RT_MAIN_FRAME");
    Check(RTP::Classify(RTP::kNavigationPreloadSubFrame) == Capability::kFrame,
          "classifies a sub-frame navigation preload as a frame");
    Check(RTP::Classify(RTP::kPrefetch) == Capability::kUnknown,
          "a prefetch fails closed -- a prefetched script is still a script");
    Check(RTP::Classify(RTP::kSubResource) == Capability::kUnknown, "a generic subresource fails closed");
    Check(RTP::Classify(RTP::kPluginResource) == Capability::kUnknown, "a plugin resource fails closed");
    Check(RTP::Classify(9999) == Capability::kUnknown,
          "a resource type a future CEF adds fails closed rather than defaulting to allowed");

    Check(!IsAllowedFor(RTP::Classify(RTP::kStylesheet), "unpkg.com"),
          "a stylesheet request redirected to unpkg.com is rejected at the new origin");
    Check(!IsAllowedFor(RTP::Classify(RTP::kScript), "youtube.com"),
          "a script request redirected to an allowed frame origin is still rejected");

    const std::string csp = PrismaUI::URLWhitelist::GenerateContentSecurityPolicy();
    const std::string scriptSrc = PrismaUI::URLWhitelist::GenerateScriptSrcDirective();
    Check(scriptSrc.find("https://") == std::string::npos, "script-src names no remote origin");
    Check(scriptSrc.find("'unsafe-eval'") == std::string::npos, "script-src forbids eval");
    Check(scriptSrc.find("data:") == std::string::npos,
          "script-src forbids data: URLs, which are executable content");
    Check(csp.find("worker-src 'none'") != std::string::npos, "CSP forbids workers");
    Check(csp.find("object-src 'none'") != std::string::npos, "CSP forbids object/embed");
    Check(csp.find("default-src 'self' prisma:;") != std::string::npos,
          "default-src is restrictive and grants no remote origin");
    Check(csp.find("form-action 'none'") != std::string::npos, "CSP forbids form submission");
    Check(
        csp.find("fonts.googleapis.com") == std::string::npos && csp.find("fonts.gstatic.com") == std::string::npos,
        "generated CSP names no Google Fonts domain");
    Check(PrismaUI::URLWhitelist::GenerateStyleSrcDirective().find("https://") == std::string::npos,
          "style-src names no remote origin");
    Check(PrismaUI::URLWhitelist::GenerateFontSrcDirective().find("https://") == std::string::npos,
          "font-src names no remote origin");
    Check(PrismaUI::URLWhitelist::GenerateFontSrcDirective().find("wikia") == std::string::npos,
          "font-src does not inherit the image origin");
    Check(PrismaUI::URLWhitelist::GenerateImgSrcDirective().find("static.wikia.nocookie.net") != std::string::npos,
          "img-src carries the image origin");
    Check(PrismaUI::URLWhitelist::GenerateConnectSrcDirective().find("https://") == std::string::npos,
          "connect-src names no remote origin");
    Check(PrismaUI::URLWhitelist::GenerateConnectSrcDirective().find("'self' prisma:") != std::string::npos,
          "connect-src still grants local 'self' and prisma: sources; it is not an empty directive");
    Check(PrismaUI::URLWhitelist::GenerateJsConnectWhitelistArray() == "[]",
          "the in-page sandbox is handed an empty fetch allowlist, not the resource origins");
    Check(PrismaUI::URLWhitelist::GenerateFrameSrcDirective().find("youtube.com") != std::string::npos,
          "frame-src still names the YouTube embed origin");
    Check(PrismaUI::URLWhitelist::GenerateFrameSrcDirective().find(
              PrismaUI::URLWhitelist::kLoopbackVideoProxyCspFrameSource) != std::string::npos,
          "frame-src allows the loopback video wrapper (ephemeral port) so the response CSP "
          "does not block http://127.0.0.1:<port>/embed");
    Check(!IsAllowedFor(Capability::kFrame, "127.0.0.1"),
          "loopback is a CSP frame source only; it is not a kFrameDomains host");

    const std::string freeCsp =
        PrismaUI::URLWhitelist::GenerateContentSecurityPolicy(PrismaUI::URLWhitelist::CspTransport::kUltralight);
    Check(freeCsp.find("https://") == std::string::npos,
          "#666: Free (Ultralight) CSP names no remote https origin");
    Check(freeCsp.find("youtube") == std::string::npos && freeCsp.find("wikia") == std::string::npos,
          "#666: Free CSP drops the YouTube/Wikia remote-embed hosts");
    Check(freeCsp.find("default-src 'self';") != std::string::npos,
          "#666: Free default-src is bare 'self' (no prisma: scheme)");
    Check(freeCsp.find("127.0.0.1") == std::string::npos,
          "#666: Free frame-src does not carry the loopback video proxy");
    Check(PrismaUI::URLWhitelist::GenerateContentSecurityPolicy(
              PrismaUI::URLWhitelist::CspTransport::kCefCompatibility)
                  .find("youtube.com") != std::string::npos,
          "#666: CEF-compatibility CSP still carries remote embeds (the change is transport-scoped)");

    const std::string ultralightCsp =
        PrismaUI::URLWhitelist::GenerateContentSecurityPolicy(PrismaUI::URLWhitelist::CspTransport::kUltralight);
    Check(ultralightCsp.find("prisma:") == std::string::npos,
          "Ultralight CSP does not include the CEF prisma transport source");
    Check(ultralightCsp.find(PrismaUI::URLWhitelist::kLoopbackVideoProxyCspFrameSource) == std::string::npos,
          "Ultralight CSP does not include the CEF loopback video-proxy exception");
    Check(ultralightCsp.find("youtube.com") == std::string::npos,
          "#666: Ultralight (Free) CSP no longer carries the remote frame allowlist (local-only)");

    const std::string legacyCsp =
        PrismaUI::URLWhitelist::GenerateContentSecurityPolicy(
            PrismaUI::URLWhitelist::CspTransport::kLegacyCompatibility);
    Check(ultralightCsp.find("127.0.0.1") == std::string::npos,
          "the shipping (Ultralight) CSP carries no loopback frame source");
    Check(legacyCsp.find(PrismaUI::URLWhitelist::kLoopbackVideoProxyCspFrameSource) != std::string::npos,
          "the legacy-compatibility CSP is the only transport that still carries the loopback "
          "frame source; the guide must not describe it as shipping behaviour");

    using PrismaUI::WebNetworkPolicy::AllowPublicHttpsAfterCoreChecks;
    using PrismaUI::WebNetworkPolicy::DenyTrustedLocalRemoteScript;
    using PrismaUI::WebNetworkPolicy::FrameClass;
    using PrismaUI::WebNetworkPolicy::IsTrustedLocalDocumentUrl;
    Check(IsTrustedLocalDocumentUrl("file:///views/boot.html", "file:///views/boot.html"),
          "the trusted local document gate accepts the exact FileSystem URL");
    Check(IsTrustedLocalDocumentUrl("file:///views/boot.html?reload=1#top", "file:///views/boot.html"),
          "the trusted local document gate ignores URL query and fragment state");
    Check(!IsTrustedLocalDocumentUrl("file:///views/boot.html.evil", "file:///views/boot.html"),
          "the trusted local document gate rejects path prefixes");
    Check(!IsTrustedLocalDocumentUrl("https://example.com/views/boot.html", "file:///views/boot.html"),
          "the trusted local document gate rejects remote documents");
    Check(DenyTrustedLocalRemoteScript(FrameClass::kTrustedLocal, Capability::kScript),
          "a trusted local view rejects remote script");
    Check(!DenyTrustedLocalRemoteScript(FrameClass::kOwnedRemoteNested, Capability::kScript),
          "YouTube nested-frame player script is not killed by the local-view script rule");
    Check(!AllowPublicHttpsAfterCoreChecks(FrameClass::kTrustedLocal, Capability::kStylesheet,
                                           "fonts.googleapis.com"),
          "a trusted local view cannot load Google Fonts CSS");
    Check(!AllowPublicHttpsAfterCoreChecks(FrameClass::kTrustedLocal, Capability::kFont, "fonts.gstatic.com"),
          "a trusted local view cannot load Google-hosted fonts");
    Check(!AllowPublicHttpsAfterCoreChecks(FrameClass::kTrustedLocal, Capability::kStylesheet, "cdn.example.com"),
          "a trusted local view cannot load an arbitrary remote stylesheet");
    Check(AllowPublicHttpsAfterCoreChecks(FrameClass::kTrustedLocal, Capability::kFrame, "youtube.com"),
          "a trusted local view may frame an allowlisted YouTube origin");
    Check(AllowPublicHttpsAfterCoreChecks(FrameClass::kTrustedLocal, Capability::kMedia, "googlevideo.com"),
          "a trusted local view may load allowlisted YouTube media");
    Check(AllowPublicHttpsAfterCoreChecks(FrameClass::kOwnedRemoteNested, Capability::kImage, "i.ytimg.com"),
          "an owned remote nested frame may fetch YouTube thumbnail hosts");
    Check(AllowPublicHttpsAfterCoreChecks(FrameClass::kOwnedRemoteNested, Capability::kScript, "www.youtube.com"),
          "an owned remote nested frame may load YouTube player script");
    Check(AllowPublicHttpsAfterCoreChecks(FrameClass::kOwnedRemoteNested, Capability::kMedia,
                                          "rr1---sn-abc.googlevideo.com"),
          "an owned remote nested frame may load googlevideo media subdomains");
    Check(!AllowPublicHttpsAfterCoreChecks(FrameClass::kOwnedRemoteNested, Capability::kConnect, "example.com"),
          "an owned remote nested frame cannot fan out to an arbitrary public host");
    Check(!AllowPublicHttpsAfterCoreChecks(FrameClass::kOwnedRemoteNested, Capability::kImage, "something.example"),
          "a public-looking hostname is not a remote-embed free pass (closes DNS-rebinding fan-out)");
    Check(!AllowPublicHttpsAfterCoreChecks(FrameClass::kOwnedRemoteNested, Capability::kStylesheet,
                                           "fonts.googleapis.com"),
          "a remote embed cannot load Google Fonts CSS either");
    Check(!AllowPublicHttpsAfterCoreChecks(FrameClass::kUnattributed, Capability::kFrame, "youtube.com"),
          "an unattributed request cannot use the YouTube allowlist");

    using PrismaUI::NetworkAuditConfig::WithoutQuery;
    Check(WithoutQuery("https://a.b/c?x=1") == "https://a.b/c", "the request audit log drops a query string");
    Check(WithoutQuery("file:///v/i.html#f") == "file:///v/i.html", "the request audit log drops a fragment");
    Check(WithoutQuery("https://a.b/c") == "https://a.b/c", "the request audit log leaves a plain URL unchanged");
    Check(WithoutQuery("").empty(), "the request audit log accepts an empty URL");
}
