#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include "core_security_tests_support.h"
#include "Engine/DockCursorPolicy.h"
#include "Engine/ExternalUrlPolicy.h"
#include "Engine/LogicalPathPolicy.h"
#include "Engine/NativeMessageLimits.h"
#include "Engine/PrismaResourcePolicy.h"
#include "Engine/StrictPort.h"
#include "PrismaUI/InputOwnershipPolicy.h"
#include "PrismaUI/InvokeResult.h"
#include "PrismaUI/PrivateNetworkGuard.h"
#include "PrismaUI/ViewOrigin.h"

#ifdef _WIN32
    #include <windows.h>
#endif

namespace {

    using CoreSecurityTests::Check;
    using CoreSecurityTests::failures;

    std::error_code RemoveTreeDeepestFirst(const std::filesystem::path& root) {
        std::error_code ec;
        if (!std::filesystem::exists(root, ec)) return {};

        std::vector<std::filesystem::path> entries;
        {
            std::filesystem::recursive_directory_iterator it(root, ec), end;
            if (ec) return ec;
            for (; it != end; it.increment(ec)) {
                if (ec) return ec;
                entries.push_back(it->path());
            }
        }

        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            return std::distance(a.begin(), a.end()) > std::distance(b.begin(), b.end());
        });

        std::error_code firstFailure;
        for (const auto& entry : entries) {
            std::error_code removeError;
            std::filesystem::remove(entry, removeError);
            if (removeError && !firstFailure) firstFailure = removeError;
        }
        std::error_code rootError;
        std::filesystem::remove(root, rootError);
        if (rootError && !firstFailure) firstFailure = rootError;
        return firstFailure;
    }

}  // namespace

void RunUrlWhitelistChecks();

int RunAll() {
    using namespace PrismaUI;

    using InputOwnershipPolicy::CaptureOwner;
    using InputOwnershipPolicy::CursorVisibilityAction;
    Check(InputOwnershipPolicy::ShouldRouteMouse(CaptureOwner::kPassiveDock, false),
          "routes passive dock clicks to CEF without a focused view");
    Check(!InputOwnershipPolicy::PrismaOwnsCursor(CaptureOwner::kPassiveDock),
          "keeps Fallout cursor ownership for a passive dock");
    Check(!InputOwnershipPolicy::PrismaOwnsCursor(CaptureOwner::kPassiveDock, false),
          "keeps Fallout cursor ownership outside a passive dock control");
    Check(InputOwnershipPolicy::PrismaOwnsCursor(CaptureOwner::kPassiveDock, true),
          "gives the topmost Prisma cursor to an interactive passive dock control");
    Check(InputOwnershipPolicy::ShouldRouteMouse(CaptureOwner::kFocusedView, true),
          "routes focused view input within its hit region");
    Check(!InputOwnershipPolicy::ShouldRouteMouse(CaptureOwner::kFocusedView, false),
          "passes focused overlay input outside its hit region to Fallout");
    Check(InputOwnershipPolicy::PrismaOwnsCursor(CaptureOwner::kFocusedView),
          "gives the focused Prisma view cursor ownership");
    Check(InputOwnershipPolicy::AfterFocusedViewRelease(true) == CaptureOwner::kPassiveDock,
          "returns debugger focus to a visible passive dock");
    Check(InputOwnershipPolicy::AfterFocusedViewRelease(false) == CaptureOwner::kNone,
          "returns debugger focus to no capture when dock is hidden");
    Check(InputOwnershipPolicy::NextCursorVisibilityAction(CaptureOwner::kFocusedView, false) ==
              CursorVisibilityAction::kHideVanilla,
          "hides the vanilla cursor once when focused Prisma input starts");
    Check(InputOwnershipPolicy::NextCursorVisibilityAction(CaptureOwner::kPassiveDock, true) ==
              CursorVisibilityAction::kRestoreVanilla,
          "restores the vanilla cursor when debugger focus returns to the dock");
    Check(InputOwnershipPolicy::NextCursorVisibilityAction(CaptureOwner::kNone, false) == CursorVisibilityAction::kNone,
          "does not alter an already-restored cursor without Prisma ownership");
    Check(Engine::ParseDockCursorState("1") == Engine::DockCursorState::kActive,
          "accepts the active dock cursor state");
    Check(Engine::ParseDockCursorState("0") == Engine::DockCursorState::kInactive,
          "accepts the inactive dock cursor state");
    Check(Engine::ParseDockCursorState("active") == Engine::DockCursorState::kInvalid,
          "rejects an unrecognized dock cursor state");

    using Engine::ExternalUrlAction;
    using Engine::ExternalUrlNavigationKind;
    Check(Engine::ClassifyExternalUrl("https://www.nexusmods.com/fallout4/mods/105454") ==
              ExternalUrlAction::kOpenExternal,
          "opens Nexus Mods HTTPS URLs externally");
    Check(Engine::ClassifyExternalUrl("https://github.com/PrismaUI") == ExternalUrlAction::kReject,
          "rejects public GitHub HTTPS; it is not on kExternalNavigationDomains");
    Check(Engine::IsAllowedVideoEmbedUrl("https://www.youtube.com/embed/aaaaaaaaaaa"),
          "allows a YouTube embed URL through the frame whitelist");
    Check(!Engine::IsAllowedVideoEmbedUrl("https://www.nexusmods.com/fallout4/mods/105454"),
          "Nexus is an external-browser destination, not a video embed");
    Check(!Engine::IsAllowedVideoEmbedUrl("https://github.com/PrismaUI"),
          "GitHub is neither an embed host nor an external-navigation host");
    Check(Engine::ClassifyExternalUrl("http://example.com") == ExternalUrlAction::kReject,
          "rejects insecure HTTP URLs");
    Check(Engine::ClassifyExternalUrl("javascript:alert(1)") == ExternalUrlAction::kReject, "rejects javascript URLs");
    Check(Engine::ClassifyExternalUrl("file:///C:/Windows/win.ini") == ExternalUrlAction::kReject,
          "rejects file URLs");
    Check(Engine::ClassifyExternalUrl("data:text/html,hello") == ExternalUrlAction::kReject, "rejects data URLs");
    Check(Engine::ClassifyExternalUrl("https://") == ExternalUrlAction::kReject, "rejects malformed URLs");
    Check(Engine::ClassifyExternalUrl("https://[github.com]/") == ExternalUrlAction::kReject,
          "rejects malformed bracketed hosts");
    Check(Engine::ClassifyExternalUrl("https://github.com/\"x") == ExternalUrlAction::kReject,
          "rejects command-line quote characters");
    Check(Engine::ClassifyExternalUrl("prisma://shell/system/dock.html") == ExternalUrlAction::kContinueInternal,
          "keeps internal Prisma URLs in CEF");
    Check(Engine::ClassifyExternalUrl("https://www.nexusmods.com:8443/fallout4/mods/105454") ==
              ExternalUrlAction::kReject,
          "rejects an allowlisted external host on a non-443 port");
    Check(Engine::ClassifyExternalUrl("https://www.nexusmods.com:443/fallout4/mods/105454") ==
              ExternalUrlAction::kOpenExternal,
          "allows an allowlisted external host on the explicit HTTPS default port");
    Check(!Engine::IsAllowedVideoEmbedUrl("https://www.youtube.com:8443/embed/aaaaaaaaaaa"),
          "rejects a YouTube embed on a non-443 port");
    Check(Engine::ClassifyExternalNavigation("https://www.nexusmods.com/fallout4/mods/105454", true, true,
                                              ExternalUrlNavigationKind::kPopup) == ExternalUrlAction::kOpenExternal,
          "routes a trusted Nexus popup to the external browser");
    Check(Engine::ClassifyExternalNavigation("https://www.nexusmods.com/fallout4/mods/105454", true, true,
                                              ExternalUrlNavigationKind::kNormal) == ExternalUrlAction::kOpenExternal,
          "routes trusted Nexus navigation to the external browser");
    Check(Engine::ClassifyExternalNavigation("https://github.com/PrismaUI", true, true,
                                              ExternalUrlNavigationKind::kNormal) == ExternalUrlAction::kReject,
          "does not open GitHub in the system browser");

    Check(ViewOrigin::NormalizeRelativePath("/Interface\\FalloutChat/panel.html") == "Interface/FalloutChat/panel.html",
          "normalizes a legacy consumer path with leading separators");
    Check(ViewOrigin::NormalizeRelativePath("Interface/A/../B.html").empty(), "rejects parent traversal");
    Check(ViewOrigin::NormalizeRelativePath("Interface/A/file.txt:stream").empty(),
          "rejects NTFS alternate-data-stream syntax");
    Check(ViewOrigin::NormalizeRelativePath("Interface/A/CON.txt").empty(), "rejects Windows device names");
    Check(ViewOrigin::NormalizeRelativePath("Interface/A/file. ").empty(),
          "rejects Windows-normalized trailing characters");
    Check(ViewOrigin::BuildUrl("Interface/My Mod/panel.html").find("My%20Mod") != std::string::npos,
          "URL-encodes path bytes instead of handing raw spaces to CEF");
    Check(ViewOrigin::HostForPath("Interface/FalloutChat/panel.html") ==
              ViewOrigin::HostForPath("interface/falloutchat/settings.html"),
          "one plugin folder gets one origin");
    Check(ViewOrigin::HostForPath("Interface/FalloutChat/panel.html") !=
              ViewOrigin::HostForPath("Interface/PrismaHUD/panel.html"),
          "different plugins get different origins");
    const std::string chatHost = ViewOrigin::HostForPath("Interface/FalloutChat/panel.html");
    Check(ViewOrigin::IsPathAllowedForHost(chatHost, "Interface/FalloutChat/settings.html"),
          "host may load another file in its own plugin folder");
    Check(!ViewOrigin::IsPathAllowedForHost(chatHost, "Interface/PrismaHUD/panel.html"),
          "host cannot alias another plugin into its own origin");
    Check(ViewOrigin::IsPathAllowedForHost(chatHost, "icons/ControllerButtons/a.png"),
          "isolated origins may load shared framework icons");
    for (const char* shared : {"prisma_input.js", "prisma_osk.js", "prisma_osk.css"}) {
        Check(ViewOrigin::IsPathAllowedForHost(chatHost, shared),
              "isolated origins may load shared on-screen keyboard assets");
    }
    Check(!ViewOrigin::IsPathAllowedForHost(chatHost, "prisma_osk.html"),
          "shared keyboard allowlist is exact");
    const std::string rootHost = ViewOrigin::HostForPath("hud_health.html");
    Check(rootHost == ViewOrigin::HostForPath("hud_health.js"),
          "legacy root-level pages and sibling assets share a compatibility origin");
    Check(ViewOrigin::IsPathAllowedForHost(rootHost, "hud_health.css"),
          "legacy root-level pages may load sibling assets");
    Check(ViewOrigin::IsPathAllowedForHost(rootHost, "css/crafting.css"),
          "legacy root-level pages may load their shared css directory");
    Check(ViewOrigin::IsPathAllowedForHost(rootHost, "js/crafting.js"),
          "legacy root-level pages may load their shared js directory");
    Check(ViewOrigin::IsPathAllowedForHost(rootHost, "sounds/buzzlight.mp3"),
          "legacy root-level pages may load their shared sounds directory");
    Check(ViewOrigin::IsPathAllowedForHost(chatHost, "Interface/icons/VaultBoy.png"),
          "isolated views may load shared Interface icons");
    Check(!ViewOrigin::IsPathAllowedForHost(chatHost, "Interface/PrismaHUD/hud_health.html"),
          "an isolated plugin host cannot load another plugin's page");

    Check(Engine::PrismaResourcePolicy::IsAllowedInitialShellChildNavigation(true, "shell"),
          "allows an initial unowned shell child to load a same-origin shell page");
    Check(!Engine::PrismaResourcePolicy::IsAllowedInitialShellChildNavigation(true, "views"),
          "does not let an initial unowned shell child enter the legacy shared view host");
    Check(!Engine::PrismaResourcePolicy::IsAllowedInitialShellChildNavigation(true, "view-0123456789abcdef"),
          "does not let an initial unowned shell child enter an isolated view host");
    Check(!Engine::PrismaResourcePolicy::IsAllowedInitialShellChildNavigation(false, "shell"),
          "does not give an arbitrary child frame shell-page authority");

    Check(Engine::ParseStrictPort("49152") == 49152, "accepts an exact decimal TCP port");
    Check(Engine::ParseStrictPort("49152x") == 0, "rejects a port with trailing junk");
    Check(Engine::ParseStrictPort("0") == 0, "rejects port zero");
    Check(Engine::ParseStrictPort("65536") == 0, "rejects a port above the TCP range");
    Check(Engine::IsNativeMessageWithinLimits("channel", "payload"),
          "accepts a native message inside the shared limits");
    Check(!Engine::IsNativeMessageWithinLimits("", "payload"), "rejects an empty native message channel");
    Check(
        !Engine::IsNativeMessageWithinLimits(std::string(Engine::kMaxNativeMessageChannelBytes + 1, 'x'), "payload"),
        "rejects an oversized native message channel");
    Check(
        !Engine::IsNativeMessageWithinLimits("channel", std::string(Engine::kMaxNativeMessagePayloadBytes + 1, 'x')),
        "rejects an oversized native message payload");
    Check(InvokeResult::Error("backend inactive") == "{\"error\":\"backend inactive\"}",
          "uses the shared Invoke error object format");
    Check(InvokeResult::Error("quote: \"") == "{\"error\":\"quote: \\\"\"}", "escapes Invoke error messages as JSON");

    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("127.0.0.1"), "IPv4 loopback");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("127.1"), "short IPv4 loopback");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("0177.0.0.1"), "octal IPv4 loopback");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("0x7f.0.0.1"), "hex IPv4 loopback");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("0x7f000001"), "integer hex IPv4 loopback");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("2130706433"), "integer IPv4 loopback");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("10.1.2.3"), "IPv4 RFC1918");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("[fe80::1]"), "IPv6 link local low range");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("febf::1"), "IPv6 link local high range");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("::ffff:192.168.1.5"), "IPv4-mapped IPv6 private address");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("0:0:0:0:0:0:0:1"), "expanded IPv6 loopback");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("0:0:0:0:0:ffff:c0a8:105"),
          "hex IPv4-mapped IPv6 private address");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("localhost."), "localhost trailing dot");
    Check(PrivateNetworkGuard::IsPrivateOrLoopbackHost("fe80::1%25Ethernet"), "IPv6 zone id");
    Check(!PrivateNetworkGuard::IsPrivateOrLoopbackHost("example.com"), "public hostname");
    Check(!PrivateNetworkGuard::IsPrivateOrLoopbackHost("8.8.8.8"), "public IPv4");
    Check(!PrivateNetworkGuard::IsPrivateOrLoopbackHost("2606:4700:4700::1111"), "public IPv6");

    const auto testRoot =
        std::filesystem::temp_directory_path() /
        ("prismaui-logical-path-policy-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto page = testRoot / "Interface" / "MyPlugin" / "index.html";
    const auto nestedPage = testRoot / "Interface" / "MyPlugin" / "nested" / "asset.html";
    std::filesystem::create_directories(page.parent_path());
    std::filesystem::create_directories(nestedPage.parent_path());
    {
        std::ofstream pageFile(page, std::ios::binary);
        pageFile << "<h1>hello world</h1>";
    }
    {
        std::ofstream nestedFile(nestedPage, std::ios::binary);
        nestedFile << "nested";
    }

    using namespace Engine::LogicalPathPolicy;
    const auto valid = ResolveBelow(testRoot, "Interface/MyPlugin/index.html");
    Check(valid.accepted(), "accepts a normal relative MO2 virtual view path");
    Check(valid.relativePath == "Interface/MyPlugin/index.html", "preserves the validated logical relative path");
    std::ifstream validPage(valid.file, std::ios::binary);
    Check(validPage.good(), "opens an existing view through its logical root path");

    const auto nested = ResolveBelow(testRoot, "Interface/MyPlugin/nested/asset.html");
    Check(nested.accepted(), "accepts a nested view path");
    std::ifstream nestedView(nested.file, std::ios::binary);
    Check(nestedView.good(), "opens a nested view through its logical root path");

    const auto missing = ResolveBelow(testRoot, "Interface/MyPlugin/missing.html");
    Check(missing.accepted(), "accepts a syntactically valid missing view path");
    Check(!std::filesystem::exists(missing.file), "leaves a valid missing view for the handler to report as 404");
    const int missingStatus = missing.accepted() && !std::filesystem::is_regular_file(missing.file) ? 404 : 400;
    Check(missingStatus == 404, "maps a valid missing view to HTTP 404 rather than HTTP 400");

    Check(!ResolveBelow(testRoot, "../escape.html").accepted(), "rejects dot-dot traversal");
    Check(!ResolveBelow(testRoot, "Interface\\MyPlugin/..\\..\\escape.html").accepted(),
          "rejects traversal expressed with mixed separators");
    Check(!ResolveBelow(testRoot, "%2e%2e%2fescape.html").accepted(), "rejects traversal after percent decoding");
    Check(!ResolveBelow(testRoot, "C:\\Windows\\win.ini").accepted(), "rejects drive-qualified paths");
    Check(!ResolveBelow(testRoot, "C:file.html").accepted(), "rejects drive-relative paths");
    Check(!ResolveBelow(testRoot, "\\\\server\\share\\file.html").accepted(), "rejects UNC paths");
    Check(!ResolveBelow(testRoot, "/rooted.html").accepted(), "rejects slash-rooted paths");
    Check(!ResolveBelow(testRoot, "\\rooted.html").accepted(), "rejects backslash-rooted paths");
    Check(!ResolveBelow(testRoot, "Interface/%zz/index.html").accepted(),
          "rejects malformed percent escapes before path use");
    Check(!ResolveBelow(testRoot, std::string("Interface/") + std::string(1, '\0') + "index.html").accepted(),
          "rejects decoded NUL characters");

#ifdef _WIN32
    const auto externalPage = testRoot.parent_path() / (testRoot.filename().wstring() + L"-external.html");
    {
        std::ofstream externalFile(externalPage, std::ios::binary);
        externalFile << "external";
    }
    const auto reparsePage = testRoot / "Interface" / "MyPlugin" / "linked.html";
    if (CreateSymbolicLinkW(reparsePage.c_str(), externalPage.c_str(), 0) != 0) {
        const auto reparse = ResolveBelow(testRoot, "Interface/MyPlugin/linked.html");
        Check(!reparse.accepted() && reparse.reason == RejectReason::kReparsePoint,
              "rejects a real reparse point below the logical views root");
        std::error_code linkError;
        std::filesystem::remove(reparsePage, linkError);
        Check(!linkError, "removes the symlink it created inside the scratch tree");
    } else {
        std::cout << "SKIP: Windows symlink test requires symlink creation permission\n";
    }
    std::error_code externalError;
    std::filesystem::remove(externalPage, externalError);
    Check(!externalError, "removes the symlink target it created outside the scratch tree");
#endif

    RunUrlWhitelistChecks();

    const std::error_code cleanupError = RemoveTreeDeepestFirst(testRoot);
    const bool treeGone = !std::filesystem::exists(testRoot);
    Check(true, "teardown returns instead of aborting the process");

    if (cleanupError || !treeGone) {
        std::cout << "NOTE: scratch directory not fully removed: " << testRoot.string();
        if (cleanupError) std::cout << " (" << cleanupError.message() << ")";
        std::cout << "\n      Non-fatal: this is filesystem behaviour of the host, not a policy "
                     "assertion. It must never abort the run.\n";
    }

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int main() {
    int status = EXIT_FAILURE;
    try {
        status = RunAll();
    } catch (const std::exception& e) {
        std::cerr << "HARNESS ERROR: " << e.what() << '\n';
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "HARNESS ERROR: unknown exception\n";
        return EXIT_FAILURE;
    }

    if (status != EXIT_SUCCESS) return status;
    std::cout << "core security tests passed\n";
    return EXIT_SUCCESS;
}
