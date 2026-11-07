#include "DeterministicTestSupport.h"
#include "PrismaUI/InputRegionPolicy.h"

#include <array>
#include <vector>

namespace DeterministicTests
{
    void TestInputRegionPolicy()
    {
        namespace Policy = PrismaUI::InputRegionPolicy;

        Policy::State state{};
        Policy::SetFocusedView(state, 10);
        Policy::SetMode(state, Policy::CaptureMode::Full);
        Expect(
            Policy::ShouldRouteMouse(state, 1000, 1000),
            "legacy full focus routes mouse outside any registered region");
        Expect(
            Policy::ShouldSwallowMouse(state, 1000, 1000),
            "legacy full focus swallows mouse outside any registered region");
        Expect(
            Policy::IsCursorOwned(state),
            "focused view owns the cursor even before overlay regions are registered");

        const std::array regions{
            Policy::InputRegion{20, 30, 100, 40},
            Policy::InputRegion{300, 40, 70, 60}
        };
        Policy::SetMode(state, Policy::CaptureMode::OverlayRegions);
        Policy::SetRegions(state, 10, regions);
        Expect(
            Policy::ShouldRouteMouse(state, 25, 35) &&
                Policy::ShouldSwallowMouse(state, 25, 35),
            "overlay focus routes and swallows mouse inside a registered region");
        Expect(
            !Policy::ShouldRouteMouse(state, 10, 10) &&
                !Policy::ShouldSwallowMouse(state, 10, 10),
            "overlay focus passes mouse outside every registered region");

        Policy::SetFocusedView(state, 11);
        Policy::SetMode(state, Policy::CaptureMode::OverlayRegions);
        Expect(
            !Policy::ShouldRouteMouse(state, 25, 35),
            "focus switch clears the previous view's regions");
        Expect(
            Policy::IsCursorOwned(state),
            "cursor remains owned after focusing a new view");

        Policy::SetRegions(state, 11, regions);
        Policy::ClearFocusedView(state, 11);
        Expect(
            !Policy::IsCursorOwned(state) &&
                !Policy::ShouldRouteMouse(state, 25, 35),
            "unfocus clears cursor ownership and overlay regions");

        Policy::SetFocusedView(state, 12);
        Policy::SetMode(state, Policy::CaptureMode::OverlayRegions);
        Policy::SetRegions(state, 12, regions);
        Policy::OnViewDestroyed(state, 12);
        Expect(
            !Policy::IsCursorOwned(state) &&
                !Policy::ShouldRouteMouse(state, 25, 35),
            "destroying a focused overlay clears all capture state");

        Policy::State wide{};
        Policy::SetFocusedView(wide, 13);
        Policy::SetMode(wide, Policy::CaptureMode::OverlayRegions);
        const std::array overflowing{
            Policy::InputRegion{2147483000, 2147483000, 2000000000, 2000000000}
        };
        Policy::SetRegions(wide, 13, overflowing);
        Expect(
            Policy::ShouldRouteMouse(wide, 2147483600, 2147483600),
            "a region extending past INT32_MAX still contains points inside it");
        Expect(
            !Policy::ShouldRouteMouse(wide, 100, 100),
            "a region extending past INT32_MAX does not swallow the whole screen");

        Policy::State capped{};
        Policy::SetFocusedView(capped, 14);
        Policy::SetMode(capped, Policy::CaptureMode::OverlayRegions);
        std::vector<Policy::InputRegion> many(Policy::kMaxInputRegions + 64,
                                              Policy::InputRegion{0, 0, 10, 10});
        Policy::SetRegions(capped, 14, many);
        Expect(
            capped.regions.size() == Policy::kMaxInputRegions,
            "SetRegions truncates an over-long region list to the cap");
        Expect(
            Policy::ShouldRouteMouse(capped, 5, 5),
            "a truncated region list still routes the mouse inside its kept regions");
    }
}
