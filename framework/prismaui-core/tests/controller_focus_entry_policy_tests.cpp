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
    using PrismaUI::ControllerActions::FocusEntrySequence;

    FocusEntrySequence sequence;

    Check(sequence.ShouldRequest(true, false, true),
          "fresh matching press requests focus entry");
    sequence.OnDispatchResult(false);
    Check(!sequence.IsOwned(), "rejected dispatch does not own the physical sequence");
    Check(!sequence.Owns(true, false, false),
          "rejected dispatch leaves the held press available to Fallout");
    Check(!sequence.Owns(true, false, true),
          "rejected dispatch leaves the release available to Fallout");

    Check(sequence.ShouldRequest(true, false, true),
          "fresh press retries after a rejected dispatch");
    sequence.OnDispatchResult(true);
    Check(sequence.IsOwned(), "accepted dispatch owns the physical sequence");
    Check(sequence.Owns(true, false, false),
          "accepted focus-entry owns held samples before focus arrives");
    Check(!sequence.ShouldRequest(true, false, false),
          "held sample never queues a second focus request");
    Check(sequence.Owns(true, false, true),
          "accepted sequence owns its physical release");
    Check(!sequence.IsOwned(), "release clears focus-entry ownership");
    Check(!sequence.Owns(true, false, true),
          "duplicate idle/release sample is not consumed");

    Check(sequence.ShouldRequest(true, false, true),
          "new physical press can request focus again");
    sequence.OnDispatchResult(true);
    Check(!sequence.Owns(true, true, false),
          "actual framework focus terminates focus-entry ownership");
    Check(!sequence.IsOwned(), "focus transition clears stale ownership");

    sequence.Reset();
    Check(!sequence.ShouldRequest(true, false, false),
          "button already held when panel becomes eligible cannot synthesize a fresh entry");
    Check(!sequence.ShouldRequest(false, false, true),
          "unregistered button cannot request focus entry");
    Check(!sequence.ShouldRequest(true, true, true),
          "focus-entry never competes with an already-focused Prisma view");

    if (failures) {
        std::cerr << failures << " controller focus-entry test(s) failed\n";
        return 1;
    }
    std::cout << "controller focus-entry policy tests passed\n";
    return 0;
}
