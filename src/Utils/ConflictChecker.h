#pragma once

#include "PrismaUI_F4_API.h"
#include "Hooks/ReattachPolicy.h"

#include <string>

struct IDXGISwapChain;
namespace PrismaUI::Hooks {
    struct HookClassification;
    struct HookInstallPlan {
        HookInstallStrategy strategy;
        void* dispatch;
        void* present;
        void* resize;
        unsigned int shadowSlots;
    };
}

namespace PrismaUI::ConflictChecker {

    void CheckEarly();

    void CheckPreHooks();

    void LogSystemSummary();

    void OnAPIRequest(void* returnAddress, PRISMA_UI_API::InterfaceVersion version);

    std::string OwnerOf(void* address);

    bool IsKnownEnbProxyModule(void* module);
    bool IsKnownEnbProxyAddress(void* address);
    bool IsKnownFrameGenAddress(void* address);
    bool IsKnownBaseOnlyFrameGenAddress(void* address);
    bool SameModuleIdentity(void* left, void* right);

    bool AnyKnownFrameGenModuleLoaded();

    struct HookTargetDecision {
        bool readable = false;
        bool ownerAllowed = false;
        bool clean = false;
        bool safe = false;
        bool chainAllowed = false;
        bool alreadyOurs = false;
    };

    HookTargetDecision ClassifyHookTargetForInstall(void* target,
                                                    PrismaUI::Hooks::HookClassification* classification = nullptr,
                                                    void* selfDetour = nullptr,
                                                    void* alternateSelfDetour = nullptr);

    bool CanInstallD3DHooks(IDXGISwapChain* swapChain);
    PrismaUI::Hooks::HookInstallStrategy SelectHookInstallStrategy(IDXGISwapChain* swapChain);
    PrismaUI::Hooks::HookInstallPlan BuildHookInstallPlan(IDXGISwapChain* swapChain,
                                                           void* presentDetour, void* alternatePresentDetour,
                                                           void* resizeDetour, void* alternateResizeDetour);
    unsigned int DetermineShadowSlotCount(IDXGISwapChain* swapChain);

    enum class GetBufferOwnerClass { Dxgi, KnownFrameGen, Foreign };
    struct GetBufferOwnerSnapshot {
        void* getBufferFn = nullptr;
        GetBufferOwnerClass owner = GetBufferOwnerClass::Dxgi;
    };

    GetBufferOwnerSnapshot InspectGetBufferOwner(IDXGISwapChain* swapChain);

    void* CurrentGetBufferFn(IDXGISwapChain* swapChain);

}
