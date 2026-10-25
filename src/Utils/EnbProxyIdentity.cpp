#include "ConflictChecker.h"

#include "Utils/ModulePath.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace PrismaUI::ConflictChecker {

namespace {

HMODULE ModuleForAddress(void* address)
{
    if (!address) return nullptr;
    HMODULE module = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(address), &module))
        return nullptr;
    return module;
}

bool PathsEqualInsensitive(const std::filesystem::path& left, const std::filesystem::path& right)
{
    if (left.empty() || right.empty()) return false;
    const auto lhs = left.lexically_normal().wstring();
    const auto rhs = right.lexically_normal().wstring();
    return ::CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_EQUAL;
}

std::filesystem::path SystemD3D11Path()
{
    wchar_t directory[MAX_PATH] = {};
    constexpr UINT capacity = static_cast<UINT>(sizeof(directory) / sizeof(directory[0]));
    const UINT length = ::GetSystemDirectoryW(directory, capacity);
    if (!length || length >= capacity) return {};
    return std::filesystem::path(directory) / L"d3d11.dll";
}

std::wstring Lowercase(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), ::towlower);
    return value;
}

bool ContainsInsensitive(const std::wstring& value, const wchar_t* needle)
{
    return Lowercase(value).find(Lowercase(needle ? needle : L"")) != std::wstring::npos;
}

bool IsEnbVersionMetadata(const std::filesystem::path& path)
{
    const auto filename = path.wstring();
    DWORD ignored = 0;
    const DWORD size = ::GetFileVersionInfoSizeW(filename.c_str(), &ignored);
    if (!size) return false;
    std::vector<unsigned char> data(size);
    if (!::GetFileVersionInfoW(filename.c_str(), 0, size, data.data())) return false;

    struct Translation { WORD language; WORD codePage; };
    Translation* translations = nullptr;
    UINT translationBytes = 0;
    if (!::VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
                          reinterpret_cast<void**>(&translations), &translationBytes) ||
        !translations || translationBytes < sizeof(Translation))
        return false;

    const UINT count = translationBytes / sizeof(Translation);
    for (UINT i = 0; i < count; ++i) {
        wchar_t subBlock[96] = {};
        ::swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\FileDescription",
                     translations[i].language, translations[i].codePage);
        wchar_t* value = nullptr;
        UINT valueChars = 0;
        if (::VerQueryValueW(data.data(), subBlock, reinterpret_cast<void**>(&value), &valueChars) &&
            value && ContainsInsensitive(value, L"ENBSeries") && ContainsInsensitive(value, L"Fallout 4"))
            return true;

    }
    return false;
}

}

bool SameModuleIdentity(void* left, void* right)
{
    const auto leftModule = ModuleForAddress(left);
    const auto rightModule = ModuleForAddress(right);
    if (!leftModule || !rightModule) return false;
    if (leftModule == rightModule) return true;
    return PathsEqualInsensitive(PrismaUI::Utils::ModulePath(leftModule),
                                 PrismaUI::Utils::ModulePath(rightModule));
}

bool IsKnownEnbProxyModule(void* module)
{
    const auto hModule = reinterpret_cast<HMODULE>(module);
    if (!hModule) return false;
    const auto path = PrismaUI::Utils::ModulePath(hModule);
    if (path.empty() || ::CompareStringOrdinal(path.filename().c_str(), -1, L"d3d11.dll", -1, TRUE) != CSTR_EQUAL)
        return false;
    const auto systemPath = SystemD3D11Path();
    return !PathsEqualInsensitive(path, systemPath) && IsEnbVersionMetadata(path);
}

bool IsKnownEnbProxyAddress(void* address)
{
    return IsKnownEnbProxyModule(reinterpret_cast<void*>(ModuleForAddress(address)));
}

}
