#include "PCH.h"

#include <cstring>

namespace {

constexpr const char* kViewPath = "Interface/PrismaControllerTest_F4/controller_test.html";
constexpr float kOpenHoldSeconds = 0.65f;

PRISMA_UI_API::IVPrismaUI11* g_api = nullptr;
PRISMA_UI_API::IVPrismaUI12* g_api12 = nullptr;
PRISMA_UI_FLAT_API::ControllerAPI g_controller{};
PrismaView g_view = 0;
std::string g_runtime;
std::atomic<bool> g_domReady{ false };
std::atomic<bool> g_open{ false };
std::atomic<bool> g_nativeGamepad{ false };
std::atomic<bool> g_openRequested{ false };
std::atomic<bool> g_recreateQueued{ false };

void OnDomReady(PrismaView view) noexcept;

const char* BridgeStateName(PRISMA_UI_API::ControllerActionBridgeState state) noexcept {
    switch (state) {
    case PRISMA_UI_API::ControllerActionBridgeState::Missing: return "missing";
    case PRISMA_UI_API::ControllerActionBridgeState::Pending: return "pending";
    case PRISMA_UI_API::ControllerActionBridgeState::Ready: return "ready";
    case PRISMA_UI_API::ControllerActionBridgeState::Failed: return "failed";
    }
    return "unknown";
}

bool HasNativeGamepad() noexcept {
    constexpr auto end = offsetof(PRISMA_UI_FLAT_API::ControllerAPI, SetNativeGamepad) +
                         sizeof(PRISMA_UI_FLAT_API::ControllerSetNativeGamepad);
    return g_controller.structSize >= end && g_controller.SetNativeGamepad != nullptr;
}

PrismaView FocusedView() noexcept {
    return g_api ? g_api->GetFocusedView() : 0;
}

void LogState(const char* label) noexcept {
    if (!g_api || !g_view || !g_api->IsValid(g_view)) return;
    auto* api8 = static_cast<PRISMA_UI_API::IVPrismaUI8*>(g_api);
    const auto bridge = g_controller.GetControllerActionBridgeState
                            ? static_cast<PRISMA_UI_API::ControllerActionBridgeState>(
                                  g_controller.GetControllerActionBridgeState(g_view))
                            : PRISMA_UI_API::ControllerActionBridgeState::Missing;
    const int style = g_api->GetControllerStyle();
    const bool usingGamepad = g_api->IsUsingGamepad();
    logger::info("[ControllerProbe] {} view={} valid={} focused={} focusedView={} health={} bridge={} device=gamepad:{} style={} native={}",
                 label, g_view, g_api->IsValid(g_view), g_api->HasFocus(g_view), FocusedView(),
                 static_cast<int>(api8->GetViewHealth(g_view)), BridgeStateName(bridge), usingGamepad,
                 style, g_nativeGamepad.load());
}

void QueueOpen() noexcept {
    if (g_openRequested.exchange(true, std::memory_order_acq_rel)) return;
    if (auto* task = F4SE::GetTaskInterface()) {
        task->AddTask([] {
            if (!g_api) return;
            if (!g_view || !g_api->IsValid(g_view)) {
                g_view = g_api->CreateView(kViewPath, OnDomReady);
                logger::info("[ControllerProbe] CreateView returned {}", g_view);
            }
            if (!g_view) {
                g_openRequested.store(false, std::memory_order_release);
                return;
            }
            if (!g_api->IsValid(g_view) || !g_domReady.load()) return;
            g_openRequested.store(false, std::memory_order_release);
            g_api->Show(g_view);
            const bool focusAccepted = g_api->Focus(g_view, false, false);
            const bool focusActual = g_api->HasFocus(g_view);
            g_open.store(true, std::memory_order_release);
            g_api->InteropCall(g_view, "setFocusState", focusActual ? "true" : "false");
            logger::info("[ControllerProbe] panel opened focusAccepted={} focusActual={} focusedView={}",
                         focusAccepted, focusActual, FocusedView());
            if (!focusActual)
                logger::warn("[ControllerProbe] panel visible but focus failed; Back hold retry remains enabled");
            LogState("open");
        });
    } else {
        g_openRequested.store(false, std::memory_order_release);
    }
}

bool IsBackButton(const RE::ButtonEvent* button) noexcept {
    if (!g_api || !button) return false;
    char name[16]{};
    return g_api->GetGamepadButtonName(static_cast<std::uint32_t>(button->GetBSButtonCode()), name, sizeof(name)) &&
           std::strcmp(name, "Back") == 0;
}

void ObserveButton(const RE::InputEvent* event) noexcept {
    if (!event || event->eventType != RE::INPUT_EVENT_TYPE::kButton ||
        event->device.get() != RE::INPUT_DEVICE::kGamepad) return;

    const auto* button = static_cast<const RE::ButtonEvent*>(event);
    if (!IsBackButton(button)) return;
    if (button->value == 0.0f || button->heldDownSecs < kOpenHoldSeconds) return;
    if (g_open.load(std::memory_order_acquire)) {
        const bool actual = g_api && g_view && g_api->IsValid(g_view) && g_api->HasFocus(g_view);
        if (actual) return;
    }
    if (FocusedView() != 0) {
        logger::info("[ControllerProbe] open gesture ignored because another view is focused");
        return;
    }
    logger::info("[ControllerProbe] Back hold accepted; queueing probe view");
    QueueOpen();
}

class MenuSink final : public RE::BSInputEventUser {
public:
    bool ShouldHandleEvent(const RE::InputEvent* event) override {
        ObserveButton(event);
        return false;
    }
};

class GameplaySink final : public RE::PlayerInputHandler {
public:
    explicit GameplaySink(RE::PlayerControlsData& data) : RE::PlayerInputHandler(data) {}

    bool ShouldHandleEvent(const RE::InputEvent* event) override {
        ObserveButton(event);
        return false;
    }
};

MenuSink g_menuSink;
GameplaySink* g_gameplaySink = nullptr;
RE::PlayerControls* g_gameplayOwner = nullptr;

template <class Controls, class Handler>
bool HasHandler(Controls* controls, Handler* handler) noexcept {
    if (!controls) return false;
    for (std::uint32_t i = 0; i < controls->handlers.size(); ++i)
        if (controls->handlers[i] == handler) return true;
    return false;
}

void InstallInputSinks() noexcept {
    bool menuPresent = false;
    if (auto* menu = RE::MenuControls::GetSingleton()) {
        menuPresent = HasHandler(menu, &g_menuSink);
        if (!menuPresent) {
            menu->RegisterHandler(&g_menuSink);
            menuPresent = HasHandler(menu, &g_menuSink);
        }
    }
    bool gameplayPresent = false;
    if (auto* player = RE::PlayerControls::GetSingleton()) {
        if (!g_gameplaySink || g_gameplayOwner != player) {
            g_gameplaySink = new GameplaySink(player->data);
            g_gameplayOwner = player;
        }
        gameplayPresent = HasHandler(player, g_gameplaySink);
        if (!gameplayPresent) {
            player->RegisterHandler(g_gameplaySink);
            gameplayPresent = HasHandler(player, g_gameplaySink);
        }
    }
    logger::info("[ControllerProbe] input sinks menu={} gameplay={}",
                 menuPresent, gameplayPresent);
}

void EnterFocus(void*) noexcept {
    if (!g_api || !g_view || !g_api->IsValid(g_view)) return;
    const bool accepted = g_api->Focus(g_view, false, false);
    const bool actual = g_api->HasFocus(g_view);
    g_api->InteropCall(g_view, "setFocusState", actual ? "true" : "false");
    logger::info("[ControllerProbe] focus entry accepted={} actual={} focusedView={}",
                 accepted, actual, FocusedView());
    LogState("after focus entry");
}

void ClosePanel() noexcept {
    if (!g_api || !g_view || !g_api->IsValid(g_view)) return;
    if (g_nativeGamepad.exchange(false) && HasNativeGamepad() && g_api->HasFocus(g_view))
        logger::info("[ControllerProbe] native gamepad disable result={}", g_controller.SetNativeGamepad(g_view, false));
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
    g_api->InteropCall(g_view, "setFocusState", "false");
    g_open.store(false, std::memory_order_release);
    logger::info("[ControllerProbe] panel closed");
}

void ReloadView() noexcept {
    if (!g_api || !g_view || !g_api->IsValid(g_view)) return;
    g_api->Invoke(g_view, "location.reload()", nullptr);
    logger::info("[ControllerProbe] document reload requested");
}

void RecreateView(const char*) noexcept {
    if (g_recreateQueued.exchange(true)) return;
    if (g_api && g_view && g_api->IsValid(g_view)) {
        if (g_nativeGamepad.exchange(false) && HasNativeGamepad())
            logger::info("[ControllerProbe] native gamepad disable for recreate result={}", g_controller.SetNativeGamepad(g_view, false));
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
        g_api->Destroy(g_view);
    }
    g_view = 0;
    g_domReady.store(false, std::memory_order_release);
    g_open.store(false, std::memory_order_release);
    logger::info("[ControllerProbe] recreate destroyed old view");
    if (auto* task = F4SE::GetTaskInterface()) {
        task->AddTask([] {
            g_recreateQueued.store(false, std::memory_order_release);
            if (g_api) g_view = g_api->CreateView(kViewPath, OnDomReady);
        });
    }
}

void ToggleNative(const char*) noexcept {
    if (!g_api || !g_view || !g_api->IsValid(g_view) || !HasNativeGamepad()) {
        logger::warn("[ControllerProbe] native gamepad unavailable");
        return;
    }
    if (!g_api->HasFocus(g_view)) {
        logger::warn("[ControllerProbe] native gamepad requires probe focus");
        return;
    }
    const bool requested = !g_nativeGamepad.load(std::memory_order_acquire);
    const bool result = g_controller.SetNativeGamepad(g_view, requested);
    if (result) g_nativeGamepad.store(requested, std::memory_order_release);
    const bool actual = g_nativeGamepad.load(std::memory_order_acquire);
    logger::info("[ControllerProbe] native gamepad requested={} result={} actual={}", requested, result, actual);
    g_api->InteropCall(g_view, "setNativeState", actual ? "true" : "false");
    LogState("after native toggle");
}

void BindAction(const char* button, const char* action) noexcept {
    if (g_controller.BindControllerAction && g_controller.BindControllerAction(g_view, button, action)) {
        logger::info("[ControllerProbe] action bind button={} action={} ok=true path=flat", button, action);
        return;
    }
    const bool viaV12 = g_api12 && g_api12->BindControllerAction(g_view, button, action);
    logger::info("[ControllerProbe] action bind button={} action={} ok={} path={}", button, action, viaV12, viaV12 ? "v12" : "none");
}

void UnbindAction(const char* button) noexcept {
    if (g_controller.UnbindControllerAction && g_controller.UnbindControllerAction(g_view, button)) {
        logger::info("[ControllerProbe] action unbind button={} ok=true path=flat", button);
        return;
    }
    const bool viaV12 = g_api12 && g_api12->UnbindControllerAction(g_view, button);
    logger::info("[ControllerProbe] action unbind button={} ok={} path={}", button, viaV12, viaV12 ? "v12" : "none");
}

void SetKeyboardBindings(const char* state) noexcept {
    if (!g_api || !g_view || !g_api->IsValid(g_view)) return;
    const bool on = state && std::strcmp(state, "on") == 0;
    for (const auto& mapping : {
             std::pair{"A", "probe.osk.A"}, {"B", "probe.osk.B"}, {"DUp", "probe.osk.DUp"},
             {"DDown", "probe.osk.DDown"}, {"DLeft", "probe.osk.DLeft"}, {"DRight", "probe.osk.DRight"}}) {
        if (on) BindAction(mapping.first, mapping.second);
        else UnbindAction(mapping.first);
    }
    logger::info("[ControllerProbe] keyboard bindings {}", on ? "on" : "off");
    LogState("after keyboard bindings");
}

void PushGlyphState(PrismaView view) noexcept {
    if (!g_api || !view || !g_api->IsValid(view)) return;
    const bool gamepad = g_api->IsUsingGamepad();
    const int style = g_api->GetControllerStyle();
    char prompt[32]{};
    const bool promptOk = g_api->GetButtonPrompt("Activate", prompt, sizeof(prompt));
    logger::info("[ControllerProbe] glyph state gamepad={} style={} activatePromptOk={} activatePrompt={} path=v9",
                 gamepad, style, promptOk, promptOk ? prompt : "");
    const std::string state = "{\"gamepad\":" + std::string(gamepad ? "true" : "false") +
                              ",\"style\":" + std::to_string(style) + "}";
    g_api->InteropCall(view, "setControllerState", state.c_str());
}

void OnDomReady(PrismaView view) noexcept {
    g_view = view;
    g_domReady.store(true, std::memory_order_release);
    g_api->SetViewRole(view, PRISMA_UI_API::ViewRole::kPanel);
    g_api->SetViewOwnsEscape(view, true);
    g_api->RegisterConsoleCallback(view, [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel, const char* message) {
        logger::info("[ControllerProbe] browser {}", message ? message : "");
    });
    for (const auto& mapping : {
             std::pair{"X", "probe.X"}, {"Y", "probe.Y"}, {"LB", "probe.LB"},
             {"RB", "probe.RB"}, {"LT", "probe.LT"}, {"RT", "probe.RT"},
             {"Start", "probe.Start"}, {"LS", "probe.LS"}, {"RS", "probe.RS"}})
        BindAction(mapping.first, mapping.second);

    bool focusEntryBound = g_controller.BindControllerFocusEntry &&
                           g_controller.BindControllerFocusEntry(view, "Back", EnterFocus, nullptr);
    const char* focusEntryPath = focusEntryBound ? "flat" : "none";
    if (!focusEntryBound && g_api12) {
        focusEntryBound = g_api12->BindControllerFocusEntry(view, "Back", EnterFocus, nullptr);
        focusEntryPath = focusEntryBound ? "v12" : "none";
    }
    logger::info("[ControllerProbe] focus entry bind Back ok={} path={}", focusEntryBound, focusEntryPath);
    const bool releaseBound = g_api->BindGameThreadUIEvent(view, "controller_release_focus",
        [](const char*, void*) {
            if (g_api && g_view && g_api->IsValid(g_view)) {
                g_api->Unfocus(g_view);
                g_api->InteropCall(g_view, "setFocusState", "false");
                const bool actual = g_api->HasFocus(g_view);
                logger::info("[ControllerProbe] focus released actual={} focusedView={}", actual, FocusedView());
            }
        }, nullptr);
    const bool nativeBound = g_api->BindGameThreadUIEvent(view, "controller_toggle_native",
        [](const char*, void*) { ToggleNative(""); }, nullptr);
    const bool reloadBound = g_api->BindGameThreadUIEvent(view, "controller_reload",
        [](const char*, void*) { ReloadView(); }, nullptr);
    const bool recreateBound = g_api->BindGameThreadUIEvent(view, "controller_recreate",
        [](const char*, void*) { RecreateView(""); }, nullptr);
    const bool closeBound = g_api->BindGameThreadUIEvent(view, "controller_close",
        [](const char*, void*) { ClosePanel(); }, nullptr);
    const bool keyboardBound = g_api->BindGameThreadUIEvent(view, "controller_osk",
        [](const char* state, void*) { SetKeyboardBindings(state); }, nullptr);
    logger::info("[ControllerProbe] ui event binds release={} native={} reload={} recreate={} close={} osk={}",
                 releaseBound, nativeBound, reloadBound, recreateBound, closeBound, keyboardBound);
    PushGlyphState(view);
    g_api->Hide(view);
    LogState("DOM ready");
    if (g_openRequested.exchange(false, std::memory_order_acq_rel)) {
        if (auto* task = F4SE::GetTaskInterface()) task->AddTask([] { QueueOpen(); });
    }
}

#if defined(PRISMA_DIAGNOSTICS)
#include "Diagnostics.h"
#endif

void OnMessage(F4SE::MessagingInterface::Message* message) {
    if (!message) return;
    if (message->type == F4SE::MessagingInterface::kGameDataReady) {
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI11>();
        logger::info("[ControllerProbe] PrismaUI API v11={}", g_api ? "acquired" : "unavailable");
        g_api12 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI12>();
        logger::info("[ControllerProbe] PrismaUI API v12={}", g_api12 ? "acquired" : "unavailable");
#if defined(PRISMA_DIAGNOSTICS)
        {
            auto* flatModule = GetModuleHandleW(L"PrismaUI_F4.dll");
            auto* vrModule = flatModule ? nullptr : GetModuleHandleW(L"PrismaUI_F4VR.dll");
            auto* providerModule = flatModule ? flatModule : vrModule;
            wchar_t modulePath[MAX_PATH]{};
            if (providerModule) GetModuleFileNameW(providerModule, modulePath, MAX_PATH);
            SetLastError(0);
            auto* procGetApi = providerModule ? GetProcAddress(providerModule, "PrismaUI_F4_GetAPI") : nullptr;
            const DWORD procGetApiError = GetLastError();
            SetLastError(0);
            auto* procRequestApi = providerModule ? GetProcAddress(providerModule, "RequestPluginAPI") : nullptr;
            const DWORD procRequestApiError = GetLastError();
            SetLastError(0);
            auto* procGetCaps = providerModule ? GetProcAddress(providerModule, "PrismaUI_F4_GetCapabilities") : nullptr;
            const DWORD procGetCapsError = GetLastError();
            logger::info("[ControllerProbe] flat API diagnostic flatModule={} vrModule={} procGetApi={} procGetApiError={} procRequestApi={} procRequestApiError={} procGetCaps={} procGetCapsError={} sizeofControllerAPI={}",
                         flatModule != nullptr, vrModule != nullptr, procGetApi != nullptr, procGetApiError,
                         procRequestApi != nullptr, procRequestApiError, procGetCaps != nullptr, procGetCapsError,
                         sizeof(g_controller));
            char modulePathNarrow[MAX_PATH]{};
            WideCharToMultiByte(CP_UTF8, 0, modulePath, -1, modulePathNarrow, MAX_PATH, nullptr, nullptr);
            logger::info("[ControllerProbe] flat API diagnostic modulePath={}", modulePathNarrow);
            using namespace ControllerProbeDiagnostics;
            const auto exportTrace = TraceExport(providerModule, "PrismaUI_F4_GetAPI");
            logger::info("[ControllerProbe] flat API export trace directoryValid={} base={} numberOfNames={} namesSorted={} found={} nameTableIndex={} unbiasedIndex={} publicOrdinal={} functionRVA={:#x} section={}",
                         exportTrace.directoryValid, exportTrace.base, exportTrace.numberOfNames, exportTrace.namesSorted,
                         exportTrace.found, exportTrace.nameTableIndex, exportTrace.unbiasedIndex, exportTrace.publicOrdinal,
                         exportTrace.functionRVA, exportTrace.section);
            LogAllExports(providerModule);
            LogMemoryProtection(providerModule);
            const auto memoryIdentity = LogImageIdentity(providerModule);
            const auto diskTrace = TraceExportFromDisk(modulePath, "PrismaUI_F4_GetAPI");
            logger::info("[ControllerProbe] disk-read export trace unverified=true opened={} parsed={} fileSize={} timeDateStamp={:#x} sizeOfImage={:#x} exportRVA={:#x} exportSize={:#x} numberOfNames={} found={} containsGetApiString={} containsRetiredString={} sha256={} textSectionSha256={}",
                         diskTrace.opened, diskTrace.parsed, diskTrace.fileSize, diskTrace.timeDateStamp,
                         diskTrace.sizeOfImage, diskTrace.exportRVA, diskTrace.exportSize, diskTrace.numberOfNames,
                         diskTrace.found, diskTrace.containsGetApiString, diskTrace.containsRetiredString,
                         diskTrace.sha256.empty() ? "n/a" : diskTrace.sha256,
                         diskTrace.textSectionSha256.empty() ? "n/a" : diskTrace.textSectionSha256);
            logger::info("[ControllerProbe] disk-vs-memory divergence timeDateStampMatch={} sizeOfImageMatch={} exportRVAMatch={} exportSizeMatch={} textSectionSha256Match={}",
                         memoryIdentity.valid && diskTrace.parsed && memoryIdentity.timeDateStamp == diskTrace.timeDateStamp,
                         memoryIdentity.valid && diskTrace.parsed && memoryIdentity.sizeOfImage == diskTrace.sizeOfImage,
                         memoryIdentity.valid && diskTrace.parsed && memoryIdentity.exportRVA == diskTrace.exportRVA,
                         memoryIdentity.valid && diskTrace.parsed && memoryIdentity.exportSize == diskTrace.exportSize,
                         !memoryIdentity.textSectionSha256.empty() && memoryIdentity.textSectionSha256 == diskTrace.textSectionSha256);
        }
#endif
        const bool controllerOk = PRISMA_UI_FLAT_API::GetAPI(
            PRISMA_UI_FLAT_API::ApiFeature::Controller,
            PRISMA_UI_FLAT_API::ControllerApiVersion,
            &g_controller, sizeof(g_controller));
        logger::info("[ControllerProbe] modern controller API={} structSize={} nativeSet={}",
                     controllerOk, g_controller.structSize, HasNativeGamepad());
        InstallInputSinks();
    } else if (message->type == F4SE::MessagingInterface::kPostLoadGame ||
               message->type == F4SE::MessagingInterface::kNewGame) {
        InstallInputSinks();
    }
}

}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* query, F4SE::PluginInfo* info) {
    if (!query || !info) return false;
    info->infoVersion = F4SE::PluginInfo::kVersion;
    info->name = "PrismaControllerTest_F4";
    info->version = 1;
    g_runtime = query->RuntimeVersion().string();
    return !query->IsEditor() &&
           (g_runtime == "1.10.163" || g_runtime == "1.10.163.0" ||
            g_runtime == "1.11.240" || g_runtime == "1.11.240.0");
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* load) {
    F4SE::Init(load, { .log = true, .logName = "PrismaControllerTest_F4" });
    logger::info("PrismaControllerTest_F4 v1.0.0 runtime={}", g_runtime);
    const auto* messaging = F4SE::GetMessagingInterface();
    if (!messaging) return false;
    messaging->RegisterListener(OnMessage);
    return true;
}
