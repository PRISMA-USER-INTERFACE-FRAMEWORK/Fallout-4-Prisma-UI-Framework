#include "PrismaUI/ControllerActions.h"

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
    using PrismaUI::ControllerActions::BridgeLifecycle;
    using PrismaUI::ControllerActions::BridgeState;
    using PrismaUI::ControllerActions::Registry;
    using PrismaUI::ControllerActions::State;

    constexpr std::uint64_t view = 7001;
    Registry registry;
    BridgeLifecycle bridges;

    Check(registry.Bind(view, 0x14000u, "X", "scrap"), "X mapping registers");
    Check(registry.Bind(view, 0x18000u, "Y", "favorite"), "Y mapping registers");
    Check(registry.Bind(view, 0x10020u, "Back", "release_focus"), "Back mapping registers");
    Check(registry.Bind(view, 0x11000u, "A", "activate"), "A mapping registers");
    Check(registry.Bind(view, 0x12000u, "B", "cancel"), "B mapping registers");
    Check(registry.Bind(view, 0x10002u, "DDown", "next"), "D-pad mapping registers");
    Check(registry.Bind(view, 0x10009u, "LT", "zoom"), "LT mapping registers");

    const auto install = bridges.Ensure(view);
    Check(install.startInstall && install.ticket != 0, "first focus generation starts one bridge install");
    Check(bridges.Complete(view, install.ticket, true), "bridge install completes");

    for (int cycle = 0; cycle < 50; ++cycle) {
        registry.OnFocusAccepted(view);
        Check(registry.HasMappings(view), "focus churn preserves registered mappings");
        Check(!bridges.Ensure(view).startInstall && bridges.Get(view) == BridgeState::kReady,
              "focus churn does not reinstall a ready per-view bridge");

        auto event = registry.Decide(view, 0x14000u, true, false);
        Check(event.state == State::kPressed && event.consumeGameEvent,
              "X immediate press after focus fires once");
        event = registry.Decide(view, 0x14000u, true, false);
        Check(event.state == State::kNone && event.consumeGameEvent,
              "duplicate X press sample is owned without a second action");
        event = registry.Decide(view, 0x14000u, false, true);
        Check(event.state == State::kReleased && event.consumeGameEvent,
              "X physical release is delivered once");
        event = registry.Decide(view, 0x14000u, false, true);
        Check(event.state == State::kNone && event.consumeGameEvent,
              "duplicate X release sample has no second release payload");

        event = registry.Decide(view, 0x11000u, true, false);
        Check(event.state == State::kPressed && event.consumeGameEvent,
              "A immediate press after focus fires once");
        event = registry.Decide(view, 0x11000u, false, false);
        Check(event.state == State::kNone && event.consumeGameEvent,
              "one-second A hold remains owned without multi-activation");
        event = registry.Decide(view, 0x11000u, false, true);
        Check(event.state == State::kReleased && event.consumeGameEvent,
              "A release remains paired after a long hold");

        event = registry.Decide(view, 0x10002u, true, false);
        Check(event.state == State::kPressed && event.consumeGameEvent,
              "D-pad immediate press after focus navigates once");
        event = registry.Decide(view, 0x10002u, true, false);
        Check(event.state == State::kNone && event.consumeGameEvent,
              "duplicate D-pad press sample cannot double-navigate");
        event = registry.Decide(view, 0x10002u, false, true);
        Check(event.state == State::kReleased && event.consumeGameEvent,
              "D-pad release remains paired");

        event = registry.Decide(view, 0x18000u, true, false);
        Check(event.state == State::kPressed && event.consumeGameEvent,
              "Y immediate press after focus fires once");
        event = registry.Decide(view, 0x18000u, false, true);
        Check(event.state == State::kReleased && event.consumeGameEvent,
              "Y release remains paired");

        event = registry.Decide(view, 0x10020u, true, false);
        Check(event.state == State::kPressed && event.consumeGameEvent,
              "Back immediate press after focus fires once");
        event = registry.Decide(view, 0x10020u, false, true);
        Check(event.state == State::kReleased && event.consumeGameEvent,
              "Back release remains paired");
    }

    auto event = registry.Decide(view, 0x14000u, true, false);
    Check(event.state == State::kPressed, "X is pressed before held-across-focus test");
    registry.OnFocusAccepted(view);
    event = registry.Decide(view, 0x14000u, false, false);
    Check(event.state == State::kNone && event.consumeGameEvent,
          "held X across focus is suppressed and owned");
    event = registry.Decide(view, 0x14000u, false, true);
    Check(event.state == State::kNone && event.consumeGameEvent,
          "held-across-focus X release emits no unmatched release payload");
    event = registry.Decide(view, 0x14000u, true, false);
    Check(event.state == State::kPressed && event.consumeGameEvent,
          "fresh X after the held sequence releases works normally");
    (void)registry.Decide(view, 0x14000u, false, true);

    event = registry.Decide(view, 0x10009u, true, false);
    Check(event.state == State::kPressed, "LT is pressed before held-across-focus test");
    registry.OnFocusAccepted(view);
    event = registry.Decide(view, 0x10009u, false, false);
    Check(event.state == State::kNone && event.consumeGameEvent,
          "held LT across focus is suppressed and owned");
    event = registry.Decide(view, 0x10009u, false, true);
    Check(event.state == State::kNone && event.consumeGameEvent,
          "suppressed LT release emits no unmatched release payload");
    event = registry.Decide(view, 0x10009u, true, false);
    Check(event.state == State::kPressed && event.consumeGameEvent,
          "fresh LT after release works normally");

    if (failures) {
        std::cerr << failures << " controller focus-churn test(s) failed\n";
        return 1;
    }
    std::cout << "controller focus-churn tests passed\n";
    return 0;
}
