#include "API/FlatAPI.h"
#include "API/ConsoleMessageMapping.h"
#include "PrismaUI_F4_Modern_API.h"
#include "flat_api_host_api.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
    int failures = 0;

    void Check(bool condition, const char* message)
    {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    }

    void ConsoleSink(PrismaView view, PRISMA_UI_FLAT_API::ConsoleMessageLevel level, const char* message)
    {
        Check(view == 42, "typed console bridge preserves the view");
        Check(level == PRISMA_UI_FLAT_API::ConsoleMessageLevel::Error,
              "typed console bridge preserves the level");
        Check(std::strcmp(message, "typed bridge") == 0, "typed console bridge preserves the message");
    }

    bool HasPointers(const PRISMA_UI_FLAT_API::CoreAPI& table)
    {
        return table.CreateView && table.Invoke && table.InteropCall && table.RegisterJSListener &&
               table.HasFocus && table.Focus && table.Unfocus && table.Show && table.Hide &&
               table.IsHidden && table.GetScrollingPixelSize && table.SetScrollingPixelSize &&
               table.IsValid && table.Destroy && table.SetOrder && table.GetOrder &&
               table.CreateInspectorView && table.SetInspectorVisibility && table.IsInspectorVisible &&
               table.SetInspectorBounds && table.HasAnyActiveFocus && table.RegisterConsoleCallback &&
               table.RegisterTranslations && table.BindUIEvent && table.EnumerateViews &&
               table.GetViewSRV && table.SetViewOffscreen && table.BindViewToGeometry &&
               table.BindViewToScreenTexture && table.UnbindViewFromGeometry && table.SuppressHUDWidget &&
               table.SuppressVanillaMenu && table.CloseVanillaMenu && table.SuppressVanillaMenuIf &&
               table.EnableActivateChoiceFilter && table.SuppressActivateChoicePerk &&
               table.EnumerateViewsEx && table.GetActivateChoiceLabel && table.TriggerActivateChoice &&
               table.GetViewHealth && table.SetViewOffscreenSize && table.SetViewOffscreenBackground;
    }

    bool HasPointers(const PRISMA_UI_FLAT_API::ControllerAPI& table)
    {
        return table.IsUsingGamepad && table.GetControllerStyle && table.SetControllerStyle &&
               table.NoteInputDevice && table.GetButtonPrompt && table.GetGamepadButtonName &&
               table.SetViewOwnsEscape && table.SetViewRole && table.GetViewRole && table.GetFocusedView &&
               table.IsAnyPanelVisible && table.FocusOverlay && table.SetInputRegions &&
               table.BindControllerAction && table.UnbindControllerAction && table.ClearControllerActions &&
               table.GetControllerActionBridgeState && table.BindControllerFocusEntry &&
               table.UnbindControllerFocusEntry && table.SetNativeGamepad;
    }

    bool HasControllerPrefixPointers(const PRISMA_UI_FLAT_API::ControllerAPI& table)
    {
        return table.IsUsingGamepad && table.GetControllerStyle && table.SetControllerStyle &&
               table.NoteInputDevice && table.GetButtonPrompt && table.GetGamepadButtonName &&
               table.SetViewOwnsEscape && table.SetViewRole && table.GetViewRole && table.GetFocusedView &&
               table.IsAnyPanelVisible && table.FocusOverlay && table.SetInputRegions &&
               table.BindControllerAction && table.UnbindControllerAction && table.ClearControllerActions &&
               table.GetControllerActionBridgeState;
    }

    bool HasPointers(const PRISMA_UI_FLAT_API::GameThreadAPI& table)
    {
        return table.DispatchToGameThread && table.IsGameThread && table.BindGameThreadUIEvent;
    }

    struct ExProbe {
        int domReady = 0;
        int js = 0;
        int console = 0;
        PrismaView domReadyView = 0;
        PRISMA_UI_FLAT_API::ConsoleMessageLevel consoleLevel = PRISMA_UI_FLAT_API::ConsoleMessageLevel::Log;
    };

    void OnDomReadyEx(PrismaView view, void* userdata)
    {
        auto* probe = static_cast<ExProbe*>(userdata);
        ++probe->domReady;
        probe->domReadyView = view;
    }

    void JSResultEx(const char* result, void* userdata)
    {
        auto* probe = static_cast<ExProbe*>(userdata);
        if (std::strcmp(result, "ok") == 0) ++probe->js;
    }

    void ConsoleSinkEx(PrismaView, PRISMA_UI_FLAT_API::ConsoleMessageLevel level, const char* message, void* userdata)
    {
        auto* probe = static_cast<ExProbe*>(userdata);
        if (std::strcmp(message, "typed userdata") == 0) ++probe->console;
        probe->consoleLevel = level;
    }
}

int main()
{
    using namespace PRISMA_UI_FLAT_API;

    CoreAPI core{};
    ControllerAPI controller{};
    GameThreadAPI gameThread{};
    Check(!PrismaUI::FlatAPI::Get(ApiFeature::Core, 1, nullptr, 0), "null output is rejected");
    Check(!PrismaUI::FlatAPI::Get(ApiFeature::Core, 1, &core, sizeof(uint32_t)),
          "output smaller than the header is rejected");
    Check(!PrismaUI::FlatAPI::Get(static_cast<ApiFeature>(99), 1, &core, sizeof(core)),
          "unknown feature is rejected");
    Check(!PrismaUI::FlatAPI::Get(ApiFeature::Core, 2, &core, sizeof(core)),
          "wrong version is rejected");

    std::array<uint8_t, sizeof(CoreAPI)> truncated{};
    Check(PrismaUI::FlatAPI::Get(ApiFeature::Core, CoreApiVersion, truncated.data(), 8),
          "consumer may request the header only");
    uint32_t header[2]{};
    std::memcpy(header, truncated.data(), sizeof(header));
    Check(header[0] == sizeof(CoreAPI) && header[1] == CoreApiVersion,
          "truncated table receives the correct header");

    std::array<uint8_t, sizeof(CoreAPI) + 64> oversized{};
    oversized.fill(0xAA);
    Check(PrismaUI::FlatAPI::Get(ApiFeature::Core, CoreApiVersion, oversized.data(),
                                 static_cast<uint32_t>(oversized.size())),
          "consumer may request a larger table");
    bool tailZeroed = true;
    for (std::size_t index = sizeof(CoreAPI); index < oversized.size(); ++index) {
        if (oversized[index] != 0) {
            tailZeroed = false;
            break;
        }
    }
    Check(tailZeroed, "provider zeroes the unsupported caller tail");

    Check(PrismaUI::FlatAPI::Get(ApiFeature::Core, CoreApiVersion, &core, sizeof(core)),
          "Core discovery succeeds");
    Check(PrismaUI::FlatAPI::Get(ApiFeature::Controller, ControllerApiVersion, &controller, sizeof(controller)),
          "Controller discovery succeeds");
    Check(PrismaUI::FlatAPI::Get(ApiFeature::GameThread, GameThreadApiVersion, &gameThread, sizeof(gameThread)),
          "GameThread discovery succeeds");
    Check(core.structSize == sizeof(core) && core.apiVersion == CoreApiVersion,
          "Core reports its size and version");
    Check(controller.structSize == sizeof(controller) && controller.apiVersion == ControllerApiVersion,
          "Controller reports its size and version");
    Check(gameThread.structSize == sizeof(gameThread) && gameThread.apiVersion == GameThreadApiVersion,
          "GameThread reports its size and version");
    Check(HasPointers(core), "Core function pointers are populated");
    Check(HasPointers(controller), "Controller function pointers are populated");
    Check(HasPointers(gameThread), "GameThread function pointers are populated");

    const auto coreSize = core.structSize;
    const auto controllerSize = controller.structSize;
    Check(coreSize != controllerSize, "feature tables keep independent layouts");
    Check(core.RegisterConsoleCallback != nullptr && controller.SetInputRegions != nullptr,
          "feature tables do not overwrite each other");

    for (int level = 0; level <= 4; ++level) {
        Check(static_cast<uint8_t>(PrismaUI::API::MapUltralightLogSeverity(level)) == level,
              "production console severity mapping preserves legacy values");
        Check(static_cast<uint8_t>(PrismaUI::API::MapFlatLogSeverity(level)) == level,
              "production flat console severity mapping preserves values");
    }
    Check(PrismaUI::API::MapFlatLogSeverity(99) == ConsoleMessageLevel::Log,
          "production flat console severity mapping defaults unknown values to log");
    int dispatchCalls = 0;
    PrismaUI::API::DispatchConsoleMessage(
        42, 2, "typed bridge", &ConsoleSink, PrismaUI::API::MapFlatLogSeverity,
        [&dispatchCalls](auto task, PrismaView view) {
            Check(view == 42, "production console bridge dispatch preserves the view");
            ++dispatchCalls;
            task();
            return true;
        });
    Check(dispatchCalls == 1, "production console bridge dispatches exactly once");
    bool dispatchExceptionPropagated = false;
    try {
        PrismaUI::API::DispatchConsoleMessage(
            42, 2, "typed bridge", &ConsoleSink, PrismaUI::API::MapFlatLogSeverity,
            [](auto, PrismaView) {
                throw std::runtime_error("host dispatch failure");
                return false;
            });
    } catch (const std::runtime_error&) {
        dispatchExceptionPropagated = true;
    }
    Check(dispatchExceptionPropagated,
          "console bridge dispatch exceptions do not terminate at the adapter boundary");

    FlatApiHostTest::state = {};
    core.RegisterConsoleCallback(42, &ConsoleSink);
    Check(FlatApiHostTest::state.consoleCallback == &ConsoleSink &&
              FlatApiHostTest::state.consoleCalls == 1,
          "typed console callback is registered and retained");
    core.RegisterConsoleCallback(42, nullptr);
    Check(FlatApiHostTest::state.consoleCallback == nullptr, "null console callback unregisters");

    const InputRegion regions[]{{1, 2, 3, 4}, {-5, -6, 7, 8}};
    Check(controller.SetInputRegions(42, regions, 2), "typed input-region bridge succeeds");
    Check(FlatApiHostTest::state.regionCount == 2 &&
              FlatApiHostTest::state.regions[0].x == 1 &&
              FlatApiHostTest::state.regions[0].height == 4 &&
              FlatApiHostTest::state.regions[1].x == -5 &&
              FlatApiHostTest::state.regions[1].width == 7,
          "typed input-region bridge copies every field");

    CoreAPI vrCore{};
    ControllerAPI vrController{};
    GameThreadAPI vrGameThread{};
    vrController.BindControllerFocusEntry = controller.BindControllerFocusEntry;
    vrController.UnbindControllerFocusEntry = controller.UnbindControllerFocusEntry;
    Check(PrismaUI::FlatAPI::GetVR(ApiFeature::Core, CoreApiVersion, &vrCore, sizeof(vrCore)),
          "VR Core discovery succeeds");
    Check(PrismaUI::FlatAPI::GetVR(ApiFeature::Controller, ControllerApiVersion, &vrController,
                                   sizeof(vrController)),
          "VR Controller discovery succeeds");
    Check(!PrismaUI::FlatAPI::GetVR(ApiFeature::Controller, ControllerApiVersion + 1, &vrController,
                                    sizeof(vrController)),
          "VR Controller rejects unknown versions");
    Check(!PrismaUI::FlatAPI::GetVR(ApiFeature::GameThread, GameThreadApiVersion, &vrGameThread,
                                    sizeof(vrGameThread)),
          "VR GameThread discovery is rejected");
    Check(vrController.structSize == __builtin_offsetof(ControllerAPI, BindControllerFocusEntry),
          "VR Controller advertises only its supported prefix");
    Check(HasControllerPrefixPointers(vrController), "VR Controller prefix pointers are populated");
    Check(vrController.BindControllerFocusEntry == nullptr && vrController.UnbindControllerFocusEntry == nullptr &&
              vrController.SetNativeGamepad == nullptr,
          "VR Controller does not advertise unsupported focus-entry or native Gamepad methods");
    Check(controller.BindControllerAction && controller.BindControllerAction(42, "X", "scrap"),
          "flat BindControllerAction reaches the provider");
    Check(vrController.BindControllerAction && !vrController.BindControllerAction(42, "X", "scrap"),
          "VR BindControllerAction fails closed without reaching the provider");

    ControllerAPI vrRaw{};
    std::memset(&vrRaw, 0xAA, sizeof(vrRaw));
    Check(PrismaUI::FlatAPI::GetVR(ApiFeature::Controller, ControllerApiVersion, &vrRaw, sizeof(vrRaw)),
          "VR Controller discovery succeeds into an oversized caller table");
    const auto* vrBytes = reinterpret_cast<const uint8_t*>(&vrRaw);
    bool vrTailZeroed = true;
    for (std::size_t index = __builtin_offsetof(ControllerAPI, BindControllerFocusEntry);
         index < sizeof(ControllerAPI); ++index) {
        if (vrBytes[index] != 0) {
            vrTailZeroed = false;
            break;
        }
    }
    Check(vrTailZeroed, "VR provider zeroes the caller tail beyond its advertised prefix");

    MetaAPI meta{};
    ViewAPI viewTable{};
    InteropAPI interop{};
    LocalizationAPI localization{};
    RenderAPI render{};
    InputAPI input{};
    MenuAPI menu{};
    Check(PrismaUI::FlatAPI::Get(ApiFeature::Meta, MetaApiVersion, &meta, sizeof(meta)),
          "Meta discovery succeeds");
    Check(meta.structSize == sizeof(meta) && meta.apiVersion == MetaApiVersion,
          "Meta reports its own size and version");
    Check(meta.ProviderName && std::strcmp(meta.ProviderName(), PrismaUIPluginName) == 0,
          "Meta exposes the provider name");
    Check(meta.HasFeature && meta.HasFeature(ApiFeature::View, ViewApiVersion),
          "Meta reports a supported feature version");
    Check(meta.HasFeature && !meta.HasFeature(ApiFeature::View, ViewApiVersion + 1),
          "Meta rejects an unsupported feature version");
    Check(meta.HasFeature && meta.HasFeature(ApiFeature::GameThread, GameThreadApiVersion),
          "flat Meta reports GameThread as supported");
    Check(PrismaUI::FlatAPI::Get(ApiFeature::View, ViewApiVersion, &viewTable, sizeof(viewTable)),
          "View discovery succeeds");
    Check(PrismaUI::FlatAPI::Get(ApiFeature::Interop, InteropApiVersion, &interop, sizeof(interop)),
          "Interop discovery succeeds");
    Check(PrismaUI::FlatAPI::Get(ApiFeature::Localization, LocalizationApiVersion, &localization,
                                 sizeof(localization)),
          "Localization discovery succeeds");
    Check(PrismaUI::FlatAPI::Get(ApiFeature::Render, RenderApiVersion, &render, sizeof(render)),
          "Render discovery succeeds");
    Check(PrismaUI::FlatAPI::Get(ApiFeature::Input, InputApiVersion, &input, sizeof(input)),
          "Input discovery succeeds");
    Check(PrismaUI::FlatAPI::Get(ApiFeature::Menu, MenuApiVersion, &menu, sizeof(menu)),
          "Menu discovery succeeds");
    Check(viewTable.apiVersion == ViewApiVersion && localization.apiVersion == LocalizationApiVersion,
          "domain tables report per-feature versions");
    Check(viewTable.SetViewCursorPolicy && viewTable.GetViewCursorPolicy,
          "View exposes typed cursor policy");
    Check(viewTable.SetViewCursorPolicy(42, CursorPolicy::Hidden),
          "cursor policy can be set through the modern API");
    Check(viewTable.GetViewCursorPolicy(42) == CursorPolicy::Hidden,
          "cursor policy reads back through the modern API");
    Check(viewTable.GetViewCursorPolicy(7) == CursorPolicy::Default,
          "cursor policy fails closed for another view");
    Check(localization.RegisterTranslationsV4 && localization.RegisterTranslationsV4(42, "plugin"),
          "localization V4 routes through the modern API");

    CallbacksAPI callbacks{};
    CallbacksAPI vrCallbacks{};
    ExProbe probe{};
    Check(PrismaUI::FlatAPI::Get(ApiFeature::Callbacks, CallbacksApiVersion, &callbacks, sizeof(callbacks)),
          "Callbacks discovery succeeds");
    Check(callbacks.structSize == sizeof(callbacks) && callbacks.apiVersion == CallbacksApiVersion,
          "Callbacks reports its size and version");
    Check(callbacks.CreateViewEx && callbacks.InvokeEx && callbacks.RegisterConsoleCallbackEx,
          "Callbacks function pointers are populated");
    Check(!PrismaUI::FlatAPI::Get(ApiFeature::Callbacks, CallbacksApiVersion + 1, &callbacks, sizeof(callbacks)),
          "Callbacks rejects unknown versions");
    Check(meta.HasFeature && meta.HasFeature(ApiFeature::Callbacks, CallbacksApiVersion),
          "flat Meta reports Callbacks as supported");
    FlatApiHostTest::state = {};
    Check(callbacks.CreateViewEx("page.html", &OnDomReadyEx, &probe) == 7 && probe.domReady == 1 &&
              probe.domReadyView == 7,
          "Callbacks CreateViewEx delivers OnDomReady with the caller userdata");
    Check(FlatApiHostTest::state.exUserdata == &probe, "Callbacks CreateViewEx passes userdata through the table");
    callbacks.InvokeEx(7, "script", &JSResultEx, &probe);
    Check(probe.js == 1, "Callbacks InvokeEx delivers the result with the caller userdata");
    callbacks.InvokeEx(7, "script", nullptr, &probe);
    Check(probe.js == 1, "Callbacks InvokeEx with a null callback is silent");
    callbacks.RegisterConsoleCallbackEx(42, &ConsoleSinkEx, &probe);
    Check(probe.console == 1 && probe.consoleLevel == ConsoleMessageLevel::Warning,
          "Callbacks RegisterConsoleCallbackEx delivers level and message with the caller userdata");
    Check(PrismaUI::FlatAPI::GetVR(ApiFeature::Callbacks, CallbacksApiVersion, &vrCallbacks, sizeof(vrCallbacks)) &&
              vrCallbacks.CreateViewEx != nullptr,
          "VR Callbacks discovery succeeds on the base dispatch path");

    MetaAPI vrMeta{};
    Check(PrismaUI::FlatAPI::GetVR(ApiFeature::Meta, MetaApiVersion, &vrMeta, sizeof(vrMeta)),
          "VR Meta discovery succeeds");
    Check(vrMeta.HasFeature && !vrMeta.HasFeature(ApiFeature::GameThread, GameThreadApiVersion),
          "VR Meta reports GameThread as unsupported");
    Check(vrMeta.HasFeature && vrMeta.HasFeature(ApiFeature::View, ViewApiVersion),
          "VR Meta still reports supported features");
    Check(PrismaUI::FlatAPI::GetVR(ApiFeature::View, ViewApiVersion, &viewTable, sizeof(viewTable)),
          "VR View discovery succeeds");

    Check(HasCapability(meta, ApiFeature::View, ViewApiVersion),
          "typed HasCapability accepts a supported feature version");
    Check(!HasCapability(meta, ApiFeature::View, ViewApiVersion + 1),
          "typed HasCapability rejects an unsupported feature version");
    Check(!HasCapability(meta, static_cast<ApiFeature>(12345), 1),
          "typed HasCapability rejects an unknown feature");
#ifdef _WIN32
    MetaAPI discovered{};
    Check(!Discover<ApiFeature::Meta>(MetaApiVersion, discovered),
          "typed discovery fails closed when no provider module is loaded");
#endif

    return failures == 0 ? 0 : 1;
}
