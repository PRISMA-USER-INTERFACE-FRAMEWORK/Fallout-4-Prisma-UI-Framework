// Pure-logic tests for the CEF render-pipeline stage classifier (master-task Phases 18/19).
// Compiles on Linux with g++ -Isrc; no engine, no D3D, no Windows.

#include "PrismaUI/RenderDiagnostics.h"

#include <cstring>
#include <iostream>

namespace {
int failures = 0;
void Check(bool value, const char* message) {
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

using namespace PrismaUI::RenderDiagnostics;

// Build a counters snapshot for a pipeline that got as far as `stageReached` and then stalled.
StageCounters Reached(std::uint64_t paints, std::uint64_t imports, std::uint64_t targets,
                      std::uint64_t composites, std::uint64_t compositeAttempts) {
    StageCounters c{};
    c.acceleratedPaintCallbacks = paints;
    c.cefFramesReceived = paints;
    c.sharedTextureOpenAttempts = paints;
    c.sharedTextureOpenSuccesses = imports;
    c.srvCreationAttempts = imports;
    c.srvCreationSuccesses = imports;
    c.renderTargetAcquireAttempts = imports;
    c.renderTargetAcquireSuccesses = targets;
    c.compositeAttempts = compositeAttempts;
    c.compositeSuccesses = composites;
    c.presentCalls = 100;
    return c;
}
}  // namespace

int main() {
    // Every SkipReason has a non-empty, non-"unknown" label (guards enum/label drift).
    for (std::uint32_t i = 0; i < kSkipReasonCount; ++i) {
        const char* s = ToString(static_cast<SkipReason>(i));
        Check(s != nullptr && s[0] != '\0', "SkipReason label is non-empty");
        Check(std::strcmp(s, "unknown") != 0, "SkipReason in-range value is not 'unknown'");
    }
    // Out-of-range value falls through to "unknown" rather than UB.
    Check(std::strcmp(ToString(static_cast<SkipReason>(9999)), "unknown") == 0,
          "out-of-range SkipReason -> unknown");

    // Stage A: CEF never painted -> everything downstream is zero.
    Check(Classify(Reached(0, 0, 0, 0, 0)) == FailureStage::kCefNeverPainted,
          "no paint callbacks -> A cef-never-painted");

    // Stage B: painted, but shared-texture import never succeeded (the cross-adapter case).
    Check(Classify(Reached(10, 0, 0, 0, 0)) == FailureStage::kImportFailed,
          "paint but no import success -> B import-failed");
    // Import attempts happen but SRV creation never lands -> still B.
    {
        StageCounters c = Reached(10, 5, 0, 0, 0);
        c.srvCreationSuccesses = 0;  // opened but SRV failed
        Check(Classify(c) == FailureStage::kImportFailed,
              "open ok but SRV creation fails -> B import-failed");
    }

    // Stage C: import worked, no render target ever acquired.
    Check(Classify(Reached(10, 10, 0, 0, 0)) == FailureStage::kNoTarget,
          "import ok but no target -> C no-target");

    // Stage D: target acquired, nothing ever composited.
    Check(Classify(Reached(10, 10, 10, 0, 5)) == FailureStage::kComposeSkipped,
          "target ok, zero composites -> D composite-skipped");

    // Healthy: at least one composite, attempts keeping pace.
    Check(Classify(Reached(10, 10, 10, 10, 10)) == FailureStage::kHealthy,
          "composited successfully -> healthy");

    // A single post-success failure is transient, still healthy (below threshold).
    Check(Classify(Reached(20, 20, 20, 18, 19)) == FailureStage::kHealthy,
          "one dropped composite after success -> still healthy");

    // Stage F: composited before, but a sustained run of composite failures now (downstream ate it).
    {
        StageCounters c = Reached(100, 100, 100, 10, 30);  // 20 failures after 10 successes
        Check(Classify(c) == FailureStage::kComposedNotVisible,
              "sustained composite failures after success -> F composed-not-visible");
    }

    // FailureStage labels are all populated.
    for (auto s : {FailureStage::kHealthy, FailureStage::kCefNeverPainted, FailureStage::kImportFailed,
                   FailureStage::kNoTarget, FailureStage::kComposeSkipped,
                   FailureStage::kComposedNotVisible}) {
        Check(ToString(s)[0] != '\0', "FailureStage label is non-empty");
    }

    // HostFrameState: every in-range value has a distinct non-"unknown" label (guards enum/label drift
    // and the kHostFrameStateCount bound the ABI relies on). Out-of-range falls through to "unknown".
    for (std::uint32_t i = 0; i < kHostFrameStateCount; ++i) {
        const char* s = ToString(static_cast<HostFrameState>(i));
        Check(s != nullptr && s[0] != '\0', "HostFrameState label is non-empty");
        Check(std::strcmp(s, "unknown") != 0, "in-range HostFrameState is not 'unknown'");
    }
    Check(std::strcmp(ToString(static_cast<HostFrameState>(9999)), "unknown") == 0,
          "out-of-range HostFrameState -> unknown");
    // The states the host code actually transitions through must round-trip to stable labels.
    Check(std::strcmp(ToString(HostFrameState::kHealthy), "healthy") == 0, "healthy label");
    Check(std::strcmp(ToString(HostFrameState::kSharedOpenFailed), "shared-open-failed") == 0,
          "shared-open-failed label");
    Check(std::strcmp(ToString(HostFrameState::kPrismaToGameOpenFailed), "prisma-to-game-open-failed") == 0,
          "prisma-to-game-open-failed label");
    Check(std::strcmp(ToString(HostFrameState::kDeviceLost), "device-lost") == 0, "device-lost label");

    if (failures != 0) {
        std::cerr << failures << " render-diagnostics test(s) failed\n";
        return 1;
    }
    std::cout << "render-diagnostics tests passed\n";
    return 0;
}
