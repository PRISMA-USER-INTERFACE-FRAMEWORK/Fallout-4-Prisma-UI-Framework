#include "PrismaUI/InputRegionPolicy.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

using namespace PrismaUI::InputRegionPolicy;

static int g_failures = 0;

static void expect(bool condition, const char* what)
{
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) ++g_failures;
}

int main()
{
    constexpr ViewId A = 10;
    constexpr ViewId B = 11;
    constexpr std::int32_t kMin = std::numeric_limits<std::int32_t>::min();
    constexpr std::int32_t kMax = std::numeric_limits<std::int32_t>::max();
    const std::array rects{InputRegion{10, 20, 30, 40}};

    State state;
    expect(!IsCursorOwned(state), "no focused view leaves the cursor unowned");
    expect(!ShouldRouteMouse(state, 15, 25) && !ShouldSwallowMouse(state, 15, 25),
           "no focused view routes or swallows mouse");

    SetFocusedView(state, A);
    expect(IsCursorOwned(state), "focused view owns the cursor");
    expect(ShouldRouteMouse(state, kMin, kMax) && ShouldSwallowMouse(state, kMin, kMax),
           "full capture routes and swallows any point while focused");

    SetMode(state, CaptureMode::OverlayRegions);
    SetRegions(state, A, rects);
    expect(ShouldRouteMouse(state, 10, 20) && ShouldSwallowMouse(state, 10, 20),
           "overlay routes and swallows the top-left corner of a declared rect");
    expect(ShouldRouteMouse(state, 39, 59), "overlay includes the last pixel before the right and bottom edges");
    expect(!ShouldRouteMouse(state, 40, 30), "overlay excludes the right edge");
    expect(!ShouldRouteMouse(state, 20, 60), "overlay excludes the bottom edge");
    expect(!ShouldRouteMouse(state, 9, 20) && !ShouldRouteMouse(state, 10, 19),
           "overlay excludes points left of and above a declared rect");
    expect(!ShouldSwallowMouse(state, 40, 30), "overlay passes mouse outside every declared rect");

    SetRegions(state, B, std::array{InputRegion{0, 0, 1000, 1000}});
    expect(ShouldRouteMouse(state, 15, 25) && !ShouldRouteMouse(state, 500, 500),
           "regions for a non-focused view are ignored");

    const std::array degenerate{
        InputRegion{0, 0, 0, 10}, InputRegion{0, 0, 10, 0},
        InputRegion{0, 0, -5, 10}, InputRegion{0, 0, 10, -5}};
    SetRegions(state, A, degenerate);
    expect(!ShouldRouteMouse(state, 0, 0) && !ShouldRouteMouse(state, 5, 5),
           "zero or negative width or height rects match nothing");

    std::vector<InputRegion> many(kMaxInputRegions + 1, InputRegion{0, 0, 1, 1});
    many.back() = InputRegion{500, 500, 10, 10};
    SetRegions(state, A, many);
    expect(state.regions.size() == kMaxInputRegions, "SetRegions truncates to kMaxInputRegions");
    expect(ShouldRouteMouse(state, 0, 0), "regions within the cap are kept");
    expect(!ShouldRouteMouse(state, 505, 505), "a region past the cap is dropped");

    State wide;
    SetFocusedView(wide, A);
    SetMode(wide, CaptureMode::OverlayRegions);
    const std::array overflowing{InputRegion{2147483000, 2147483000, 2000000000, 2000000000}};
    SetRegions(wide, A, overflowing);
    expect(ShouldRouteMouse(wide, 2147483600, 2147483600),
           "a region extending past INT32_MAX still contains points inside it");
    expect(!ShouldRouteMouse(wide, 100, 100),
           "a region extending past INT32_MAX does not cover the origin");

    SetMode(state, CaptureMode::OverlayRegions);
    SetRegions(state, A, rects);
    SetFocusedView(state, A);
    expect(state.regions.size() == 1 && state.mode == CaptureMode::OverlayRegions,
           "re-focusing the same view keeps its regions and mode");

    SetFocusedView(state, B);
    expect(state.regions.empty() && state.mode == CaptureMode::Full,
           "focusing a different view clears regions and resets mode to Full");
    expect(ShouldRouteMouse(state, 0, 0) && IsCursorOwned(state),
           "the newly focused view captures the full screen with cursor ownership");

    SetMode(state, CaptureMode::OverlayRegions);
    SetRegions(state, B, rects);
    ClearFocusedView(state, A);
    expect(IsCursorOwned(state) && state.regions.size() == 1,
           "ClearFocusedView for an unfocused view leaves focus and regions alone");
    OnViewDestroyed(state, A);
    expect(IsCursorOwned(state) && state.regions.size() == 1,
           "OnViewDestroyed for an unfocused view leaves focus and regions alone");

    ClearFocusedView(state, B);
    expect(!IsCursorOwned(state) && state.regions.empty() && state.mode == CaptureMode::Full,
           "ClearFocusedView for the focused view clears focus, mode and regions");

    SetFocusedView(state, A);
    SetMode(state, CaptureMode::OverlayRegions);
    SetRegions(state, A, rects);
    OnViewDestroyed(state, A);
    expect(!IsCursorOwned(state) && !ShouldRouteMouse(state, 15, 25) && state.focusedView == 0,
           "OnViewDestroyed for the focused view clears all capture state");

    SetFocusedView(state, A);
    SetFocusedView(state, 0);
    expect(!IsCursorOwned(state), "clearing focus with view 0 unowns the cursor");

    if (g_failures == 0) {
        std::printf("\nAll input-region-policy assertions passed.\n");
        return 0;
    }

    std::printf("\n%d assertion(s) FAILED.\n", g_failures);
    return 1;
}
