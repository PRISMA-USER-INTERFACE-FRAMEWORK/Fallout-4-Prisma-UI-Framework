#include "PrismaUI/OffscreenFrameState.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

float Uv(std::uint32_t pixel, std::uint32_t extent) {
    return static_cast<float>(static_cast<double>(pixel) / static_cast<double>(extent));
}

}  // namespace

int main() {
    using PrismaUI::OffscreenFrameState::CanPublish;
    using PrismaUI::OffscreenFrameState::NeedsCopy;
    using PrismaUI::OffscreenFrameState::ShouldReleaseKeyedMutex;
    using PrismaUI::OffscreenFrameState::Snapshot;

    Snapshot live{7, 41, true, true};

    Check(!NeedsCopy(live, true, 41), "same generation must skip the copy path");
    Check(NeedsCopy(live, true, 42), "new generation must request a copy");
    Check(NeedsCopy(live, false, 41), "without the generation ABI the legacy path must keep copying");

    Check(CanPublish(live.epoch, live.epoch),
          "a copied frame for the same view epoch must publish");
    Check(!CanPublish(8, 7),
          "destroy/recreate between snapshot and publish must reject the stale result");
    Check(!CanPublish(0, 0), "epoch zero is never a publishable live view");

    Check(!ShouldReleaseKeyedMutex(false, false), "no keyed mutex means no ReleaseSync");
    Check(!ShouldReleaseKeyedMutex(true, false), "failed AcquireSync must never ReleaseSync");
    Check(ShouldReleaseKeyedMutex(true, true), "successful AcquireSync must ReleaseSync");

    using PrismaUI::OffscreenFrameState::ResolveSourceCoordinate;
    struct Rounded {
        std::uint32_t extent;
        std::uint32_t pixel;
    };
    const Rounded rounded[] = {{3376, 1743}, {3376, 1844}, {3376, 1954}, {3440, 1725}, {3440, 1766},
                               {3440, 1807}, {3552, 1784}, {3552, 1803}, {3552, 1822}};
    for (const auto& c : rounded) {
        const double scaled = static_cast<double>(Uv(c.pixel, c.extent)) * static_cast<double>(c.extent);
        Check(std::abs(scaled - static_cast<double>(std::llround(scaled))) > 0.0001,
              "vector must exceed the old fixed epsilon");
        std::uint32_t out = 0;
        Check(ResolveSourceCoordinate(Uv(c.pixel, c.extent), c.extent, out) && out == c.pixel,
              "a float32 uv must resolve to its pixel");
    }
    std::uint32_t out = 0;
    Check(!ResolveSourceCoordinate(static_cast<float>(0.1000002), 1000, out), "a misaligned uv is rejected");
    Check(ResolveSourceCoordinate(0.0f, 0, out) && out == 0, "zero extent with zero uv");
    Check(ResolveSourceCoordinate(1.0f, 1, out) && out == 1, "one pixel edge");
    Check(ResolveSourceCoordinate(1.0f, 16384, out) && out == 16384, "max extent edge");
    Check(!ResolveSourceCoordinate(-0.001f, 1000, out), "negative uv is rejected");
    Check(!ResolveSourceCoordinate(1.001f, 1000, out), "uv past the extent is rejected");
    Check(!ResolveSourceCoordinate(std::numeric_limits<float>::quiet_NaN(), 1000, out), "NaN is rejected");
    Check(!ResolveSourceCoordinate(std::numeric_limits<float>::infinity(), 1000, out), "infinity is rejected");

    std::cout << "offscreen_frame_state_tests ok\n";
    return 0;
}
