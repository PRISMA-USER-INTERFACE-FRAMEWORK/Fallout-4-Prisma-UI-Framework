#include "PrismaUI/UltralightGamepad.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

struct Event {
    enum class Kind { Details, Connected, Disconnected, Axis, Button };
    Kind kind = Kind::Details;
    unsigned index = 0;
    double value = 0;
    friend bool operator==(const Event&, const Event&) = default;
};

using Kind = Event::Kind;

struct RecordingRenderer {
    std::vector<Event> events;
    std::string id;
    unsigned details[3] = {};

    void SetGamepadDetails(uint32_t index, const char* name, uint32_t axisCount, uint32_t buttonCount) {
        id = name;
        details[0] = index;
        details[1] = axisCount;
        details[2] = buttonCount;
        events.push_back({Kind::Details});
    }
    void FireGamepadEvent(const ultralight::GamepadEvent& event) {
        const bool connected = event.type == ultralight::GamepadEvent::kType_GamepadConnected;
        events.push_back({connected ? Kind::Connected : Kind::Disconnected, event.index});
    }
    void FireGamepadAxisEvent(const ultralight::GamepadAxisEvent& event) {
        events.push_back({Kind::Axis, event.axis_index, event.value});
    }
    void FireGamepadButtonEvent(const ultralight::GamepadButtonEvent& event) {
        events.push_back({Kind::Button, event.button_index, event.value});
    }
};

std::vector<Event> Values(const PrismaUI::Web::GamepadState& state) {
    std::vector<Event> out;
    for (unsigned i = 0; i < state.axes.size(); ++i) out.push_back({Kind::Axis, i, state.axes[i]});
    for (unsigned i = 0; i < state.buttons.size(); ++i) out.push_back({Kind::Button, i, state.buttons[i]});
    return out;
}

std::vector<Event> Connect(const PrismaUI::Web::GamepadState& state) {
    std::vector<Event> out{{Kind::Details}, {Kind::Connected}};
    const auto values = Values(state);
    out.insert(out.end(), values.begin(), values.end());
    return out;
}

}

int main() {
    using namespace PrismaUI::Web;
    using Type = GamepadInput::Type;
    RecordingRenderer renderer;
    GamepadState pad, published;
    auto publish = [&] {
        renderer.events.clear();
        PublishGamepad(renderer, pad, published);
        assert(published.connected == pad.connected && published.axes == pad.axes && published.buttons == pad.buttons);
    };

    publish();
    assert(renderer.events.empty());

    pad.Apply({Type::Button, 0, 1, 0, true});
    publish();
    assert(renderer.id == "Prisma Fallout controller" && renderer.details[0] == 0 && renderer.details[1] == 4 &&
           renderer.details[2] == 16);
    assert(renderer.events == Connect(pad));
    publish();
    assert(renderer.events.empty());

    pad.Apply({Type::Stick, 0, 0, 0});
    pad.Apply({Type::Stick, 0, 0.75f, -0.5f});
    publish();
    assert((renderer.events == std::vector<Event>{{Kind::Axis, 0, 0.75}, {Kind::Axis, 1, -0.5}}));

    pad.Apply({Type::Stick, 0, 0, 0});
    pad.Apply({Type::Button, 0, 0});
    publish();
    assert((renderer.events == std::vector<Event>{{Kind::Axis, 0, 0}, {Kind::Axis, 1, 0}, {Kind::Button, 0, 0}}));

    pad.Apply({Type::Button, 0, 1, 0, true});
    publish();
    pad = {};
    publish();
    auto disconnect = Values(pad);
    disconnect.push_back({Kind::Disconnected});
    assert(renderer.events == disconnect);

    pad.Apply({Type::Connection, 0, 1});
    publish();
    assert(renderer.events == Connect(pad));
    publish();
    assert(renderer.events.empty());
}
