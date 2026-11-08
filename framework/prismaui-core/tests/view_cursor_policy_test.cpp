#include "PrismaUI/ViewCursorPolicy.h"

#include <cstdio>

using namespace PrismaUI::ViewCursorPolicy;

static int g_failures = 0;

static void expect(bool condition, const char* what)
{
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) ++g_failures;
}

int main()
{
    constexpr ViewId A = 101;
    constexpr ViewId B = 202;
    constexpr ViewId C = 303;

    State state;

    expect(!ShouldDrawCompositorCursor(state, false), "cursor not owned -> hidden");
    expect(ShouldDrawCompositorCursor(state, true), "owned cursor with no focused view uses framework fallback");

    SetPolicy(state, A, Policy::Hidden);
    OnFocusChanged(state, A);
    expect(GetPolicy(state, A) == Policy::Hidden, "focused view keeps its hidden policy");
    expect(!ShouldDrawCompositorCursor(state, true), "hidden focused view suppresses compositor cursor");

    SetPolicy(state, A, Policy::Default);
    expect(GetPolicy(state, A) == Policy::Default, "default policy clears the per-view override");
    expect(ShouldDrawCompositorCursor(state, true), "default focused view keeps compositor cursor");

    SetPolicy(state, A, Policy::Hidden);
    SetPolicy(state, B, Policy::Hidden);
    OnFocusChanged(state, B);
    expect(GetPolicy(state, A) == Policy::Default, "focus transfer clears the outgoing view policy");
    expect(GetPolicy(state, B) == Policy::Hidden, "focus transfer preserves the incoming view policy");
    expect(!ShouldDrawCompositorCursor(state, true), "incoming hidden view suppresses only its own cursor");

    SetPolicy(state, B, Policy::Default);
    expect(ShouldDrawCompositorCursor(state, true), "incoming view can restore the framework cursor");

    SetPolicy(state, B, Policy::Hidden);
    OnFocusChanged(state, 0);
    expect(GetPolicy(state, B) == Policy::Default, "unfocus clears the outgoing view policy");
    expect(ShouldDrawCompositorCursor(state, true), "dock/system cursor fallback is unaffected by a prior hidden view");

    SetPolicy(state, C, Policy::Hidden);
    OnFocusChanged(state, C);
    OnViewDestroyed(state, C);
    expect(state.focusedView == 0, "destroying the focused view clears cursor-policy ownership");
    expect(GetPolicy(state, C) == Policy::Default, "destroying a view removes its cursor policy");

    OnFocusChanged(state, A);
    expect(GetPolicy(state, A) == Policy::Default, "refocusing a previous view does not revive a stale hidden policy");
    expect(ShouldDrawCompositorCursor(state, true), "refocused default view renders the compositor cursor");

    if (g_failures == 0) {
        std::printf("\nAll view-cursor-policy assertions passed.\n");
        return 0;
    }

    std::printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
