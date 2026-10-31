#include "DockBridge.h"

#include "URLWhitelist.h"
#include "WebRuntime.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <shellapi.h>
#include <string>
#include <string_view>

namespace PrismaUI::DockBridge {

    namespace {

        std::atomic<bool> g_cursorActive{false};

        std::string EscapeJson(std::string_view value) {
            std::string escaped;
            escaped.reserve(value.size());
            for (const unsigned char character : value) {
                switch (character) {
                    case '\\': escaped += "\\\\"; break;
                    case '"': escaped += "\\\""; break;
                    case '\n': escaped += "\\n"; break;
                    case '\r': escaped += "\\r"; break;
                    case '\t': escaped += "\\t"; break;
                    default:
                        if (character >= 0x20) escaped += static_cast<char>(character);
                }
            }
            return escaped;
        }

        std::string TitleFor(std::string_view path, std::string_view owner) {
            const auto interfacePos = path.find("Interface/");
            std::string title;
            if (interfacePos != std::string_view::npos) {
                const auto start = interfacePos + std::string_view{"Interface/"}.size();
                const auto end = path.find('/', start);
                title.assign(path.substr(start, end == std::string_view::npos ? path.size() - start : end - start));
            }
            if (title.empty()) title.assign(owner);
            const auto extension = title.rfind(".dll");
            if (extension != std::string::npos && extension + 4 == title.size()) title.erase(extension);
            return title;
        }

        bool IsAllowedNexusUrl(std::string_view url) {
            constexpr std::string_view prefix{"https://"};
            if (!url.starts_with(prefix)) return false;
            const auto hostStart = prefix.size();
            const auto hostEnd = url.find_first_of("/?#", hostStart);
            std::string host{url.substr(hostStart, hostEnd == std::string_view::npos ? std::string_view::npos : hostEnd - hostStart)};
            if (host.empty() || host.find_first_of("@:") != std::string::npos) return false;
            std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return URLWhitelist::IsAllowedExternalNavigation(host);
        }

        void HandleMessage(std::string message) {
            const auto separator = message.find('\n');
            if (separator == std::string::npos) return;
            const std::string_view channel{message.data(), separator};
            const std::string_view payload{message.data() + separator + 1, message.size() - separator - 1};
            if (channel == "dockCursor") {
                if (payload == "1") g_cursorActive.store(true, std::memory_order_release);
                if (payload == "0") g_cursorActive.store(false, std::memory_order_release);
                return;
            }
            if (channel == "openUrl" && IsAllowedNexusUrl(payload)) {
                ShellExecuteA(nullptr, "open", std::string(payload).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        }

        std::string BuildPayload() {
            std::string views{"["};
            bool first = true;
            WebRuntime::EnumerateViews([&](WebRuntime::ViewId id, const std::string& path, const std::string& owner) {
                if (owner == "PrismaUI") return;
                const auto title = TitleFor(path, owner);
                if (title.empty()) return;
                if (!first) views += ',';
                first = false;
                views += "{\"id\":" + std::to_string(id) + ",\"title\":\"" + EscapeJson(title) +
                         "\",\"hidden\":" + (WebRuntime::IsHidden(id) ? "true" : "false") +
                         ",\"thumb\":\"../Interface/" + EscapeJson(title) + "/dockimage.png\"}";
            });
            return "{\"welcome\":false,\"views\":" + views + "]}";
        }

        WebRuntime::ViewId DockView() {
            WebRuntime::ViewId dock = 0;
            WebRuntime::EnumerateViews([&](WebRuntime::ViewId id, const std::string& path, const std::string& owner) {
                if (!dock && owner == "PrismaUI" && path == "system/dock.html") dock = id;
            });
            return dock;
        }

    }

    void Show() {
        g_cursorActive.store(false, std::memory_order_release);
        const auto dock = DockView();
        if (!dock) return;
        WebRuntime::RegisterJSListener(dock, "__prismaDockNative", HandleMessage);
        WebRuntime::InteropCall(dock, "setViews", BuildPayload());
    }

    void Hide() { g_cursorActive.store(false, std::memory_order_release); }

    bool IsCursorActive() { return g_cursorActive.load(std::memory_order_acquire); }

}
