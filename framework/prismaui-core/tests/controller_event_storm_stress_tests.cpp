#include "PrismaUI/ControllerActions.h"
#include "PrismaUI/ControllerEventGate.h"

#include <array>
#include <iostream>

namespace {
int failures = 0;

void Check(bool value, const char* message)
{
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}

int main()
{
    namespace Input = PrismaUI::ControllerInputPolicy;
    using PrismaUI::ControllerActions::Registry;
    using PrismaUI::ControllerActions::State;

    constexpr int rawSamples = 7200;  // two minutes of 60 Hz held-button samples
    constexpr std::uint64_t view = 9001;

    Registry registry;
    constexpr std::array<std::uint32_t, 4> codes{0x10002u, 0x11000u, 0x14000u, 0x10009u};
    for (const auto code : codes) {
        Check(registry.Bind(view, code, "X", "action"), "native code registers");
        Check(registry.Decide(view, code, true, false).state == State::kPressed, "fresh edge delivers");
        for (int i = 0; i < rawSamples; ++i) {
            const auto decision = registry.Decide(view, code, false, false);
            Check(decision.state == State::kNone && decision.consumeGameEvent,
                  "held samples stay owned without a custom repeat timer");
        }
        Check(registry.Decide(view, code, false, true).state == State::kReleased,
              "one native release closes the owned sequence");
    }

    using Input::CrossChainEventGate;
    using Input::InputChain;
    struct InputEvent {};
    InputEvent a{};
    InputEvent b{};
    CrossChainEventGate gate;

    gate.Start(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    Check(!gate.ConsumeDuplicate(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu),
          "first chain press is routed normally");
    gate.Remember(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    Check(gate.ConsumeDuplicate(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kGameplay),
          "consecutive second-chain duplicate is suppressed");

    CrossChainEventGate interleaved;
    interleaved.Start(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    interleaved.Remember(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    interleaved.Start(&b, 2u, 0x1200u, 1.0F, 0.0F, InputChain::kMenu);
    interleaved.Remember(&b, 2u, 0x1200u, 1.0F, 0.0F, InputChain::kMenu);
    Check(interleaved.ConsumeDuplicate(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kGameplay),
          "interleaved event does not clear the earlier duplicate");

    CrossChainEventGate heldSample;
    heldSample.Start(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    heldSample.Remember(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    heldSample.Start(&a, 2u, 0x1000u, 1.0F, 0.25F, InputChain::kMenu);
    Check(!heldSample.ConsumeDuplicate(&a, 2u, 0x1000u, 1.0F, 0.25F, InputChain::kMenu),
          "new held sample is not the original press");
    heldSample.Remember(&a, 2u, 0x1000u, 1.0F, 0.25F, InputChain::kMenu);
    Check(heldSample.ConsumeDuplicate(&a, 2u, 0x1000u, 1.0F, 0.25F, InputChain::kGameplay),
          "new held sample still deduplicates across chains");

    CrossChainEventGate buttons;
    buttons.Start(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    buttons.Remember(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    buttons.Start(&a, 1u, 0x1400u, 1.0F, 0.0F, InputChain::kMenu);
    Check(!buttons.ConsumeDuplicate(&a, 1u, 0x1400u, 1.0F, 0.0F, InputChain::kMenu),
          "different button is not a duplicate");
    buttons.Remember(&a, 1u, 0x1400u, 1.0F, 0.0F, InputChain::kMenu);
    Check(buttons.ConsumeDuplicate(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kGameplay) &&
              buttons.ConsumeDuplicate(&a, 1u, 0x1400u, 1.0F, 0.0F, InputChain::kGameplay),
          "related button storage keeps identities separate");

    CrossChainEventGate release;
    release.Start(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    release.Remember(&a, 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    release.Start(&a, 2u, 0x1000u, 0.0F, 0.4F, InputChain::kMenu);
    Check(!release.ConsumeDuplicate(&a, 2u, 0x1000u, 0.0F, 0.4F, InputChain::kMenu),
          "release remains independent from press");
    release.Remember(&a, 2u, 0x1000u, 0.0F, 0.4F, InputChain::kMenu);
    Check(release.ConsumeDuplicate(&a, 2u, 0x1000u, 0.0F, 0.4F, InputChain::kGameplay),
          "release deduplicates independently across chains");

    CrossChainEventGate bounded;
    std::array<InputEvent, CrossChainEventGate::kCapacity + 1> events{};
    bounded.Start(&events[0], 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    bounded.Remember(&events[0], 1u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    for (std::size_t i = 1; i <= CrossChainEventGate::kCapacity; ++i) {
        bounded.Start(&events[i], static_cast<std::uint32_t>(i + 1),
                      static_cast<std::uint32_t>(0x3000u + i), 1.0F,
                      static_cast<float>(i), InputChain::kMenu);
        bounded.Remember(&events[i], static_cast<std::uint32_t>(i + 1),
                         static_cast<std::uint32_t>(0x3000u + i), 1.0F,
                         static_cast<float>(i), InputChain::kMenu);
    }
    Check(!bounded.ConsumeDuplicate(&events[0], 1u, 0x1000u, 1.0F, 0.0F, InputChain::kGameplay),
          "old event identity leaves the bounded dedupe ring");

    for (const auto first : {InputChain::kMenu, InputChain::kGameplay, InputChain::kConverter}) {
        CrossChainEventGate ordered;
        ordered.Start(&a, 7u, 0x1100u, 1.0F, 0.0F, first);
        Check(!ordered.ConsumeDuplicate(&a, 7u, 0x1100u, 1.0F, 0.0F, first),
              "first-arriving chain handles the physical event");
        ordered.Remember(&a, 7u, 0x1100u, 1.0F, 0.0F, first);
        int duplicates = 0;
        for (const auto other : {InputChain::kMenu, InputChain::kGameplay, InputChain::kConverter}) {
            if (other == first) continue;
            ordered.Start(&a, 7u, 0x1100u, 1.0F, 0.0F, other);
            if (ordered.ConsumeDuplicate(&a, 7u, 0x1100u, 1.0F, 0.0F, other)) ++duplicates;
        }
        Check(duplicates == 2,
              "menu/gameplay/converter route one physical event once regardless of arrival order");
    }

    CrossChainEventGate sharedChain;
    sharedChain.Start(&a, 9u, 0x1100u, 1.0F, 0.0F, InputChain::kMenu);
    sharedChain.Remember(&a, 9u, 0x1100u, 1.0F, 0.0F, InputChain::kMenu);
    sharedChain.Start(&a, 9u, 0x1100u, 1.0F, 0.0F, InputChain::kMenu);
    Check(!sharedChain.ConsumeDuplicate(&a, 9u, 0x1100u, 1.0F, 0.0F, InputChain::kMenu),
          "reusing the menu identity for the converter re-arms the event and double-routes it");

    CrossChainEventGate reused;
    reused.Start(&a, 10u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    reused.Remember(&a, 10u, 0x1000u, 1.0F, 0.0F, InputChain::kMenu);
    Check(!reused.ConsumeDuplicate(&a, 11u, 0x1000u, 1.0F, 0.0F, InputChain::kGameplay),
          "a newer event with reused pointer and state is not a duplicate");

    if (failures) {
        std::cerr << failures << " controller event-storm stress test(s) failed\n";
        return 1;
    }
    std::cout << "controller event-storm stress tests passed\n";
    return 0;
}
