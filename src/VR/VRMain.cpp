#include "PCH.h"

#include "API/API.h"
#include "API/FlatAPI.h"
#include "Menus/PauseHold/PauseHold.h"
#include "PrismaUI/FreezeDiagnostics.h"
#include "PrismaUI/WebRuntime.h"
#include "PrismaUI/PapyrusVM.h"
#include "VR/SceneDepthCapture.h"
#include "VR/VRCompositor.h"
#include "VR/VRHooks.h"
#include "VR/VRPointerSink.h"
#include "VR/VRViewState.h"

#include <atomic>
#include <thread>

namespace
{
    std::atomic<bool> g_runtimeInstalled = false;

    [[nodiscard]] bool IsExactSupportedRuntime() noexcept
    {
        return REL::Module::IsVR() &&
               REL::Module::get().version() == F4SE::RUNTIME_VR_1_2_72;
    }

    void F4SEAPI HandleMessage(F4SE::MessagingInterface::Message* message)
    {
        if (!message ||
            message->type != F4SE::MessagingInterface::kGameDataReady ||
            g_runtimeInstalled.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        try {
            PrismaUI::FreezeDiagnostics::StartWatchdog();
            if (auto* ui = RE::UI::GetSingleton()) {
                ui->RegisterMenu(PauseHold::MENU_NAME.data(), PauseHold::Creator);
            } else {
                logger::critical("PrismaUI could not register its pause-hold menu");
            }

            if (!Hooks::EngineVRHooks::Install()) {
                logger::critical(
                    "PrismaUI could not install the required stereo submission boundary");
                return;
            }
            if (!PrismaUI::SceneDepthCapture::Install()) {
                logger::warn("PrismaUI scene-depth occlusion is unavailable");
            }
            if (!Hooks::D3DHooks::Install()) {
                logger::warn("PrismaUI swap-chain resize invalidation is unavailable");
            }

            std::thread([] { PrismaUI::WebRuntime::LoadAndInit(); }).detach();
            logger::info("PrismaUI FO4VR runtime integration is ready");
        } catch (...) {
            logger::critical("PrismaUI runtime integration failed");
        }
    }
}

extern "C" __declspec(dllexport) uint64_t PrismaUI_F4_GetCapabilities()
{
    return static_cast<uint64_t>(PRISMA_UI_API::PrismaCapability::InputRegions);
}

extern "C" __declspec(dllexport) bool PrismaUI_F4_GetAPI(
    PRISMA_UI_FLAT_API::ApiFeature feature, uint32_t version, void* table, uint32_t tableSize) noexcept
{
    return PrismaUI::FlatAPI::GetVR(feature, version, table, tableSize);
}

F4SE_EXPORT constinit auto F4SEPlugin_Version = []() noexcept {
    F4SE::PluginVersionData version{};

    version.PluginVersion({PRISMA_VERSION_MAJOR, PRISMA_VERSION_MINOR, PRISMA_VERSION_PATCH, 0});
    version.PluginName("PrismaUI_F4");
    version.AuthorName("PrismaUI");
    version.UsesAddressLibrary(true);
    version.UsesSigScanning(false);
    version.IsLayoutDependent(true);
    version.HasNoStructUse(false);
    version.CompatibleVersions({F4SE::RUNTIME_VR_1_2_72});
    return version;
}();

F4SE_EXPORT bool F4SEAPI F4SEPlugin_Query(
    const F4SE::QueryInterface* f4se,
    F4SE::PluginInfo* info)
{
    if (!f4se || !info) {
        return false;
    }
    info->infoVersion = F4SE::PluginInfo::kVersion;
    info->name = "PrismaUI_F4";

    info->version = REL::Version(PRISMA_VERSION_MAJOR, PRISMA_VERSION_MINOR, PRISMA_VERSION_PATCH, 0).pack();
    return !f4se->IsEditor() && IsExactSupportedRuntime();
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* f4se)
{
    if (!f4se) {
        return false;
    }
    F4SE::Init(f4se);
    if (!IsExactSupportedRuntime()) {
        logger::critical("PrismaUI requires Fallout 4 VR 1.2.72");
        return false;
    }

    F4SE::AllocTrampoline(4096);

    const auto messaging = F4SE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(HandleMessage)) {
        logger::critical("PrismaUI could not register its F4SE message listener");
        return false;
    }

    if (const auto* papyrus = F4SE::GetPapyrusInterface()) {
        papyrus->Register(PrismaUI::PapyrusVM::Register);
    }

    logger::info("PrismaUI_F4 FO4VR provider loaded (Ultralight backend)");
    return true;
}

extern "C" __declspec(dllexport) void* F4SEAPI RequestPluginAPI(
    const PRISMA_UI_API::InterfaceVersion a_interfaceVersion)
{
    auto* api = PluginAPI::PrismaUIInterface::GetSingleton();

    switch (a_interfaceVersion) {
    case PRISMA_UI_API::InterfaceVersion::V1:
        return static_cast<PRISMA_UI_API::IVPrismaUI1*>(api);
    case PRISMA_UI_API::InterfaceVersion::V2:
        return static_cast<PRISMA_UI_API::IVPrismaUI2*>(api);
    case PRISMA_UI_API::InterfaceVersion::V3:
        return static_cast<PRISMA_UI_API::IVPrismaUI3*>(api);
    case PRISMA_UI_API::InterfaceVersion::V4:
        return static_cast<PRISMA_UI_API::IVPrismaUI4*>(api);
    case PRISMA_UI_API::InterfaceVersion::V5:
        return static_cast<PRISMA_UI_API::IVPrismaUI5*>(api);
    case PRISMA_UI_API::InterfaceVersion::V6:
        return static_cast<PRISMA_UI_API::IVPrismaUI6*>(api);
    case PRISMA_UI_API::InterfaceVersion::V7:
        return static_cast<PRISMA_UI_API::IVPrismaUI7*>(api);
    case PRISMA_UI_API::InterfaceVersion::V8:
        return static_cast<PRISMA_UI_API::IVPrismaUI8*>(api);
    case PRISMA_UI_API::InterfaceVersion::V9:
        return static_cast<PRISMA_UI_API::IVPrismaUI9*>(api);
    case PRISMA_UI_API::InterfaceVersion::V10:
        return static_cast<PRISMA_UI_API::IVPrismaUI10*>(api);
    default:
        logger::warn("RequestPluginAPI: unsupported interface version");
        return nullptr;
    }
}

extern "C" __declspec(dllexport) bool F4SEAPI PrismaUI_F4_RegisterTranslationsV4(
    PrismaView view, const char* pluginName)
{
    return PluginAPI::RegisterTranslationsV4(view, pluginName);
}
