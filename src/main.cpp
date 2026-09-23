#include "PCH.h"
#include "RuntimeSupportPolicy.h"
#include "API/API.h"
#include "API/FlatAPI.h"
#include "Hooks/Hooks.h"
#include "Utils/ConflictChecker.h"
#include "Utils/DllLoader.h"
#include "Utils/ModulePath.h"
#include "PrismaUI/ActorPreviewSpike.h"
#include "PrismaUI/FreezeDiagnostics.h"
#include "PrismaUI/WebRuntime.h"
#include "PrismaUI/WebInput.h"
#include "PrismaUI/GameThreadDispatcher.h"
#include "PrismaUI/PapyrusVM.h"
#include "PrismaUI/VanillaUISuppressor.h"
#include "PrismaUI/ControllerGlyphs.h"
#include "PrismaUI/DockBridge.h"
#include "Menus/PauseHold/PauseHold.h"
#include <tlhelp32.h>
#include <chrono>
#include <filesystem>
#include <thread>
#include <commctrl.h>
#include <shellapi.h>

static bool g_overlayDetected = false;
static std::string g_detectedOverlayName;
static std::atomic<uint64_t> g_gameLoadEpoch{ 0 };

namespace PrismaUI::ModelPreview {
    void SetGameLoadActive(bool active) noexcept;
}

namespace {
    static std::string GetIniPath() {
        return PrismaUI::Utils::PluginIniPath().string();
    }

    static bool ReadIniBool(const char* section, const char* key, bool defaultValue) {
        const std::string path = GetIniPath();
        const int def = defaultValue ? 1 : 0;
        const int val = GetPrivateProfileIntA(section, key, def, path.c_str());
        return val != 0;
    }
}

namespace {
    bool IsConflictingOverlayRunning() {
        const char* conflicting_procs[] = {
            "RTSS.exe",
            "RTSSHooked.exe",
            "RTSSHooksLoader64.exe",
            "MSIAfterburner.exe",
            "OverdriveNTool.exe",
        };

        const char* friendly_names[] = {
            "RivaTuner Statistics Server (RTSS.exe)",
            "RivaTuner (RTSSHooked.exe)",
            "RivaTuner Loader (RTSSHooksLoader64.exe) - close from system tray",
            "MSI Afterburner (MSIAfterburner.exe)",
            "AMD Overdrive (OverdriveNTool.exe)",
        };

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32 entry{};
        entry.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot, &entry)) {
            do {
                for (size_t i = 0; i < 5; ++i) {
                    if (_stricmp(entry.szExeFile, conflicting_procs[i]) == 0) {
                        logger::warn("[PrismaUI Overlay Detection] Found: {}", entry.szExeFile);
                        g_detectedOverlayName = friendly_names[i];
                        CloseHandle(snapshot);
                        return true;
                    }
                }
            } while (Process32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return false;
    }

    void NotifyUserOverlayDetected() {

        std::wstring overlayW(g_detectedOverlayName.begin(), g_detectedOverlayName.end());

        std::wstring content =
            L"Detected: " + overlayW + L"\n\n"
            L"This software hooks DirectX and can conflict with PrismaUI's rendering.\n"
            L"Click Continue to load anyway - PrismaUI will remember this choice.";

        TASKDIALOGCONFIG tdc      = {};
        tdc.cbSize                = sizeof(tdc);
        tdc.dwFlags               = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
        tdc.pszWindowTitle        = L"PrismaUI Framework";
        tdc.pszMainIcon           = TD_WARNING_ICON;
        tdc.pszMainInstruction    = L"GPU Overlay Software Detected";
        tdc.pszContent            = content.c_str();
        tdc.dwCommonButtons       = TDCBF_OK_BUTTON;
        tdc.nDefaultButton        = IDOK;
        tdc.pszFooter             = L"How to disable the overlay: youtube.com/watch?v=1NPqDMlYGz0";
        tdc.pszFooterIcon         = TD_INFORMATION_ICON;

        ACTCTXW actCtx        = {};
        actCtx.cbSize         = sizeof(actCtx);
        actCtx.dwFlags        = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
        actCtx.lpResourceName = MAKEINTRESOURCEW(2);
        actCtx.hModule        = PrismaUI::Utils::ThisModule();

        HANDLE    hCtx   = CreateActCtxW(&actCtx);
        ULONG_PTR cookie = 0;
        const bool activated = (hCtx != INVALID_HANDLE_VALUE) && ActivateActCtx(hCtx, &cookie);

        if (!activated)
            logger::warn("[PrismaUI] Could not activate comctl32 v6 context (hCtx={}, GLE={})",
                (void*)hCtx, GetLastError());

        using FnTaskDialogIndirect = HRESULT(WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);
        FnTaskDialogIndirect pfnTaskDialog = nullptr;
        HMODULE hComCtl = LoadLibraryW(L"comctl32.dll");
        if (hComCtl)
            pfnTaskDialog = reinterpret_cast<FnTaskDialogIndirect>(
                GetProcAddress(hComCtl, "TaskDialogIndirect"));

        logger::info("[PrismaUI] overlay dialog: activated={} pfn={}", activated, (void*)pfnTaskDialog);

        int     nButton = 0;
        HRESULT hr      = E_NOTIMPL;
        if (pfnTaskDialog)
            hr = pfnTaskDialog(&tdc, &nButton, nullptr, nullptr);

        logger::info("[PrismaUI] TaskDialogIndirect hr=0x{:08x} nButton={}", (uint32_t)hr, nButton);

        if (hComCtl) FreeLibrary(hComCtl);
        if (activated)                    DeactivateActCtx(0, cookie);
        if (hCtx != INVALID_HANDLE_VALUE) ReleaseActCtx(hCtx);

        if (FAILED(hr) || nButton == 0) {
            logger::warn("[PrismaUI] TaskDialogIndirect unavailable (hr=0x{:08x} nButton={}), using MessageBoxW", (uint32_t)hr, nButton);
            std::wstring fallback =
                L"GPU overlay software detected: " + overlayW + L"\n\n"
                L"This software hooks DirectX and can conflict with PrismaUI's rendering.\n\n"
                L"Click OK to continue loading anyway. Compatibility is not guaranteed -- you may\n"
                L"see visual artifacts; close the overlay if issues occur.\n\n"
                L"How to disable the RTSS overlay:\n"
                L"https://www.youtube.com/watch?v=1NPqDMlYGz0";
            MessageBoxW(nullptr, fallback.c_str(), L"PrismaUI - Overlay Detected", MB_OK | MB_ICONWARNING);
        }

        const std::string iniPath = GetIniPath();
        WritePrivateProfileStringA("Compatibility", "bAllowOverlays", "1", iniPath.c_str());
        logger::warn("[PrismaUI] bAllowOverlays=1 written to {} -- overlay warning suppressed on next "
                     "launch (this only silences the prompt; it does NOT enable a compatibility mode)", iniPath);
    }
}

class PrismaDockMenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event,
                                          RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {

        if (a_event.menuName == RE::PauseMenu::MENU_NAME &&
            !PrismaUI::VanillaUISuppressor::IsMenuSuppressed("PauseMenu")) {
            if (a_event.opening) {
                const auto focused = PrismaUI::WebRuntime::GetFocusedView();
                if (focused) PrismaUI::WebRuntime::Unfocus(focused);
                PrismaUI::DockBridge::Show();
                PrismaUI::WebRuntime::ShowDock();
            } else {
                PrismaUI::DockBridge::Hide();
                PrismaUI::WebRuntime::HideDock();
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    static PrismaDockMenuSink* GetSingleton()
    {
        static PrismaDockMenuSink singleton;
        return std::addressof(singleton);
    }
};

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* message)
{
    if (!message) return;

    if (message->type == F4SE::MessagingInterface::kPreLoadGame) {
        const auto epoch = g_gameLoadEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;
        PrismaUI::FreezeDiagnostics::SetGameLoadActive(true);
        PrismaUI::ModelPreview::SetGameLoadActive(true);
        logger::info("[LOAD] PreLoadGame epoch={} -- ModelPreview engine reads suspended", epoch);
    }

    if (message->type == F4SE::MessagingInterface::kGameDataReady) {
        PrismaUI::ConflictChecker::LogSystemSummary();
        PrismaUI::ConflictChecker::CheckPreHooks();
        PrismaUI::FreezeDiagnostics::StartWatchdog();
        Hooks::D3DHooks::Install();

        PrismaUI::WebRuntime::StartRecoveryWatchdog();
        PrismaUI::WebRuntime::TryInstallGameInput();

        logger::info("[PrismaUI] Bringing up Ultralight backend on worker thread");
        std::thread([] { PrismaUI::WebRuntime::LoadAndInit(); }).detach();

        if (auto* ui = RE::UI::GetSingleton()) {

            ui->RegisterMenu(PauseHold::MENU_NAME.data(), PauseHold::Creator);
            ui->RegisterSink(PrismaDockMenuSink::GetSingleton());
            logger::info("[PrismaUI] Registered PauseHold + pause-menu sink for the Prisma Dock");
        }

        PrismaUI::VanillaUISuppressor::Install();
        PrismaUI::ControllerGlyphs::InstallInputTracking();
    }

    if (message->type == F4SE::MessagingInterface::kPostLoadGame ||
        message->type == F4SE::MessagingInterface::kNewGame) {
        const auto epoch = g_gameLoadEpoch.load(std::memory_order_acquire);
        logger::info("[LOAD] {} epoch={} -- starting three-second grace",
                     message->type == F4SE::MessagingInterface::kPostLoadGame ? "PostLoadGame" : "NewGame", epoch);
        std::thread([epoch] {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            if (g_gameLoadEpoch.load(std::memory_order_acquire) != epoch) return;
            PrismaUI::FreezeDiagnostics::SetGameLoadActive(false);
            PrismaUI::ModelPreview::SetGameLoadActive(false);
            logger::info("[LOAD] grace complete epoch={} -- ModelPreview engine reads resumed", epoch);
        }).detach();

        PrismaUI::ActorPreviewSpike::Shutdown();

        std::thread([] {
            if (!PrismaUI::WebRuntime::WaitUntilActive(30000)) {
                logger::warn("[PrismaUI] Boot animation skipped -- Ultralight backend still not active after 30s");
                return;
            }
            std::this_thread::sleep_for(std::chrono::seconds(3));
            PrismaUI::WebRuntime::ShowBootAnimation();
        }).detach();
    }
}

#if !defined(PRISMA_VERSION_MAJOR) || !defined(PRISMA_VERSION_MINOR) || !defined(PRISMA_VERSION_PATCH)
#    error "PRISMA_VERSION_* not defined -- build through xmake.lua, which derives them from PRISMA_VERSION"
#endif

F4SE_PLUGIN_VERSION = []() noexcept {
    F4SE::PluginVersionData v{};

    v.PluginVersion({ PRISMA_VERSION_MAJOR, PRISMA_VERSION_MINOR, PRISMA_VERSION_PATCH, 0 });
    v.PluginName("PrismaUI_F4");
    v.AuthorName("PrismaUI");
    v.UsesAddressLibrary(true);
    v.UsesSigScanning(false);
    v.IsLayoutDependent(true);
    v.HasNoStructUse(false);
    return v;
}();

F4SE_PLUGIN_QUERY(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
    a_info->infoVersion = F4SE::PluginInfo::kVersion;
    a_info->name = "PrismaUI_F4";

    a_info->version = REL::Version(PRISMA_VERSION_MAJOR, PRISMA_VERSION_MINOR, PRISMA_VERSION_PATCH, 0).pack();

    if (a_f4se->IsEditor()) {
        return false;
    }
    const auto runtime = a_f4se->RuntimeVersion();
    return PrismaUI::RuntimeSupportPolicy::IsSupportedGameVersion(
        runtime.major(), runtime.minor(), runtime.patch());
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_intfc)
{
    F4SE::Init(a_intfc, F4SE::InitInfo{
        .logName        = "PrismaUI_F4",
        .trampoline     = true,
        .trampolineSize = 1 << 10
    });

    PrismaUI::GameThreadDispatcher::CaptureCurrentThread();

    g_overlayDetected = IsConflictingOverlayRunning();
    if (g_overlayDetected) {
        const bool allowOverlays = ReadIniBool("Compatibility", "bAllowOverlays", false);
        logger::warn("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger::warn("[PrismaUI] Overlay detected: {}", g_detectedOverlayName);
        if (allowOverlays) {
            logger::warn("[PrismaUI] bAllowOverlays=1 in PrismaUI_F4.ini — continuing anyway");
            logger::warn("[PrismaUI] Rendering artifacts or crashes may occur. Use at your own risk.");
            logger::warn("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        } else {
            logger::warn("[PrismaUI] Showing the one-time overlay warning; loading continues after acknowledgement.");
            logger::warn("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            NotifyUserOverlayDetected();
        }
    }

    const auto* messaging = F4SE::GetMessagingInterface();
    if (!messaging) {
        logger::critical("Failed to get F4SE messaging interface!");
        return false;
    }
    messaging->RegisterListener(F4SEMessageHandler);

    if (const auto* papyrus = F4SE::GetPapyrusInterface()) {
        papyrus->Register(PrismaUI::PapyrusVM::Register);
        logger::info("PrismaUI: Papyrus interface registered (PrismaUI script natives)");
    }

    logger::info("[PrismaUI] Ultralight backend");

    logger::info("PrismaUI_F4 Framework loaded successfully");
    return true;
}

extern "C" __declspec(dllexport) void* F4SEAPI RequestPluginAPI(const PRISMA_UI_API::InterfaceVersion a_interfaceVersion)
{
    auto api = PluginAPI::PrismaUIInterface::GetSingleton();

    switch (a_interfaceVersion) {
    case PRISMA_UI_API::InterfaceVersion::V1:
        logger::info("RequestPluginAPI returned V1 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI1*>(api);
    case PRISMA_UI_API::InterfaceVersion::V2:
        logger::info("RequestPluginAPI returned V2 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI2*>(api);
    case PRISMA_UI_API::InterfaceVersion::V3:
        logger::info("RequestPluginAPI returned V3 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI3*>(api);
    case PRISMA_UI_API::InterfaceVersion::V4:
        logger::info("RequestPluginAPI returned V4 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI4*>(api);
    case PRISMA_UI_API::InterfaceVersion::V5:
        logger::info("RequestPluginAPI returned V5 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI5*>(api);
    case PRISMA_UI_API::InterfaceVersion::V6:
        logger::info("RequestPluginAPI returned V6 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI6*>(api);
    case PRISMA_UI_API::InterfaceVersion::V7:
        logger::info("RequestPluginAPI returned V7 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI7*>(api);
    case PRISMA_UI_API::InterfaceVersion::V8:
        logger::info("RequestPluginAPI returned V8 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI8*>(api);
    case PRISMA_UI_API::InterfaceVersion::V9:
        logger::info("RequestPluginAPI returned V9 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI9*>(api);
    case PRISMA_UI_API::InterfaceVersion::V10:
        logger::info("RequestPluginAPI returned V10 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI10*>(api);
    case PRISMA_UI_API::PrismaUIInterfaceV11:
        logger::info("RequestPluginAPI returned V11 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI11*>(api);
    case PRISMA_UI_API::PrismaUIInterfaceV12:
        logger::info("RequestPluginAPI returned V12 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI12*>(api);
    default:
        logger::info("RequestPluginAPI: unsupported interface version");
        return nullptr;
	}
}

extern "C" __declspec(dllexport) uint64_t PrismaUI_F4_GetCapabilities()
{
    return static_cast<uint64_t>(PRISMA_UI_API::PrismaCapability::InputRegions);
}

extern "C" __declspec(dllexport) bool PrismaUI_F4_GetAPI(
    PRISMA_UI_FLAT_API::ApiFeature feature, uint32_t version, void* table, uint32_t tableSize) noexcept
{
    return PrismaUI::FlatAPI::Get(feature, version, table, tableSize);
}

extern "C" __declspec(dllexport) bool F4SEAPI PrismaUI_F4_RegisterTranslationsV4(
    PrismaView view, const char* pluginName)
{
    return PluginAPI::RegisterTranslationsV4(view, pluginName);
}
