#include "ConflictChecker.h"

#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "Utils/ModulePath.h"

namespace PrismaUI::ConflictChecker {

    struct VerifiedOwnedProxyBuild {
        const wchar_t* fileName;
        std::uint32_t timeDateStamp;
        const char* label;
    };

    static constexpr VerifiedOwnedProxyBuild kVerifiedOwnedProxyBuilds[] = {
        {L"Upscaling.dll", 0x6AC8FD7B, "Upscaling 108772"},
    };

    static bool ReadModuleMemory(std::uintptr_t address, void* out, std::size_t size) {
        if (!address || !out || size == 0 || size > 4096) return false;
        __try {
            std::memcpy(out, reinterpret_cast<const void*>(address), size);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    static bool ModuleTimeDateStamp(HMODULE module, std::uint32_t* out) {
        const auto base = reinterpret_cast<std::uintptr_t>(module);
        IMAGE_DOS_HEADER dos{};
        if (!ReadModuleMemory(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE) return false;
        IMAGE_NT_HEADERS nt{};
        if (!ReadModuleMemory(base + static_cast<std::uintptr_t>(dos.e_lfanew), &nt, sizeof(nt))) return false;
        if (nt.Signature != IMAGE_NT_SIGNATURE) return false;
        *out = nt.FileHeader.TimeDateStamp;
        return true;
    }

    VerifiedOwnedProxyMatch MatchVerifiedOwnedProxy(void* present, void* getBuffer, void* resizeBuffers) {
        VerifiedOwnedProxyMatch match;
        if (!present || !getBuffer || !resizeBuffers) return match;
        if (!SameModuleIdentity(present, getBuffer) || !SameModuleIdentity(getBuffer, resizeBuffers)) {
            return match;
        }

        HMODULE owner = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(getBuffer), &owner) ||
            !owner) {
            return match;
        }

        std::uint32_t stamp = 0;
        if (!ModuleTimeDateStamp(owner, &stamp)) return match;

        const auto path = PrismaUI::Utils::ModulePath(owner);
        const std::wstring fileName = path.filename().wstring();
        for (const auto& build : kVerifiedOwnedProxyBuilds) {
            if (build.timeDateStamp != stamp) continue;
            if (::CompareStringOrdinal(fileName.c_str(), -1, build.fileName, -1, TRUE) != CSTR_EQUAL) continue;
            match.verified = true;
            match.label = build.label;
            return match;
        }
        return match;
    }

}
