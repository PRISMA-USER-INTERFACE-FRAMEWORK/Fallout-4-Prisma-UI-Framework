#include "PrismaUI/GamepadState.h"

#include <cassert>
#include <limits>

int main() {
    using namespace PrismaUI::Web;
    using Type = GamepadInput::Type;
    GamepadState pad;
    pad.Apply({Type::Button, 0, 1, 0, false});
    assert(pad.connected && pad.buttons[0] == 0);
    pad.Apply({Type::Button, 0, 0});
    pad.Apply({Type::Button, 0, 1, 0, true});
    assert(pad.buttons[0] == 1);
    pad.Apply({Type::Button, 6, 0.42f, 0, true});
    assert(pad.buttons[6] == static_cast<double>(0.42f));
    pad.Apply({Type::Button, 6, 0.25f});
    assert(pad.buttons[6] == 0.25);
    pad.Apply({Type::Stick, 0, 0.75f, -0.5f});
    assert(pad.axes[0] == 0 && pad.axes[1] == 0);
    pad.Apply({Type::Stick, 0, 0, 0});
    pad.Apply({Type::Stick, 0, 0.75f, -0.5f});
    assert(pad.axes[0] == 0.75 && pad.axes[1] == -0.5);
    pad.Apply({Type::Stick, 1, 0, 0});
    pad.Apply({Type::Stick, 1, -0.125f, 0.625f});
    assert(pad.axes[2] == -0.125 && pad.axes[3] == 0.625);
    GamepadState zone;
    zone.Apply({Type::Stick, 0, 0.2f, 0.2f});
    assert(!zone.centered[0] && zone.axes[0] == 0 && zone.axes[1] == 0);
    zone.Apply({Type::Stick, 0, 0, 0});
    assert(zone.centered[0]);
    zone.Apply({Type::Stick, 0, 0.1f, 0.1f});
    assert(zone.axes[0] == static_cast<double>(0.1f) && zone.axes[1] == static_cast<double>(0.1f));
    GamepadState ramp;
    ramp.Apply({Type::Connection, 0, 1});
    ramp.Apply({Type::Stick, 0, 0.18f, 0});
    assert(ramp.centered[0] && ramp.axes[0] == static_cast<double>(0.18f));
    ramp.Apply({Type::Stick, 0, 0.02f, 0});
    assert(ramp.axes[0] == static_cast<double>(0.02f));
    ramp.Apply({Type::Stick, 0, 0.05f, 0});
    assert(ramp.axes[0] == static_cast<double>(0.05f));
    ramp.Apply({Type::Stick, 0, 0.3f, 0});
    assert(ramp.axes[0] == static_cast<double>(0.3f));
    ramp.Apply({Type::Stick, 0, 1.0f, 0});
    assert(ramp.axes[0] == 1.0);
    GamepadState noisy;
    noisy.Apply({Type::Stick, 1, 0.03f, -0.02f});
    assert(noisy.centered[1] && noisy.axes[2] == static_cast<double>(0.03f) && noisy.axes[3] == static_cast<double>(-0.02f));
    noisy.Apply({Type::Stick, 1, 0.5f, 0});
    assert(noisy.axes[2] == 0.5 && noisy.axes[3] == 0);
    GamepadState held;
    held.Apply({Type::Stick, 0, 0.6f, 0.05f});
    assert(!held.centered[0] && held.axes[0] == 0 && held.axes[1] == 0);
    for (int i = 0; i < 10000; ++i) held.Apply({Type::Stick, 0, (i & 1) ? -0.6f : 0.6f, 0.05f});
    assert(!held.centered[0] && held.axes == GamepadState{}.axes);
    held.Apply({Type::Stick, 0, 0.05f, 0.05f});
    assert(held.centered[0] && held.axes[0] == static_cast<double>(0.05f));
    held.Apply({Type::Stick, 0, 0.5f, 0});
    assert(held.axes[0] == 0.5);
    const auto before = pad;
    pad.Apply({Type::Stick, 0, std::numeric_limits<float>::quiet_NaN(), 0});
    pad.Apply({Type::Stick, 2, 0.5f, 0.5f});
    pad.Apply({Type::Button, 16, 1, 0, true});
    pad.Apply({Type::Button, 0, 1.5f, 0, true});
    assert(pad.axes == before.axes && pad.buttons == before.buttons);
    for (int i = 0; i < 10000; ++i) {
        pad.Apply({Type::Stick, 0, 0.75f, -0.5f});
        pad.Apply({Type::Button, 0, 1});
    }
    assert(pad.axes == before.axes && pad.buttons == before.buttons);
    pad.Apply({Type::Connection, 0, 0});
    assert(!pad.connected && pad.axes == GamepadState{}.axes && pad.buttons == GamepadState{}.buttons);
    pad.Apply({Type::Connection, 0, 1});
    pad.Apply({Type::Button, 0, 1});
    pad.Apply({Type::Stick, 0, 0.75f, -0.5f});
    assert(pad.connected && pad.buttons[0] == 0 && pad.axes[0] == 0);
    pad = {};
    pad.Apply({Type::Button, 0, 1});
    assert(pad.buttons[0] == 0);
}
