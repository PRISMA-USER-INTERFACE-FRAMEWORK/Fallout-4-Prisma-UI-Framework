#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "PrismaUI/NetworkSandbox.h"

namespace {

    void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    std::size_t Count(std::string_view text, std::string_view needle) {
        std::size_t count = 0;
        for (std::size_t pos = 0; (pos = text.find(needle, pos)) != std::string_view::npos; pos += needle.size()) {
            ++count;
        }
        return count;
    }

    void CheckParserFirstEnvelope(std::string_view original, std::string_view wrapped) {
        constexpr std::string_view prefix = "<!doctype html><html><head><meta http-equiv=\"Content-Security-Policy\"";
        constexpr std::string_view body = "</head><body>";
        Check(wrapped.starts_with(prefix), "generated CSP must be the first token in a document head");
        const std::size_t bodyStart = wrapped.find(body);
        Check(bodyStart != std::string_view::npos, "generated CSP must close its actual head before source content");
        Check(wrapped.substr(bodyStart + body.size(), original.size()) == original,
              "source document must begin only after the parser-first CSP envelope");
    }

}  // namespace

int main() {
    const std::string allowlist = PrismaUI::NetworkSandbox::GenerateScript();
    Check(Count(allowlist, "var wrappedFetch = function") == 1,
          "allowlist sandbox must install exactly one fetch wrapper");
    Check(Count(allowlist, "var WrappedXHR = function") == 1, "allowlist sandbox must install exactly one XHR wrapper");
    Check(allowlist.find("isAllowedUrl(url)") != std::string::npos,
          "allowlist sandbox must gate fetch/XHR with isAllowedUrl");
    Check(allowlist.find("Blocked by PrismaUI network sandbox (not whitelisted): ") != std::string::npos,
          "allowlist sandbox must preserve its rejection text");
    Check(allowlist.find("Content-Security-Policy") != std::string::npos, "allowlist sandbox must still inject CSP");
    Check(allowlist.find("'WebSocket'") != std::string::npos,
          "allowlist sandbox must still disable direct socket APIs");

    const std::string wrapped = PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(
        "<html><head><title>test</title></head><body>ok</body></html>");
    CheckParserFirstEnvelope("<html><head><title>test</title></head><body>ok</body></html>", wrapped);
    Check(Count(wrapped, "Content-Security-Policy") == 1, "asset-boundary CSP must be injected exactly once");
    const std::string headless = PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml("<body>ok</body>");
    CheckParserFirstEnvelope("<body>ok</body>", headless);

    const std::string header = PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(
        "<html><header>not a head</header><script>run()</script></html>");
    CheckParserFirstEnvelope("<html><header>not a head</header><script>run()</script></html>", header);
    const std::string commented = PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(
        "<html><!-- <head> --><script>run()</script></html>");
    CheckParserFirstEnvelope("<html><!-- <head> --><script>run()</script></html>", commented);
    const std::string rawText = PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(
        "<html><script>const head = '<head>';</script><style>.x::before{content:'<head>';}</style></html>");
    CheckParserFirstEnvelope(
        "<html><script>const head = '<head>';</script><style>.x::before{content:'<head>';}</style></html>", rawText);
    const std::string mixedCase = PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(
        "<HTML><HeAd><script>run()</script></HeAd></HTML>");
    CheckParserFirstEnvelope("<HTML><HeAd><script>run()</script></HeAd></HTML>", mixedCase);
    const std::string malformed = PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(
        "<html><head data-x='unterminated<script>run()</script>");
    CheckParserFirstEnvelope("<html><head data-x='unterminated<script>run()</script>", malformed);
    const std::string existing = PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(
        "<html><head><meta http-equiv=\"Content-Security-Policy\" content=\"default-src "
        "'none'\"><script>run()</script></head></html>");
    CheckParserFirstEnvelope(
        "<html><head><meta http-equiv=\"Content-Security-Policy\" content=\"default-src "
        "'none'\"><script>run()</script></head></html>",
        existing);
    Check(Count(existing, "Content-Security-Policy") == 2, "the generated CSP must not remove an existing policy");
    const std::string textarea = PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(
        "<html><textarea><head></textarea><script>run()</script></html>");
    CheckParserFirstEnvelope("<html><textarea><head></textarea><script>run()</script></html>", textarea);
    const std::string title = PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(
        "<html><title><head></title><script>run()</script></html>");
    CheckParserFirstEnvelope("<html><title><head></title><script>run()</script></html>", title);

    const std::string inspectorSource =
        "<!DOCTYPE html><html><head><meta http-equiv=\"Content-Security-Policy\" "
        "content=\"style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline' data:\">"
        "<script>boot()</script></head><body>inspector</body></html>";
    const std::string inspector =
        PrismaUI::NetworkSandbox::InjectInspectorContentSecurityPolicyIntoHtml(inspectorSource);
    Check(inspector.starts_with("<!DOCTYPE html><html><head><meta http-equiv=\"Content-Security-Policy\""),
          "inspector CSP must be inserted into the existing head without wrapping the document");
    Check(Count(inspector, "Content-Security-Policy") == 2,
          "inspector hardening must preserve the upstream CSP and add one intersecting policy");
    Check(inspector.find("connect-src 'none'") != std::string::npos,
          "inspector CSP must disable network connections");
    Check(inspector.find("worker-src 'self' blob:") != std::string::npos,
          "inspector CSP must preserve local formatter and heap workers");
    Check(inspector.find("script-src 'self' 'unsafe-inline' data:") != std::string::npos,
          "inspector CSP must preserve the upstream bootstrap script requirements");
    Check(inspector.find("<script>boot()</script>") != std::string::npos,
          "inspector CSP insertion must preserve original document content");

    const std::string ultralightCsp =
        PrismaUI::URLWhitelist::GenerateContentSecurityPolicy(PrismaUI::URLWhitelist::CspTransport::kUltralight);
    Check(ultralightCsp.find("prisma:") == std::string::npos && ultralightCsp.find("127.0.0.1") == std::string::npos,
          "Ultralight CSP must not inherit CEF transport exceptions");
    Check(ultralightCsp.find("youtube.com") == std::string::npos,
          "#666: Ultralight (Free) CSP is local-only and must not carry the remote frame allowlist");

    const std::string privateGuard = PrismaUI::NetworkSandbox::GeneratePrivateNetworkGuardScript();
    Check(Count(privateGuard, "var wrappedFetch = function") == 1,
          "private-network guard must install exactly one fetch wrapper");
    Check(Count(privateGuard, "var WrappedXHR = function") == 1,
          "private-network guard must install exactly one XHR wrapper");
    Check(privateGuard.find("isSafeUrl(url)") != std::string::npos,
          "private-network guard must gate fetch/XHR with isSafeUrl");
    Check(privateGuard.find("Blocked by PrismaUI private-network guard: ") != std::string::npos,
          "private-network guard must preserve its rejection text");
    Check(privateGuard.find("privatenet") != std::string::npos,
          "private-network guard must preserve native blocked-network reporting");

    std::cout << "network_sandbox_script_tests ok\n";
    return 0;
}
