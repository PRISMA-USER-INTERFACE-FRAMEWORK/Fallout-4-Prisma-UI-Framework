#pragma once

#include <filesystem>
#include <string>

namespace PrismaUI::Utils
{
    inline HMODULE ThisModule() noexcept
    {
        HMODULE h{};
        ::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ThisModule),
            &h);
        return h;
    }

    inline std::filesystem::path ModulePath(HMODULE module)
    {
        std::wstring buf(MAX_PATH, L'\0');
        for (;;) {
            const DWORD n = ::GetModuleFileNameW(module, buf.data(), static_cast<DWORD>(buf.size()));
            if (n == 0) return {};
            if (n < buf.size()) {
                buf.resize(n);
                return std::filesystem::path(buf);
            }
            buf.resize(buf.size() * 2);
        }
    }

    inline std::filesystem::path ModuleDir(HMODULE module)
    {
        return ModulePath(module).parent_path();
    }

    inline std::filesystem::path ThisModulePath()
    {
        return ModulePath(ThisModule());
    }

    inline std::filesystem::path ThisModuleDir()
    {
        return ModuleDir(ThisModule());
    }

    inline std::filesystem::path PluginIniPath()
    {
        return ThisModuleDir() / L"PrismaUI_F4.ini";
    }
}
