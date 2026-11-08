#pragma once

#include <cstdint>

namespace PrismaUI::RenderDiagnostics {

enum class SkipReason : std::uint32_t {
    kNone = 0,
    kHostAbiUnresolved,
    kCefNotActive,
    kCompositorNotReady,
    kNoCefFrame,
    kNoGameRenderer,
    kNoImmediateContext,
    kNoRenderTarget,
    kCefTextureImportFailed,
    kRendererRebuilding,
    kDeviceLost,
    kInvalidDimensions,
    kUnsupportedConfiguration,
};
inline constexpr std::uint32_t kSkipReasonCount = 13;

[[nodiscard]] constexpr const char* ToString(SkipReason r) noexcept {
    switch (r) {
        case SkipReason::kNone:                     return "none";
        case SkipReason::kHostAbiUnresolved:        return "host-abi-unresolved";
        case SkipReason::kCefNotActive:             return "cef-not-active";
        case SkipReason::kCompositorNotReady:       return "compositor-not-ready";
        case SkipReason::kNoCefFrame:               return "no-cef-frame";
        case SkipReason::kNoGameRenderer:           return "no-game-renderer";
        case SkipReason::kNoImmediateContext:       return "no-immediate-context";
        case SkipReason::kNoRenderTarget:           return "no-render-target";
        case SkipReason::kCefTextureImportFailed:   return "cef-texture-import-failed";
        case SkipReason::kRendererRebuilding:       return "renderer-rebuilding";
        case SkipReason::kDeviceLost:               return "device-lost";
        case SkipReason::kInvalidDimensions:        return "invalid-dimensions";
        case SkipReason::kUnsupportedConfiguration: return "unsupported-configuration";
    }
    return "unknown";
}

enum class FailureStage : std::uint32_t {
    kHealthy = 0,
    kCefNeverPainted,
    kImportFailed,
    kNoTarget,
    kComposeSkipped,
    kComposedNotVisible,
};

struct StageCounters {
    std::uint64_t acceleratedPaintCallbacks = 0;
    std::uint64_t cefFramesReceived = 0;
    std::uint64_t sharedTextureOpenAttempts = 0;
    std::uint64_t sharedTextureOpenSuccesses = 0;
    std::uint64_t srvCreationAttempts = 0;
    std::uint64_t srvCreationSuccesses = 0;
    std::uint64_t renderTargetAcquireAttempts = 0;
    std::uint64_t renderTargetAcquireSuccesses = 0;
    std::uint64_t compositeAttempts = 0;
    std::uint64_t compositeSuccesses = 0;
    std::uint64_t presentCalls = 0;
};

inline constexpr std::uint64_t kDownstreamSuspectThreshold = 8;

[[nodiscard]] constexpr FailureStage Classify(const StageCounters& c) noexcept {
    if (c.compositeSuccesses > 0) {

        if (c.compositeAttempts > c.compositeSuccesses &&
            c.compositeAttempts - c.compositeSuccesses >= kDownstreamSuspectThreshold) {
            return FailureStage::kComposedNotVisible;
        }
        return FailureStage::kHealthy;
    }
    if (c.acceleratedPaintCallbacks == 0) {
        return FailureStage::kCefNeverPainted;
    }

    if (c.sharedTextureOpenSuccesses == 0 || c.srvCreationSuccesses == 0) {
        return FailureStage::kImportFailed;
    }

    if (c.renderTargetAcquireSuccesses == 0) {
        return FailureStage::kNoTarget;
    }

    return FailureStage::kComposeSkipped;
}

[[nodiscard]] constexpr const char* ToString(FailureStage s) noexcept {
    switch (s) {
        case FailureStage::kHealthy:            return "healthy";
        case FailureStage::kCefNeverPainted:    return "A:cef-never-painted";
        case FailureStage::kImportFailed:       return "B:shared-texture-import-failed";
        case FailureStage::kNoTarget:           return "C:no-game-render-target";
        case FailureStage::kComposeSkipped:     return "D:composite-skipped";
        case FailureStage::kComposedNotVisible: return "F:composed-but-not-visible";
    }
    return "unknown";
}

enum class HostFrameState : std::uint32_t {
    kHealthy = 0,
    kWaitingForFirstFrame,
    kNoAcceleratedPaint,
    kNoSharedHandle,
    kSharedOpenFailed,
    kAdapterMismatch,
    kSharedCopyFailed,
    kSrvCreateFailed,
    kFrameInvalidated,
    kDeviceLost,
    kPrismaToGameOpenFailed,
};
inline constexpr std::uint32_t kHostFrameStateCount = 11;

[[nodiscard]] constexpr const char* ToString(HostFrameState s) noexcept {
    switch (s) {
        case HostFrameState::kHealthy:              return "healthy";
        case HostFrameState::kWaitingForFirstFrame: return "waiting-for-first-frame";
        case HostFrameState::kNoAcceleratedPaint:   return "no-accelerated-paint";
        case HostFrameState::kNoSharedHandle:       return "no-shared-handle";
        case HostFrameState::kSharedOpenFailed:     return "shared-open-failed";
        case HostFrameState::kAdapterMismatch:      return "adapter-mismatch";
        case HostFrameState::kSharedCopyFailed:     return "shared-copy-failed";
        case HostFrameState::kSrvCreateFailed:      return "srv-create-failed";
        case HostFrameState::kFrameInvalidated:     return "frame-invalidated";
        case HostFrameState::kDeviceLost:           return "device-lost";
        case HostFrameState::kPrismaToGameOpenFailed: return "prisma-to-game-open-failed";
    }
    return "unknown";
}

}
