#include "VR/SceneDepthDrawPolicy.h"

#include <iostream>

namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}
}  // namespace

int main() {
    using PrismaUI::SceneDepthDrawPolicy::Decide;
    using PrismaUI::SceneDepthDrawPolicy::Mode;
    using PrismaUI::SceneDepthDrawPolicy::ReportOcclusionApplied;

    Check(Decide(false, false) == Mode::kColorOnly,
          "no request draws color-only");
    Check(Decide(false, true) == Mode::kColorOnly,
          "an unused snapshot does not force occlusion");
    Check(Decide(true, false) == Mode::kColorOnly,
          "a requested occlusion with no snapshot still draws (color-only fallback)");
    Check(Decide(true, true) == Mode::kOccluded,
          "requested + valid snapshot uses the depth path");

    Check(!ReportOcclusionApplied(true, false),
          "fallback must not advertise SpatialState_SceneDepthOcclusion");
    Check(ReportOcclusionApplied(true, true),
          "successful capture reports occlusion applied");
    Check(!ReportOcclusionApplied(false, true),
          "an unused snapshot is not reported as applied occlusion");

    if (failures != 0) {
        std::cerr << failures << " scene-depth draw policy test(s) failed\n";
        return 1;
    }
    std::cout << "scene-depth draw policy tests passed\n";
    return 0;
}
