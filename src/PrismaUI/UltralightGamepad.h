#pragma once

#include "GamepadState.h"
#include <Ultralight/Renderer.h>

namespace PrismaUI::Web {

inline void PublishGamepad(ultralight::Renderer& renderer, const GamepadState& state, GamepadState& published) {
    const bool connectionChanged = state.connected != published.connected;
    ultralight::GamepadEvent connection{};
    connection.index = 0;
    connection.type = state.connected ? ultralight::GamepadEvent::kType_GamepadConnected
                                      : ultralight::GamepadEvent::kType_GamepadDisconnected;
    if (connectionChanged && state.connected) {
        renderer.SetGamepadDetails(0, "Prisma Fallout controller", 4, 16);
        renderer.FireGamepadEvent(connection);
    }
    for (unsigned i = 0; i < state.axes.size(); ++i) {
        if (connectionChanged || state.axes[i] != published.axes[i]) {
            ultralight::GamepadAxisEvent event{};
            event.index = 0;
            event.axis_index = i;
            event.value = state.axes[i];
            renderer.FireGamepadAxisEvent(event);
        }
    }
    for (unsigned i = 0; i < state.buttons.size(); ++i) {
        if (connectionChanged || state.buttons[i] != published.buttons[i]) {
            ultralight::GamepadButtonEvent event{};
            event.index = 0;
            event.button_index = i;
            event.value = state.buttons[i];
            renderer.FireGamepadButtonEvent(event);
        }
    }
    if (connectionChanged && !state.connected) renderer.FireGamepadEvent(connection);
    published = state;
}

}
