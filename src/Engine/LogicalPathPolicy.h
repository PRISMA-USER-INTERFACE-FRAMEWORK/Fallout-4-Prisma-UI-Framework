#pragma once

#include "../PrismaUI/ViewOrigin.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

namespace PrismaUI::Engine::LogicalPathPolicy {

enum class RejectReason {
    kNone,
    kEmpty,
    kTooLong,
    kMalformedPercentEncoding,
    kRootedPath,
    kDrivePath,
    kInvalidComponent,
    kDisallowedHost,
    kReparsePoint,
};

struct Resolution {
    std::filesystem::path file;
    std::string relativePath;
    RejectReason reason = RejectReason::kNone;

    [[nodiscard]] bool accepted() const { return reason == RejectReason::kNone; }
};

inline const char* RejectReasonText(const RejectReason reason) {
    switch (reason) {
    case RejectReason::kNone: return "none";
    case RejectReason::kEmpty: return "empty path";
    case RejectReason::kTooLong: return "path exceeds the maximum length";
    case RejectReason::kMalformedPercentEncoding: return "malformed percent encoding";
    case RejectReason::kRootedPath: return "rooted or UNC path";
    case RejectReason::kDrivePath: return "drive-qualified path";
    case RejectReason::kInvalidComponent: return "invalid path component";
    case RejectReason::kDisallowedHost: return "unknown or mismatched prisma host";
    case RejectReason::kReparsePoint: return "reparse point below the trusted root";
    }
    return "unknown path validation error";
}

inline int HexValue(const char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

inline bool DecodePercentEscapes(const std::string_view raw, std::string& decoded) {
    decoded.clear();
    decoded.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] != '%') {
            decoded.push_back(raw[i]);
            continue;
        }
        if (i + 2 >= raw.size()) return false;
        const int high = HexValue(raw[i + 1]);
        const int low = HexValue(raw[i + 2]);
        if (high < 0 || low < 0) return false;
        decoded.push_back(static_cast<char>((high << 4) | low));
        i += 2;
    }
    return true;
}

inline bool HasNonCanonicalProtectedPrefix(const std::string_view rawPath,
                                           const std::string_view decodedPath) {
    const std::size_t separator = decodedPath.find_first_of("/\\");
    const std::string_view first = decodedPath.substr(0, separator);
    const std::string lower = PrismaUI::ViewOrigin::ToLowerAscii(first);
    if (lower == "inspector") return rawPath.rfind("inspector/", 0) != 0;
    if (lower == "resources") return rawPath.rfind("resources/", 0) != 0;
    return false;
}

inline std::string LowerExtension(const std::string_view path) {
    const std::size_t end = path.find_last_not_of("/\\");
    if (end == std::string_view::npos) return {};
    const std::string_view trimmed = path.substr(0, end + 1);
    const std::size_t slash = trimmed.find_last_of("/\\");
    const std::size_t dot = trimmed.find_last_of('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) return {};
    return PrismaUI::ViewOrigin::ToLowerAscii(trimmed.substr(dot + 1));
}

inline bool HasNonCanonicalHtmlExtension(const std::string_view rawPath,
                                         const std::string_view decodedPath) {
    const std::string decodedExtension = LowerExtension(decodedPath);
    if (decodedExtension != "html" && decodedExtension != "htm") return false;
    if (!rawPath.empty() && (rawPath.back() == '/' || rawPath.back() == '\\')) return true;
    return LowerExtension(rawPath) != decodedExtension;
}

inline bool IsDriveQualified(const std::string_view path) {
    return path.size() >= 2 &&
           std::isalpha(static_cast<unsigned char>(path[0])) != 0 && path[1] == ':';
}

inline bool IsInvalidComponent(const std::string_view component) {
    if (component.empty() || component == "." || component == ".." || component.size() > 255 ||
        component.back() == '.' || component.back() == ' ' ||
        PrismaUI::ViewOrigin::IsWindowsReservedSegment(component)) {
        return true;
    }
    for (const unsigned char character : component) {
        if (character < 0x20 || character == 0x7f || character == ':' || character == '<' ||
            character == '>' || character == '"' || character == '|' || character == '?' ||
            character == '*' || character == '#') {
            return true;
        }
    }
    return false;
}

inline bool HasReparsePointBelowRoot(const std::filesystem::path& root,
                                     const std::string_view normalizedRelativePath) {
#ifdef _WIN32
    std::filesystem::path current = root;
    std::size_t start = 0;
    while (start < normalizedRelativePath.size()) {
        const std::size_t slash = normalizedRelativePath.find('/', start);
        const std::string_view component = normalizedRelativePath.substr(
            start, slash == std::string_view::npos ? std::string_view::npos : slash - start);
        current /= std::filesystem::path(component);
        const DWORD attributes = GetFileAttributesW(current.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            break;
        }
        if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            return true;
        }
        if (slash == std::string_view::npos) {
            break;
        }
        start = slash + 1;
    }
#else
    (void)root;
    (void)normalizedRelativePath;
#endif
    return false;
}

inline Resolution ValidateSchemePath(const std::string_view rawPath) {
    Resolution result;
    if (rawPath.empty()) {
        result.reason = RejectReason::kEmpty;
        return result;
    }
    if (rawPath.size() > 4096) {
        result.reason = RejectReason::kTooLong;
        return result;
    }

    std::string decoded;
    if (!DecodePercentEscapes(rawPath, decoded)) {
        result.reason = RejectReason::kMalformedPercentEncoding;
        return result;
    }
    if (decoded.empty() || decoded.find('\0') != std::string::npos) {
        result.reason = RejectReason::kEmpty;
        return result;
    }
    if (HasNonCanonicalProtectedPrefix(rawPath, decoded) ||
        HasNonCanonicalHtmlExtension(rawPath, decoded)) {
        result.reason = RejectReason::kInvalidComponent;
        return result;
    }
    if (decoded.front() == '/' || decoded.front() == '\\') {
        result.reason = RejectReason::kRootedPath;
        return result;
    }
    if (IsDriveQualified(decoded)) {
        result.reason = RejectReason::kDrivePath;
        return result;
    }

    std::replace(decoded.begin(), decoded.end(), '\\', '/');
    std::string normalized;
    normalized.reserve(decoded.size());
    std::size_t start = 0;
    while (start < decoded.size()) {
        const std::size_t slash = decoded.find('/', start);
        const std::string_view component = std::string_view(decoded).substr(
            start, slash == std::string_view::npos ? std::string_view::npos : slash - start);
        if (IsInvalidComponent(component)) {
            result.reason = RejectReason::kInvalidComponent;
            return result;
        }
        if (!normalized.empty()) normalized.push_back('/');
        normalized.append(component);
        if (slash == std::string_view::npos) break;
        start = slash + 1;
    }

    if (normalized.empty()) {
        result.reason = RejectReason::kEmpty;
        return result;
    }
    result.relativePath = std::move(normalized);
    return result;
}

inline Resolution ResolveValidatedBelow(const std::filesystem::path& root,
                                        const std::string_view normalizedRelativePath) {
    Resolution result;
    result.relativePath = normalizedRelativePath;
    if (HasReparsePointBelowRoot(root, normalizedRelativePath)) {
        result.reason = RejectReason::kReparsePoint;
        result.relativePath.clear();
        return result;
    }

    result.file = root / std::filesystem::path(normalizedRelativePath);
    return result;
}

inline Resolution ResolveBelow(const std::filesystem::path& root, const std::string_view rawPath) {
    Resolution result = ValidateSchemePath(rawPath);
    if (!result.accepted()) return result;
    return ResolveValidatedBelow(root, result.relativePath);
}

}
