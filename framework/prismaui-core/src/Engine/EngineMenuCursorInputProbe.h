#pragma once

#include <atomic>
#include <cstdint>

#include "EngineMenuCursor.h"
#include "RE/C/CursorMoveEvent.h"
#include "RE/M/MenuControls.h"
#include "RE/M/MouseMoveEvent.h"
#include "RE/P/PipboyMenu.h"
#include "RE/P/PlayerControls.h"
#include "RE/P/PlayerInputHandler.h"
#include "RE/U/UI.h"

namespace PrismaUI::Engine {

enum class CursorInputChain : std::uint8_t {
    Menu = 0,
    Gameplay = 1,
};

enum class CursorInputKind : std::uint8_t {
    MouseDelta = 0,
    CursorPosition = 1,
};

struct CursorInputEvent {
    CursorInputChain chain = CursorInputChain::Menu;
    CursorInputKind kind = CursorInputKind::MouseDelta;
    std::int32_t x = 0;
    std::int32_t y = 0;
};

struct CursorInputSample {
    CursorInputEvent event;
    MenuCursorSnapshot cursor;
    bool pipboyMenuPresent = false;
    bool pipboyMenuOnStack = false;
    bool pipboyCursorEnabled = false;
};

using CursorInputCallback = void (*)(const CursorInputEvent&) noexcept;

[[nodiscard]] inline CursorInputSample CaptureCursorInputSample(const CursorInputEvent& event) noexcept
{
    CursorInputSample sample;
    sample.event = event;
    sample.cursor = SnapshotMenuCursor();

    const auto* ui = RE::UI::GetSingleton();
    if (!ui) return sample;

    const auto pipboy = ui->GetMenu<RE::PipboyMenu>();
    const auto* menu = pipboy.get();
    if (!menu) return sample;

    sample.pipboyMenuPresent = true;
    sample.pipboyMenuOnStack = menu->OnStack();
    sample.pipboyCursorEnabled = menu->pipboyCursorEnabled;
    return sample;
}

namespace detail {

inline std::atomic<CursorInputCallback> g_cursorInputCallback{nullptr};

inline void EmitCursorInput(CursorInputChain chain, const RE::InputEvent* event) noexcept
{
    const auto callback = g_cursorInputCallback.load(std::memory_order_acquire);
    if (!callback || !event) return;

    CursorInputEvent input;
    input.chain = chain;

    if (event->eventType == RE::INPUT_EVENT_TYPE::kMouseMove) {
        const auto* mouse = static_cast<const RE::MouseMoveEvent*>(event);
        input.kind = CursorInputKind::MouseDelta;
        input.x = mouse->mouseInputX;
        input.y = mouse->mouseInputY;
    } else if (event->eventType == RE::INPUT_EVENT_TYPE::kCursorMove) {
        const auto* cursor = static_cast<const RE::CursorMoveEvent*>(event);
        input.kind = CursorInputKind::CursorPosition;
        input.x = cursor->cursorPosX;
        input.y = cursor->cursorPosY;
    } else {
        return;
    }

    callback(input);
}

class MenuCursorInputSink final : public RE::BSInputEventUser {
public:
    bool ShouldHandleEvent(const RE::InputEvent* event) override
    {
        EmitCursorInput(CursorInputChain::Menu, event);
        return false;
    }

    static MenuCursorInputSink* GetSingleton() noexcept
    {
        static MenuCursorInputSink sink;
        return &sink;
    }
};

class GameplayCursorInputSink final : public RE::PlayerInputHandler {
public:
    explicit GameplayCursorInputSink(RE::PlayerControlsData& data) noexcept : RE::PlayerInputHandler(data) {}

    bool ShouldHandleEvent(const RE::InputEvent* event) override
    {
        EmitCursorInput(CursorInputChain::Gameplay, event);
        return false;
    }
};

}

inline bool InstallMenuCursorInputProbe(CursorInputCallback callback) noexcept
{
    detail::g_cursorInputCallback.store(callback, std::memory_order_release);
    bool menuInstalled = false;
    bool gameplayInstalled = false;

    if (auto* controls = RE::MenuControls::GetSingleton()) {
        auto* sink = detail::MenuCursorInputSink::GetSingleton();
        bool present = false;
        for (std::uint32_t i = 0; i < controls->handlers.size(); ++i) {
            if (controls->handlers[i] == sink) {
                present = true;
                break;
            }
        }
        if (!present) controls->RegisterHandler(sink);
        menuInstalled = true;
    }

    if (auto* controls = RE::PlayerControls::GetSingleton()) {
        static detail::GameplayCursorInputSink* sink = nullptr;
        if (!sink) sink = new detail::GameplayCursorInputSink(controls->data);
        bool present = false;
        for (std::uint32_t i = 0; i < controls->handlers.size(); ++i) {
            if (controls->handlers[i] == sink) {
                present = true;
                break;
            }
        }
        if (!present) controls->RegisterHandler(sink);
        gameplayInstalled = true;
    }

    return menuInstalled || gameplayInstalled;
}

}
