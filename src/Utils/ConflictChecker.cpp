#include "ConflictChecker.h"
#include "PrismaUI_F4_API.h"

#include <Psapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <windows.h>
#include <wrl/client.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "Utils/ModulePath.h"
#include "Hooks/InlineHookClassifier.h"
#pragma comment(lib, "Psapi.lib")
#pragma intrinsic(_ReturnAddress)
namespace PrismaUI::ConflictChecker {

    constexpr unsigned int kMaxSwapchainSlots = 41;

    static std::wstring GetModuleBasenameW(HMODULE hMod) {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(hMod, path, MAX_PATH);
        std::wstring full(path);
        auto pos = full.rfind(L'\\');
        return (pos != std::wstring::npos) ? full.substr(pos + 1) : full;
    }

    static std::string WideToUtf8(const std::wstring& ws) {
        if (ws.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string out(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), len, nullptr, nullptr);
        return out;
    }

    static std::string WideToUtf8(const wchar_t* ws) {
        return ws ? WideToUtf8(std::wstring(ws)) : std::string{};
    }

    std::string OwnerOf(void* address) {
        HMODULE hOwner = nullptr;
        if (GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(address),
                &hOwner) && hOwner) {
            return WideToUtf8(GetModuleBasenameW(hOwner));
        }
        return "<unknown>";
    }

    void CheckEarly() {
        logger::info("[ConflictChecker] CheckEarly: scanning loaded modules");

        HANDLE hProcess = GetCurrentProcess();

        std::vector<HMODULE> modules(1024);
        DWORD cbNeeded = 0;
        bool enumOk = false;
        for (int attempt = 0; attempt < 4; ++attempt) {
            const DWORD bufBytes = static_cast<DWORD>(modules.size() * sizeof(HMODULE));
            if (!EnumProcessModules(hProcess, modules.data(), bufBytes, &cbNeeded)) {
                logger::warn("[ConflictChecker] CheckEarly: EnumProcessModules failed (error {})", GetLastError());
                break;
            }
            if (cbNeeded <= bufBytes) { enumOk = true; break; }
            modules.resize(cbNeeded / sizeof(HMODULE) + 16);
        }

        if (enumOk) {
            const DWORD count = std::min<DWORD>(cbNeeded / sizeof(HMODULE),
                                                static_cast<DWORD>(modules.size()));
            std::vector<std::wstring> matches;

            for (DWORD i = 0; i < count; ++i) {
                std::wstring basename = GetModuleBasenameW(modules[i]);
                std::wstring lower = basename;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
                if (lower == L"prismaui_f4.dll") {
                    wchar_t path[MAX_PATH] = {};
                    GetModuleFileNameW(modules[i], path, MAX_PATH);
                    matches.push_back(path);
                }
            }

            if (matches.size() == 1) {
                logger::info("[ConflictChecker] Module check OK: single instance at '{}'",
                             WideToUtf8(matches[0]));
            } else if (matches.size() > 1) {
                logger::critical("[ConflictChecker] DUPLICATE: PrismaUI_F4.dll loaded {} times:",
                                 matches.size());
                for (const auto& p : matches) {
                    logger::critical("[ConflictChecker]   -> '{}'", WideToUtf8(p));
                }
            } else {
                logger::warn("[ConflictChecker] CheckEarly: PrismaUI_F4.dll not found in module list (unexpected)");
            }
        }

        HMODULE hOurs = PrismaUI::Utils::ThisModule();
        if (hOurs) {
            void* exportFn = reinterpret_cast<void*>(GetProcAddress(hOurs, "RequestPluginAPI"));
            if (exportFn) {
                HMODULE hOwner = nullptr;
                GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(exportFn), &hOwner);

                if (hOwner && hOwner != hOurs) {
                    logger::critical(
                        "[ConflictChecker] CONFLICT: RequestPluginAPI export resolves to '{}' instead of our module"
                        " — another DLL is squatting on our name",
                        WideToUtf8(GetModuleBasenameW(hOwner)));
                } else {
                    logger::info("[ConflictChecker] Export check OK: RequestPluginAPI owned by PrismaUI_F4.dll");
                }
            } else {
                logger::warn("[ConflictChecker] CheckEarly: GetProcAddress(RequestPluginAPI) returned null");
            }
        } else {
            logger::warn("[ConflictChecker] CheckEarly: GetModuleHandleW(PrismaUI_F4.dll) returned null");
        }
    }

    static const wchar_t* kKnownConflictDlls[] = {
        L"ReShade.dll",
        L"ReShade64.dll",
        L"FallSouls.dll",
        L"FallSouls_NG.dll",
        L"enbseries.dll",
        L"dxvk.dll",
        L"d3d11_log.dll",
        L"RTSSHooks64.dll",   
    };

    static const wchar_t* kKnownFrameGenDlls[] = {
        L"AAAFrameGeneration.dll",   
        L"amd_fidelityfx_dx12.dll",
        L"nvngx_dlssg.dll",          
        L"dlssg_to_fsr3.dll",        
        L"sl.interposer.dll",        
        L"sl.dlss_g.dll",            
    };

    static bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) return false;
        }
        return true;
    }

    static bool IsFrameGenModule(void* address) {
        const std::string owner = OwnerOf(address);
        for (const auto* dllName : kKnownFrameGenDlls) {
            if (EqualsIgnoreCase(owner, WideToUtf8(dllName))) return true;
        }
        return false;
    }

    bool IsKnownFrameGenAddress(void* address) { return IsFrameGenModule(address); }

    bool IsKnownBaseOnlyFrameGenAddress(void* address) {
        return IsFrameGenModule(address) &&
               EqualsIgnoreCase(OwnerOf(address), "AAAFrameGeneration.dll");
    }

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

    PrismaUI::Hooks::HookInstallStrategy SelectHookInstallStrategy(IDXGISwapChain* swapChain) {
        return BuildHookInstallPlan(swapChain, nullptr, nullptr, nullptr, nullptr).strategy;
    }

    PrismaUI::Hooks::HookInstallPlan BuildHookInstallPlan(IDXGISwapChain* swapChain,
                                                           void* presentDetour, void* alternatePresentDetour,
                                                           void* resizeDetour, void* alternateResizeDetour) {
        PrismaUI::Hooks::HookInstallPlan plan{
            PrismaUI::Hooks::HookInstallStrategy::Reject, nullptr, nullptr, nullptr, 0};
        if (!swapChain) return plan;
        void** dispatch = *reinterpret_cast<void***>(swapChain);
        if (!dispatch) return plan;
        void* present = dispatch[8];
        void* resize = dispatch[13];
        const auto presentDecision = ClassifyHookTargetForInstall(
            present, nullptr, presentDetour, alternatePresentDetour);
        const auto resizeDecision = ClassifyHookTargetForInstall(
            resize, nullptr, resizeDetour, alternateResizeDetour);
        const bool presentKnownEnbProxy = IsKnownEnbProxyAddress(present);
        const bool resizeKnownEnbProxy = IsKnownEnbProxyAddress(resize);
        const bool sameEnbProxyModule = presentKnownEnbProxy && resizeKnownEnbProxy &&
                                        SameModuleIdentity(present, resize);
        const bool enbPairCompatible = PrismaUI::Hooks::IsChainEnbPairCompatible(
            presentKnownEnbProxy, resizeKnownEnbProxy, sameEnbProxyModule);
        if (*reinterpret_cast<void***>(swapChain) != dispatch || dispatch[8] != present ||
            dispatch[13] != resize) {
            logger::warn("[ConflictChecker] hook install policy: swapchain dispatch changed during observation; yielding coverage");
            return plan;
        }
        plan.dispatch = dispatch;
        plan.present = present;
        plan.resize = resize;
        const auto strategy = PrismaUI::Hooks::ChooseHookInstallStrategy(
            {presentDecision.readable, presentDecision.ownerAllowed, presentDecision.clean, presentDecision.chainAllowed},
            {resizeDecision.readable, resizeDecision.ownerAllowed, resizeDecision.clean, resizeDecision.chainAllowed});
        const char* strategyName = strategy == PrismaUI::Hooks::HookInstallStrategy::DirectMinHook
                                       ? "direct-minhook"
                                       : strategy == PrismaUI::Hooks::HookInstallStrategy::SwapchainChain
                                             ? "swapchain-chain"
                                             : "reject";
        logger::info("[ConflictChecker] hook plan: strategy={} Present owner='{}' readable={} direct={} chain={} "
                     "ResizeBuffers owner='{}' readable={} direct={} chain={}",
                     strategyName, OwnerOf(present), presentDecision.readable, presentDecision.safe,
                     presentDecision.chainAllowed, OwnerOf(resize), resizeDecision.readable,
                     resizeDecision.safe, resizeDecision.chainAllowed);
        if (strategy == PrismaUI::Hooks::HookInstallStrategy::DirectMinHook) {
            plan.strategy = strategy;
        } else if (strategy == PrismaUI::Hooks::HookInstallStrategy::SwapchainChain && enbPairCompatible) {
            plan.shadowSlots = DetermineShadowSlotCount(swapChain);
            if (plan.shadowSlots < 18 || plan.shadowSlots > kMaxSwapchainSlots) {
                logger::warn("[ConflictChecker] chain rejected: swapchain interface width unproven; yielding coverage");
                plan.strategy = PrismaUI::Hooks::HookInstallStrategy::Reject;
            } else {
                plan.strategy = strategy;
            }
        } else if (strategy == PrismaUI::Hooks::HookInstallStrategy::SwapchainChain) {
            logger::warn("[ConflictChecker] chain rejected: incompatible ENB proxy identities; yielding coverage");
        } else {
            logger::warn("[ConflictChecker] chain rejected: target not chainable; yielding coverage");
        }
        return plan;
    }

    bool CanInstallD3DHooks(IDXGISwapChain* swapChain) {
        return SelectHookInstallStrategy(swapChain) != PrismaUI::Hooks::HookInstallStrategy::Reject;
    }

    bool AnyKnownFrameGenModuleLoaded() {
        for (const auto* dllName : kKnownFrameGenDlls) {
            if (GetModuleHandleW(dllName)) return true;
        }
        return false;
    }

    GetBufferOwnerSnapshot InspectGetBufferOwner(IDXGISwapChain* swapChain) {
        GetBufferOwnerSnapshot s;
        if (!swapChain) return s;

        void** vtable = *reinterpret_cast<void***>(swapChain);
        void* present = vtable[8];
        s.getBufferFn = vtable[9];
        void* resizeBuffers = vtable[13];
        if (IsFrameGenModule(s.getBufferFn)) {
            s.owner = GetBufferOwnerClass::KnownFrameGen;
        } else if (EqualsIgnoreCase(OwnerOf(s.getBufferFn), "dxgi.dll")) {
            s.owner = GetBufferOwnerClass::Dxgi;
        } else {
            s.owner = GetBufferOwnerClass::Foreign;
        }
        const char* label = s.owner == GetBufferOwnerClass::KnownFrameGen
                                ? "known frame-gen proxy -> Borrowed (no Release)"
                            : s.owner == GetBufferOwnerClass::Dxgi
                                ? "dxgi.dll -> normal COM ownership (Owned)"
                                : "foreign/unresolved -> SafeSkip (no GetBuffer unless force-overridden)";
        logger::info("[ConflictChecker] swapchain vtable owners -- Present[8]='{}', GetBuffer[9]='{}', "
                     "ResizeBuffers[13]='{}' -- GetBuffer[9] {}",
                     OwnerOf(present), OwnerOf(s.getBufferFn), OwnerOf(resizeBuffers), label);
        return s;
    }

    void* CurrentGetBufferFn(IDXGISwapChain* swapChain) {
        if (!swapChain) return nullptr;
        return (*reinterpret_cast<void***>(swapChain))[9];
    }
    struct APIConsumer { std::string name; PRISMA_UI_API::InterfaceVersion version; };
    static std::vector<APIConsumer> s_apiConsumers;
    static std::mutex s_apiConsumersMutex;

    static std::string FormatInterfaceVersionLabel(PRISMA_UI_API::InterfaceVersion version) {
        using IV = PRISMA_UI_API::InterfaceVersion;
        char buf[48];
        switch (version) {
        case IV::V1: return "V1 (token 0x00)";
        case IV::V2: return "V2 (token 0x01)";
        case IV::V3: return "V3 (token 0x02)";
        case IV::V4: return "V4 (token 0x03)";
        case IV::V5: return "V5 (token 0x04)";
        case IV::V6: return "V6 (token 0x05)";
        case IV::V7: return "V7 (token 0x06)";
        case IV::V8: return "V8 (token 0x07)";
        case IV::V9: return "V9 (token 0x88)";
        case IV::V10: return "V10 (token 0x89)";
        case IV::V11: return "V11 (token 0x8A)";
        case IV::V12: return "V12 (token 0x8B)";
        default:
            std::snprintf(buf, sizeof(buf), "unknown (token 0x%02X)",
                          static_cast<unsigned>(static_cast<uint8_t>(version)));
            return buf;
        }
    }

    static void PrintAPIConsumerSummary() {
        std::lock_guard lock(s_apiConsumersMutex);
        if (s_apiConsumers.empty()) {
            logger::warn("[ConflictChecker] No Prisma plugins connected via RequestPluginAPI by kGameDataReady");
            return;
        }
        std::string summary;
        for (size_t i = 0; i < s_apiConsumers.size(); ++i) {
            if (i > 0) summary += ", ";
            summary += s_apiConsumers[i].name;
            summary += " (";
            summary += FormatInterfaceVersionLabel(s_apiConsumers[i].version);
            summary += ")";
        }
        logger::info("[ConflictChecker] Prisma API consumers ({}): {}", s_apiConsumers.size(), summary);
    }

    void CheckPreHooks() {
        logger::info("[ConflictChecker] CheckPreHooks: scanning for D3D hook conflicts");

        bool anyKnownConflict = false;
        for (const auto* dllName : kKnownConflictDlls) {
            if (GetModuleHandleW(dllName)) {
                logger::warn("[ConflictChecker] WARNING: '{}' is loaded — may conflict with D3D Present hook",
                             WideToUtf8(dllName));
                anyKnownConflict = true;
            }
        }
        if (!anyKnownConflict) {
            logger::info("[ConflictChecker] Known-DLL scan OK: no known conflicting modules detected");
        }

        for (const auto* dllName : kKnownFrameGenDlls) {
            if (GetModuleHandleW(dllName)) {
                logger::info("[ConflictChecker] Frame Generation detected: '{}' is loaded. It installs a "
                             "proxy swapchain and owns Present/ResizeBuffers; Prisma hooks the proxy "
                             "(supported since the #75 fix). Only active in borderless windowed.",
                             WideToUtf8(dllName));
            }
        }

        auto* rendererData = RE::BSGraphics::GetRendererData();
        if (!rendererData || !rendererData->renderWindow[0].swapChain) {
            logger::warn("[ConflictChecker] CheckPreHooks: cannot get IDXGISwapChain for vtable check"
                         " — skipping vtable integrity check");
        } else {
            auto* swapChain = rendererData->renderWindow[0].swapChain;
            void** vtable = *reinterpret_cast<void***>(swapChain);

            uintptr_t dxgiBase = 0, dxgiEnd = 0;
            HMODULE hDxgi = GetModuleHandleW(L"dxgi.dll");
            if (hDxgi) {
                MODULEINFO mi = {};
                if (GetModuleInformation(GetCurrentProcess(), hDxgi, &mi, sizeof(mi))) {
                    dxgiBase = reinterpret_cast<uintptr_t>(mi.lpBaseOfDll);
                    dxgiEnd  = dxgiBase + mi.SizeOfImage;
                }
            }

            auto checkVtableEntry = [&](int idx, const char* name) {
                uintptr_t addr = reinterpret_cast<uintptr_t>(vtable[idx]);
                const bool inDxgi = (dxgiBase != 0 && addr >= dxgiBase && addr < dxgiEnd);
                if (inDxgi) {

                    uint8_t prologue[16] = {};
                    auto reader = [](uintptr_t a, void* o, size_t n) -> bool {
                        if (n == 0 || n > 4096) return false;
                        __try { std::memcpy(o, reinterpret_cast<const void*>(a), n); return true; }
                        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
                    };
                    auto resolver = [](uintptr_t a) -> std::string { return OwnerOf(reinterpret_cast<void*>(a)); };
                    bool inlineDetour = false;
                    if (reader(addr, prologue, sizeof prologue)) {
                        auto hc = PrismaUI::Hooks::ClassifyHookTarget(addr, prologue, sizeof prologue, reader, resolver);
                        if (hc.isInlineJmp) {
                            inlineDetour = true;
                            logger::error(
                                "[ConflictChecker] CONFLICT: vtable[{}] ({}) points inside dxgi.dll but its prologue"
                                " jumps into '{}' — inline overlay detour (e.g. RTSS); rendering may be unstable",
                                idx, name, hc.jmpTargetModule.empty() ? "unknown-module" : hc.jmpTargetModule.c_str());
                        }
                    }
                    if (!inlineDetour)
                        logger::info("[ConflictChecker] vtable[{}] ({}) OK — inside dxgi.dll, prologue intact", idx, name);
                } else if (IsFrameGenModule(vtable[idx])) {
                    logger::info(
                        "[ConflictChecker] vtable[{}] ({}) owned by frame-gen proxy '{}' at 0x{:016X}"
                        " — expected; Prisma hooks the proxy swapchain",
                        idx, name, OwnerOf(vtable[idx]), addr);
                } else {
                    const std::string owner = OwnerOf(vtable[idx]);
                    logger::error(
                        "[ConflictChecker] CONFLICT: vtable[{}] ({}) already hooked by '{}' at 0x{:016X}"
                        " — rendering may be unstable",
                        idx, name, owner, addr);
                }
            };

            checkVtableEntry(8,  "Present");
            checkVtableEntry(13, "ResizeBuffers");
        }

        PrintAPIConsumerSummary();
    }

    void OnAPIRequest(void* returnAddress, PRISMA_UI_API::InterfaceVersion version) {

        std::string callerName = "<unknown>";
        HMODULE hCaller = nullptr;
        if (GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(returnAddress),
                &hCaller) && hCaller) {
            callerName = WideToUtf8(GetModuleBasenameW(hCaller));
        }

        const std::string versionLabel = FormatInterfaceVersionLabel(version);
        logger::info("[ConflictChecker] API request: caller='{}' requested {}", callerName, versionLabel);

        bool supported = false;
        switch (version) {
        case PRISMA_UI_API::InterfaceVersion::V1:
        case PRISMA_UI_API::InterfaceVersion::V2:
        case PRISMA_UI_API::InterfaceVersion::V3:
        case PRISMA_UI_API::InterfaceVersion::V4:
        case PRISMA_UI_API::InterfaceVersion::V5:
        case PRISMA_UI_API::InterfaceVersion::V6:
        case PRISMA_UI_API::InterfaceVersion::V7:
        case PRISMA_UI_API::InterfaceVersion::V8:
        case PRISMA_UI_API::InterfaceVersion::V9:
        case PRISMA_UI_API::InterfaceVersion::V10:
        case PRISMA_UI_API::PrismaUIInterfaceV11:
        case PRISMA_UI_API::PrismaUIInterfaceV12:
            supported = true;
            break;
        default:
            break;
        }

        if (!supported) {
            logger::error(
                "[ConflictChecker] CONFLICT: '{}' requested unsupported interface version {}"
                " — plugin likely built against a different PrismaUI_F4",
                callerName, versionLabel);
        }

        {
            std::lock_guard lock(s_apiConsumersMutex);
            for (const auto& c : s_apiConsumers) {
                if (c.name == callerName) return;
            }
            s_apiConsumers.push_back({callerName, version});
        }
    }

}
