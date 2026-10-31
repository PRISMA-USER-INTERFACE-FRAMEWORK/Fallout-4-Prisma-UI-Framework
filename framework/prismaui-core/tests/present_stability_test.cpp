// Pure tests for EvaluatePresentStability -- the reattach safety gate's stale-episode + observation
// -spacing state machine. Covers startup, recovery-between-probes, separate stale episodes, and a
// superseded probe executing right after its replacement (sub-spacing observations).
#include "Hooks/ReattachPolicy.h"

#include <cstdio>

using namespace PrismaUI::Hooks;

static int g_failures = 0;
static void expect(bool cond, const char* what) {
    std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

int main() {
    constexpr uintptr_t B = 0x7000'1000;
    constexpr uintptr_t C = 0x7000'2000;
    constexpr uintptr_t SWAPCHAIN_A = 0x3000'1000;
    constexpr uintptr_t SWAPCHAIN_B = 0x3000'2000;
    const StabilityConfig cfg{3000, 500};

    // --- Startup: never-presented is measured from watchdog startup, not instantly stale ----------
    {
        StabilityState st;
        StabilityInputs in;
        in.lastPresentMs = 0; in.startMs = 1000; in.liveTarget = B; in.generation = 1;
        in.nowMs = 3400;  // 2400ms since startup -> not yet stale
        expect(!EvaluatePresentStability(in, st, cfg).presentStale, "startup: within 3s window -> not stale");
        in.nowMs = 4100;  // 3100ms since startup -> stale
        expect(EvaluatePresentStability(in, st, cfg).presentStale, "startup: past 3s window -> stale");
    }
    // startMs unknown (0) and never presented -> cannot be stale (no reference yet)
    {
        StabilityState st;
        StabilityInputs in;
        in.lastPresentMs = 0; in.startMs = 0; in.liveTarget = B; in.nowMs = 999999;
        expect(!EvaluatePresentStability(in, st, cfg).presentStale, "no startup ref + never presented -> not stale");
    }

    // --- Healthy Present clears the episode snapshot ---------------------------------------------
    {
        StabilityState st;
        StabilityInputs in;
        in.startMs = 1000; in.liveTarget = B; in.generation = 1;
        in.lastPresentMs = 5000; in.nowMs = 5100;  // 100ms since present -> healthy
        auto v = EvaluatePresentStability(in, st, cfg);
        expect(!v.presentStale && !v.stable, "healthy present -> not stale, not stable");
        expect(st.lastProbeMs == 0, "healthy present resets the episode snapshot");
    }

    // --- First stale observation is never stable (only one observation) --------------------------
    {
        StabilityState st;
        StabilityInputs in;
        in.startMs = 1000; in.lastPresentMs = 1000; in.liveTarget = B; in.generation = 1;
        in.nowMs = 5000;  // 4000ms silent
        auto v = EvaluatePresentStability(in, st, cfg);
        expect(v.presentStale && !v.stable, "first stale observation -> stale but not yet stable");
    }

    // --- Two spaced observations in the same episode -> stable ------------------------------------
    {
        StabilityState st;
        StabilityInputs in;
        in.startMs = 1000; in.lastPresentMs = 1000; in.liveTarget = B; in.generation = 1;
        in.nowMs = 5000; EvaluatePresentStability(in, st, cfg);          // obs 1
        in.nowMs = 5600;                                                 // +600ms, same target/gen
        expect(EvaluatePresentStability(in, st, cfg).stable, "two same-episode observations >=500ms apart -> stable");
    }

    // --- Two observations <500ms apart -> not stable (superseded task runs right after replacement)-
    {
        StabilityState st;
        StabilityInputs in;
        in.startMs = 1000; in.lastPresentMs = 1000; in.liveTarget = B; in.generation = 1;
        in.nowMs = 5000; EvaluatePresentStability(in, st, cfg);          // obs 1
        in.nowMs = 5400;                                                 // +400ms only
        expect(!EvaluatePresentStability(in, st, cfg).stable, "second observation <500ms apart -> not stable");
    }

    // --- Different target / generation between observations -> not stable -------------------------
    {
        StabilityState st;
        StabilityInputs in;
        in.startMs = 1000; in.lastPresentMs = 1000; in.generation = 1;
        in.liveTarget = B; in.nowMs = 5000; EvaluatePresentStability(in, st, cfg);
        in.liveTarget = C; in.nowMs = 5600;
        expect(!EvaluatePresentStability(in, st, cfg).stable, "target changed between observations -> not stable");

        StabilityState st2;
        in.liveTarget = B; in.generation = 1; in.nowMs = 5000; EvaluatePresentStability(in, st2, cfg);
        in.generation = 2; in.nowMs = 5600;
        expect(!EvaluatePresentStability(in, st2, cfg).stable, "generation changed between observations -> not stable");
    }

    {
        StabilityState st;
        StabilityInputs in;
        in.startMs = 1000; in.lastPresentMs = 1000; in.generation = 1;
        in.liveTarget = B; in.liveSwapChain = SWAPCHAIN_A; in.nowMs = 5000;
        EvaluatePresentStability(in, st, cfg);
        in.liveSwapChain = SWAPCHAIN_B; in.nowMs = 5600;
        expect(!EvaluatePresentStability(in, st, cfg).stable,
               "swapchain changed between observations -> not stable");
        in.nowMs = 6200;
        expect(EvaluatePresentStability(in, st, cfg).stable,
               "same replacement swapchain observed twice -> stable");
    }

    // --- Stable verdict must NOT carry across separate outages (counter-based present-since) ------
    {
        StabilityState st;
        StabilityInputs in;
        in.startMs = 1000; in.generation = 1; in.liveTarget = B;
        // Episode A: Present last fired at ms 1000 (count 5); one observation at 5000.
        in.lastPresentMs = 1000; in.presentCount = 5; in.nowMs = 5000; EvaluatePresentStability(in, st, cfg);
        // Present recovered and re-froze (count advanced to 9) -> a *new* outage. First observation of
        // episode B sees the same target B, but the counter moved, so a Present occurred in between.
        in.lastPresentMs = 8000; in.presentCount = 9; in.nowMs = 12000;
        expect(!EvaluatePresentStability(in, st, cfg).stable,
               "first observation of a new outage is not stable even if the target matches an old one");
        // Second observation of episode B, spaced, counter unchanged -> now stable.
        in.nowMs = 12600;
        expect(EvaluatePresentStability(in, st, cfg).stable, "two observations within the new outage -> stable");
    }

    // --- A Present in the SAME millisecond as the prior observation is caught by the counter ------
    {
        StabilityState st;
        StabilityInputs in;
        in.startMs = 1000; in.generation = 1; in.liveTarget = B;
        // obs1 during an outage: last Present was long ago (ms 1000, count 3).
        in.lastPresentMs = 1000; in.presentCount = 3; in.nowMs = 5000; EvaluatePresentStability(in, st, cfg);
        // A single Present then fires at ms 5000 -- the SAME ms obs1 was taken -- so lastPresentMs
        // becomes 5000 and the counter bumps to 4. Present freezes again; obs2 at ms 8100 is stale.
        // A `lastPresentMs > lastProbeMs` compare would miss it (5000 > 5000 is false); the counter does not.
        in.lastPresentMs = 5000; in.presentCount = 4; in.nowMs = 8100;
        expect(!EvaluatePresentStability(in, st, cfg).stable,
               "a same-millisecond Present blocks stability (counter, not timestamp, decides present-since)");
    }

    if (g_failures == 0) {
        std::printf("\nAll present-stability assertions passed.\n");
        return 0;
    }
    std::printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
