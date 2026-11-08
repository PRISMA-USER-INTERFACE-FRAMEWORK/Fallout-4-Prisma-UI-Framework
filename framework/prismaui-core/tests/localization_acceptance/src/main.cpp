#include "PCH.h"

#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4_L10N.h"
#include "PluginRuntimePolicy.h"

namespace logger = spdlog;
using namespace PRISMA_UI_API;

namespace {

    constexpr const char* kPluginName = "PrismaLocalizationAcceptance";
    constexpr const char* kViewPath = "Interface/PrismaLocalizationAcceptance/test.html";

    IVPrismaUI1* g_api = nullptr;
    PrismaView g_view = 0;
    bool g_registered = false;

    void ShowView(PrismaView view) {
        if (!g_api || !view || !g_api->IsValid(view)) return;
        g_api->SetOrder(view, 100);
        g_api->Show(view);
    }

    void PublishRegistration() {
        if (g_api && g_view) {
            g_api->Invoke(g_view, g_registered
                ? "window.PrismaLocalizationAcceptance={registration:true};"
                : "window.PrismaLocalizationAcceptance={registration:false};");
        }
    }

    void OnDomReady(PrismaView view) {
        logger::info("localization acceptance DOM ready: view={}", view);
        PublishRegistration();
        ShowView(view);
    }

    void EnsureAcceptanceView() {
        if (!g_api) g_api = RequestPluginAPI<IVPrismaUI1>();
        if (!g_api) {
            logger::warn("PrismaUI provider API unavailable");
            return;
        }
        if (!g_view) {
            g_view = g_api->CreateView(kViewPath, OnDomReady);
            if (!g_view) {
                logger::error("failed to create localization acceptance view");
                return;
            }
            g_api->Hide(g_view);
        }
        if (!g_registered) {
            g_registered = RegisterTranslationsV4(g_view, kPluginName);
            logger::info("RegisterTranslationsV4('{}') -> {}", kPluginName, g_registered);
        }
        PublishRegistration();
        ShowView(g_view);
    }

    void OnF4SEMessage(F4SE::MessagingInterface::Message* message) {
        if (!message) return;
        switch (message->type) {
        case F4SE::MessagingInterface::kGameDataReady:
        case F4SE::MessagingInterface::kPostLoadGame:
        case F4SE::MessagingInterface::kNewGame:
            EnsureAcceptanceView();
            break;
        default:
            break;
        }
    }

}

#if defined(PRISMA_LOCALIZATION_ACCEPTANCE_VR)
F4SE_EXPORT constinit auto F4SEPlugin_Version = []() noexcept {
    F4SE::PluginVersionData version{};
    version.PluginVersion({1, 0, 0, 0});
    version.PluginName(kPluginName);
    version.AuthorName("PrismaUI");
    version.UsesAddressLibrary(true);
    version.UsesSigScanning(false);
    version.IsLayoutDependent(false);
    version.HasNoStructUse(true);
    version.CompatibleVersions({F4SE::RUNTIME_VR_1_2_72});
    return version;
}();

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* f4se) {
    if (!f4se) return false;
    F4SE::Init(f4se);
    const auto* messaging = F4SE::GetMessagingInterface();
    return messaging && messaging->RegisterListener(OnF4SEMessage);
}
#else
extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* f4se,
                                                                F4SE::PluginInfo* info) {
    if (!f4se || !info || f4se->IsEditor()) return false;
    info->infoVersion = F4SE::PluginInfo::kVersion;
    info->name = kPluginName;
    info->version = 1;
    const auto runtime = f4se->RuntimeVersion();
    return PrismaUI::PluginRuntimePolicy::IsOgOrAeBuild(runtime.major(), runtime.minor(), runtime.patch());
}

extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* f4se) {
    if (!f4se) return false;
    const auto runtime = f4se->RuntimeVersion();
    if (!PrismaUI::PluginRuntimePolicy::IsOgOrAeBuild(runtime.major(), runtime.minor(), runtime.patch())) return false;
    F4SE::Init(f4se, F4SE::InitInfo{ .log = false });
    const auto* messaging = F4SE::GetMessagingInterface();
    if (!messaging) return false;
    messaging->RegisterListener(OnF4SEMessage);
    return true;
}
#endif
