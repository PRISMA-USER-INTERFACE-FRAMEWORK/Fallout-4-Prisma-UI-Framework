#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PrismaUI::FreezeDiagnostics {

    enum class Stage : uint32_t {
        Idle = 0,
        Present,
        BeginPresentFrame,
        DrainGpuGeneration,
        OffscreenUpdate,
        Composite,
        ModelPreviewTick,
        ModelPreviewRead,
        UltralightUpdate,
        UltralightRefreshDisplay,
        UltralightRender,
        UltralightCallback,
        DispatchTask,
        DispatchSync,
        CreateView,
        DestroyView,
        GameThreadCallback,
        DropViewWait,
        ShutdownJoin,
        Count
    };

    enum class Lane : uint32_t {
        Present = 0,
        UltralightOwner,
        GameDispatch,
        ModelPreviewServicing,
        Count
    };

    inline constexpr std::size_t kLaneCount = static_cast<std::size_t>(Lane::Count);

    const char* StageName(Stage stage) noexcept;
    const char* LaneName(Lane lane) noexcept;

    struct LaneSnapshot {
        uint32_t threadId = 0;
        Stage    stage = Stage::Idle;
        int64_t  heartbeatMs = 0;
        uint64_t view = 0;
        uint64_t generation = 0;
        int64_t  detail = 0;
    };

    struct Snapshot {
        std::array<LaneSnapshot, kLaneCount> lanes{};

        bool     minimized = false;
        bool     everPresented = false;
        bool     gameLoadActive = false;
        int64_t  gameLoadSinceMs = 0;

        uint64_t completedGeneration = 0;
        uint64_t activeGeneration = 0;
        bool     frameRequested = false;

        Stage    dispatchSyncOp = Stage::Idle;
        int64_t  dispatchSyncSinceMs = 0;
        uint32_t dispatchSyncThread = 0;

        uint64_t dropViewView = 0;
        int64_t  dropViewSinceMs = 0;

        int64_t  shutdownJoinSinceMs = 0;

        int32_t  mpJobs = 0;
        int32_t  mpReads = 0;
        int32_t  mpClaimedRead = 0;
        int32_t  mpWorkersActive = 0;

        int64_t  lastRenderMs = 0;
        int64_t  lastPresentMs = 0;
        int64_t  lastCompositeMs = 0;
    };

    struct Thresholds {
        int64_t laneStuckMs = 5000;
        int64_t presentStoppedMs = 5000;
        int64_t dropViewMs = 5000;
        int64_t dispatchSyncMs = 5000;
        int64_t shutdownJoinMs = 5000;
        int64_t reportIntervalMs = 5000;
        int64_t loadDumpMs = 45000;
    };

    struct WatchdogState {
        bool    episodeActive = false;
        int64_t episodeStartMs = 0;
        int64_t lastReportMs = 0;
        bool    dumpRequestedThisEpisode = false;
    };

    struct Verdict {
        bool frozen = false;
        bool episodeStart = false;
        bool episodeEnd = false;
        bool shouldReport = false;
        bool shouldWriteDump = false;
        std::vector<std::string> reasons;
        std::string hint;
    };

    Verdict EvaluateFreeze(const Snapshot& snap, const Thresholds& thresholds, WatchdogState& state,
                           int64_t nowMs);

    std::vector<std::string> FormatSnapshotLines(const Snapshot& snap, const Verdict& verdict, int64_t nowMs);

}
