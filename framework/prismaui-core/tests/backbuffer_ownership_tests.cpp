// Unit tests for the PRODUCTION backbuffer-ownership classifier.
//
// The rejected first attempt at #89 shipped a test whose Detect::OwnershipProbe was an oracle
// (`return !AddRefsOnGetBuffer(c)`), so it proved that correct classification fixes the lifetime
// model -- never in doubt -- and nothing about whether the runtime decision could be produced.
// This file compiles the real ClassifyBackbufferOwnership and drives it directly.
//
// Build: g++ -std=c++17 -O0 tests/backbuffer_ownership_tests.cpp -o t && ./t

#include "../src/PrismaUI/BackbufferOwnership.h"

#include <cstdio>
#include <string>

using namespace PrismaUI::Backbuffer;

static int g_failures = 0;

static void expect(const std::string& what, const Observations& o, Ownership wantOwn, Reason wantReason) {
    const Classification got = ClassifyBackbufferOwnership(o);
    const bool ok = got.ownership == wantOwn && got.reason == wantReason;
    if (!ok) {
        printf("  [FAIL] %s\n         want %s/%s, got %s/%s\n", what.c_str(),
               ToString(wantOwn), ToString(wantReason), ToString(got.ownership), ToString(got.reason));
        ++g_failures;
    } else {
        printf("  ok    %-58s -> %s (%s)\n", what.c_str(), ToString(got.ownership), ToString(got.reason));
    }
}

static void check(bool cond, const std::string& what) {
    if (!cond) { printf("  [FAIL] %s\n", what.c_str()); ++g_failures; }
    else       { printf("  ok    %s\n", what.c_str()); }
}

// Full Auto pipeline: classify the observations, then resolve the runtime presentation mode.
static void expectAuto(const std::string& what, const Observations& o, PresentationMode want) {
    const Ownership own = ClassifyBackbufferOwnership(o).ownership;
    const PresentationMode got = ResolveBackbufferPresentationMode(own, o);
    if (got != want) {
        printf("  [FAIL] %s\n         want %s, got %s\n", what.c_str(), ToString(want), ToString(got));
        ++g_failures;
    } else {
        printf("  ok    %-58s -> %s\n", what.c_str(), ToString(got));
    }
}

// Resolver in isolation: a given ownership verdict must map to a mode independently of how it was
// reached (used to prove Owned/Borrowed pass through regardless of dimensions).
static void expectMode(const std::string& what, Ownership own, const Observations& o, PresentationMode want) {
    const PresentationMode got = ResolveBackbufferPresentationMode(own, o);
    if (got != want) {
        printf("  [FAIL] %s\n         want %s, got %s\n", what.c_str(), ToString(want), ToString(got));
        ++g_failures;
    } else {
        printf("  ok    %-58s -> %s\n", what.c_str(), ToString(got));
    }
}

// An ordinary, fully-working swapchain with nothing unusual loaded.
static Observations Plain() {
    Observations o;
    o.haveSwapChain = true;
    return o;
}

int main() {
    printf("backbuffer ownership classifier\n\n");

    printf("== automatic classification ==\n");
    {
        expect("no FG loaded + ordinary GetBuffer owner", Plain(), Ownership::Owned, Reason::NoFrameGen);
    }
    {
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.vtableOwnerIsKnownFrameGen = true;
        expect("FG loaded + known FG owns GetBuffer", o, Ownership::Borrowed,
               Reason::KnownFrameGenGetBufferOwner);
    }
    {
        Observations o = Plain();
        o.frameGenModuleLoaded = true;   // amd_fidelityfx_dx12.dll
        o.vtableOwnerIsKnownFrameGen = false;  // ENB's d3d11.dll owns vtable[9]
        expect("FG loaded + foreign wrapper + no definitive evidence", o, Ownership::Unknown,
               Reason::FrameGenLoadedUnresolved);
    }
    {
        // Native-resolution frame gen: nothing diverges, so there is still no evidence either way.
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.haveEngineBackbufferSize = true;
        o.swapChainDescWidth = 2560; o.swapChainDescHeight = 1440;
        o.engineBackbufferWidth = 2560; o.engineBackbufferHeight = 1440;
        expect("FG loaded + foreign wrapper + equal dimensions", o, Ownership::Unknown,
               Reason::FrameGenLoadedUnresolved);
        check(!HasSizeDivergence(o), "equal dimensions report no size divergence");
    }
    {
        // The reporter's observed numbers. Divergence is real evidence a proxy interposes, but it
        // does NOT prove borrowed GetBuffer semantics, so it must not resolve the classification.
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.haveEngineBackbufferSize = true;
        o.swapChainDescWidth = 3840; o.swapChainDescHeight = 2160;
        o.engineBackbufferWidth = 2560; o.engineBackbufferHeight = 1440;
        expect("FG loaded + foreign wrapper + 4K/1440p divergence", o, Ownership::Unknown,
               Reason::FrameGenLoadedUnresolved);
        check(HasSizeDivergence(o), "4K vs 1440p is reported as a size divergence (diagnostic only)");
    }
    {
        // FG library present but this swapchain is ordinary. Presence alone must never imply borrowed.
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        expect("FG loaded but irrelevant to this swapchain", o, Ownership::Unknown,
               Reason::FrameGenLoadedUnresolved);
        check(ClassifyBackbufferOwnership(o).ownership != Ownership::Borrowed,
              "module presence alone never classifies Borrowed");
    }
    {
        Observations o;  // haveSwapChain stays false
        expect("no swapchain yet", o, Ownership::Unknown, Reason::NoSwapChain);
    }

    printf("\n== R6: unrecognized foreign GetBuffer owner -> SafeSkip (crash safety) ==\n");
    {
        // An unknown ReShade/addon/custom module owns vtable[9]. Both guesses are unsafe: Owned
        // over-releases a borrowed pointer (crash); Borrowed Detaches an owned pointer (a leak that can
        // FAIL ResizeBuffers). So the classifier stays Unknown and Auto stays OFF the backbuffer.
        // (runtime_validation_matrix.md R6)
        Observations o = Plain();
        o.getBufferOwnerForeign = true;
        expect("foreign owner (classify) + no FG loaded", o, Ownership::Unknown, Reason::ForeignGetBufferOwner);
        expectAuto("foreign owner (Auto) + no FG loaded -> SafeSkip", o, PresentationMode::SafeSkip);
    }
    {
        // Same foreign owner, with an FG DLL also loaded -- still SafeSkip.
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.getBufferOwnerForeign = true;
        expect("foreign owner (classify) + FG loaded", o, Ownership::Unknown, Reason::ForeignGetBufferOwner);
        expectAuto("foreign owner (Auto) + FG loaded -> SafeSkip", o, PresentationMode::SafeSkip);
    }
    {
        // Equal engine/swapchain sizes are the LegacyOwned (Release) trigger for a NORMAL Unknown. A
        // foreign owner must NOT reach it -- it stays SafeSkip.
        Observations o = Plain();
        o.getBufferOwnerForeign = true;
        o.haveEngineBackbufferSize = true;
        o.swapChainDescWidth = 3840; o.swapChainDescHeight = 2160;
        o.engineBackbufferWidth = 3840; o.engineBackbufferHeight = 2160;
        expectAuto("foreign owner + equal sizes -> SafeSkip (NOT LegacyOwned)", o, PresentationMode::SafeSkip);
    }
    {
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.vtableOwnerIsVerifiedOwnedProxy = true;
        o.getBufferOwnerForeign = false;
        expect("verified owned proxy -> Owned", o, Ownership::Owned, Reason::KnownOwnedProxy);
        expectAuto("verified owned proxy (Auto) -> Owned mode", o, PresentationMode::Owned);
    }
    {
        // Ordering: a proxy that is both known frame-gen and verified-owned stays Borrowed, because
        // Prisma must not Release a pointer a frame-gen module handed back without an AddRef.
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.vtableOwnerIsKnownFrameGen = true;
        o.vtableOwnerIsVerifiedOwnedProxy = true;
        o.getBufferOwnerForeign = false;
        expect("known frame-gen owner outranks the verified owned proxy", o, Ownership::Borrowed,
               Reason::KnownFrameGenGetBufferOwner);
    }
    {
        Observations o = Plain();
        o.vtableOwnerIsVerifiedOwnedProxy = true;
        o.userOverride = OwnershipOverride::Borrowed;
        expect("forced Borrowed overrides the verified owned proxy", o, Ownership::Borrowed,
               Reason::ForcedByUser);
    }
    {
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.vtableOwnerIsVerifiedOwnedProxy = false;
        o.getBufferOwnerForeign = true;
        expect("unverified foreign owner (classify)", o, Ownership::Unknown, Reason::ForeignGetBufferOwner);
        expectAuto("unverified foreign owner (Auto) -> SafeSkip", o, PresentationMode::SafeSkip);
    }
    {
        // An allowlisted FG owner is NOT "foreign" (GetBufferOwnerIsForeign returns false for it), so
        // it stays on the known-FG borrowed path, not the foreign-owner one.
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.vtableOwnerIsKnownFrameGen = true;
        o.getBufferOwnerForeign = false;
        expect("allowlisted FG owner is not treated as foreign", o, Ownership::Borrowed,
               Reason::KnownFrameGenGetBufferOwner);
    }
    {
        // A user force still wins over the foreign-owner SafeSkip default (tester opt-in for a wrapper
        // known to be borrowed).
        Observations o = Plain();
        o.getBufferOwnerForeign = true;
        o.userOverride = OwnershipOverride::Borrowed;
        expect("forced Borrowed overrides the foreign-owner SafeSkip", o, Ownership::Borrowed,
               Reason::ForcedByUser);
        expectAuto("forced Borrowed on a foreign owner -> Borrowed mode", o, PresentationMode::Borrowed);
    }

    printf("\n== compatibility override (evaluated before automatic classification) ==\n");
    {
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.userOverride = OwnershipOverride::Owned;
        expect("forced Owned bypasses an otherwise-Unknown chain", o, Ownership::Owned, Reason::ForcedByUser);
    }
    {
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.userOverride = OwnershipOverride::Borrowed;
        expect("forced Borrowed bypasses an otherwise-Unknown chain", o, Ownership::Borrowed,
               Reason::ForcedByUser);
    }
    {
        // A forced mode must win even where Auto would have produced a confident, opposite answer.
        Observations o = Plain();
        o.userOverride = OwnershipOverride::Borrowed;
        expect("forced Borrowed overrides an Auto verdict of Owned", o, Ownership::Borrowed,
               Reason::ForcedByUser);
        Observations p = Plain();
        p.frameGenModuleLoaded = true;
        p.vtableOwnerIsKnownFrameGen = true;
        p.userOverride = OwnershipOverride::Owned;
        expect("forced Owned overrides an Auto verdict of Borrowed", p, Ownership::Owned, Reason::ForcedByUser);
    }
    {
        // The classifier is pure: running Auto must not mutate the caller's configuration.
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        const OwnershipOverride before = o.userOverride;
        (void)ClassifyBackbufferOwnership(o);
        (void)ClassifyBackbufferOwnership(o);
        check(o.userOverride == before, "Auto never silently mutates the configured override");
    }

    printf("\n== invariants ==\n");
    {
        // Unclassified is a state the classifier never returns; it only ever exists as the cached
        // "not asked yet" value, so Unknown and Unclassified can never be conflated by a caller.
        bool everUnclassified = false;
        for (int fg = 0; fg < 2; ++fg) {
            for (int vt = 0; vt < 2; ++vt) {
                for (int sc = 0; sc < 2; ++sc) {
                    for (int ov = 0; ov < 3; ++ov) {
                        Observations o;
                        o.haveSwapChain = sc != 0;
                        o.frameGenModuleLoaded = fg != 0;
                        o.vtableOwnerIsKnownFrameGen = vt != 0;
                        o.userOverride = static_cast<OwnershipOverride>(ov);
                        if (ClassifyBackbufferOwnership(o).ownership == Ownership::Unclassified) {
                            everUnclassified = true;
                        }
                    }
                }
            }
        }
        check(!everUnclassified, "classification never returns Unclassified (that state means 'not asked')");
    }
    {
        // Owned may only ever be reached with no FG relevance, or by explicit user force.
        bool ok = true;
        for (int vt = 0; vt < 2; ++vt) {
            Observations o = Plain();
            o.frameGenModuleLoaded = true;
            o.vtableOwnerIsKnownFrameGen = vt != 0;
            if (ClassifyBackbufferOwnership(o).ownership == Ownership::Owned) ok = false;
        }
        check(ok, "Auto never classifies Owned while a frame-gen module is loaded");
    }
    {
        // Crash-safety invariant: a foreign GetBuffer owner must never auto-classify Owned, with or
        // without an FG DLL loaded (only an explicit user force may).
        bool ok = true;
        for (int fg = 0; fg < 2; ++fg) {
            Observations o = Plain();
            o.getBufferOwnerForeign = true;
            o.frameGenModuleLoaded = fg != 0;
            if (ClassifyBackbufferOwnership(o).ownership == Ownership::Owned) ok = false;
        }
        check(ok, "Auto never classifies Owned when a foreign module owns GetBuffer");
    }
    {
        // A foreign owner must never reach a Release path (Owned or LegacyOwned) under Auto -- SafeSkip
        // only -- across FG-loaded x size-known x equal/divergent sizes.
        bool ok = true;
        for (int fg = 0; fg < 2; ++fg)
        for (int es = 0; es < 2; ++es)
        for (int eq = 0; eq < 2; ++eq) {
            Observations o = Plain();
            o.getBufferOwnerForeign = true;
            o.frameGenModuleLoaded = fg != 0;
            o.haveEngineBackbufferSize = es != 0;
            o.swapChainDescWidth = 3840; o.swapChainDescHeight = 2160;
            o.engineBackbufferWidth = eq ? 3840 : 2560; o.engineBackbufferHeight = eq ? 2160 : 1440;
            const Ownership own = ClassifyBackbufferOwnership(o).ownership;
            if (ResolveBackbufferPresentationMode(own, o) != PresentationMode::SafeSkip) ok = false;
        }
        check(ok, "Auto foreign owner always resolves to SafeSkip (never Owned/Borrowed/LegacyOwned)");
    }

    printf("\n== presentation policy (SEPARATE decision; Auto runtime behaviour) ==\n");
    {
        // Proven verdicts pass straight through, unchanged by size.
        Observations owned = Plain();  // no FG -> Owned
        expectAuto("no FG -> Owned verdict resolves to Owned mode", owned, PresentationMode::Owned);

        Observations borrowed = Plain();
        borrowed.frameGenModuleLoaded = true;
        borrowed.vtableOwnerIsKnownFrameGen = true;  // -> Borrowed
        expectAuto("known FG GetBuffer owner -> Borrowed mode", borrowed, PresentationMode::Borrowed);
    }
    {
        // The BROKEN reporter config: FG + foreign wrapper + 4K/1440p divergence. Ownership is Unknown
        // and the topology mismatch keeps Auto OFF the backbuffer. This is the narrow fallback.
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.haveEngineBackbufferSize = true;
        o.swapChainDescWidth = 3840; o.swapChainDescHeight = 2160;
        o.engineBackbufferWidth = 2560; o.engineBackbufferHeight = 1440;
        expectAuto("Unknown + 4K/1440p divergence -> SafeSkip (NOT Owned, NOT Borrowed)", o,
                   PresentationMode::SafeSkip);
    }
    {
        // The WORKING reporter config: FG + foreign wrapper + equal 4K dimensions. Ownership is STILL
        // Unknown -- this resolves to LegacyOwned (preserve the working path), explicitly NOT Owned.
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.haveEngineBackbufferSize = true;
        o.swapChainDescWidth = 3840; o.swapChainDescHeight = 2160;
        o.engineBackbufferWidth = 3840; o.engineBackbufferHeight = 2160;
        expectAuto("Unknown + equal 4K dimensions -> LegacyOwned (UNPROVEN, NOT Owned)", o,
                   PresentationMode::LegacyOwned);
        check(ResolveBackbufferPresentationMode(Ownership::Unknown, o) != PresentationMode::Owned,
              "equal-size Unknown is NEVER resolved to Owned mode");
    }
    {
        // Engine size not yet sampled this frame: skip safely and let the next frame retry.
        Observations o = Plain();
        o.frameGenModuleLoaded = true;
        o.haveEngineBackbufferSize = false;
        expectAuto("Unknown + no engine size -> SafeSkip (defer/retry)", o, PresentationMode::SafeSkip);
    }
    {
        // A forced mode produces its mode regardless of dimensions.
        Observations diverge = Plain();
        diverge.haveEngineBackbufferSize = true;
        diverge.swapChainDescWidth = 3840; diverge.swapChainDescHeight = 2160;
        diverge.engineBackbufferWidth = 2560; diverge.engineBackbufferHeight = 1440;

        Observations forcedOwned = diverge;
        forcedOwned.userOverride = OwnershipOverride::Owned;
        expectAuto("forced Owned -> Owned even with divergent dimensions", forcedOwned,
                   PresentationMode::Owned);

        Observations forcedBorrowed = diverge;
        forcedBorrowed.userOverride = OwnershipOverride::Borrowed;
        expectAuto("forced Borrowed -> Borrowed even with divergent dimensions", forcedBorrowed,
                   PresentationMode::Borrowed);

        // Resolver in isolation: Owned/Borrowed verdicts ignore the size fields entirely.
        expectMode("Owned verdict ignores dimensions", Ownership::Owned, diverge, PresentationMode::Owned);
        expectMode("Borrowed verdict ignores dimensions", Ownership::Borrowed, diverge,
                   PresentationMode::Borrowed);
    }
    {
        // Structural invariant: an Unclassified verdict (should never reach policy) is always SafeSkip,
        // and no observation can make an Unknown verdict resolve to Owned.
        Observations o = Plain();
        expectMode("Unclassified verdict -> SafeSkip", Ownership::Unclassified, o, PresentationMode::SafeSkip);
        bool everOwned = false;
        for (int es = 0; es < 2; ++es) {
            for (int w = 0; w < 2; ++w) {
                Observations u = Plain();
                u.frameGenModuleLoaded = true;
                u.haveEngineBackbufferSize = es != 0;
                u.swapChainDescWidth = 3840; u.swapChainDescHeight = 2160;
                u.engineBackbufferWidth = w ? 3840 : 2560; u.engineBackbufferHeight = w ? 2160 : 1440;
                if (ResolveBackbufferPresentationMode(Ownership::Unknown, u) == PresentationMode::Owned)
                    everOwned = true;
            }
        }
        check(!everOwned, "an Unknown verdict never resolves to Owned mode (LegacyOwned or SafeSkip only)");
    }
    // NOTE: the behavioural invariants for these modes -- SafeSkip performs zero GetBuffer / zero RTV
    // rebuilds / zero ambiguous releases, and LegacyOwned takes the normal GetBuffer+Release path --
    // are integration properties of the compositor. They belong to the production-wiring tests (step 7,
    // against the final compositor and presenter) and the deliberate 20+-RTV-rebuild reporter stress
    // gate, not to this pure-function suite.

    if (g_failures == 0) { printf("\nbackbuffer ownership classifier: PASS\n"); return 0; }
    printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
