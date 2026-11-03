#include "ConflictChecker.h"

#include "Hooks/InlineHookClassifier.h"

#include <windows.h>
#include <cstdint>
#include <cstring>

namespace PrismaUI::ConflictChecker {

    static bool ReadHookMemory(uintptr_t address, void* out, size_t size) {
        if (!address || !out || size == 0 || size > 4096) return false;
        __try {
            std::memcpy(out, reinterpret_cast<const void*>(address), size);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    HookTargetDecision ClassifyHookTargetForInstall(
        void* target, PrismaUI::Hooks::HookClassification* outClassification,
        void* selfDetour, void* alternateSelfDetour) {
        HookTargetDecision decision;
        if (!target) return decision;
        uint8_t prologue[16]{};
        if (!ReadHookMemory(reinterpret_cast<uintptr_t>(target), prologue, sizeof(prologue))) return decision;
        decision.readable = true;
        const std::string owner = OwnerOf(target);
        decision.ownerAllowed = EqualsIgnoreCase(owner, "dxgi.dll") || IsFrameGenModule(target);
        const auto classification = PrismaUI::Hooks::ClassifyHookTarget(
            reinterpret_cast<uintptr_t>(target), prologue, sizeof(prologue), ReadHookMemory,
            [](uintptr_t address) { return OwnerOf(reinterpret_cast<void*>(address)); });
        if (outClassification) *outClassification = classification;
        const auto targetAddress = reinterpret_cast<uintptr_t>(target);
        const auto selfAddress = reinterpret_cast<uintptr_t>(selfDetour);
        const auto alternateAddress = reinterpret_cast<uintptr_t>(alternateSelfDetour);
        decision.alreadyOurs = (selfDetour &&
                                (targetAddress == selfAddress || classification.jmpTarget == selfAddress)) ||
                               (alternateSelfDetour &&
                                (targetAddress == alternateAddress ||
                                 classification.jmpTarget == alternateAddress));
        decision.clean = classification.status == PrismaUI::Hooks::DecodeStatus::Clean;
        decision.safe = decision.ownerAllowed && decision.clean;
        decision.chainAllowed =
            decision.readable && !decision.alreadyOurs &&
            (classification.status == PrismaUI::Hooks::DecodeStatus::Clean ||
             (classification.status == PrismaUI::Hooks::DecodeStatus::InlineJmp &&
              classification.isInlineJmp && classification.jmpTarget != 0));
        return decision;
    }

}
