#pragma once

#include <windows.h>

#include <filesystem>
#include <string>

#include "../Utils/ModulePath.h"
#include "UltralightDevToolsPolicy.h"

namespace PrismaUI::DevToolsConfig {

inline constexpr int kDefaultPort = UltralightDevToolsPolicy::kDefaultPort;

inline std::wstring IniPathIn(const std::filesystem::path& dir) {
    return (dir / L"PrismaUI_F4.ini").wstring();
}

inline bool Enabled(const std::wstring& iniPath) {
    return ::GetPrivateProfileIntW(L"DevTools", L"bEnabled", 0, iniPath.c_str()) != 0;
}

inline int Port(const std::wstring& iniPath) {
    const int requested = ::GetPrivateProfileIntW(L"DevTools", L"iPort", kDefaultPort, iniPath.c_str());
    return static_cast<int>(UltralightDevToolsPolicy::NormalizePort(requested));
}

inline std::filesystem::path ModuleDir(HMODULE mod) {
    return PrismaUI::Utils::ModuleDir(mod);
}

}
