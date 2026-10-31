// Pure tests for the FreezeDiagnostics detection core; see docs/code-notes/prismaui-core.md.
#include "PrismaUI/FreezeDiagnosticsCore.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace PrismaUI::FreezeDiagnostics;

static int g_failures = 0;
static void expect(bool cond, const char* what) {
    std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

static LaneSnapshot& lane(Snapshot& s, Lane l) { return s.lanes[static_cast<std::size_t>(l)]; }

static Snapshot Healthy(int64_t now) {
    Snapshot s;
    s.everPresented = true;
    s.minimized = false;
    lane(s, Lane::Present).stage = Stage::Idle;
    lane(s, Lane::Present).heartbeatMs = now;
    lane(s, Lane::UltralightOwner).stage = Stage::Idle;
    lane(s, Lane::UltralightOwner).heartbeatMs = now;
    lane(s, Lane::GameDispatch).stage = Stage::Idle;
    lane(s, Lane::GameDispatch).heartbeatMs = now;
    lane(s, Lane::ModelPreviewServicing).stage = Stage::Idle;
    lane(s, Lane::ModelPreviewServicing).heartbeatMs = now;
    return s;
}

static bool hasReasonContaining(const Verdict& v, const char* needle) {
    for (const auto& r : v.reasons)
        if (r.find(needle) != std::string::npos) return true;
    return false;
}

static bool linesContain(const std::vector<std::string>& lines, const char* needle) {
    for (const auto& l : lines)
        if (l.find(needle) != std::string::npos) return true;
    return false;
}

int main() {
    const Thresholds th;

    {
        WatchdogState st;
        Snapshot s = Healthy(100000);
        Verdict v = EvaluateFreeze(s, th, st, 100000);
        expect(!v.frozen, "healthy snapshot is not frozen");
        expect(!v.shouldWriteDump && !v.shouldReport, "healthy snapshot requests no report/dump");
        expect(!st.episodeActive, "healthy snapshot leaves no active episode");
    }

    {
        WatchdogState st;
        Snapshot s = Healthy(0);
        lane(s, Lane::Present).stage = Stage::Present;
        lane(s, Lane::Present).heartbeatMs = 1000;
        Verdict a = EvaluateFreeze(s, th, st, 5500);
        expect(!a.frozen, "active present 4.5s -> not frozen");
        Verdict b = EvaluateFreeze(s, th, st, 7000);
        expect(b.frozen && hasReasonContaining(b, "present stalled"), "active present 6s -> frozen");
    }
    {
        WatchdogState st;
        Snapshot s = Healthy(1000);
        Verdict v = EvaluateFreeze(s, th, st, 20000);
        expect(!v.frozen, "completed present idle+stale -> not frozen (engine may be loading/Bink)");
    }
    {
        WatchdogState st;
        Snapshot s = Healthy(0);
        lane(s, Lane::Present).stage = Stage::BeginPresentFrame;
        lane(s, Lane::Present).heartbeatMs = 1000;
        Verdict v = EvaluateFreeze(s, th, st, 7000);
        expect(v.frozen && hasReasonContaining(v, "present stalled"), "active BeginPresentFrame 6s -> frozen");
        expect(v.hint.find("BeginPresentFrame") != std::string::npos, "active BeginPresentFrame hint remains precise");
    }
    {
        WatchdogState st;
        Snapshot s = Healthy(0);
        lane(s, Lane::Present).stage = Stage::Present;
        lane(s, Lane::Present).heartbeatMs = 1000;
        s.minimized = true;
        Verdict v = EvaluateFreeze(s, th, st, 20000);
        expect(!v.frozen, "active present while minimized -> not frozen");
    }
    {
        WatchdogState st;
        Snapshot s = Healthy(0);
        s.everPresented = false;
        lane(s, Lane::Present).heartbeatMs = 0;
        Verdict v = EvaluateFreeze(s, th, st, 60000);
        expect(!v.frozen, "never-presented -> not frozen");
    }

    {
        WatchdogState st;
        Snapshot s = Healthy(10000);
        lane(s, Lane::UltralightOwner).stage = Stage::UltralightRender;
        lane(s, Lane::UltralightOwner).heartbeatMs = 3000;
        Verdict v = EvaluateFreeze(s, th, st, 10000);
        expect(v.frozen && hasReasonContaining(v, "ultralight-owner"), "owner stuck in active stage -> frozen");
        expect(v.hint.find("UltralightRender") != std::string::npos, "hint names UltralightRender");
    }
    {
        WatchdogState st;
        Snapshot s = Healthy(10000);
        lane(s, Lane::UltralightOwner).stage = Stage::Idle;
        lane(s, Lane::UltralightOwner).heartbeatMs = 1000;
        Verdict v = EvaluateFreeze(s, th, st, 10000);
        expect(!v.frozen, "owner idle+stale -> not frozen (parked, exempt)");
    }

    {
        WatchdogState st;
        Snapshot s = Healthy(10000);
        s.dropViewView = 17;
        s.dropViewSinceMs = 3000;
        lane(s, Lane::GameDispatch).stage = Stage::GameThreadCallback;
        lane(s, Lane::GameDispatch).view = 17;
        lane(s, Lane::GameDispatch).heartbeatMs = 10000;
        Verdict v = EvaluateFreeze(s, th, st, 10000);
        expect(v.frozen && hasReasonContaining(v, "drop-view"), "drop-view over threshold -> frozen");
        expect(v.hint.find("Destroy(view=17)") != std::string::npos, "hint blames Destroy(view=17) same-view callback");
    }
    {
        WatchdogState st;
        Snapshot s = Healthy(10000);
        s.shutdownJoinSinceMs = 3000;
        Verdict v = EvaluateFreeze(s, th, st, 10000);
        expect(v.frozen && hasReasonContaining(v, "shutdown-join"), "shutdown-join over threshold -> frozen");
    }

    {
        WatchdogState st;
        Snapshot s = Healthy(0);
        lane(s, Lane::Present).stage = Stage::Present;
        lane(s, Lane::Present).heartbeatMs = 1000;

        Verdict v1 = EvaluateFreeze(s, th, st, 7000);
        expect(v1.frozen && v1.episodeStart, "episode starts on first active-present detection");
        expect(v1.shouldReport && v1.shouldWriteDump, "episode start -> report + dump");

        Verdict v2 = EvaluateFreeze(s, th, st, 7500);
        expect(v2.frozen && !v2.shouldReport && !v2.shouldWriteDump, "within report interval -> no report, no dump");

        Verdict v3 = EvaluateFreeze(s, th, st, 12500);
        expect(v3.frozen && v3.shouldReport, "after report interval -> report again");
        expect(!v3.shouldWriteDump, "still one dump per episode -> no second dump");

        Snapshot healthy = Healthy(13000);
        Verdict v4 = EvaluateFreeze(healthy, th, st, 13100);
        expect(!v4.frozen && v4.episodeEnd, "present scope exits idle -> episode ends");

        Snapshot s2 = Healthy(0);
        lane(s2, Lane::Present).stage = Stage::Present;
        lane(s2, Lane::Present).heartbeatMs = 13000;
        Verdict v5 = EvaluateFreeze(s2, th, st, 19000);
        expect(v5.frozen && v5.episodeStart && v5.shouldWriteDump, "a new active-present freeze episode dumps again");
    }

    {
        WatchdogState st;
        Snapshot s = Healthy(10000);
        lane(s, Lane::UltralightOwner).stage = Stage::UltralightRender;
        lane(s, Lane::UltralightOwner).heartbeatMs = 3000;
        s.completedGeneration = 42;
        s.activeGeneration = 42;
        s.frameRequested = true;
        Verdict v = EvaluateFreeze(s, th, st, 10000);
        auto lines = FormatSnapshotLines(s, v, 10000);
        expect(linesContain(lines, "[FREEZE] suspected hard freeze"), "format: header present");
        expect(linesContain(lines, "ultralight-owner:"), "format: owner line present");
        expect(linesContain(lines, "gpu: completedGeneration=42"), "format: gpu line present");
        expect(linesContain(lines, "[FREEZE][LIKELY]"), "format: likely-hint line present");
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "freeze diagnostics tests passed" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
