#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace PrismaUI::ViewOrigin {

inline std::string ToLowerAscii(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const unsigned char c : value) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

inline bool IsWindowsReservedSegment(std::string_view segment) {
    const std::size_t dot = segment.find('.');
    const std::string base = ToLowerAscii(segment.substr(0, dot));
    if (base == "con" || base == "prn" || base == "aux" || base == "nul") return true;
    return base.size() == 4 &&
           (base.rfind("com", 0) == 0 || base.rfind("lpt", 0) == 0) &&
           base[3] >= '1' && base[3] <= '9';
}

inline std::string EncodeUrlPath(std::string_view path) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(path.size());
    for (const unsigned char c : path) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0f]);
        }
    }
    return out;
}

inline std::string NormalizeRelativePath(std::string_view raw) {
    if (raw.empty() || raw.size() > 4096 || raw.find('\0') != std::string_view::npos) return {};
    std::string value(raw);
    std::replace(value.begin(), value.end(), '\\', '/');

    const auto first = value.find_first_not_of('/');
    if (first == std::string::npos) return {};
    value.erase(0, first);

    std::vector<std::string> parts;
    std::size_t pos = 0;
    while (pos <= value.size()) {
        const std::size_t slash = value.find('/', pos);
        const std::string part = value.substr(pos, slash == std::string::npos ? std::string::npos
                                                                             : slash - pos);
        if (!part.empty() && part != ".") {
            if (part == ".." || part.size() > 255 || part.back() == '.' || part.back() == ' ' ||
                IsWindowsReservedSegment(part)) {
                return {};
            }
            for (const unsigned char c : part) {
                if (c == 0 || c < 0x20 || c == 0x7f || c == ':' || c == '?' || c == '#') return {};
            }
            parts.push_back(part);
        }
        if (slash == std::string::npos) break;
        pos = slash + 1;
    }
    if (parts.empty()) return {};

    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out.push_back('/');
        out += parts[i];
    }
    return out;
}

inline std::string ScopeForPath(std::string_view normalizedPath) {
    const std::string path = NormalizeRelativePath(normalizedPath);
    if (path.empty()) return {};

    std::vector<std::string_view> parts;
    std::size_t pos = 0;
    while (pos <= path.size()) {
        const std::size_t slash = path.find('/', pos);
        parts.emplace_back(path.data() + pos,
                           (slash == std::string::npos ? path.size() : slash) - pos);
        if (slash == std::string::npos) break;
        pos = slash + 1;
    }

    if (parts.size() >= 2 && ToLowerAscii(parts[0]) == "interface") {
        return "interface/" + ToLowerAscii(parts[1]);
    }
    if (parts.size() >= 2) {
        const std::string topLevelDirectory = ToLowerAscii(parts[0]);
        if (topLevelDirectory == "css" || topLevelDirectory == "js" ||
            topLevelDirectory == "sounds") {
            return "root";
        }
        return topLevelDirectory;
    }

    return "root";
}

inline std::uint64_t Fnv1a64(std::string_view value) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char c : value) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

inline std::string HostForPath(std::string_view path) {
    const std::string scope = ScopeForPath(path);
    if (scope.empty()) return {};
    std::ostringstream out;
    out << "view-" << std::hex << std::nouppercase << std::setfill('0') << std::setw(16)
        << Fnv1a64(scope);
    return out.str();
}

inline bool IsIsolatedViewHost(std::string_view host) {
    if (host.size() != 21 || host.substr(0, 5) != "view-") return false;
    return std::all_of(host.begin() + 5, host.end(), [](const unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

inline bool IsSharedFrameworkPath(std::string_view rawPath) {
    const std::string path = ToLowerAscii(NormalizeRelativePath(rawPath));
    if (path.empty()) return false;
    return path == "prisma_controller_glyphs.js" ||
           path == "prisma_controller_glyphs.css" ||
           path == "prisma_input.js" ||
           path == "prisma_osk.js" ||
           path == "prisma_osk.css" ||
           path == "notification-banner.html" ||
           path.rfind("icons/", 0) == 0 ||
           path.rfind("interface/icons/", 0) == 0;
}

inline bool IsPathAllowedForHost(std::string_view host, std::string_view rawPath) {
    const std::string normalized = NormalizeRelativePath(rawPath);
    if (normalized.empty() || !IsIsolatedViewHost(host)) return false;
    if (IsSharedFrameworkPath(normalized)) return true;
    return HostForPath(normalized) == host;
}

inline std::string BuildUrl(std::string_view rawPath) {
    const std::string path = NormalizeRelativePath(rawPath);
    if (path.empty()) return {};
    const std::string host = HostForPath(path);
    if (host.empty()) return {};
    return "prisma://" + host + "/" + EncodeUrlPath(path);
}

}
