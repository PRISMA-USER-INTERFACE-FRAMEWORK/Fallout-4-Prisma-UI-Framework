#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <windows.h>

namespace PrismaUI::WebRuntimeUltralight {

    [[nodiscard]] std::filesystem::path UltralightPackageRoot();
    [[nodiscard]] std::filesystem::path UltralightViewRoot();

    class UltralightRuntime final {
    public:
        ~UltralightRuntime();

        bool Load(std::string& reason);

    private:
        std::vector<HMODULE> modules_;
    };

}
