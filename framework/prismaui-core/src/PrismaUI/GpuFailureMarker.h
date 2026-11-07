#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace PrismaUI::GpuFailureMarker {

    inline std::optional<std::string> ReadReason(const std::filesystem::path& marker, std::string_view version) {
        std::ifstream in(marker);
        std::string storedVersion;
        std::string reason;
        if (!std::getline(in, storedVersion) || storedVersion != version) return std::nullopt;
        if (!std::getline(in, reason)) return std::nullopt;
        return reason;
    }

    inline bool WriteReason(const std::filesystem::path& marker, std::string_view version, const char* reason) {
        std::ofstream out(marker);
        return static_cast<bool>(out << version << '\n' << reason << '\n' << std::flush);
    }

}
