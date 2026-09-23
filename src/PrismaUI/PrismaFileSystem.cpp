#include "PrismaFileSystem.h"

#include "NetworkSandbox.h"
#include "UltralightString.h"
#include "../Engine/LogicalPathPolicy.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string_view>
#include <utility>
#include <vector>

namespace PrismaUI::WebRuntimeUltralight {

    namespace {

        constexpr std::size_t kMaximumFileBytes = 64u * 1024u * 1024u;

        std::string Lower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        std::string Extension(std::string_view path) {
            const auto slash = path.find_last_of('/');
            const auto dot = path.find_last_of('.');
            if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return {};
            return Lower(std::string(path.substr(dot + 1)));
        }

        std::string MimeType(std::string_view path) {
            const auto ext = Extension(path);
            if (ext == "html" || ext == "htm") return "text/html";
            if (ext == "js" || ext == "mjs") return "text/javascript";
            if (ext == "css") return "text/css";
            if (ext == "json") return "application/json";
            if (ext == "png") return "image/png";
            if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
            if (ext == "gif") return "image/gif";
            if (ext == "svg") return "image/svg+xml";
            if (ext == "webp") return "image/webp";
            if (ext == "ico") return "image/x-icon";
            if (ext == "woff2") return "font/woff2";
            if (ext == "woff") return "font/woff";
            if (ext == "ttf") return "font/ttf";
            if (ext == "otf") return "font/otf";
            if (ext == "mp4") return "video/mp4";
            if (ext == "webm") return "video/webm";
            if (ext == "mp3") return "audio/mpeg";
            if (ext == "ogg") return "audio/ogg";
            if (ext == "wasm") return "application/wasm";
            return "application/octet-stream";
        }

        std::string UrlPath(std::string value) {
            const auto query = value.find_first_of("?#");
            if (query != std::string::npos) value.resize(query);
            if (value.rfind("file://", 0) == 0) {
                value.erase(0, 7);
                while (!value.empty() && value.front() == '/') value.erase(value.begin());
            }
            std::replace(value.begin(), value.end(), '\\', '/');
            return value;
        }

    }

    PrismaFileSystem::PrismaFileSystem(std::filesystem::path root) :
        root_(std::move(root)), resourcesRoot_(root_.parent_path() / L"resources"),
        inspectorRoot_(root_.parent_path() / L"inspector") {}

    bool PrismaFileSystem::FileExists(const ultralight::String& filePath) {
        const auto resolved = Resolve(ToUtf8(filePath));
        std::error_code ec;
        return !resolved.empty() && std::filesystem::is_regular_file(resolved, ec) && !ec;
    }

    ultralight::String PrismaFileSystem::GetFileMimeType(const ultralight::String& filePath) {
        return ultralight::String(MimeType(UrlPath(ToUtf8(filePath))).c_str());
    }

    ultralight::String PrismaFileSystem::GetFileCharset(const ultralight::String&) {
        return ultralight::String("UTF-8");
    }

    ultralight::RefPtr<ultralight::Buffer> PrismaFileSystem::OpenFile(const ultralight::String& filePath) {
        const std::string request = ToUtf8(filePath);
        const auto resolved = Resolve(request);
        std::error_code ec;
        if (resolved.empty() || !std::filesystem::is_regular_file(resolved, ec) || ec) return {};
        const auto size = std::filesystem::file_size(resolved, ec);
        if (ec || size > kMaximumFileBytes) return {};

        std::vector<char> bytes(static_cast<std::size_t>(size));
        std::ifstream stream(resolved, std::ios::binary);
        if (!stream || (size != 0 && !stream.read(bytes.data(), static_cast<std::streamsize>(size)))) return {};

        std::string payload(bytes.begin(), bytes.end());
        const auto logicalPath = UrlPath(request);
        const auto ext = Extension(logicalPath);
        if (ext == "html" || ext == "htm") {
            payload = logicalPath.rfind("inspector/", 0) == 0
                          ? PrismaUI::NetworkSandbox::InjectInspectorContentSecurityPolicyIntoHtml(payload)
                          : PrismaUI::NetworkSandbox::InjectContentSecurityPolicyIntoHtml(payload);
        }
        return ultralight::Buffer::CreateFromCopy(payload.data(), payload.size());
    }

    std::filesystem::path PrismaFileSystem::Resolve(const std::string& request) const {
        auto logicalPath = UrlPath(request);
        auto* root = &root_;
        constexpr std::string_view inspectorPrefix = "inspector/";
        constexpr std::string_view resourcePrefix = "resources/";
        if (logicalPath.rfind(inspectorPrefix, 0) == 0) {
            logicalPath.erase(0, inspectorPrefix.size());
            root = &inspectorRoot_;
        } else if (logicalPath.rfind(resourcePrefix, 0) == 0) {
            logicalPath.erase(0, resourcePrefix.size());
            root = &resourcesRoot_;
        }
        const auto validation = PrismaUI::Engine::LogicalPathPolicy::ResolveBelow(*root, logicalPath);
        if (!validation.accepted()) return {};
        std::error_code ec;
        if (!std::filesystem::is_regular_file(validation.file, ec) || ec) return {};
        return validation.file;
    }

}
