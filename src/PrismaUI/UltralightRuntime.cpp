#include "UltralightRuntime.h"

#include "UltralightRuntimeManifest.g.h"
#include "Utils/ModulePath.h"

#include <bcrypt.h>
#include <delayimp.h>

#include <array>
#include <cctype>
#include <fstream>
#include <mutex>
#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    namespace {

        std::string Lower(std::string value) {
            for (auto& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return value;
        }

        constexpr std::array kUltralightRuntimeNames{
            L"UltralightCore.dll",
            L"WebCore.dll",
            L"Ultralight.dll",
            L"AppCore.dll",
        };

        constexpr std::array kUltralightRuntimeNamesA{
            "UltralightCore.dll",
            "WebCore.dll",
            "Ultralight.dll",
            "AppCore.dll",
        };

        std::string ToHexLower(const std::array<unsigned char, 32>& digest) {
            static constexpr char kHex[] = "0123456789abcdef";
            std::string out;
            out.reserve(digest.size() * 2);
            for (const unsigned char byte : digest) {
                out.push_back(kHex[byte >> 4]);
                out.push_back(kHex[byte & 0x0f]);
            }
            return out;
        }

        bool Sha256File(const std::filesystem::path& path, std::array<unsigned char, 32>& digest) {
            BCRYPT_ALG_HANDLE algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            if (::BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return false;
            const auto cleanupAlgorithm = [&] { ::BCryptCloseAlgorithmProvider(algorithm, 0); };
            DWORD objectBytes = 0;
            DWORD resultBytes = 0;
            if (::BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes), &resultBytes, 0) != 0 || !objectBytes) {
                cleanupAlgorithm();
                return false;
            }
            std::vector<unsigned char> object(objectBytes);
            if (::BCryptCreateHash(algorithm, &hash, object.data(), objectBytes, nullptr, 0, 0) != 0) {
                cleanupAlgorithm();
                return false;
            }
            const auto cleanupHash = [&] {
                ::BCryptDestroyHash(hash);
                cleanupAlgorithm();
            };
            std::ifstream stream(path, std::ios::binary);
            if (!stream) {
                cleanupHash();
                return false;
            }
            std::vector<char> buffer(1024 * 1024);
            while (stream) {
                stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const auto count = stream.gcount();
                if (count > 0 && ::BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(count), 0) != 0) {
                    cleanupHash();
                    return false;
                }
            }
            const bool success = ::BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) == 0;
            cleanupHash();
            return success;
        }

        bool ValidateRuntimePackage(const std::filesystem::path& packageRoot, std::string& reason) {
            std::error_code ec;
            for (const auto& directory : {packageRoot / L"libs", packageRoot / L"resources"}) {
                if (!std::filesystem::is_directory(directory, ec) || ec) {
                    reason = "required runtime directory is missing: " + directory.string();
                    return false;
                }
            }
            for (const auto& spec : kPinnedRuntimeFiles) {
                const auto path = packageRoot / std::filesystem::path(spec.relative);
                if (!std::filesystem::is_regular_file(path, ec) || ec) {
                    reason = "required runtime file is missing: " + path.string();
                    return false;
                }
                if (std::filesystem::file_size(path, ec) != spec.size || ec) {
                    reason = "runtime file size mismatch: " + path.string();
                    return false;
                }
                std::array<unsigned char, 32> digest{};
                if (!Sha256File(path, digest) || ToHexLower(digest) != spec.sha256) {
                    reason = "runtime file SHA-256 mismatch: " + path.string();
                    return false;
                }
            }
            const auto libs = packageRoot / L"libs";
            std::error_code iterEc;
            std::filesystem::directory_iterator libsIt(libs, iterEc);
            if (iterEc) {
                reason = "cannot enumerate runtime libs: " + libs.string();
                return false;
            }
            const std::filesystem::directory_iterator libsEnd;
            for (; libsIt != libsEnd; libsIt.increment(iterEc)) {
                if (iterEc) {
                    reason = "error enumerating runtime libs: " + libs.string();
                    return false;
                }
                const auto extension = Lower(libsIt->path().extension().string());
                if (extension != ".dll") continue;
                const auto name = libsIt->path().filename().wstring();
                bool listed = false;
                for (const auto* expected : kUltralightRuntimeNames) {
                    if (_wcsicmp(name.c_str(), expected) == 0) listed = true;
                }
                if (!listed) {
                    reason = "unlisted DLL in runtime libs: " + libsIt->path().string();
                    return false;
                }
            }
            return true;
        }

        struct UltralightRuntimeState {
            std::mutex mutex;
            std::filesystem::path root;
            std::array<HMODULE, kUltralightRuntimeNames.size()> modules{};
        };

        UltralightRuntimeState& RuntimeState() {
            static UltralightRuntimeState state;
            return state;
        }

        FARPROC WINAPI UltralightDelayLoadHook(unsigned notification, PDelayLoadInfo delayInfo) noexcept {
            if (notification != dliNotePreLoadLibrary || !delayInfo || !delayInfo->szDll) return nullptr;

            std::size_t index = kUltralightRuntimeNamesA.size();
            for (std::size_t i = 0; i < kUltralightRuntimeNamesA.size(); ++i) {
                if (_stricmp(delayInfo->szDll, kUltralightRuntimeNamesA[i]) == 0) {
                    index = i;
                    break;
                }
            }
            if (index == kUltralightRuntimeNamesA.size()) return nullptr;

            auto& state = RuntimeState();
            std::lock_guard lock(state.mutex);
            if (state.modules[index]) return reinterpret_cast<FARPROC>(state.modules[index]);
            const auto root = state.root.empty() ? UltralightPackageRoot() / L"libs" : state.root;
            const auto module = ::LoadLibraryExW(
                (root / kUltralightRuntimeNames[index]).c_str(), nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (module) {
                state.root = root;
                state.modules[index] = module;
                return reinterpret_cast<FARPROC>(module);
            }
            return nullptr;
        }

    }

    std::filesystem::path UltralightPackageRoot() {
        const auto moduleDir = PrismaUI::Utils::ThisModuleDir();
        return moduleDir.parent_path().parent_path() / L"PrismaUI_F4";
    }

    std::filesystem::path UltralightViewRoot() {
        return UltralightPackageRoot() / L"views";
    }

    UltralightRuntime::~UltralightRuntime() {
        auto& state = RuntimeState();
        std::lock_guard lock(state.mutex);
        state.root.clear();
        state.modules.fill(nullptr);
        modules_.clear();
    }

    bool UltralightRuntime::Load(std::string& reason) {
        if (!modules_.empty()) return true;

        const auto packageRoot = UltralightPackageRoot();
        if (!ValidateRuntimePackage(packageRoot, reason)) {
            logger::error("[WebRuntime] Ultralight runtime preflight failed: {}", reason);
            return false;
        }
        const auto root = packageRoot / L"libs";
        std::vector<HMODULE> loaded;
        bool complete = true;
        for (const auto* name : kUltralightRuntimeNames) {
            const auto path = root / name;
            const auto module = ::LoadLibraryExW(
                path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (!module) {
                logger::warn("[WebRuntime] failed to load Ultralight runtime {} (GLE={})",
                             path.string(), ::GetLastError());
                complete = false;
                break;
            }
            loaded.push_back(module);
        }
        if (complete) {
            auto& state = RuntimeState();
            {
                std::lock_guard lock(state.mutex);
                state.root = root;
                for (std::size_t i = 0; i < loaded.size(); ++i) state.modules[i] = loaded[i];
            }
            modules_ = std::move(loaded);
            logger::info("[WebRuntime] Ultralight runtime loaded from {}", root.string());
            return true;
        }
        for (auto it = loaded.rbegin(); it != loaded.rend(); ++it) {
            if (*it) ::FreeLibrary(*it);
        }

        reason = "pinned Ultralight runtime could not be loaded from " + root.string();
        logger::error("[WebRuntime] {}", reason);
        return false;
    }

    extern "C" const PfnDliHook __pfnDliNotifyHook2 = &UltralightDelayLoadHook;

}
