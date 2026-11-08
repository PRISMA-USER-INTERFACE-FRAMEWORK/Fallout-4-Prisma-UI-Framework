#include "PrismaUI/FocusCursorSamplePolicy.h"

#include <cstdio>

using PrismaUI::FocusCursorSamplePolicy::Episode;

static int g_failures = 0;

static void expect(bool condition, const char* what)
{
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) ++g_failures;
}

int main()
{
    Episode episode;

    expect(!episode.IsCurrent(7, 1), "no episode is current before Begin");
    expect(!episode.ClaimAfterRegistration(7, 1), "no post-registration sample before Begin");
    expect(!episode.ClaimMovementSample(7, 1), "no movement sample before Begin");

    episode.Begin(7, 1);
    expect(episode.IsCurrent(7, 1), "the started episode is current");
    expect(episode.View() == 7 && episode.Generation() == 1, "the episode keeps its view and generation");
    expect(!episode.IsCurrent(8, 1), "a different view is not current");
    expect(!episode.IsCurrent(7, 2), "a different generation is not current");
    expect(!episode.ClaimMovementSample(7, 1), "the movement sample waits for the post-registration sample");

    expect(episode.ClaimAfterRegistration(7, 1), "the post-registration sample is claimed once");
    expect(!episode.ClaimAfterRegistration(7, 1), "the post-registration sample cannot be claimed twice");

    expect(episode.ClaimMovementSample(7, 1), "the movement sample is claimed once");
    expect(!episode.ClaimMovementSample(7, 1), "the movement sample cannot be claimed twice after it fires");

    episode.ReleaseMovementSample(7, 1);
    expect(episode.ClaimMovementSample(7, 1), "a released movement sample can be retried in the same episode");
    expect(!episode.ClaimMovementSample(7, 1), "the retried movement sample still fires only once");

    episode.End();
    expect(!episode.IsCurrent(7, 1), "an ended episode is not current");
    expect(!episode.ClaimAfterRegistration(7, 1), "post-registration claims are rejected after End");
    expect(!episode.ClaimMovementSample(7, 1), "movement claims are rejected after End");

    episode.Begin(7, 2);
    expect(!episode.ClaimAfterRegistration(7, 1), "a superseded generation cannot claim post-registration");
    expect(!episode.ClaimMovementSample(7, 1), "a superseded generation cannot claim a movement sample");
    expect(episode.ClaimAfterRegistration(7, 2), "the current generation claims post-registration");
    expect(episode.ClaimMovementSample(7, 2), "the current generation claims the movement sample");

    episode.Begin(9, 3);
    expect(!episode.IsCurrent(7, 2), "a replaced view episode is not current");
    expect(episode.ClaimAfterRegistration(9, 3), "the replacement episode claims post-registration");
    expect(episode.ClaimMovementSample(9, 3), "the replacement episode claims the movement sample");

    episode.ReleaseMovementSample(7, 3);
    expect(episode.ClaimMovementSample(9, 3) == false, "a release from a stale view does not reopen the sample");

    episode.Begin(0, 4);
    expect(!episode.IsCurrent(0, 4) && !episode.IsCurrent(9, 4), "a zero view starts no episode");

    episode.Begin(11, 0);
    expect(!episode.IsCurrent(11, 0), "a zero generation starts no episode");

    if (g_failures == 0) {
        std::printf("\nAll focus-cursor-sample-policy assertions passed.\n");
        return 0;
    }

    std::printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
