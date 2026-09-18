#pragma once

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <stddef.h>
#include <stdint.h>
#include <type_traits>

typedef uint64_t PrismaView;

namespace PRISMA_UI_FLAT_API {
    constexpr const auto PrismaUIPluginName = "PrismaUI_F4";
    using OnDomReadyCallback = void (*)(PrismaView);
    using JSCallback = void (*)(const char*);
    using JSListenerCallback = void (*)(const char*);
    using GameThreadTaskCallback = void (*)(void*);
    using GameThreadUIEventCallback = void (*)(const char*, void*);
    enum class ConsoleMessageLevel : uint8_t { Log = 0, Warning, Error, Debug, Info };
    using ConsoleMessageCallback = void (*)(PrismaView, ConsoleMessageLevel, const char*);
    using ViewEnumCallback = void (*)(PrismaView, const char*, void*);
    using ViewEnumCallbackEx = void (*)(PrismaView, const char*, const char*, void*);
    using MenuSuppressPredicate = bool (*)();
    enum class ViewHealth : int { kUnknown = -1, kCreating, kDomReady, kLive, kLoadFailed, kDomReadyTimeout, kUnresponsive, kJsError };
    enum class ViewRole : uint32_t { kUnspecified = 0, kWidget = 1, kPanel = 2 };
    enum class ControllerActionBridgeState : uint8_t { Missing = 0, Pending, Ready, Failed };
    enum class CursorPolicy : uint32_t { Default = 0, Hidden = 1 };
    struct InputRegion { int32_t x; int32_t y; int32_t width; int32_t height; };
    enum class ApiFeature : uint32_t { Core = 1, Controller = 2, GameThread = 3 };
    inline constexpr uint32_t CoreApiVersion = 1;
    inline constexpr uint32_t ControllerApiVersion = 1;
    inline constexpr uint32_t GameThreadApiVersion = 1;

    using CoreCreateView = PrismaView (*)(const char*, OnDomReadyCallback) noexcept;
    using CoreInvoke = void (*)(PrismaView, const char*, JSCallback) noexcept;
    using CoreInteropCall = void (*)(PrismaView, const char*, const char*) noexcept;
    using CoreRegisterJSListener = void (*)(PrismaView, const char*, JSListenerCallback) noexcept;
    using CoreHasFocus = bool (*)(PrismaView) noexcept;
    using CoreFocus = bool (*)(PrismaView, bool, bool) noexcept;
    using CoreUnfocus = void (*)(PrismaView) noexcept;
    using CoreShow = void (*)(PrismaView) noexcept;
    using CoreHide = void (*)(PrismaView) noexcept;
    using CoreIsHidden = bool (*)(PrismaView) noexcept;
    using CoreGetScrollingPixelSize = int (*)(PrismaView) noexcept;
    using CoreSetScrollingPixelSize = void (*)(PrismaView, int) noexcept;
    using CoreIsValid = bool (*)(PrismaView) noexcept;
    using CoreDestroy = void (*)(PrismaView) noexcept;
    using CoreSetOrder = void (*)(PrismaView, int) noexcept;
    using CoreGetOrder = int (*)(PrismaView) noexcept;
    using CoreCreateInspectorView = void (*)(PrismaView) noexcept;
    using CoreSetInspectorVisibility = void (*)(PrismaView, bool) noexcept;
    using CoreIsInspectorVisible = bool (*)(PrismaView) noexcept;
    using CoreSetInspectorBounds = void (*)(PrismaView, float, float, unsigned int, unsigned int) noexcept;
    using CoreHasAnyActiveFocus = bool (*)() noexcept;
    using CoreRegisterConsoleCallback = void (*)(PrismaView, ConsoleMessageCallback) noexcept;
    using CoreRegisterTranslations = void (*)(PrismaView, const char*) noexcept;
    using CoreBindUIEvent = void (*)(PrismaView, const char*, JSListenerCallback) noexcept;
    using CoreEnumerateViews = void (*)(ViewEnumCallback, void*) noexcept;
    using CoreGetViewSRV = void* (*)(PrismaView) noexcept;
    using CoreSetViewOffscreen = void (*)(PrismaView, bool) noexcept;
    using CoreBindViewToGeometry = bool (*)(PrismaView, void*, const char*) noexcept;
    using CoreBindViewToScreenTexture = bool (*)(PrismaView, void*, const char*) noexcept;
    using CoreUnbindViewFromGeometry = void (*)(PrismaView) noexcept;
    using CoreSuppressHUDWidget = bool (*)(const char*, bool) noexcept;
    using CoreSuppressVanillaMenu = bool (*)(const char*, bool) noexcept;
    using CoreCloseVanillaMenu = bool (*)(const char*) noexcept;
    using CoreSuppressVanillaMenuIf = void (*)(const char*, MenuSuppressPredicate) noexcept;
    using CoreEnableActivateChoiceFilter = void (*)(bool, bool) noexcept;
    using CoreSuppressActivateChoicePerk = void (*)(uint32_t, bool) noexcept;
    using CoreEnumerateViewsEx = void (*)(ViewEnumCallbackEx, void*) noexcept;
    using CoreGetActivateChoiceLabel = bool (*)(uint32_t, char*, size_t) noexcept;
    using CoreTriggerActivateChoice = bool (*)(uint32_t) noexcept;
    using CoreGetViewHealth = ViewHealth (*)(PrismaView) noexcept;
    using CoreSetViewOffscreenSize = void (*)(PrismaView, int, int) noexcept;
    using CoreSetViewOffscreenBackground = void (*)(PrismaView, uint32_t) noexcept;
    struct CoreAPI {
        uint32_t structSize;
        uint32_t apiVersion;
        CoreCreateView CreateView;
        CoreInvoke Invoke;
        CoreInteropCall InteropCall;
        CoreRegisterJSListener RegisterJSListener;
        CoreHasFocus HasFocus;
        CoreFocus Focus;
        CoreUnfocus Unfocus;
        CoreShow Show;
        CoreHide Hide;
        CoreIsHidden IsHidden;
        CoreGetScrollingPixelSize GetScrollingPixelSize;
        CoreSetScrollingPixelSize SetScrollingPixelSize;
        CoreIsValid IsValid;
        CoreDestroy Destroy;
        CoreSetOrder SetOrder;
        CoreGetOrder GetOrder;
        CoreCreateInspectorView CreateInspectorView;
        CoreSetInspectorVisibility SetInspectorVisibility;
        CoreIsInspectorVisible IsInspectorVisible;
        CoreSetInspectorBounds SetInspectorBounds;
        CoreHasAnyActiveFocus HasAnyActiveFocus;
        CoreRegisterConsoleCallback RegisterConsoleCallback;
        CoreRegisterTranslations RegisterTranslations;
        CoreBindUIEvent BindUIEvent;
        CoreEnumerateViews EnumerateViews;
        CoreGetViewSRV GetViewSRV;
        CoreSetViewOffscreen SetViewOffscreen;
        CoreBindViewToGeometry BindViewToGeometry;
        CoreBindViewToScreenTexture BindViewToScreenTexture;
        CoreUnbindViewFromGeometry UnbindViewFromGeometry;
        CoreSuppressHUDWidget SuppressHUDWidget;
        CoreSuppressVanillaMenu SuppressVanillaMenu;
        CoreCloseVanillaMenu CloseVanillaMenu;
        CoreSuppressVanillaMenuIf SuppressVanillaMenuIf;
        CoreEnableActivateChoiceFilter EnableActivateChoiceFilter;
        CoreSuppressActivateChoicePerk SuppressActivateChoicePerk;
        CoreEnumerateViewsEx EnumerateViewsEx;
        CoreGetActivateChoiceLabel GetActivateChoiceLabel;
        CoreTriggerActivateChoice TriggerActivateChoice;
        CoreGetViewHealth GetViewHealth;
        CoreSetViewOffscreenSize SetViewOffscreenSize;
        CoreSetViewOffscreenBackground SetViewOffscreenBackground;
    };

    using ControllerIsUsingGamepad = bool (*)() noexcept;
    using ControllerGetStyle = int (*)() noexcept;
    using ControllerSetStyle = void (*)(int) noexcept;
    using ControllerNoteInputDevice = void (*)(int) noexcept;
    using ControllerGetButtonPrompt = bool (*)(const char*, char*, size_t) noexcept;
    using ControllerGetGamepadButtonName = bool (*)(uint32_t, char*, size_t) noexcept;
    using ControllerSetViewOwnsEscape = void (*)(PrismaView, bool) noexcept;
    using ControllerSetViewRole = void (*)(PrismaView, ViewRole) noexcept;
    using ControllerGetViewRole = ViewRole (*)(PrismaView) noexcept;
    using ControllerGetFocusedView = PrismaView (*)() noexcept;
    using ControllerIsAnyPanelVisible = bool (*)(PrismaView) noexcept;
    using ControllerFocusOverlay = bool (*)(PrismaView, bool, bool) noexcept;
    using ControllerSetInputRegions = bool (*)(PrismaView, const InputRegion*, uint32_t) noexcept;
    using ControllerBindAction = bool (*)(PrismaView, const char*, const char*) noexcept;
    using ControllerUnbindAction = bool (*)(PrismaView, const char*) noexcept;
    using ControllerClearActions = void (*)(PrismaView) noexcept;
    using ControllerGetActionBridgeState = ControllerActionBridgeState (*)(PrismaView) noexcept;
    using ControllerBindFocusEntry = bool (*)(PrismaView, const char*, GameThreadTaskCallback, void*) noexcept;
    using ControllerUnbindFocusEntry = bool (*)(PrismaView, const char*) noexcept;
    using ControllerSetNativeGamepad = bool (*)(PrismaView, bool) noexcept;
    struct ControllerAPI {
        uint32_t structSize;
        uint32_t apiVersion;
        ControllerIsUsingGamepad IsUsingGamepad;
        ControllerGetStyle GetControllerStyle;
        ControllerSetStyle SetControllerStyle;
        ControllerNoteInputDevice NoteInputDevice;
        ControllerGetButtonPrompt GetButtonPrompt;
        ControllerGetGamepadButtonName GetGamepadButtonName;
        ControllerSetViewOwnsEscape SetViewOwnsEscape;
        ControllerSetViewRole SetViewRole;
        ControllerGetViewRole GetViewRole;
        ControllerGetFocusedView GetFocusedView;
        ControllerIsAnyPanelVisible IsAnyPanelVisible;
        ControllerFocusOverlay FocusOverlay;
        ControllerSetInputRegions SetInputRegions;
        ControllerBindAction BindControllerAction;
        ControllerUnbindAction UnbindControllerAction;
        ControllerClearActions ClearControllerActions;
        ControllerGetActionBridgeState GetControllerActionBridgeState;
        ControllerBindFocusEntry BindControllerFocusEntry;
        ControllerUnbindFocusEntry UnbindControllerFocusEntry;
        ControllerSetNativeGamepad SetNativeGamepad;
    };

    using GameThreadDispatch = bool (*)(GameThreadTaskCallback, void*) noexcept;
    using GameThreadIsCurrent = bool (*)() noexcept;
    using GameThreadBindUIEvent = bool (*)(PrismaView, const char*, GameThreadUIEventCallback, void*) noexcept;
    struct GameThreadAPI {
        uint32_t structSize;
        uint32_t apiVersion;
        GameThreadDispatch DispatchToGameThread;
        GameThreadIsCurrent IsGameThread;
        GameThreadBindUIEvent BindGameThreadUIEvent;
    };

    static_assert(std::is_standard_layout_v<CoreAPI>);
    static_assert(std::is_standard_layout_v<ControllerAPI>);
    static_assert(std::is_standard_layout_v<GameThreadAPI>);
    static_assert(std::is_trivially_copyable_v<CoreAPI>);
    static_assert(std::is_trivially_copyable_v<ControllerAPI>);
    static_assert(std::is_trivially_copyable_v<GameThreadAPI>);
    static_assert(__builtin_offsetof(CoreAPI, structSize) == 0);
    static_assert(__builtin_offsetof(CoreAPI, apiVersion) == 4);
    static_assert(__builtin_offsetof(ControllerAPI, structSize) == 0);
    static_assert(__builtin_offsetof(ControllerAPI, apiVersion) == 4);
    static_assert(__builtin_offsetof(GameThreadAPI, structSize) == 0);
    static_assert(__builtin_offsetof(GameThreadAPI, apiVersion) == 4);
    using GetAPIProc = bool (*)(ApiFeature, uint32_t, void*, uint32_t) noexcept;

#ifdef _WIN32
    inline HMODULE GetPrismaProviderModule() noexcept {
        if (auto* flat = GetModuleHandleW(L"PrismaUI_F4.dll")) return flat;
        return GetModuleHandleW(L"PrismaUI_F4VR.dll");
    }
    inline bool GetAPI(ApiFeature feature, uint32_t version, void* table, uint32_t tableSize) noexcept {
        const auto module = GetPrismaProviderModule();
        if (!module) return false;
        const auto fn = reinterpret_cast<GetAPIProc>(GetProcAddress(module, "PrismaUI_F4_GetAPI"));
        return fn ? fn(feature, version, table, tableSize) : false;
    }
#endif
}
