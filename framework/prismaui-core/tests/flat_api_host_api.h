#pragma once

#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4_Modern_API.h"

#include <array>
#include <cstdint>

namespace FlatApiHostTest {
    struct State {
        PrismaView consoleView = 0;
        PRISMA_UI_FLAT_API::ConsoleMessageCallback consoleCallback = nullptr;
        uint32_t regionCount = 0;
        std::array<PRISMA_UI_API::InputRegion, 256> regions{};
        uint32_t consoleCalls = 0;
        PrismaView cursorView = 0;
        PRISMA_UI_FLAT_API::CursorPolicy cursorPolicy = PRISMA_UI_FLAT_API::CursorPolicy::Default;
        PrismaView translationV4View = 0;
        void* exUserdata = nullptr;
    };

    inline State state;
}

namespace PluginAPI {
    class PrismaUIInterface {
    public:
        static PrismaUIInterface* GetSingleton() noexcept
        {
            static PrismaUIInterface instance;
            return &instance;
        }

        template <typename... Args> PrismaView CreateView(Args&&...) noexcept { return 1; }

#define PRISMA_FLAT_HOST_VOID(name) template <typename... Args> void name(Args&&...) noexcept {}
#define PRISMA_FLAT_HOST_BOOL(name) template <typename... Args> bool name(Args&&...) noexcept { return true; }
#define PRISMA_FLAT_HOST_INT(name) template <typename... Args> int name(Args&&...) noexcept { return 0; }

        PRISMA_FLAT_HOST_VOID(Invoke)
        PRISMA_FLAT_HOST_VOID(InteropCall)
        PRISMA_FLAT_HOST_VOID(RegisterJSListener)
        PRISMA_FLAT_HOST_BOOL(HasFocus)
        PRISMA_FLAT_HOST_BOOL(Focus)
        PRISMA_FLAT_HOST_VOID(Unfocus)
        PRISMA_FLAT_HOST_VOID(Show)
        PRISMA_FLAT_HOST_VOID(Hide)
        PRISMA_FLAT_HOST_BOOL(IsHidden)
        PRISMA_FLAT_HOST_INT(GetScrollingPixelSize)
        PRISMA_FLAT_HOST_VOID(SetScrollingPixelSize)
        PRISMA_FLAT_HOST_BOOL(IsValid)
        PRISMA_FLAT_HOST_VOID(Destroy)
        PRISMA_FLAT_HOST_VOID(SetOrder)
        PRISMA_FLAT_HOST_INT(GetOrder)
        PRISMA_FLAT_HOST_VOID(CreateInspectorView)
        PRISMA_FLAT_HOST_VOID(SetInspectorVisibility)
        PRISMA_FLAT_HOST_BOOL(IsInspectorVisible)
        PRISMA_FLAT_HOST_VOID(SetInspectorBounds)
        PRISMA_FLAT_HOST_BOOL(HasAnyActiveFocus)
        PRISMA_FLAT_HOST_VOID(RegisterTranslations)
        PRISMA_FLAT_HOST_VOID(BindUIEvent)
        PRISMA_FLAT_HOST_VOID(EnumerateViews)
        template <typename... Args> void* GetViewSRV(Args&&...) noexcept { return nullptr; }
        PRISMA_FLAT_HOST_VOID(SetViewOffscreen)
        PRISMA_FLAT_HOST_BOOL(BindViewToGeometry)
        PRISMA_FLAT_HOST_BOOL(BindViewToScreenTexture)
        PRISMA_FLAT_HOST_VOID(UnbindViewFromGeometry)
        PRISMA_FLAT_HOST_BOOL(SuppressHUDWidget)
        PRISMA_FLAT_HOST_BOOL(SuppressVanillaMenu)
        PRISMA_FLAT_HOST_BOOL(CloseVanillaMenu)
        PRISMA_FLAT_HOST_VOID(SuppressVanillaMenuIf)
        PRISMA_FLAT_HOST_VOID(EnableActivateChoiceFilter)
        PRISMA_FLAT_HOST_VOID(SuppressActivateChoicePerk)
        PRISMA_FLAT_HOST_VOID(EnumerateViewsEx)
        PRISMA_FLAT_HOST_BOOL(GetActivateChoiceLabel)
        PRISMA_FLAT_HOST_BOOL(TriggerActivateChoice)
        template <typename... Args> PRISMA_UI_API::ViewHealth GetViewHealth(Args&&...) noexcept { return {}; }
        PRISMA_FLAT_HOST_VOID(SetViewOffscreenSize)
        PRISMA_FLAT_HOST_VOID(SetViewOffscreenBackground)
        PRISMA_FLAT_HOST_BOOL(IsUsingGamepad)
        PRISMA_FLAT_HOST_INT(GetControllerStyle)
        PRISMA_FLAT_HOST_VOID(SetControllerStyle)
        PRISMA_FLAT_HOST_VOID(NoteInputDevice)
        PRISMA_FLAT_HOST_BOOL(GetButtonPrompt)
        PRISMA_FLAT_HOST_BOOL(GetGamepadButtonName)
        PRISMA_FLAT_HOST_VOID(SetViewOwnsEscape)
        PRISMA_FLAT_HOST_VOID(SetViewRole)
        template <typename... Args> PRISMA_UI_API::ViewRole GetViewRole(Args&&...) noexcept { return {}; }
        template <typename... Args> PrismaView GetFocusedView(Args&&...) noexcept { return 0; }
        PRISMA_FLAT_HOST_BOOL(IsAnyPanelVisible)
        PRISMA_FLAT_HOST_BOOL(FocusOverlay)

        void RegisterConsoleCallbackFlat(
            PrismaView view, PRISMA_UI_FLAT_API::ConsoleMessageCallback callback) noexcept
        {
            FlatApiHostTest::state.consoleView = view;
            FlatApiHostTest::state.consoleCallback = callback;
            if (callback) {
                ++FlatApiHostTest::state.consoleCalls;
                callback(view, PRISMA_UI_FLAT_API::ConsoleMessageLevel::Error, "typed bridge");
            }
        }

        PrismaView CreateViewEx(const char*, PRISMA_UI_FLAT_API::OnDomReadyCallbackEx callback, void* userdata) noexcept
        {
            FlatApiHostTest::state.exUserdata = userdata;
            if (callback) callback(7, userdata);
            return 7;
        }

        void InvokeEx(PrismaView, const char*, PRISMA_UI_FLAT_API::JSCallbackEx callback, void* userdata) noexcept
        {
            FlatApiHostTest::state.exUserdata = userdata;
            if (callback) callback("ok", userdata);
        }

        void RegisterConsoleCallbackEx(
            PrismaView view, PRISMA_UI_FLAT_API::ConsoleMessageCallbackEx callback, void* userdata) noexcept
        {
            FlatApiHostTest::state.exUserdata = userdata;
            if (callback) callback(view, PRISMA_UI_FLAT_API::ConsoleMessageLevel::Warning, "typed userdata", userdata);
        }

        bool SetInputRegions(
            PrismaView view, const PRISMA_UI_API::InputRegion* regions, uint32_t count) noexcept
        {
            if (!view || (count > 0 && !regions)) return false;
            FlatApiHostTest::state.regionCount = count;
            const auto kept = count < FlatApiHostTest::state.regions.size()
                                  ? count
                                  : static_cast<uint32_t>(FlatApiHostTest::state.regions.size());
            for (uint32_t i = 0; i < kept; ++i) FlatApiHostTest::state.regions[i] = regions[i];
            return true;
        }

        template <typename... Args> PRISMA_UI_API::ControllerActionBridgeState
        GetControllerActionBridgeState(Args&&...) noexcept { return {}; }
        PRISMA_FLAT_HOST_BOOL(BindControllerAction)
        PRISMA_FLAT_HOST_BOOL(UnbindControllerAction)
        PRISMA_FLAT_HOST_VOID(ClearControllerActions)
        PRISMA_FLAT_HOST_BOOL(BindControllerFocusEntry)
        PRISMA_FLAT_HOST_BOOL(UnbindControllerFocusEntry)
        PRISMA_FLAT_HOST_BOOL(SetNativeGamepad)
        PRISMA_FLAT_HOST_BOOL(DispatchToGameThread)
        PRISMA_FLAT_HOST_BOOL(IsGameThread)
        PRISMA_FLAT_HOST_BOOL(BindGameThreadUIEvent)

#undef PRISMA_FLAT_HOST_VOID
#undef PRISMA_FLAT_HOST_BOOL
#undef PRISMA_FLAT_HOST_INT
    };
}
