// Decision-logic proof for the composite-stale emergency release in
// WebRuntimeLifecycle's MaybeRecoverDeadFocus.
//
// The integration (timestamps, generation dedup, Unfocus) needs the game + Host and cannot run here,
// but the DECISION is a pure function. This mirrors it verbatim and asserts:
//   * only a kLIVE view is eligible (a loading kCreating/kDomReady view is NOT killed at 2s), and
//   * staleness is scoped to the current focused-view EPISODE (focusEpisodeStartMs, which resets on
//     an A->B handoff), so B never inherits A's stale clock, yet a first-ever dark menu still recovers.
//
// Build: g++ -std=c++17 -O1 tests/health_failsafe_test.cpp -o health_failsafe_test && ./health_failsafe_test
// Exit code: 0 = all pass.

#include <cstdint>
#include <cstdio>

// ---- verbatim mirror of the constants + decision in MaybeRecoverDeadFocus -----------------------
constexpr int64_t kCompositeStaleMs = 2000;
constexpr int64_t kPresentAliveMs   = 500;

static int64_t imax(int64_t a, int64_t b) { return a > b ? a : b; }

struct Snapshot {
    int64_t now;
    int64_t focusEpisodeStartMs;  // when the CURRENT focused view was focused (resets on view change)
    int64_t lastCompositeMs;      // last successful compositor draw (0 == never)
    int64_t lastPresentMs;        // last flat present
    bool    ownershipActive;      // g_focusSession
    bool    isLive;               // ViewHealth == kLive
};

static bool ShouldEmergencyRelease(const Snapshot& s) {
    if (!s.ownershipActive) return false;
    if (!s.isLive) return false;                                   // loading views get their lifecycle, not this
    if (s.now - s.lastPresentMs >= kPresentAliveMs) return false;  // Present stalled or VR
    const int64_t relevantStart = imax(s.focusEpisodeStartMs, s.lastCompositeMs);
    if (relevantStart == 0) return false;                          // no baseline yet
    return (s.now - relevantStart) > kCompositeStaleMs;
}

static int g_failures = 0;
static void expect(bool cond, const char* what) {
    printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

int main() {
    printf("Composite-stale emergency-release proof (kLive-gated, focus-episode scoped)\n\n");
    const int64_t T = 1'000'000;
    const int64_t freshPresent = T - 3;

    // --- kLive, genuinely stuck -> RELEASE ---
    expect(ShouldEmergencyRelease({T, T - 3000, 0, freshPresent, true, true}),
           "kLive first-ever focus, never composited, dark >2s -> RELEASE");
    expect(ShouldEmergencyRelease({T, T - 2100, T - 22000, freshPresent, true, true}),
           "kLive focus reaches 2.1s with no composite (old menu composited 22s ago) -> RELEASE");

    // --- focus episode scoping (A->B handoff keeps g_focusSession true) ---
    expect(!ShouldEmergencyRelease({T, T - 100, T - 20000, freshPresent, true, true}),
           "A->B handoff: B focused 100ms ago, A composited 20s ago -> no release (B has its own window)");
    expect(!ShouldEmergencyRelease({T, T - 5000, T - 100, freshPresent, true, true}),
           "composite 100ms ago within this episode -> no release (timer reset)");

    // --- loading views must NOT be killed (Host allows up to 10s DOM-ready) ---
    expect(!ShouldEmergencyRelease({T, T - 3000, 0, freshPresent, true, /*isLive*/ false}),
           "kCreating/kDomReady loading view dark >2s -> no release (not kLive)");

    // --- other guards ---
    expect(!ShouldEmergencyRelease({T, T - 60000, T - 8, freshPresent, true, true}),
           "healthy continuous composites -> never release");
    expect(!ShouldEmergencyRelease({T, T - 5000, 0, freshPresent, /*ownership*/ false, true}),
           "no ownership -> no release");
    expect(!ShouldEmergencyRelease({T, T - 5000, 0, /*lastPresent*/ T - 900, true, true}),
           "Present stalled (presentAge>=500) or VR -> no release");

    // --- boundary ---
    expect(!ShouldEmergencyRelease({T, T - kCompositeStaleMs, 0, freshPresent, true, true}),
           "staleAge exactly kCompositeStaleMs -> no release");
    expect(ShouldEmergencyRelease({T, T - (kCompositeStaleMs + 1), 0, freshPresent, true, true}),
           "staleAge one ms past threshold -> RELEASE");

    // ---- unresponsive grace is wall-clock, not frame-count ------------------------------------
    constexpr int64_t kUnresponsiveGraceMs = 3000;
    auto unresponsiveDue = [](int64_t now, int64_t sinceMs) {
        return sinceMs != 0 && (now - sinceMs) >= kUnresponsiveGraceMs;
    };
    expect(!unresponsiveDue(T, 0), "unresponsive clock not started -> no release");
    expect(!unresponsiveDue(T, T - (kUnresponsiveGraceMs - 1)),
           "unresponsive for 2999ms -> no release");
    expect(unresponsiveDue(T, T - kUnresponsiveGraceMs),
           "unresponsive for 3000ms -> RELEASE (framerate-independent)");

    if (g_failures == 0) {
        printf("\nAll emergency-release decisions hold. PASS.\n");
        return 0;
    }
    printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
