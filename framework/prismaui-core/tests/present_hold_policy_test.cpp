#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>
#include "PrismaUI/PresentHoldPolicy.h"

using PrismaUI::PresentHold::Action;
using PrismaUI::PresentHold::AnyContentChanged;
using PrismaUI::PresentHold::ChooseOverlaySource;
using PrismaUI::PresentHold::Classify;
using PrismaUI::PresentHold::CommittedContainsRemoved;
using PrismaUI::PresentHold::HeldSlotKey;
using PrismaUI::PresentHold::HeldSlotReusable;
using PrismaUI::PresentHold::HeldSourceEpochValid;
using PrismaUI::PresentHold::kDarkLatchMs;
using PrismaUI::PresentHold::kHeldRefreshMinMs;
using PrismaUI::PresentHold::kOverlayHoldMs;
using PrismaUI::PresentHold::kStarvationMinMs;
using PrismaUI::PresentHold::OverlayAction;
using PrismaUI::PresentHold::OverlayContentClean;
using PrismaUI::PresentHold::OverlayInput;
using PrismaUI::PresentHold::OverlaySourceChoice;
using PrismaUI::PresentHold::PlanHeldCapture;
using PrismaUI::PresentHold::Policy;
using PrismaUI::PresentHold::ReconcileOverlay;
using PrismaUI::PresentHold::ReconcileOverlays;
using PrismaUI::PresentHold::ClassifyReuseDiagnostic;
using PrismaUI::PresentHold::IsStarvationCandidate;
using PrismaUI::PresentHold::ReuseDiagnosticInput;
using PrismaUI::PresentHold::ReuseReason;
using PrismaUI::PresentHold::ViewContent;
using PrismaUI::PresentHold::ViewSource;
static int g_failures = 0;
static void Check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}
static const char* Name(Action a) {
    switch (a) {
        case Action::Blank: return "Blank";
        case Action::ComposeCommit: return "ComposeCommit";
        case Action::BlitCommitted: return "BlitCommitted";
        case Action::Dark: return "Dark";
    }
    return "?";
}

static void CheckAction(Action got, Action want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "FAIL: %s (got %s, want %s)\n", what, Name(got), Name(want));
        ++g_failures;
    }
}

static std::vector<ViewSource> Views(std::size_t n, bool fresh, bool held) {
    std::vector<ViewSource> v;
    for (std::size_t i = 0; i < n; ++i) v.push_back(ViewSource{i + 1, fresh, held});
    return v;
}

static std::vector<uint64_t> Ids(std::size_t n) {
    std::vector<uint64_t> ids;
    for (std::size_t i = 0; i < n; ++i) ids.push_back(i + 1);
    return ids;
}

int main() {
    Check(!HeldSourceEpochValid(false, 7, 7), "device loss makes a held source ineligible before epoch advance");
    Check(!HeldSourceEpochValid(true, 7, 8), "a held source from an older device epoch is ineligible");
    Check(HeldSourceEpochValid(true, 7, 7), "a held source is eligible only for a valid matching epoch");

    {
        const auto allFresh = Classify(Views(24, true, false), Ids(24));
        Check(allFresh.complete && allFresh.committedFaithful, "24 fresh views classify complete + faithful");

        auto oneStaleHeld = Views(24, true, false);
        oneStaleHeld[6] = ViewSource{7, false, true};
        Check(Classify(oneStaleHeld, Ids(24)).complete,
              "a partial generation with a held source for the stale view stays complete");

        auto oneStaleNoHeld = Views(24, true, false);
        oneStaleNoHeld[6] = ViewSource{7, false, false};
        const auto missing = Classify(oneStaleNoHeld, Ids(24));
        Check(!missing.complete, "a displayed view with no source makes the frame incomplete (no element blink)");
        Check(missing.committedFaithful, "the committed frame is still faithful, so it presents whole instead");

        auto newView = Views(24, true, false);
        newView[6] = ViewSource{7, false, false};
        auto committedWithout7 = Ids(24);
        committedWithout7.erase(committedWithout7.begin() + 6);
        Check(Classify(newView, committedWithout7).complete,
              "a never-displayed view without a source may be omitted (progressive load)");

        auto hidOne = Views(24, false, true);
        hidOne.erase(hidOne.begin() + 11);
        const auto hid = Classify(hidOne, Ids(24));
        Check(hid.complete, "hide 1 of 24 during starvation: remaining held sources keep the frame complete");
        Check(!hid.committedFaithful, "the committed frame contains the hidden view and must not be blitted");

        auto hidAndMissing = Views(24, false, true);
        hidAndMissing.erase(hidAndMissing.begin() + 11);
        hidAndMissing[2] = ViewSource{3, false, false};
        const auto worst = Classify(hidAndMissing, Ids(24));
        Check(!worst.complete && !worst.committedFaithful,
              "hidden view + sourceless displayed view is neither composable nor blittable");
    }

    {
        Check(ChooseOverlaySource(true, true, true, 1000, 0) == OverlaySourceChoice::Live,
              "live overlays win when observable and available");
        Check(ChooseOverlaySource(false, true, true, 1000, 900) == OverlaySourceChoice::Held,
              "an overlay hiccup within the hold window uses the held copy (no blink)");
        Check(ChooseOverlaySource(false, true, true, 1000 + kOverlayHoldMs + 1, 1000) == OverlaySourceChoice::None,
              "a sustained overlay absence past the window is accepted as removal (one transition)");
        Check(ChooseOverlaySource(false, false, true, 1000000, 0) == OverlaySourceChoice::Held,
              "with no lease the live state is unobservable; absence of evidence is not removal");
        Check(ChooseOverlaySource(false, true, false, 1000, 1000) == OverlaySourceChoice::None,
              "nothing held and nothing live draws nothing");
        for (int64_t t = 1000; t < 1000 + kOverlayHoldMs; t += 16) {
            const bool live = (t / 16) % 2 == 0;
            const auto choice = ChooseOverlaySource(live, true, true, t, live ? t : t - 16);
            Check(choice != OverlaySourceChoice::None,
                  "an alternating overlay hiccup never produces a blink within the hold window");
        }
    }

    {
        Policy p;
        int64_t t = 1000;
        CheckAction(p.OnPresent(t, false, false, false, false, false).action, Action::Blank, "hidden UI blanks");
        CheckAction(p.OnPresent(t += 16, true, false, false, false, false).action, Action::Dark,
                    "visible with nothing composable and nothing committed is dark");
        CheckAction(p.OnPresent(t += 16, true, true, false, false, true).action, Action::ComposeCommit,
                    "a complete frame composes and commits");
        p.OnCommitSucceeded();
        p.OnBlitSucceeded();
        CheckAction(p.OnPresent(t += 16, true, false, true, true, false).action, Action::BlitCommitted,
                    "starvation presents the committed frame");
        CheckAction(p.OnPresent(t += 16, true, true, true, true, false).action, Action::BlitCommitted,
                    "a stable all-held complete frame blits instead of recomposing ~24 layers");
        CheckAction(p.OnPresent(t += 16, true, true, true, true, true).action, Action::ComposeCommit,
                    "dirty content (fresh, set change, or overlay change) recomposes");
    }

    {
        Policy p;
        int64_t t = 1000;
        for (int i = 0; i < 200; ++i) {
            CheckAction(p.OnPresent(t += 16, true, true, true, true, true).action, Action::ComposeCommit,
                        "compose-failure cadence: complete present composes");
            CheckAction(p.OnComposeFailed(true, true), Action::BlitCommitted,
                        "repeated compose failure falls back to the committed frame, never dark");
            p.OnBlitSucceeded();
        }
        Check(p.ComposeFailures() == 200, "compose failures are counted");
    }

    {
        const ReuseDiagnosticInput normal{true, false, false, false, false, false, false, true, true};
        const auto normalDiagnostic = ClassifyReuseDiagnostic(normal, false);
        Check(normalDiagnostic.reason == ReuseReason::NoRecompose && !normalDiagnostic.starvationCandidate,
              "complete unchanged faithful reuse is no-recompose, not starvation");

        const ReuseDiagnosticInput missing{true, true, false, false, false, false, true, true, true};
        Check(ClassifyReuseDiagnostic(missing, false).reason == ReuseReason::FreshSourceMissing,
              "missing fresh content has its own reuse reason");
        Check(IsStarvationCandidate(missing, true), "missing fresh content held past the threshold is a starvation candidate");
        Check(!IsStarvationCandidate(missing, false), "a temporary fresh-source loss is not starvation");

        const ReuseDiagnosticInput generation{true, false, true, false, false, false, true, true, true};
        Check(ClassifyReuseDiagnostic(generation, true).reason == ReuseReason::ContentGenerationStalled,
              "stalled content generations are classified separately");
        const ReuseDiagnosticInput viewSet{true, false, false, true, false, false, true, true, true};
        Check(ClassifyReuseDiagnostic(viewSet, false).reason == ReuseReason::ViewSetChange,
              "view-set changes are classified separately");
        Check(!IsStarvationCandidate(viewSet, true), "view-set changes cannot be starvation");
        const ReuseDiagnosticInput overlay{true, false, false, false, true, false, true, true, true};
        Check(ClassifyReuseDiagnostic(overlay, false).reason == ReuseReason::OverlayChange,
              "overlay changes are classified separately");
        const ReuseDiagnosticInput compose{true, false, false, false, false, true, true, true, true};
        Check(ClassifyReuseDiagnostic(compose, false).reason == ReuseReason::ComposeFailure,
              "compose fallback is classified separately");
        const ReuseDiagnosticInput noLease{false, true, false, false, false, false, true, true, true};
        const auto noLeaseDiagnostic = ClassifyReuseDiagnostic(noLease, true);
        Check(noLeaseDiagnostic.reason == ReuseReason::PresentLeaseUnavailable &&
                  !noLeaseDiagnostic.starvationCandidate,
              "a missing presentation lease is not producer starvation");

        Policy p;
        int64_t t = 1000;
        CheckAction(p.OnPresent(t, true, true, false, false, true).action, Action::ComposeCommit,
                    "changed content with a fresh target composes");
        p.OnCommitSucceeded();
        (void)p.ObserveStarvation(t, true);
        Check(!p.ObserveStarvation(t + 1, false), "a successful ComposeCommit clears the starvation timer");
        p.OnBlitSucceeded();
        CheckAction(p.OnPresent(t += 16, true, false, true, true, false).action, Action::BlitCommitted,
                    "a held frame survives a temporary fresh-source loss");
        Check(ClassifyReuseDiagnostic({true, true, false, false, false, false, false, true, true}, false).reason ==
                  ReuseReason::FreshSourceMissing,
              "temporary fresh-source loss remains diagnosable without being starvation");

        Policy starvation;
        Check(!starvation.ObserveStarvation(1000, true), "starvation timer starts without warning immediately");
        Check(!starvation.ObserveStarvation(1000 + kStarvationMinMs - 1, true),
              "starvation timer waits for the sustained interval");
        Check(starvation.ObserveStarvation(1000 + kStarvationMinMs, true),
              "sustained missing content becomes a starvation candidate");
        Check(!starvation.ObserveStarvation(1000 + kStarvationMinMs + 1, false),
              "lease loss or normal reuse clears the starvation timer");
        (void)starvation.ObserveStarvation(4000, true);
        Check(!starvation.ObserveStarvation(4001, false), "hidden or removed views clear the starvation timer");

        Policy staticUi;
        const std::map<uint64_t, uint64_t> staticGeneration{{1, 7}};
        bool staticCandidate = false;
        for (int64_t now = 1000; now <= 61000; now += 16) {
            (void)staticUi.ObserveFreshContentGenerations(staticGeneration, now);
            staticCandidate = staticCandidate || staticUi.ObserveStarvation(now, false);
        }
        Check(!staticCandidate, "static unchanged UI for 60 seconds never becomes starvation");

        Policy generations;
        const std::map<uint64_t, uint64_t> observed{{1, 7}};
        Check(generations.ObserveFreshContentGenerations(observed, 1000), "first generation observation is fresh");
        Check(!generations.ObserveFreshContentGenerations(observed, 1001), "same generation is normal reuse");
        (void)generations.ObserveStarvation(1001, true);
        Check(!generations.ContentGenerationStalled(1000 + kStarvationMinMs),
              "a repeated generation only starts the stall timer");
        Check(generations.ContentGenerationStalled(1001 + kStarvationMinMs),
              "an unchanged fresh generation becomes stalled after the interval");
        Check(generations.ObserveFreshContentGenerations({{1, 8}}, 2900), "fresh generation arrives before timeout");
        Check(!generations.ObserveStarvation(2900, false), "fresh generation arrival resets starvation timing");
        Check(!generations.ContentGenerationStalled(3002), "a new content generation clears the stall");
    }

    for (const int64_t frameMs : {16, 4, 3}) {
        Policy p;
        int64_t t = 1000;
        CheckAction(p.OnPresent(t, true, true, false, false, true).action, Action::ComposeCommit, "latch: compose");
        p.OnCommitSucceeded();
        p.OnBlitSucceeded();
        uint64_t transitions = 0;
        bool wasVisible = true;
        const int64_t start = t;
        const int presents = static_cast<int>((kDarkLatchMs * 10) / frameMs);
        for (int i = 0; i < presents; ++i) {
            t += frameMs;
            const auto d = p.OnPresent(t, true, true, true, true, true);
            bool visibleNow = false;
            if (d.action == Action::ComposeCommit || d.action == Action::BlitCommitted) {
                p.OnBlitFailed(t);
            }
            Check(d.action != Action::Blank, "latch cadence: a visible episode never sees a hide-style Blank");
            if (wasVisible && !visibleNow) ++transitions;
            wasVisible = visibleNow;
        }
        const uint64_t bound = static_cast<uint64_t>((t - start) / kDarkLatchMs) + 2;
        Check(transitions <= bound,
              "persistent blit failure is wall-clock rate-bound at any frame rate, never per-present");
        Check(p.Latched(t), "after persistent blit failure the policy is latched dark");
        while (p.Latched(t + frameMs)) {
            t += frameMs;
            CheckAction(p.OnPresent(t, true, true, true, true, true).action, Action::Dark,
                        "during the wall-clock latch every present is dark, even complete fresh ones");
        }
        t += frameMs;
        CheckAction(p.OnPresent(t, true, true, true, true, true).action, Action::ComposeCommit,
                    "after the latch window the pipeline retries");
        p.OnCommitSucceeded();
        p.OnBlitSucceeded();
        CheckAction(p.OnPresent(t += frameMs, true, false, true, true, false).action, Action::BlitCommitted,
                    "recovered pipeline holds normally again");
    }

    {
        Policy p;
        int64_t t = 1000;
        CheckAction(p.OnPresent(t, true, true, false, false, true).action, Action::ComposeCommit, "skip: compose");
        p.OnCommitSucceeded();
        p.OnBlitSkipped(t);
        Check(!p.Latched(t + 1), "SafeSkip before anything displayed does not latch (startup/persistent skip)");
        CheckAction(p.OnPresent(t += 16, true, true, false, true, true).action, Action::ComposeCommit,
                    "pre-display SafeSkip does not gate the next present");
        p.OnCommitSucceeded();
        p.OnBlitSucceeded();
        p.OnBlitSkipped(t += 16);
        Check(p.Latched(t + 1), "a SafeSkip AFTER visible output latches: Success->SafeSkip->Success would strobe");
        CheckAction(p.OnPresent(t += 16, true, true, true, true, true).action, Action::Dark,
                    "the post-display skip latch holds fresh presents dark");
        t += kDarkLatchMs;
        CheckAction(p.OnPresent(t, true, true, true, true, true).action, Action::ComposeCommit,
                    "the skip latch expires on the wall clock");
    }

    {
        Policy p;
        int64_t t = 1000;
        (void)p.OnPresent(t, true, true, false, false, true);
        p.OnBlitFailed(t);
        Check(p.Latched(t + 1), "latch armed");
        CheckAction(p.OnPresent(t += 16, false, false, false, false, false).action, Action::Blank,
                    "hide blanks while latched");
        Check(!p.Latched(t), "hiding the UI clears the dark latch (episode-scoped)");
        CheckAction(p.OnPresent(t += 16, true, true, false, false, true).action, Action::ComposeCommit,
                    "a reopened UI composes immediately");
    }

    {
        Policy p;
        int64_t t = 1000;
        Check(p.ShouldRefreshHeldSources(t), "the first capture opportunity is immediate");
        p.OnHeldSourcesRefreshed(t);
        Check(!p.ShouldRefreshHeldSources(t + kHeldRefreshMinMs - 1), "refresh is not due within the interval");
        Check(p.ShouldRefreshHeldSources(t + kHeldRefreshMinMs), "refresh becomes due after the interval");
        uint64_t refreshes = 0;
        for (int i = 0; i < 4000; ++i) {
            t += 4;
            if (p.ShouldRefreshHeldSources(t)) {
                p.OnHeldSourcesRefreshed(t);
                ++refreshes;
            }
        }
        Check(refreshes <= static_cast<uint64_t>(4000 * 4 / kHeldRefreshMinMs) + 1,
              "held-source deep copies stay wall-clock rate-limited even at 240 FPS");
    }

    {
        Policy p;
        int64_t t = 1000;
        for (int i = 0; i < 300; ++i) {
            const bool complete = true;
            const bool fresh = (i % 2) == 0;
            const auto d = p.OnPresent(t += 16, true, complete, true, i > 0, fresh);
            if (d.action == Action::ComposeCommit) {
                p.OnCommitSucceeded();
                p.OnBlitSucceeded();
            } else if (d.action == Action::BlitCommitted) {
                p.OnBlitSucceeded();
            } else if (i > 0) {
                Check(false, "healthy alternating fresh/starved cadence must never go dark");
            }
        }
    }

    {
        std::map<uint64_t, uint64_t> committed;
        for (uint64_t id = 1; id <= 24; ++id) committed[id] = 100 + id;

        std::vector<ViewContent> steady;
        for (uint64_t id = 1; id <= 24; ++id) steady.push_back(ViewContent{id, true, 100 + id});
        Check(!AnyContentChanged(steady, committed),
              "24 carry-forward views at their committed content generation are not dirty (blit, never recompose)");

        auto oneChanged = steady;
        oneChanged[9] = ViewContent{10, true, 999};
        Check(AnyContentChanged(oneChanged, committed),
              "a single view whose content generation advanced marks the frame dirty");

        auto newFresh = steady;
        newFresh.push_back(ViewContent{25, true, 500});
        Check(AnyContentChanged(newFresh, committed),
              "a fresh view absent from the committed set is dirty (first paint must compose)");

        auto unknownGen = steady;
        unknownGen[0] = ViewContent{1, true, 0};
        Check(AnyContentChanged(unknownGen, committed),
              "an unknown content generation (0) is treated as dirty (never silently hold stale pixels)");

        std::vector<ViewContent> heldOnly;
        for (uint64_t id = 1; id <= 24; ++id) heldOnly.push_back(ViewContent{id, false, 0});
        Check(!AnyContentChanged(heldOnly, committed),
              "held-only views never force a recompose; their pixels are already committed");

        Policy p;
        int64_t t = 1000;
        CheckAction(p.OnPresent(t, true, true, false, false, true).action, Action::ComposeCommit,
                    "content gate: the first complete frame composes");
        p.OnCommitSucceeded();
        p.OnBlitSucceeded();
        CheckAction(p.OnPresent(t += 16, true, true, true, true, AnyContentChanged(steady, committed)).action,
                    Action::BlitCommitted,
                    "content gate: unchanged carry-forward views blit the committed frame, no recompose");
    }

    {
        Check(OverlayContentClean(5, 5), "a known overlay generation matching the committed one is clean (may hold)");
        Check(!OverlayContentClean(5, 6), "an advanced overlay generation is dirty (animated preview must recompose)");
        Check(!OverlayContentClean(6, 5), "any overlay generation mismatch is dirty");
        Check(!OverlayContentClean(0, 0),
              "generation 0 is unknown/live content (e.g. the engine local map) and is never clean -- always recomposes");
        Check(!OverlayContentClean(0, 5), "an unknown live generation against a known committed one is dirty");
        Check(!OverlayContentClean(5, 0), "a known live generation against an unknown committed one is dirty");
    }

    {
        using OA = OverlayAction;
        Check(ReconcileOverlay(true, false, false) == OA::DrawLive, "live -> DrawLive");
        Check(ReconcileOverlay(false, true, false) == OA::DrawHeld, "held not live -> DrawHeld");
        Check(ReconcileOverlay(false, true, true) == OA::Drop, "tombstoned -> Drop");
        Check(ReconcileOverlay(false, false, false) == OA::Drop, "nothing -> Drop");
        Check(ReconcileOverlay(true, true, true) == OA::DrawLive, "live over tombstone -> DrawLive");

        std::vector<OverlayInput> in;
        in.push_back({10, 2, true, true, false});
        in.push_back({11, 0, false, true, true});
        in.push_back({12, 1, false, true, false});
        in.push_back({13, 3, true, false, false});
        const auto out = ReconcileOverlays(in);
        Check(out.size() == 3, "independence: 1 of 4 drops");
        Check(out[0].key == 12 && out[1].key == 10 && out[2].key == 13, "z-order preserved");
        Check(out[0].action == OA::DrawHeld, "order0 held");
        Check(out[1].action == OA::DrawLive, "order1 live");
        Check(out[2].action == OA::DrawLive, "order2 new live");

        std::vector<OverlayInput> starved;
        starved.push_back({20, 0, false, true, false});
        starved.push_back({21, 1, false, true, true});
        const auto sout = ReconcileOverlays(starved);
        Check(sout.size() == 1 && sout[0].key == 20 && sout[0].action == OA::DrawHeld, "no-lease removal");

        Check(ReconcileOverlay(false, true, false) == OA::DrawHeld, "replacement loading keeps held");
        Check(ReconcileOverlay(true, true, false) == OA::DrawLive, "replacement ready draws live");

        const std::vector<uint64_t> oldHeld = {1, 2, 3};
        const std::vector<uint64_t> liveKeys = {2, 4};
        Check(PlanHeldCapture(oldHeld, liveKeys, {true, false}) == oldHeld, "capture failure -> unchanged");
        Check((PlanHeldCapture(oldHeld, liveKeys, {true, true}) == std::vector<uint64_t>{1, 2, 3, 4}),
              "capture success -> order kept, new appended");
    }

    {
        using OA = OverlayAction;
        std::vector<OverlayInput> abc;
        abc.push_back({30, 0, true, true, false});
        abc.push_back({20, 1, false, true, false});
        abc.push_back({10, 2, true, false, false});
        auto r1 = ReconcileOverlays(abc);
        Check(r1.size() == 3 && r1[0].key == 30 && r1[1].key == 20 && r1[2].key == 10, "zorder by index not key");
        Check(r1[1].action == OA::DrawHeld, "B temp-lost draws held");
        abc[1] = {20, 1, true, true, false};
        abc[2] = {10, 2, false, true, true};
        auto r2 = ReconcileOverlays(abc);
        Check(r2.size() == 2 && r2[0].key == 30 && r2[1].key == 20, "C removed, A B keep order");
        Check(r2[1].action == OA::DrawLive, "B returned draws live");

        const std::vector<uint64_t> committedOverlays = {100, 101};
        const std::vector<uint64_t> removed = {101};
        Check(CommittedContainsRemoved(committedOverlays, removed), "stale committed detected");
        Check(!CommittedContainsRemoved({100}, removed), "non-stale committed not flagged");
        Policy stale;
        int64_t ts = 1000;
        CheckAction(stale.OnPresent(ts, true, true, false, false, true).action, Action::ComposeCommit,
                    "stale first compose");
        stale.OnCommitSucceeded();
        stale.OnBlitSucceeded();
        CheckAction(stale.OnPresent(ts += 16, true, false, false, false, true).action, Action::Dark,
                    "incomplete + invalidated committed -> dark not stale blit");
    }

    {
        // Fields: w,h,texFormat,mip,array,sampleCount,sampleQuality,srvFormat,srvViewDim,srvMostDetailedMip,srvMipLevels
        // DXGI codes: B8G8R8A8_TYPELESS=90 UNORM=87 UNORM_SRGB=91; D3D11_SRV_DIMENSION_TEXTURE2D=4
        const HeldSlotKey typelessUnorm{512, 512, 90, 1, 1, 1, 0, 87, 4, 0, 1};
        const HeldSlotKey typelessSrgb{512, 512, 90, 1, 1, 1, 0, 91, 4, 0, 1};
        const HeldSlotKey typedUnorm{512, 512, 87, 1, 1, 1, 0, 87, 4, 0, 1};
        Check(HeldSlotReusable(typelessUnorm, typelessUnorm), "identical held slot key reuses");
        Check(!HeldSlotReusable(typelessUnorm, typelessSrgb),
              "typeless texture + differing typed SRV format must NOT reuse");
        Check(!HeldSlotReusable(typelessUnorm, typedUnorm), "differing texture format must not reuse");
        HeldSlotKey biggerDim = typelessUnorm;
        biggerDim.width = 1024;
        Check(!HeldSlotReusable(typelessUnorm, biggerDim), "differing size must not reuse");
        HeldSlotKey diffView = typelessUnorm;
        diffView.srvViewDim = 5;
        Check(!HeldSlotReusable(typelessUnorm, diffView), "differing SRV view dimension must not reuse");
        HeldSlotKey diffDetailedMip = typelessUnorm;
        diffDetailedMip.srvMostDetailedMip = 1;
        Check(!HeldSlotReusable(typelessUnorm, diffDetailedMip), "differing SRV MostDetailedMip must not reuse");
        HeldSlotKey diffSrvMips = typelessUnorm;
        diffSrvMips.srvMipLevels = 2;
        Check(!HeldSlotReusable(typelessUnorm, diffSrvMips), "differing SRV MipLevels must not reuse");
        HeldSlotKey diffSample = typelessUnorm;
        diffSample.sampleCount = 4;
        Check(!HeldSlotReusable(typelessUnorm, diffSample), "differing sample count must not reuse");
    }

    if (g_failures == 0) {
        std::printf(
            "ok    present policy: wall-clock latch (incl. SafeSkip-after-display), completeness, overlay "
            "continuity, content-generation recompose gate\n");
        return 0;
    }
    std::fprintf(stderr, "%d present policy failure(s)\n", g_failures);
    return 1;
}
