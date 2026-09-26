#include "PrismaUI/FreezeDiagnosticsCore.h"

#include <cstdio>

namespace PrismaUI::FreezeDiagnostics {

    namespace {

        std::string U64(uint64_t v) { return std::to_string(v); }
        std::string I64(int64_t v) { return std::to_string(v); }

        std::string Seconds(int64_t ms) {
            if (ms < 0) ms = 0;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%lld.%01llds", static_cast<long long>(ms / 1000),
                          static_cast<long long>((ms % 1000) / 100));
            return std::string(buf);
        }

        bool IsActiveStage(Stage stage) { return stage != Stage::Idle; }

        const LaneSnapshot& L(const Snapshot& s, Lane lane) {
            return s.lanes[static_cast<std::size_t>(lane)];
        }

    }

    const char* StageName(Stage stage) noexcept {
        switch (stage) {
            case Stage::Idle: return "Idle";
            case Stage::Present: return "Present";
            case Stage::BeginPresentFrame: return "BeginPresentFrame";
            case Stage::DrainGpuGeneration: return "DrainGpuGeneration";
            case Stage::OffscreenUpdate: return "OffscreenUpdate";
            case Stage::Composite: return "Composite";
            case Stage::ModelPreviewTick: return "ModelPreviewTick";
            case Stage::ModelPreviewRead: return "ModelPreviewRead";
            case Stage::UltralightUpdate: return "UltralightUpdate";
            case Stage::UltralightRefreshDisplay: return "UltralightRefreshDisplay";
            case Stage::UltralightRender: return "UltralightRender";
            case Stage::UltralightCallback: return "UltralightCallback";
            case Stage::DispatchTask: return "DispatchTask";
            case Stage::DispatchSync: return "DispatchSync";
            case Stage::CreateView: return "CreateView";
            case Stage::DestroyView: return "DestroyView";
            case Stage::GameThreadCallback: return "GameThreadCallback";
            case Stage::DropViewWait: return "DropViewWait";
            case Stage::ShutdownJoin: return "ShutdownJoin";
            case Stage::Count: return "?";
        }
        return "?";
    }

    const char* LaneName(Lane lane) noexcept {
        switch (lane) {
            case Lane::Present: return "present";
            case Lane::UltralightOwner: return "ultralight-owner";
            case Lane::GameDispatch: return "game-dispatch";
            case Lane::ModelPreviewServicing: return "model-preview";
            case Lane::Count: return "?";
        }
        return "?";
    }

    static std::string DeriveHint(const Snapshot& snap, int64_t nowMs) {
        const LaneSnapshot& owner = L(snap, Lane::UltralightOwner);
        const LaneSnapshot& dispatch = L(snap, Lane::GameDispatch);
        const LaneSnapshot& present = L(snap, Lane::Present);

        if (snap.dropViewView != 0 && snap.dropViewSinceMs != 0 &&
            dispatch.stage == Stage::GameThreadCallback && dispatch.view == snap.dropViewView) {
            return "Destroy(view=" + U64(snap.dropViewView) +
                   ") is waiting for that same view's in-flight game-thread callback";
        }
        if (IsActiveStage(owner.stage) && owner.heartbeatMs != 0 &&
            (owner.stage == Stage::UltralightRender || owner.stage == Stage::UltralightUpdate ||
             owner.stage == Stage::UltralightRefreshDisplay || owner.stage == Stage::UltralightCallback)) {
            return std::string("Ultralight owner has remained inside ") + StageName(owner.stage) + " for " +
                   Seconds(nowMs - owner.heartbeatMs);
        }
        if (snap.shutdownJoinSinceMs != 0) {
            return std::string("Backend Shutdown is blocked joining the Ultralight owner thread (owner stage=") +
                   StageName(owner.stage) + ")";
        }
        if (present.stage == Stage::BeginPresentFrame || present.stage == Stage::DrainGpuGeneration) {
            return std::string("Present is stuck in ") + StageName(present.stage) +
                   " (generation=" + U64(snap.completedGeneration) + "/" + U64(snap.activeGeneration) + ")";
        }
        if (snap.dispatchSyncOp != Stage::Idle && snap.dispatchSyncSinceMs != 0) {
            return std::string("DispatchSync(") + StageName(snap.dispatchSyncOp) + ") has been outstanding for " +
                   Seconds(nowMs - snap.dispatchSyncSinceMs);
        }
        return {};
    }

    Verdict EvaluateFreeze(const Snapshot& snap, const Thresholds& th, WatchdogState& state, int64_t nowMs) {
        Verdict v;

        const LaneSnapshot& present = L(snap, Lane::Present);
        if (snap.everPresented && !snap.minimized && IsActiveStage(present.stage) && present.heartbeatMs != 0 &&
            nowMs - present.heartbeatMs > th.presentStoppedMs) {
            v.reasons.push_back("present stalled age=" + I64(nowMs - present.heartbeatMs) + "ms stage=" +
                                StageName(present.stage));
        }

        for (std::size_t i = 0; i < kLaneCount; ++i) {
            const Lane lane = static_cast<Lane>(i);
            if (lane == Lane::Present) continue;
            const LaneSnapshot& ls = snap.lanes[i];
            if (IsActiveStage(ls.stage) && ls.heartbeatMs != 0 && nowMs - ls.heartbeatMs > th.laneStuckMs) {
                v.reasons.push_back(std::string(LaneName(lane)) + " stalled age=" +
                                    I64(nowMs - ls.heartbeatMs) + "ms stage=" + StageName(ls.stage));
            }
        }

        if (snap.dropViewView != 0 && snap.dropViewSinceMs != 0 &&
            nowMs - snap.dropViewSinceMs > th.dropViewMs) {
            v.reasons.push_back("drop-view waiting view=" + U64(snap.dropViewView) + " age=" +
                                I64(nowMs - snap.dropViewSinceMs) + "ms");
        }
        if (snap.dispatchSyncOp != Stage::Idle && snap.dispatchSyncSinceMs != 0 &&
            nowMs - snap.dispatchSyncSinceMs > th.dispatchSyncMs) {
            v.reasons.push_back(std::string("dispatch-sync outstanding op=") + StageName(snap.dispatchSyncOp) +
                                " age=" + I64(nowMs - snap.dispatchSyncSinceMs) + "ms");
        }
        if (snap.shutdownJoinSinceMs != 0 && nowMs - snap.shutdownJoinSinceMs > th.shutdownJoinMs) {
            v.reasons.push_back("shutdown-join age=" + I64(nowMs - snap.shutdownJoinSinceMs) + "ms");
        }

        v.frozen = !v.reasons.empty();

        if (v.frozen) {
            if (!state.episodeActive) {
                state.episodeActive = true;
                state.episodeStartMs = nowMs;
                state.lastReportMs = nowMs;
                state.dumpRequestedThisEpisode = true;
                v.episodeStart = true;
                v.shouldReport = true;
                v.shouldWriteDump = true;
            } else if (nowMs - state.lastReportMs >= th.reportIntervalMs) {
                state.lastReportMs = nowMs;
                v.shouldReport = true;
            }
            v.hint = DeriveHint(snap, nowMs);
        } else if (state.episodeActive) {
            state.episodeActive = false;
            state.dumpRequestedThisEpisode = false;
            v.episodeEnd = true;
        }

        return v;
    }

    std::vector<std::string> FormatSnapshotLines(const Snapshot& snap, const Verdict& verdict, int64_t nowMs) {
        std::vector<std::string> out;
        out.push_back("[FREEZE] suspected hard freeze");
        for (const auto& reason : verdict.reasons) out.push_back("[FREEZE] " + reason);

        const LaneSnapshot& present = L(snap, Lane::Present);
        out.push_back("[FREEZE] present: age=" + I64(nowMs - present.heartbeatMs) + "ms thread=" +
                      U64(present.threadId) + " stage=" + StageName(present.stage) + " generation=" +
                      U64(present.generation) + " minimized=" + (snap.minimized ? "1" : "0"));

        const LaneSnapshot& owner = L(snap, Lane::UltralightOwner);
        out.push_back("[FREEZE] ultralight-owner: age=" + I64(nowMs - owner.heartbeatMs) + "ms thread=" +
                      U64(owner.threadId) + " stage=" + StageName(owner.stage));

        const LaneSnapshot& dispatch = L(snap, Lane::GameDispatch);
        out.push_back("[FREEZE] game-dispatch: thread=" + U64(dispatch.threadId) + " stage=" +
                      StageName(dispatch.stage) + " view=" + U64(dispatch.view) + " age=" +
                      I64(nowMs - dispatch.heartbeatMs) + "ms");

        if (snap.dropViewView != 0) {
            out.push_back("[FREEZE] drop-view: waiting=1 view=" + U64(snap.dropViewView) + " age=" +
                          I64(nowMs - snap.dropViewSinceMs) + "ms");
        }
        if (snap.dispatchSyncOp != Stage::Idle) {
            out.push_back(std::string("[FREEZE] dispatch-sync: op=") + StageName(snap.dispatchSyncOp) +
                          " thread=" + U64(snap.dispatchSyncThread) + " age=" +
                          I64(nowMs - snap.dispatchSyncSinceMs) + "ms");
        }
        if (snap.shutdownJoinSinceMs != 0) {
            out.push_back("[FREEZE] shutdown-join: age=" + I64(nowMs - snap.shutdownJoinSinceMs) + "ms");
        }

        out.push_back("[FREEZE] gpu: completedGeneration=" + U64(snap.completedGeneration) +
                      " activeGeneration=" + U64(snap.activeGeneration) + " frameRequested=" +
                      (snap.frameRequested ? "1" : "0"));

        const LaneSnapshot& mp = L(snap, Lane::ModelPreviewServicing);
        out.push_back("[FREEZE] model-preview: stage=" + std::string(StageName(mp.stage)) + " jobs=" +
                      std::to_string(snap.mpJobs) + " reads=" + std::to_string(snap.mpReads) + " claimedRead=" +
                      std::to_string(snap.mpClaimedRead) + " workersActive=" +
                      std::to_string(snap.mpWorkersActive));

        out.push_back("[FREEZE] last-success: render=" + I64(snap.lastRenderMs) + "ms present=" +
                      I64(snap.lastPresentMs) + "ms composite=" + I64(snap.lastCompositeMs) + "ms (steady-clock)");

        if (!verdict.hint.empty()) out.push_back("[FREEZE][LIKELY] " + verdict.hint);
        return out;
    }

}
