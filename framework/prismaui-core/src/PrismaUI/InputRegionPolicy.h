#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace PrismaUI::InputRegionPolicy {

using ViewId = std::uint64_t;

enum class CaptureMode : std::uint8_t {
    Full = 0,
    OverlayRegions = 1,
};

struct InputRegion {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
};

inline constexpr std::size_t kMaxInputRegions = 256;

struct State {
    ViewId focusedView = 0;
    CaptureMode mode = CaptureMode::Full;
    bool cursorOwned = false;
    std::vector<InputRegion> regions;
};

void SetFocusedView(State& state, ViewId view);
void ClearFocusedView(State& state, ViewId view);
void OnViewDestroyed(State& state, ViewId view);
void SetMode(State& state, CaptureMode mode);
void SetRegions(State& state, ViewId view, std::span<const InputRegion> regions);
void ClearRegions(State& state);
bool IsCursorOwned(const State& state);
bool ShouldRouteMouse(const State& state, std::int32_t x, std::int32_t y);
bool ShouldSwallowMouse(const State& state, std::int32_t x, std::int32_t y);

}
