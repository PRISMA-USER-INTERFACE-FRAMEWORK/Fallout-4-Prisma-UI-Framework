#include "Menus/PauseHold/PauseHoldPolicy.h"

#include <iostream>

namespace {

int failures = 0;

void Check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

}  // namespace

int main() {
    using PrismaUI::PauseHoldPolicy::State;

    State state;
    bool messageValue = true;

    Check(!state.TargetPaused(), "a new pause hold targets the unpaused state");
    Check(!state.AdvanceVerificationFrame(),
          "verification is disarmed before any engine message is queued");
    Check(state.ShouldSendMessage(messageValue),
          "the first explicit target produces a balancing engine message");
    Check(!messageValue, "the first message reflects the current unpaused target");
    Check(!state.AdvanceVerificationFrame(),
          "merely preparing a message does not arm verification");
    Check(state.ShouldSendMessage(messageValue),
          "a message remains pending until the queue actually accepts it");

    state.MarkMessageSent(false);
    Check(!state.ShouldSendMessage(messageValue),
          "an identical target does not queue a duplicate hide message");

    for (int frame = 0; frame < State::kVerifyAfterCompleteFrames; ++frame) {
        Check(!state.AdvanceVerificationFrame(),
              "verification waits through every required complete frame");
    }
    Check(state.AdvanceVerificationFrame(),
          "verification becomes due on the tick after the settling frames");
    Check(!state.AdvanceVerificationFrame(),
          "a completed verification does not fire repeatedly");

    state.SetTarget(true);
    Check(state.ShouldSendMessage(messageValue),
          "changing the target from unpaused to paused queues a show message");
    Check(messageValue, "the queued show message uses the latest target");
    state.MarkMessageSent(messageValue);

    state.InvalidateMessageState();
    Check(state.ShouldSendMessage(messageValue),
          "forced engine removal makes the requested pause message pending again");
    Check(messageValue, "reconciliation preserves the latest paused target");
    Check(!state.AdvanceVerificationFrame(),
          "invalidating engine state disarms stale verification progress");
    state.MarkMessageSent(messageValue);

    for (int frame = 0; frame < 3; ++frame) {
        Check(!state.AdvanceVerificationFrame(),
              "a newly sent message begins a fresh verification countdown");
    }

    // A duplicate request must not restart the countdown. The shipping path still schedules a UI
    // task for Set(true), but ShouldSendMessage rejects it and leaves verification progress intact.
    state.SetTarget(true);
    Check(!state.ShouldSendMessage(messageValue),
          "a duplicate pause request remains suppressed while verification is pending");
    for (int frame = 3; frame < State::kVerifyAfterCompleteFrames; ++frame) {
        Check(!state.AdvanceVerificationFrame(),
              "duplicate suppression preserves the original countdown progress");
    }
    Check(state.AdvanceVerificationFrame(),
          "the original countdown still completes after a duplicate request");

    // A genuinely newer message must reset the countdown, even if the render thread had already
    // advanced the previous one. This is the race the CAS-based implementation protects.
    state.SetTarget(false);
    Check(state.ShouldSendMessage(messageValue),
          "changing back to unpaused queues a hide message");
    state.MarkMessageSent(messageValue);
    for (int frame = 0; frame < 5; ++frame) {
        Check(!state.AdvanceVerificationFrame(),
              "the replacement message owns its own countdown");
    }

    state.SetTarget(true);
    Check(state.ShouldSendMessage(messageValue),
          "a newer pause target queues another show message");
    state.MarkMessageSent(messageValue);
    for (int frame = 0; frame < State::kVerifyAfterCompleteFrames; ++frame) {
        Check(!state.AdvanceVerificationFrame(),
              "a newer message resets all prior verification progress");
    }
    Check(state.AdvanceVerificationFrame(),
          "the replacement countdown verifies exactly once");

    // Verification reads the latest requested target, matching PauseHold's existing behavior when a
    // target changes after the engine message was queued but before the delayed check runs.
    state.SetTarget(false);
    Check(!state.TargetPaused(), "the verifier observes the latest requested target");

    if (failures != 0) {
        std::cerr << failures << " pause-hold policy test(s) failed\n";
        return 1;
    }

    std::cout << "pause hold policy tests passed\n";
    return 0;
}
