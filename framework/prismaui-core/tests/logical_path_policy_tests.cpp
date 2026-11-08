#include "Engine/LogicalPathPolicy.h"

#include <filesystem>
#include <iostream>

int main() {
    using namespace PrismaUI::Engine::LogicalPathPolicy;

    int failures = 0;
    const auto check = [&](bool value, const char* message) {
        if (!value) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    };

    const std::filesystem::path root{"views"};

    for (const char* alias : {
             "inspector%2fMain.html",
             "inspector%2FMain.html",
             "inspector%5cMain.html",
             "%69nspector/Main.html",
             "Inspector/Main.html",
             "%72esources/icudt67l.dat",
             "Resources/icudt67l.dat",
         }) {
        const auto resolution = ResolveBelow(root, alias);
        check(!resolution.accepted() && resolution.reason == RejectReason::kInvalidComponent,
              "rejects a non-canonical protected namespace alias");
    }

    const auto canonicalInspector = ResolveBelow(root, "inspector/Main.html");
    check(canonicalInspector.accepted() && canonicalInspector.relativePath == "inspector/Main.html",
          "accepts the canonical inspector namespace spelling");

    const auto canonicalResources = ResolveBelow(root, "resources/icudt67l.dat");
    check(canonicalResources.accepted() && canonicalResources.relativePath == "resources/icudt67l.dat",
          "accepts the canonical resources namespace spelling");

    for (const char* alias : {
             "Interface/MyPlugin/panel%2ehtml",
             "Interface/MyPlugin/panel.htm%6c",
             "Interface/MyPlugin/panel.html/",
             "Interface/MyPlugin/panel.htm%2f",
         }) {
        const auto resolution = ResolveBelow(root, alias);
        check(!resolution.accepted() && resolution.reason == RejectReason::kInvalidComponent,
              "rejects a raw URL spelling that hides a resolved HTML extension");
    }

    const auto canonicalHtml = ResolveBelow(root, "Interface/MyPlugin/panel.HTML");
    check(canonicalHtml.accepted() && canonicalHtml.relativePath == "Interface/MyPlugin/panel.HTML",
          "accepts a plainly spelled case-insensitive HTML extension");

    const auto encodedOrdinaryPath = ResolveBelow(root, "Interface%2fMy%20Plugin/panel.html");
    check(encodedOrdinaryPath.accepted() &&
              encodedOrdinaryPath.relativePath == "Interface/My Plugin/panel.html",
          "keeps ordinary percent-decoding outside protected namespaces");

    const auto traversal = ResolveBelow(root, "%2e%2e%2fescape.html");
    check(!traversal.accepted(), "rejects traversal after percent decoding");

    if (failures != 0) {
        std::cerr << failures << " logical path policy test(s) failed\n";
        return 1;
    }

    std::cout << "logical path policy tests passed\n";
    return 0;
}
