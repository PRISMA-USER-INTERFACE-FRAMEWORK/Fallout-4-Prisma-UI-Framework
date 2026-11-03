#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace PrismaUI::Hooks {
    enum class DecodeStatus { Clean, InlineJmp, Truncated, IndirectUnreadable, BadDisplacement };

    struct HookClassification {
        DecodeStatus status = DecodeStatus::Clean;
        bool isInlineJmp = false;
        std::string ownerModule;
        std::string jmpTargetModule;
        uintptr_t jmpTarget = 0;
    };

    using MemoryReader = std::function<bool(uintptr_t addr, void* out, size_t n)>;
    using ModuleResolver = std::function<std::string(uintptr_t addr)>;

    HookClassification ClassifyHookTarget(uintptr_t codeAddress, const uint8_t* bytes, size_t n,
                                          const MemoryReader& readMem, const ModuleResolver& ownerOf);
}
