#include "PrismaUI/ControllerActions.h"
#include "PrismaUI/ControllerEventGate.h"
#include "PrismaUI/ControllerForwardGuard.h"
#include "PrismaUI/ControllerRoutePolicy.h"

#include <iostream>
#include <string>

namespace {
int failures = 0;

void Check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}

namespace Input = PrismaUI::ControllerInputPolicy;

int main() {
    using PrismaUI::ControllerActions::BridgeLifecycle;
    using PrismaUI::ControllerActions::BridgeState;
    using PrismaUI::ControllerActions::Registry;
    using PrismaUI::ControllerActions::State;

    constexpr std::uint64_t viewA = 41;
    constexpr std::uint64_t viewB = 42;
    Registry registry;

    constexpr std::uint32_t x = 0x14000u;
    constexpr std::uint32_t trigger = 0x10009u;
    Check(!registry.Decide(viewA, x, true, false).mappingActive, "unbound input stays available");
    Check(!registry.Bind(0, x, "X", "scrap") && !registry.Bind(viewA, x, "X", "bad action") &&
          !registry.Bind(viewA, x, "X", std::string(65, 'a')), "invalid registrations are rejected");
    Check(registry.Bind(viewA, x, "X", "scrap"), "native X code registers");
    auto decision = registry.Decide(viewA, x, true, false);
    Check(decision.state == State::kPressed && decision.consumeGameEvent, "native press delivers once");
    const auto payload = decision.Payload();
    Check(payload.find(R"("button":"X")") != std::string::npos &&
              payload.find(R"("buttonCode":)") != std::string::npos,
          "payload preserves the canonical button and adds the native button code");
    Check(registry.Decide(viewA, x, true, false).state == State::kNone, "duplicate press is suppressed");
    Check(!registry.Decide(viewB, x, true, false).mappingActive, "bindings cannot leak to another view");
    Check(!registry.Decide(viewA, 0x4000u, true, false).mappingActive, "untagged codes are not reinterpreted");
    for (int i = 0; i < 10000; ++i) {
        decision = registry.Decide(viewA, x, false, false);
        Check(decision.state == State::kNone && decision.consumeGameEvent, "held input has no custom repeat");
    }
    decision = registry.Decide(viewA, x, false, true);
    Check(decision.state == State::kReleased && decision.consumeGameEvent, "release pairs with owned press");
    Check(registry.Decide(viewA, x, false, true).state == State::kNone, "duplicate release is suppressed");
    constexpr std::uint32_t ddown = 0x10002u;
    Check(registry.Bind(viewA, ddown, "DDown", "next"), "native D-pad code registers");
    decision = registry.Decide(viewA, ddown, true, false);
    Check(decision.state == State::kPressed, "D-pad press delivers immediately");
    decision = registry.Decide(viewA, ddown, false, false, Registry::kRepeatDelaySecs - 0.01F);
    Check(decision.state == State::kNone, "D-pad does not repeat before the compatibility delay");
    decision = registry.Decide(viewA, ddown, false, false, Registry::kRepeatDelaySecs);
    Check(decision.state == State::kRepeat, "D-pad repeat preserves the browser event contract");
    decision = registry.Decide(viewA, ddown, false, true);
    Check(decision.state == State::kReleased, "D-pad release remains paired");
    registry.Unbind(viewA, ddown);
    Check(registry.Bind(viewA, x, "X", "take"), "rebinding replaces the action");
    decision = registry.Decide(viewA, x, true, false);
    const auto retained = decision;
    registry.Clear(viewA);
    Check(retained.Payload().find("take") != std::string::npos, "in-flight payload survives clear");
    Check(!registry.HasMappings(viewA), "clear removes registrations");

    Check(registry.Bind(viewA, trigger, "LT", "zoom"), "native trigger code registers");
    decision = registry.Decide(viewA, trigger, true, false);
    Check(decision.state == State::kPressed, "native trigger edge needs no Prisma threshold");
    registry.OnFocusAccepted(viewA);
    Check(registry.Decide(viewA, trigger, false, false).state == State::kNone,
          "held trigger cannot synthesize a press after focus handoff");
    Check(registry.Decide(viewA, trigger, false, true).state == State::kNone,
          "unowned release does not create a payload");
    Check(registry.Decide(viewA, trigger, true, false).state == State::kPressed,
          "fresh trigger press after handoff works");
    Check(registry.Decide(viewA, trigger, false, true).state == State::kReleased,
          "native trigger release pairs normally");
    Check(registry.Unbind(viewA, trigger) && !registry.HasMappings(viewA), "unbind removes its registration");

    BridgeLifecycle bridges;
    const auto firstInstall = bridges.Ensure(viewA);
    Check(firstInstall.startInstall && firstInstall.ticket != 0 &&
              bridges.Get(viewA) == BridgeState::kPending,
          "first binding starts one pending bridge install");
    const auto secondBinding = bridges.Ensure(viewA);
    Check(!secondBinding.startInstall && secondBinding.ticket == 0,
          "additional bindings do not compete with a pending bridge install");
    Check(!bridges.Complete(viewA, firstInstall.ticket + 1, true) &&
              bridges.Get(viewA) == BridgeState::kPending,
          "out-of-order callback cannot complete a different bridge generation");
    Check(bridges.Complete(viewA, firstInstall.ticket, true) &&
              bridges.Get(viewA) == BridgeState::kReady,
          "matching callback makes the per-view bridge ready");
    Check(!bridges.Ensure(viewA).startInstall,
          "focus churn or later bindings do not reinstall an already-ready bridge");

    bridges.Invalidate(viewA);
    Check(bridges.Get(viewA) == BridgeState::kMissing,
          "reload invalidation drops ready bridge state");
    Check(!bridges.Complete(viewA, firstInstall.ticket, true),
          "stale callback from pre-reload generation cannot complete invalidated bridge");
    const auto reloadInstall = bridges.Ensure(viewA);
    Check(reloadInstall.startInstall && reloadInstall.ticket != 0 &&
              reloadInstall.ticket != firstInstall.ticket,
          "post-reload DOM-ready starts a new bridge generation ticket");
    Check(bridges.Get(viewA) == BridgeState::kPending,
          "reloading view remains pending until new generation completes");
    Check(!bridges.Complete(viewA, firstInstall.ticket, true) &&
              bridges.Get(viewA) == BridgeState::kPending,
          "old pre-reload ticket cannot complete the reloaded generation");
    Check(bridges.Complete(viewA, reloadInstall.ticket, true) &&
              bridges.Get(viewA) == BridgeState::kReady,
          "reloaded bridge completes to ready on matching ticket");

    Check(registry.Bind(viewA, x, "X", "scrap"), "re-register binding for reload held-state test");
    decision = registry.Decide(viewA, x, true, false);
    Check(decision.state == State::kPressed, "button press is active before reload");
    registry.ResetHeld(viewA);
    Check(registry.HasMappings(viewA), "reload preserves registered mappings");
    decision = registry.Decide(viewA, x, false, true);
    Check(decision.state == State::kNone, "stale release after reload has no payload");
    decision = registry.Decide(viewA, x, true, false);
    Check(decision.state == State::kPressed, "fresh press after reload is accepted normally");
    decision = registry.Decide(viewA, x, false, true);
    Check(decision.state == State::kReleased, "subsequent release pairs with new press");
    registry.Clear(viewA);

    const auto pendingBeforeDestroy = bridges.Ensure(viewB);
    Check(pendingBeforeDestroy.startInstall, "second view gets an independent bridge generation");
    bridges.Clear(viewB);
    Check(bridges.Get(viewB) == BridgeState::kMissing &&
              !bridges.Complete(viewB, pendingBeforeDestroy.ticket, true),
          "destroy during install invalidates the pending callback");
    const auto recreated = bridges.Ensure(viewB);
    Check(recreated.startInstall && recreated.ticket != pendingBeforeDestroy.ticket,
          "recreated view gets a fresh bridge generation");
    Check(!bridges.Complete(viewB, pendingBeforeDestroy.ticket, true) &&
              bridges.Get(viewB) == BridgeState::kPending,
          "stale callback cannot mark a reused view handle ready");
    Check(bridges.Complete(viewB, recreated.ticket, false) &&
              bridges.Get(viewB) == BridgeState::kFailed,
          "failed install is represented explicitly instead of looking ready");
    const auto retry = bridges.Ensure(viewB);
    Check(retry.startInstall && retry.ticket != recreated.ticket,
          "a later binding or focus can retry a failed bridge install");
    Check(bridges.Complete(viewB, retry.ticket, true) && bridges.Get(viewB) == BridgeState::kReady,
          "successful retry reaches ready without reviving stale generations");

    constexpr std::uint64_t viewC = 43;
    BridgeLifecycle bounded;
    for (int attempt = 0; attempt < 3; ++attempt) {
        const auto install = bounded.Ensure(viewC);
        Check(install.startInstall, "provider starts each bounded bridge attempt");
        Check(bounded.Complete(viewC, install.ticket, false), "provider records each failed attempt");
    }
    Check(!bounded.Ensure(viewC).startInstall,
          "provider stops retrying after the bounded failed-install budget");

    constexpr std::uint64_t viewD = 44;
    const auto stale = bounded.Ensure(viewD);
    Check(stale.startInstall && bounded.Complete(viewD, stale.ticket, false) && bounded.CanRetry(viewD),
          "provider exposes a failed generation to the deferred retry");
    bounded.Invalidate(viewD);
    Check(!bounded.CanRetry(viewD), "provider cancels a deferred retry after view invalidation");

    using PrismaUI::ControllerInputPolicy::ForwardGuard;
    Check(!Input::VirtualPointerEligible(false), "virtual A is ineligible until the native adapter installs");
    Check(Input::VirtualPointerEligible(true), "virtual A is eligible once the native adapter is ready");

    ForwardGuard forward;
    Check(forward.Note({39u, true}), "first forwarded menu key is admitted");
    Check(!forward.Note({39u, true}), "the second menu destination does not re-forward the key");
    Check(forward.Note({39u, false}), "press and release identities stay separate");
    for (std::uint32_t i = 0; i < ForwardGuard::kCapacity; ++i) (void)forward.Note({40u + i, true});
    const auto recorded = forward.Count();
    Check(recorded == ForwardGuard::kCapacity, "the forward guard fills to its capacity");
    Check(!forward.Note({90u, true}), "a full guard suppresses an unrecorded pair instead of forwarding it");
    Check(!forward.Note({41u, true}), "an already forwarded key stays suppressed at capacity");
    Check(forward.Count() == recorded, "capacity exhaustion never records past the table");
    forward = {};
    Check(forward.Note({39u, true}), "a new converter invocation forwards the same key again");

    if (failures) {
        std::cerr << failures << " controller action routing test(s) failed\n";
        return 1;
    }
    std::cout << "controller action routing tests passed\n";
    return 0;
}
