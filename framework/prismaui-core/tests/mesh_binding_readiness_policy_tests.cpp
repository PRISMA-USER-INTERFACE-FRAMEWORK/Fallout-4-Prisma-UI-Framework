// Retry policy must be host-testable; source:docs/code-notes/prismaui-core.md.
#include "API/MeshBindingReadinessPolicy.h"

#include <cstdlib>
#include <iostream>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

using namespace PrismaUI::MeshBinding::Readiness;

}

int main() {
    Check(ResolveAvailability(false, false) == Availability::DisabledByConfig, "config off wins over layout state");
    Check(ResolveAvailability(false, true) == Availability::DisabledByConfig, "config off is reported as config");
    Check(ResolveAvailability(true, false) == Availability::UnsupportedRuntime, "layout not verified is unsupported");
    Check(ResolveAvailability(true, true) == Availability::Available, "both enabled is available");
    Check(AvailabilityReason(Availability::UnsupportedRuntime)[0] != '\0', "every availability has a reason");

    const Policy policy{};
    Check(policy.timeout == Milliseconds(2000), "default timeout is two seconds");
    Check(policy.interval == Milliseconds(100), "default retry interval is one hundred milliseconds");
    Check(policy.maxAttempts == 15, "the attempt bound stays below the timeout/interval ceiling");
    Check(Milliseconds(policy.interval * policy.maxAttempts) < policy.timeout,
          "the attempt bound must fire before the elapsed-time bound");

    const auto start = Clock::time_point{} + Milliseconds(1000);
    State state = Begin(start);
    Check(state.attempts == 0, "a new bind starts at zero attempts");
    Check(IsDue(state, start), "the first attempt is due immediately");

    State scheduled = Begin(start);
    NoteAttempt(scheduled, start, policy);
    Check(!IsDue(scheduled, start + policy.interval - Milliseconds(1)),
          "a retry is not due before the interval elapses");
    Check(IsDue(scheduled, start + policy.interval), "a retry is due after the interval elapses");

    Check(Classify(state, start, policy, Availability::Available, true, true) == Decision::AttemptNow,
          "a live, available first attempt runs");
    Check(Classify(state, start, policy, Availability::Available, false, true) == Decision::ViewGone,
          "a stale view cancels immediately");
    Check(Classify(state, start, policy, Availability::Available, true, false) == Decision::ReferenceGone,
          "a stale reference cancels immediately");
    Check(Classify(state, start, policy, Availability::DisabledByConfig, true, true) == Decision::Unavailable,
          "config-disabled mesh binding never retries");
    Check(Classify(state, start, policy, Availability::UnsupportedRuntime, true, true) == Decision::Unavailable,
          "an unsupported runtime never retries");
    Check(Classify(state, start - Milliseconds(1), policy, Availability::Available, true, true) == Decision::Wait,
          "an attempt before the next due time waits");

    State retried = Begin(start);
    NoteAttempt(retried, start, policy);
    Check(retried.attempts == 1, "an attempt is recorded");
    Check(IsDue(retried, start + policy.interval), "the next attempt is due one interval later");
    Check(Classify(retried, start + policy.interval, policy, Availability::Available, true, true) ==
              Decision::AttemptNow,
          "eventual readiness is retried");

    State expiredByTime = Begin(start);
    Check(Classify(expiredByTime, start + policy.timeout, policy, Availability::Available, true, true) ==
              Decision::Expired,
          "the elapsed-time bound expires the bind");
    Check(Classify(expiredByTime, start + policy.timeout - Milliseconds(1), policy, Availability::Available, true,
                   true) == Decision::AttemptNow,
          "the bind may still attempt just before expiry");

    State expiredByAttempts = Begin(start);
    expiredByAttempts.attempts = policy.maxAttempts;
    Check(Classify(expiredByAttempts, start, policy, Availability::Available, true, true) == Decision::Expired,
          "the attempt bound expires the bind");

    State bounded = Begin(start);
    Check(Elapsed(start + Milliseconds(2000), bounded.started) == Milliseconds(2000), "elapsed time is reported");

    std::cout << "mesh_binding_readiness_policy_tests ok\n";
    return 0;
}
