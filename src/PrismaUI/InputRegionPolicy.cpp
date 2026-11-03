#include "InputRegionPolicy.h"

namespace PrismaUI::InputRegionPolicy {

namespace {

bool Contains(const InputRegion& region, std::int32_t x, std::int32_t y)
{
    if (region.width <= 0 || region.height <= 0) return false;
    const std::int64_t right = static_cast<std::int64_t>(region.x) + region.width;
    const std::int64_t bottom = static_cast<std::int64_t>(region.y) + region.height;
    return x >= region.x && y >= region.y && x < right && y < bottom;
}

bool HitTest(const State& state, std::int32_t x, std::int32_t y)
{
    for (const auto& region : state.regions) {
        if (Contains(region, x, y)) return true;
    }
    return false;
}

void Reset(State& state)
{
    state.focusedView = 0;
    state.mode = CaptureMode::Full;
    state.cursorOwned = false;
    state.regions.clear();
}

}

void SetFocusedView(State& state, ViewId view)
{
    if (state.focusedView != view) {
        state.regions.clear();
        state.mode = CaptureMode::Full;
    }
    state.focusedView = view;
    state.cursorOwned = view != 0;
}

void ClearFocusedView(State& state, ViewId view)
{
    if (view != 0 && state.focusedView != view) return;
    Reset(state);
}

void OnViewDestroyed(State& state, ViewId view)
{
    if (view != 0 && state.focusedView == view) {
        Reset(state);
    }
}

void SetMode(State& state, CaptureMode mode)
{
    state.mode = mode;
}

void SetRegions(State& state, ViewId view, std::span<const InputRegion> regions)
{
    if (view == 0 || state.focusedView != view) return;
    const auto kept = regions.size() > kMaxInputRegions
                          ? regions.subspan(0, kMaxInputRegions)
                          : regions;
    state.regions.assign(kept.begin(), kept.end());
}

void ClearRegions(State& state)
{
    state.regions.clear();
}

bool IsCursorOwned(const State& state)
{
    return state.cursorOwned;
}

bool ShouldRouteMouse(const State& state, std::int32_t x, std::int32_t y)
{
    if (state.focusedView == 0) return false;
    if (state.mode == CaptureMode::Full) return true;
    return HitTest(state, x, y);
}

bool ShouldSwallowMouse(const State& state, std::int32_t x, std::int32_t y)
{
    return ShouldRouteMouse(state, x, y);
}

}
