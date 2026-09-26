#include "ControllerGlyphs.h"
#include "ControllerActions.h"
#include "ControllerEventGate.h"
#include "ControllerForwardGuard.h"
#include "ControllerRoutePolicy.h"
#include "NativeMenuHookInstall.h"
#ifndef PRISMAUI_FO4VR
#include "VirtualPointer.h"
#endif
#include "WebRuntime.h"
#include "Engine/EngineControllerInput.h"
#include "Menus/FocusMenu/FocusMenu.h"
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <utility>
#include "PCH.h"
#include <Windows.h>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <string_view>
#include <vector>
#include "Utils/ModulePath.h"
namespace PrismaUI::ControllerGlyphs {
    namespace {
        std::atomic<Device> g_active{ Device::kKeyboard };
        std::atomic<Style>  g_style{ Style::kXbox };

        std::string KeyName(std::uint32_t vk)
        {
            switch (vk) {
                case 0x08: return "Bksp";  case 0x09: return "Tab";   case 0x0D: return "Enter";
                case 0x10: return "Shift"; case 0x11: return "Ctrl";  case 0x12: return "Alt";
                case 0x1B: return "Esc";   case 0x20: return "Space"; case 0x2E: return "Del";
                case 0x25: return "\xE2\x86\x90"; case 0x26: return "\xE2\x86\x91";
                case 0x27: return "\xE2\x86\x92"; case 0x28: return "\xE2\x86\x93";
                default: break;
            }
            if (vk >= 0x30 && vk <= 0x5A) return std::string(1, static_cast<char>(vk));
            if (vk >= 0x70 && vk <= 0x7B) return "F" + std::to_string(vk - 0x6F);          
            char buf[16]; std::snprintf(buf, sizeof(buf), "0x%X", vk); return buf;   
        }

        std::string IniPath()
        {

            return PrismaUI::Utils::PluginIniPath().string();
        }

        void NotifyPromptStateChanged()
        {
            if (!WebRuntime::IsActive()) return;

            const bool gamepad = g_active.load() == Device::kGamepad;
            const int style = g_style.load() == Style::kPlayStation ? 1 : 0;
            const std::string script =
                "window.dispatchEvent(new CustomEvent('prisma-input-device-change',{detail:{gamepad:" +
                std::string(gamepad ? "true" : "false") + ",style:" + std::to_string(style) + "}}));true";

            std::vector<WebRuntime::ViewId> views;
            WebRuntime::EnumerateViews([&views](WebRuntime::ViewId id, const std::string&, const std::string&) {
                if (id) views.push_back(id);
            });
            for (const auto view : views) {
                if (WebRuntime::IsValid(view)) WebRuntime::Invoke(view, script, nullptr);
            }
        }

        bool IsRealInput(const RE::InputEvent* a_ev)
        {
            if (!a_ev) return false;

            return a_ev->eventType.get() == RE::INPUT_EVENT_TYPE::kButton &&
                   static_cast<const RE::ButtonEvent*>(a_ev)->QJustPressed();
        }

        constexpr std::array kGamepadButtons{
            RE::BS_BUTTON_CODE::kAButton, RE::BS_BUTTON_CODE::kBButton,
            RE::BS_BUTTON_CODE::kXButton, RE::BS_BUTTON_CODE::kYButton,
            RE::BS_BUTTON_CODE::kLShoulder, RE::BS_BUTTON_CODE::kRShoulder,
            RE::BS_BUTTON_CODE::kLTrigger, RE::BS_BUTTON_CODE::kRTrigger,
            RE::BS_BUTTON_CODE::kBack, RE::BS_BUTTON_CODE::kStart,
            RE::BS_BUTTON_CODE::kLStick, RE::BS_BUTTON_CODE::kRStick,
            RE::BS_BUTTON_CODE::kDPAD_Up, RE::BS_BUTTON_CODE::kDPAD_Down,
            RE::BS_BUTTON_CODE::kDPAD_Left, RE::BS_BUTTON_CODE::kDPAD_Right,
        };

        unsigned GamepadIndex(std::uint32_t code) {
            return static_cast<unsigned>(std::find(kGamepadButtons.begin(), kGamepadButtons.end(),
                static_cast<RE::BS_BUTTON_CODE>(code)) - kGamepadButtons.begin());
        }

        std::mutex g_controllerRoutingMutex;
        ControllerInputPolicy::CrossChainEventGate g_crossChainEventGate;
#ifndef PRISMAUI_FO4VR
        std::atomic<bool> g_nativeMenuReady{false};
#endif

        bool RouteGamepadToView(const RE::ButtonEvent* a_ev, ControllerInputPolicy::InputChain chain)
        {
            if (!a_ev) return false;

            const bool gamepad = a_ev->device.get() == RE::INPUT_DEVICE::kGamepad;
            if (!gamepad) return false;
            std::lock_guard lock(g_controllerRoutingMutex);
            const auto code = static_cast<std::uint32_t>(a_ev->GetBSButtonCode());
            g_crossChainEventGate.Start(a_ev, a_ev->timeCode, code, a_ev->value,
                                        a_ev->heldDownSecs, chain);
            if (g_crossChainEventGate.ConsumeDuplicate(a_ev, a_ev->timeCode, code, a_ev->value,
                                                        a_ev->heldDownSecs, chain)) {
                return true;
            }
            const auto consume = [&](bool shouldConsume) {
                if (shouldConsume) {
                    g_crossChainEventGate.Remember(a_ev, a_ev->timeCode, code, a_ev->value,
                                                   a_ev->heldDownSecs, chain);
                }
                return shouldConsume;
            };
            const auto focusedView = PrismaUI::WebRuntime::GetFocusedView();
            if (WebRuntime::UsesNativeGamepad(focusedView)) {
                const auto index = GamepadIndex(code);
                if (index < kGamepadButtons.size()) {
                    WebRuntime::SendGamepad(focusedView, {Web::GamepadInput::Type::Button,
                        index, a_ev->value, 0, a_ev->QJustPressed()});
                }
                return consume(true);
            }
            const auto action = PrismaUI::ControllerActions::Handle(
                focusedView, code, a_ev->QJustPressed(), a_ev->QReleased(), a_ev->heldDownSecs);
            if (action.mappingActive) return consume(action.consumeGameEvent);

            const auto focusEntry = PrismaUI::ControllerActions::HandleFocusEntry(
                code, a_ev->QJustPressed(), a_ev->QReleased());
            if (focusEntry.mappingActive) return consume(focusEntry.consumeGameEvent);

#ifndef PRISMAUI_FO4VR
            if (ControllerInputPolicy::VirtualPointerEligible(g_nativeMenuReady.load()) &&
                VirtualPointer::OnButton(code, a_ev->QJustPressed(), a_ev->QReleased())) {
                return consume(true);
            }
#endif
            return false;
        }

        bool RouteNativeGamepad(const RE::InputEvent* event) {
            if (!event || event->device.get() != RE::INPUT_DEVICE::kGamepad) return false;
            const auto view = WebRuntime::GetFocusedView();
            if (!WebRuntime::UsesNativeGamepad(view)) return false;
            if (event->eventType == RE::INPUT_EVENT_TYPE::kThumbstick) {
                const auto* stick = static_cast<const RE::ThumbstickEvent*>(event);
                if (stick->idCode == RE::ThumbstickEvent::kLeft || stick->idCode == RE::ThumbstickEvent::kRight)
                    WebRuntime::SendGamepad(view, {Web::GamepadInput::Type::Stick,
                        stick->idCode == RE::ThumbstickEvent::kLeft ? 0u : 1u, stick->xValue, stick->yValue});
                return true;
            }
            if (event->eventType == RE::INPUT_EVENT_TYPE::kDeviceConnect) {
                const auto* connection = static_cast<const RE::DeviceConnectEvent*>(event);
                WebRuntime::SendGamepad(view, {Web::GamepadInput::Type::Connection,
                    0, connection->connected ? 1.0f : 0.0f});
            }
            return false;
        }

#ifndef PRISMAUI_FO4VR
        using MenuSender = RE::BSUIScaleformData::SendEventFunction;
        using ButtonConverter = RE::GFxConvertHandler::ButtonEventHandler;
        using StickConverter = RE::GFxConvertHandler::ThumbstickEventHandler;
        MenuSender g_sendMenuEvent = nullptr;
        ButtonConverter g_convertButton = nullptr;
        StickConverter g_convertStick = nullptr;
        struct ConversionContext {
            WebRuntime::ViewId view = 0;
            std::uint64_t generation = 0;
            bool fresh = false;
            ControllerInputPolicy::ForwardGuard forwarded;
        };
        thread_local ConversionContext g_conversion;
        thread_local bool g_traceConversion = false;

        struct ConversionScope {
            ConversionContext previous = std::exchange(g_conversion, {});
            bool previousTrace = std::exchange(g_traceConversion, false);
            ~ConversionScope() { g_conversion = previous; g_traceConversion = previousTrace; }
        };

        bool IsNativeConverter(void* object) {
            const auto* controls = RE::MenuControls::GetSingleton();
            return g_nativeMenuReady.load() && controls && controls->convertHandler == object;
        }

        void SendMenuEvent(const RE::BSFixedString& menu, void* event) {
            if (g_traceConversion) {
                if (const auto key = Engine::ReadControllerMenuKey(event)) logger::debug(
                    "[PrismaUI ControllerGlyphs] converted menu='{}' view={} generation={} fresh={} key={} pressed={}",
                    menu.c_str(), g_conversion.view, g_conversion.generation, g_conversion.fresh, key->key, key->pressed);
                else logger::debug("[PrismaUI ControllerGlyphs] converted menu='{}' view={} generation={} fresh={} undecodable",
                                   menu.c_str(), g_conversion.view, g_conversion.generation, g_conversion.fresh);
            }
            if (g_conversion.generation && (menu == FocusMenu::MENU_NAME || menu == "TopMenu")) {
                if (const auto key = Engine::ReadControllerMenuKey(event)) {
                    if (g_conversion.forwarded.Note({key->key, key->pressed})) {
                        WebRuntime::SendControllerKey(g_conversion.view, g_conversion.generation,
                            key->key, key->pressed, g_conversion.fresh);
                    } else {
                        logger::debug(
                            "[PrismaUI ControllerGlyphs] converted menu='{}' repeated {}/{} already forwarded "
                            "for this converter invocation",
                            menu.c_str(), key->key, key->pressed ? 1 : 0);
                    }
                }
            }
            g_sendMenuEvent(menu, static_cast<Scaleform::GFx::Event*>(event));
        }

        void ConvertButton(void* object, RE::ButtonEvent* event) {
            ConversionScope scope;
            bool consumed = false;
            bool focusedController = false;
            if (IsNativeConverter(object) && event) {
                if (IsRealInput(event)) NoteInputDevice(event->device.get());
                if (event->device.get() == RE::INPUT_DEVICE::kGamepad) {
                    const auto view = WebRuntime::GetFocusedView();
                    const auto generation = WebRuntime::ControllerInputGeneration(view);
                    g_traceConversion = event->QJustPressed() || event->QReleased();
                    if (g_traceConversion) logger::debug(
                        "[PrismaUI ControllerGlyphs] ConvertButton code={:#x} value={} held={} view={} generation={} fresh={}",
                        static_cast<std::uint32_t>(event->GetBSButtonCode()), event->value, event->heldDownSecs, view,
                        generation, event->QJustPressed());
                    consumed = RouteGamepadToView(event, ControllerInputPolicy::InputChain::kConverter);
                    focusedController = generation != 0;
                    if (!consumed) g_conversion = {view, generation, event->QJustPressed()};
                }
            }
            g_convertButton(static_cast<RE::GFxConvertHandler*>(object), event);
            if (event && (consumed || focusedController)) event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
        }

        void ConvertStick(void* object, RE::ThumbstickEvent* event) {
            ConversionScope scope;
            bool consumed = false;
            bool focusedController = false;
            if (IsNativeConverter(object) && event && event->device.get() == RE::INPUT_DEVICE::kGamepad) {
                const auto view = WebRuntime::GetFocusedView();
                const auto generation = WebRuntime::ControllerInputGeneration(view);
                if (!WebRuntime::UsesNativeGamepad(view))
                    VirtualPointer::OnStick(event->idCode == RE::ThumbstickEvent::kRight, event->xValue, event->yValue);
                g_traceConversion = event->prevDir != event->currDir;
                if (g_traceConversion) logger::debug(
                    "[PrismaUI ControllerGlyphs] ConvertStick id={} prev={} curr={} x={} y={} view={} generation={}",
                    event->idCode, static_cast<int>(event->prevDir), static_cast<int>(event->currDir), event->xValue,
                    event->yValue, view, generation);
                consumed = RouteNativeGamepad(event);
                focusedController = generation != 0;
                if (!consumed) g_conversion = {view, generation,
                    event->prevDir != event->currDir && event->currDir != RE::DIRECTION_VAL::kNone};
            }
            g_convertStick(static_cast<RE::GFxConvertHandler*>(object), event);
            if (event && (consumed || focusedController)) event->handled = RE::InputEvent::HANDLED_RESULT::kStop;
        }

        bool InstallNativeMenuInput() {
            const auto sites = Engine::ControllerConversionTargets();
            if (!sites.sender || !sites.button || !sites.stick) return false;
            const auto initialized = MH_Initialize();
            if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) return false;
            const std::array<void*, 3> targets{reinterpret_cast<void*>(sites.sender),
                reinterpret_cast<void*>(sites.button), reinterpret_cast<void*>(sites.stick)};
            const std::array<void*, 3> detours{reinterpret_cast<void*>(&SendMenuEvent),
                reinterpret_cast<void*>(&ConvertButton), reinterpret_cast<void*>(&ConvertStick)};
            const std::array<void**, 3> originals{reinterpret_cast<void**>(&g_sendMenuEvent),
                reinterpret_cast<void**>(&g_convertButton), reinterpret_cast<void**>(&g_convertStick)};
            if (!InstallNativeMenuHooks(targets, detours, originals, NativeMenuHookOps{
                    [](void* target, void* detour, void** original) {
                        return static_cast<int>(MH_CreateHook(target, detour, original));
                    },
                    [](void* target) { return static_cast<int>(MH_EnableHook(target)); },
                    [](void* target) { return static_cast<int>(MH_DisableHook(target)); },
                    [](void* target) { return static_cast<int>(MH_RemoveHook(target)); }})) {
                return false;
            }
            g_nativeMenuReady.store(true);
            return true;
        }
#else
        bool InstallNativeMenuInput() { return false; }
#endif

        class InputSink : public RE::BSInputEventUser
        {
        public:
            bool ShouldHandleEvent(const RE::InputEvent* e) override
            {
                if (IsRealInput(e)) NoteInputDevice(e->device.get());
                if (RouteNativeGamepad(e) || (e && e->eventType == RE::INPUT_EVENT_TYPE::kButton &&
                    RouteGamepadToView(static_cast<const RE::ButtonEvent*>(e),
                                       ControllerInputPolicy::InputChain::kMenu))) {

                    const_cast<RE::InputEvent*>(e)->handled = RE::InputEvent::HANDLED_RESULT::kStop;
                    return false;
                }
                return false;
            }
            static InputSink* GetSingleton() { static InputSink s; return &s; }
        };

        class GameplayInputSink : public RE::PlayerInputHandler
        {
        public:
            explicit GameplayInputSink(RE::PlayerControlsData& a_data) : RE::PlayerInputHandler(a_data) {}

            bool ShouldHandleEvent(const RE::InputEvent* e) override
            {
                if (IsRealInput(e)) NoteInputDevice(e->device.get());
                if (RouteNativeGamepad(e) || (e && e->eventType == RE::INPUT_EVENT_TYPE::kButton &&
                    RouteGamepadToView(static_cast<const RE::ButtonEvent*>(e),
                                       ControllerInputPolicy::InputChain::kGameplay))) {
                    const_cast<RE::InputEvent*>(e)->handled = RE::InputEvent::HANDLED_RESULT::kStop;
                    return false;
                }
                return false;
            }
        };
    }

    bool IsControllerButton(std::uint32_t buttonCode) noexcept
    {
        return GamepadIndex(buttonCode) < kGamepadButtons.size();
    }

    bool SetNativeGamepad(std::uint64_t view, bool enabled)
    {
        std::lock_guard lock(g_controllerRoutingMutex);
        const bool wasNative = WebRuntime::UsesNativeGamepad(view);
        if (!WebRuntime::SetNativeGamepad(view, enabled)) return false;
        if (wasNative != enabled) ControllerActions::OnFocusAccepted(view);
        return true;
    }

    void InstallInputTracking()
    {
        static bool s_installed = false;
        if (s_installed) return;

        char buf[32]{};
        GetPrivateProfileStringA("Controller", "Style", "xbox", buf, sizeof(buf), IniPath().c_str());
        g_style.store(_stricmp(buf, "playstation") == 0 ? Style::kPlayStation : Style::kXbox);
        logger::info("[PrismaUI ControllerGlyphs] controller style = {}",
                     g_style.load() == Style::kPlayStation ? "playstation" : "xbox");

        logger::info("[PrismaUI ControllerGlyphs] native menu conversion = {}", InstallNativeMenuInput());
        bool menuOk = false, gameOk = false;

        if (auto* mc = RE::MenuControls::GetSingleton()) {
#if defined(PRISMAUI_FO4VR)

            auto* sink = InputSink::GetSingleton();
            bool present = false;
            for (std::uint32_t i = 0; i < mc->handlers.size(); ++i) {
                if (mc->handlers[i] == sink) { present = true; break; }
            }
            if (!present) mc->handlers.push_back(sink);
#else
            mc->RegisterHandler(InputSink::GetSingleton());
#endif
            menuOk = true;
        } else {
            logger::warn("[PrismaUI ControllerGlyphs] MenuControls unavailable -- menu-chain tracking not installed");
        }

        if (auto* pc = RE::PlayerControls::GetSingleton()) {            static GameplayInputSink* s_gameplay = nullptr;
            if (!s_gameplay) s_gameplay = new GameplayInputSink(pc->data);
            bool present = false;
            for (std::uint32_t i = 0; i < pc->handlers.size(); ++i)
                if (pc->handlers[i] == s_gameplay) { present = true; break; }
            if (!present) pc->RegisterHandler(s_gameplay);
            gameOk = true;
        } else {
            logger::warn("[PrismaUI ControllerGlyphs] PlayerControls unavailable -- gameplay-chain "
                         "tracking not installed; prompts will only correct themselves once a menu opens");
        }

        s_installed = menuOk || gameOk;
        logger::info("[PrismaUI ControllerGlyphs] input-device tracking installed (menu={} gameplay={})",
                     menuOk, gameOk);
    }

    void NoteInputDevice(RE::INPUT_DEVICE device)
    {
        Device next;
        if (device == RE::INPUT_DEVICE::kGamepad) {
            next = Device::kGamepad;
        } else if (device == RE::INPUT_DEVICE::kKeyboard || device == RE::INPUT_DEVICE::kMouse) {
            next = Device::kKeyboard;
        } else {
            return;
        }
        if (g_active.exchange(next) != next) {

            logger::debug("[PrismaUI ControllerGlyphs] input device -> {}",
                          next == Device::kGamepad ? "GAMEPAD" : "KEYBOARD");
            NotifyPromptStateChanged();
        }
    }

    bool  UsingGamepad()   { return g_active.load() == Device::kGamepad; }
    Style GetStyle()       { return g_style.load(); }
    void  SetStyle(Style s)
    {
        const bool changed = g_style.exchange(s) != s;
        WritePrivateProfileStringA("Controller", "Style",
                                   s == Style::kPlayStation ? "playstation" : "xbox", IniPath().c_str());
        if (changed) NotifyPromptStateChanged();
    }

    std::string CanonicalFromCode(std::uint32_t code)
    {
        constexpr std::array<std::string_view, 16> names{
            "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "Back", "Start", "LS", "RS",
            "DUp", "DDown", "DLeft", "DRight",
        };
        const auto index = GamepadIndex(code | static_cast<std::uint32_t>(RE::BS_BUTTON_CODE::kGamepad));
        return index < names.size() ? std::string(names[index]) : std::string{};
    }

    bool CodeFromCanonical(std::string_view name, std::uint32_t& code) noexcept
    {
        if (name.empty()) return false;
        for (const auto button : kGamepadButtons) {
            const auto tagged = static_cast<std::uint32_t>(button);
            if (CanonicalFromCode(tagged) == name) {
                code = tagged;
                return true;
            }
        }
        return false;
    }

    std::string ButtonPrompt(const char* userEvent)
    {
        if (!userEvent) return {};
        auto* cm = RE::ControlMap::GetSingleton();
        if (!cm) return {};

        const bool pad = UsingGamepad();
        const auto device = pad ? RE::INPUT_DEVICE::kGamepad : RE::INPUT_DEVICE::kKeyboard;

        constexpr RE::UserEvents::INPUT_CONTEXT_ID contexts[] = {
            RE::UserEvents::INPUT_CONTEXT_ID::kBasicMenuNav,
            RE::UserEvents::INPUT_CONTEXT_ID::kMainGameplay,
            RE::UserEvents::INPUT_CONTEXT_ID::kWorkshop,
            RE::UserEvents::INPUT_CONTEXT_ID::kQuickContainerMenu,
            RE::UserEvents::INPUT_CONTEXT_ID::kMainMenu,
            RE::UserEvents::INPUT_CONTEXT_ID::kPauseMenu,
        };

        std::uint32_t key = 0xFFFFFFFF;
        for (auto ctx : contexts) {
            key = cm->GetMappedKey(userEvent, device, ctx);
            if (key != 0xFF && key != 0xFFFF && key != 0xFFFFFFFF) break;
        }

        if (key == 0xFF || key == 0xFFFF || key == 0xFFFFFFFF) return {};

        if (!pad) return KeyName(key);
        auto canon = CanonicalFromCode(key);
        return canon.empty() ? std::string{} : "gp:" + canon;
    }
}
