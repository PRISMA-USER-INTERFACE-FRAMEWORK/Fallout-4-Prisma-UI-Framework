#include "Hooks/InlineHookClassifier.h"

#include <cstring>
#include <limits>

namespace PrismaUI::Hooks {
    namespace {
        bool ReadI32(const uint8_t* p, size_t n, size_t off, int32_t& out) {
            if (!p || off > n || n - off < sizeof(out)) return false;
            std::memcpy(&out, p + off, sizeof(out));
            return true;
        }

        bool IsCanonicalAddress(uintptr_t address) {
#if UINTPTR_MAX > 0xFFFFFFFFu
            constexpr uintptr_t lowMask = (uintptr_t{1} << 48) - 1;
            const uintptr_t upper = address & ~lowMask;
            const bool sign = (address & (uintptr_t{1} << 47)) != 0;
            return sign ? upper == ~lowMask : upper == 0;
#else
            (void)address;
            return true;
#endif
        }

        bool AddRelative(uintptr_t codeAddress, uintptr_t instructionLength, int32_t displacement,
                         uintptr_t& out) {
            constexpr uintptr_t max = (std::numeric_limits<uintptr_t>::max)();
            if (codeAddress > max - instructionLength) return false;
            const uintptr_t base = codeAddress + instructionLength;
            if (displacement >= 0) {
                const uintptr_t delta = static_cast<uintptr_t>(displacement);
                if (base > max - delta) return false;
                out = base + delta;
            } else {
                const uintptr_t delta = static_cast<uintptr_t>(-static_cast<int64_t>(displacement));
                if (base < delta) return false;
                out = base - delta;
            }
            return IsCanonicalAddress(out);
        }
    }

    HookClassification ClassifyHookTarget(uintptr_t codeAddress, const uint8_t* bytes, size_t n,
                                          const MemoryReader& readMem, const ModuleResolver& ownerOf) {
        HookClassification c;
        c.ownerModule = ownerOf ? ownerOf(codeAddress) : std::string{};
        if (!bytes || n == 0) {
            c.status = DecodeStatus::Truncated;
            return c;
        }

        if (bytes[0] == 0xE9) {
            int32_t rel = 0;
            if (!ReadI32(bytes, n, 1, rel)) {
                c.status = DecodeStatus::Truncated;
                return c;
            }
            if (!AddRelative(codeAddress, 5, rel, c.jmpTarget)) {
                c.status = DecodeStatus::BadDisplacement;
                return c;
            }
            c.status = DecodeStatus::InlineJmp;
            c.isInlineJmp = true;
            c.jmpTargetModule = ownerOf ? ownerOf(c.jmpTarget) : std::string{};
            return c;
        }

        if (bytes[0] == 0xEB) {
            if (n < 2) {
                c.status = DecodeStatus::Truncated;
                return c;
            }
            const auto displacement = static_cast<int8_t>(bytes[1]);
            if (!AddRelative(codeAddress, 2, static_cast<int32_t>(displacement), c.jmpTarget)) {
                c.status = DecodeStatus::BadDisplacement;
                return c;
            }
            c.status = DecodeStatus::InlineJmp;
            c.isInlineJmp = true;
            c.jmpTargetModule = ownerOf ? ownerOf(c.jmpTarget) : std::string{};
            return c;
        }

        if (n >= 2 && bytes[0] == 0xFF && bytes[1] == 0x25) {
            int32_t disp = 0;
            if (!ReadI32(bytes, n, 2, disp)) {
                c.status = DecodeStatus::Truncated;
                return c;
            }
            uintptr_t slot = 0;
            if (!AddRelative(codeAddress, 6, disp, slot)) {
                c.status = DecodeStatus::BadDisplacement;
                return c;
            }
            uint64_t dest = 0;
            if (!readMem || !readMem(slot, &dest, sizeof(dest))) {
                c.status = DecodeStatus::IndirectUnreadable;
                return c;
            }
            c.jmpTarget = static_cast<uintptr_t>(dest);
            if (!IsCanonicalAddress(c.jmpTarget)) {
                c.jmpTarget = 0;
                c.status = DecodeStatus::BadDisplacement;
                return c;
            }
            c.status = DecodeStatus::InlineJmp;
            c.isInlineJmp = true;
            c.jmpTargetModule = ownerOf ? ownerOf(c.jmpTarget) : std::string{};
            return c;
        }

        if (n >= 12 && bytes[0] == 0x48 && bytes[1] == 0xB8 && bytes[10] == 0xFF && bytes[11] == 0xE0) {
            uint64_t imm = 0;
            std::memcpy(&imm, bytes + 2, sizeof(imm));
            c.jmpTarget = static_cast<uintptr_t>(imm);
            if (!IsCanonicalAddress(c.jmpTarget)) {
                c.jmpTarget = 0;
                c.status = DecodeStatus::BadDisplacement;
                return c;
            }
            c.status = DecodeStatus::InlineJmp;
            c.isInlineJmp = true;
            c.jmpTargetModule = ownerOf ? ownerOf(c.jmpTarget) : std::string{};
            return c;
        }

        c.status = DecodeStatus::Clean;
        return c;
    }
}
