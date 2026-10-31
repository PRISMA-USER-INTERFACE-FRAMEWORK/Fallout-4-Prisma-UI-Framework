#include "PCH.h"

#include "PresentProfiler.h"

#include "Utils/ModulePath.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

#include <windows.h>

#include <wrl/client.h>

namespace PrismaUI::PresentProfiler
{
    namespace
    {

        std::atomic<bool> s_enabled{false};
        std::atomic<bool> s_configured{false};
        double s_reportSeconds = 5.0;

        double s_qpcToMs = 0.0;
        std::int64_t s_windowStartQpc = 0;

        std::vector<double> s_cpuMs;
        std::vector<double> s_gpuMs[kStageCount];

        std::uint64_t s_presentCount = 0;
        std::uint64_t s_windowStartPresents = 0;
        std::uint64_t s_windowStartPaints = 0;
        std::uint64_t s_windowStartDrops = 0;
        bool s_haveWindowBaseline = false;

        std::uint64_t s_lockBuckets[kLockBucketCount] = {};

        bool s_csvHeaderLogged = false;

        constexpr int kGpuSlots = 4;
        struct GpuSlot
        {
            Microsoft::WRL::ComPtr<ID3D11Query> disjoint;
            Microsoft::WRL::ComPtr<ID3D11Query> tsBegin;
            Microsoft::WRL::ComPtr<ID3D11Query> tsEnd;
            bool inFlight = false;
        };
        struct StageRing
        {
            GpuSlot slots[kGpuSlots];
            int write = 0;
            int active = -1;
        };
        StageRing s_stage[kStageCount];
        bool s_gpuReady = false;
        bool s_gpuTried = false;

        std::mutex s_snapMutex;
        Snapshot   s_snapshot;

        std::int64_t NowQpc() noexcept
        {
            LARGE_INTEGER v{};
            QueryPerformanceCounter(&v);
            return v.QuadPart;
        }

        double Percentile(const std::vector<double>& sorted, int pct) noexcept
        {
            if (sorted.empty()) return 0.0;
            std::size_t idx = (sorted.size() * static_cast<std::size_t>(pct)) / 100;
            if (idx >= sorted.size()) idx = sorted.size() - 1;
            return sorted[idx];
        }

        double Average(const std::vector<double>& v) noexcept
        {
            if (v.empty()) return 0.0;
            double sum = 0.0;
            for (double x : v) sum += x;
            return sum / static_cast<double>(v.size());
        }

        void EnsureGpuQueries(ID3D11Device* dev) noexcept
        {
            if (s_gpuReady || s_gpuTried || !dev) return;
            s_gpuTried = true;
            for (auto& ring : s_stage) {
                for (auto& slot : ring.slots) {
                    D3D11_QUERY_DESC dj{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
                    D3D11_QUERY_DESC ts{ D3D11_QUERY_TIMESTAMP, 0 };
                    if (FAILED(dev->CreateQuery(&dj, slot.disjoint.GetAddressOf())) ||
                        FAILED(dev->CreateQuery(&ts, slot.tsBegin.GetAddressOf())) ||
                        FAILED(dev->CreateQuery(&ts, slot.tsEnd.GetAddressOf()))) {
                        logger::warn("[PrismaPerf] GPU timestamp queries unavailable -- reporting CPU only");
                        for (auto& r : s_stage)
                            for (auto& s : r.slots) { s.disjoint.Reset(); s.tsBegin.Reset(); s.tsEnd.Reset(); }
                        return;
                    }
                }
            }
            s_gpuReady = true;
        }

        void CollectGpu(ID3D11DeviceContext* ctx) noexcept
        {
            if (!s_gpuReady || !ctx) return;
            for (int st = 0; st < kStageCount; ++st) {
                for (auto& slot : s_stage[st].slots) {
                    if (!slot.inFlight) continue;
                    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{};
                    if (ctx->GetData(slot.disjoint.Get(), &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
                        continue;
                    std::uint64_t begin = 0, end = 0;
                    const bool haveBegin =
                        ctx->GetData(slot.tsBegin.Get(), &begin, sizeof(begin), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
                    const bool haveEnd =
                        ctx->GetData(slot.tsEnd.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
                    slot.inFlight = false;
                    if (haveBegin && haveEnd && !dj.Disjoint && dj.Frequency != 0 && end >= begin) {
                        s_gpuMs[st].push_back(
                            static_cast<double>(end - begin) * 1000.0 / static_cast<double>(dj.Frequency));
                    }
                }
            }
        }

        void PublishSnapshot(const Snapshot& snap) noexcept
        {
            std::lock_guard<std::mutex> lock(s_snapMutex);
            s_snapshot = snap;
        }

        void Report(std::uint64_t paintCount, std::uint64_t dropTotal) noexcept
        {
            const std::int64_t now = NowQpc();
            const double windowSec = static_cast<double>(now - s_windowStartQpc) * s_qpcToMs / 1000.0;
            if (windowSec < s_reportSeconds) return;

            const std::uint64_t presents = s_presentCount - s_windowStartPresents;
            const std::uint64_t paints = paintCount >= s_windowStartPaints ? paintCount - s_windowStartPaints : 0;
            const std::uint64_t drops = dropTotal - s_windowStartDrops;
            const double perSec = windowSec > 0.0 ? 1.0 / windowSec : 0.0;

            std::sort(s_cpuMs.begin(), s_cpuMs.end());
            const double cpuAvg = Average(s_cpuMs);

            Snapshot snap;
            snap.enabled = true;
            snap.gpuAvailable = s_gpuReady;
            snap.windowSeconds = windowSec;
            snap.windowPresents = presents;
            snap.presentsPerSec = static_cast<double>(presents) * perSec;
            snap.webPaintsPerSec = static_cast<double>(paints) * perSec;
            snap.droppedCtx = drops;
            snap.cpuAvgMs = cpuAvg;
            snap.cpuP50Ms = Percentile(s_cpuMs, 50);
            snap.cpuP95Ms = Percentile(s_cpuMs, 95);
            snap.cpuP99Ms = Percentile(s_cpuMs, 99);
            snap.cpuMaxMs = s_cpuMs.empty() ? 0.0 : s_cpuMs.back();

            double gpuTotal = 0.0;
            double stageAvg[kStageCount] = {};
            double stagePerFrame[kStageCount] = {};
            for (int st = 0; st < kStageCount; ++st) {
                std::sort(s_gpuMs[st].begin(), s_gpuMs[st].end());
                stageAvg[st] = Average(s_gpuMs[st]);
                snap.gpuStageAvgMs[st] = stageAvg[st];
                double stageSum = 0.0;
                for (double x : s_gpuMs[st]) stageSum += x;
                stagePerFrame[st] = presents > 0 ? stageSum / static_cast<double>(presents) : 0.0;
                gpuTotal += stagePerFrame[st];
            }
            snap.gpuTotalAvgMs = gpuTotal;
            for (int i = 0; i < kLockBucketCount; ++i) snap.lockBuckets[i] = s_lockBuckets[i];

            logger::info("[PrismaPerf] window={:.1f}s presents={} ({:.1f}/s) dropped-ctx={} web-paints={} ({:.1f}/s)",
                         windowSec, presents, snap.presentsPerSec, drops, paints, snap.webPaintsPerSec);
            logger::info("[PrismaPerf] CPU ms avg={:.3f} p50={:.3f} p95={:.3f} p99={:.3f} max={:.3f} (n={})",
                         cpuAvg, snap.cpuP50Ms, snap.cpuP95Ms, snap.cpuP99Ms, snap.cpuMaxMs, s_cpuMs.size());

            logger::info("[PrismaPerf] GPU ms/frame total={:.3f} | offscreen={:.3f} tickcore={:.3f} draw={:.3f} "
                         "(ran n={}/{}/{} of {} presents)",
                         gpuTotal, stagePerFrame[0], stagePerFrame[1], stagePerFrame[2],
                         s_gpuMs[0].size(), s_gpuMs[1].size(), s_gpuMs[2].size(), presents);
            logger::info("[PrismaPerf] ctx-lock us <10={} 10-100={} 100-500={} 0.5-1ms={} 1-2ms={} dropped={}",
                         s_lockBuckets[0], s_lockBuckets[1], s_lockBuckets[2], s_lockBuckets[3],
                         s_lockBuckets[4], s_lockBuckets[5]);

            if (!s_csvHeaderLogged) {
                s_csvHeaderLogged = true;
                logger::info("[PrismaPerf.csv] header: window_s,presents,presents_ps,webpaints_ps,dropped_ctx,"
                             "cpu_avg_ms,cpu_p50_ms,cpu_p95_ms,cpu_p99_ms,cpu_max_ms,"
                             "gpu_total_ms,gpu_offscreen_ms,gpu_tickcore_ms,gpu_draw_ms,"
                             "lock_lt10us,lock_10_100us,lock_100_500us,lock_500us_1ms,lock_1_2ms,lock_dropped");
            }
            logger::info("[PrismaPerf.csv] {:.3f},{},{:.2f},{:.2f},{},"
                         "{:.4f},{:.4f},{:.4f},{:.4f},{:.4f},"
                         "{:.4f},{:.4f},{:.4f},{:.4f},"
                         "{},{},{},{},{},{}",
                         windowSec, presents, snap.presentsPerSec, snap.webPaintsPerSec, drops,
                         cpuAvg, snap.cpuP50Ms, snap.cpuP95Ms, snap.cpuP99Ms, snap.cpuMaxMs,
                         gpuTotal, stagePerFrame[0], stagePerFrame[1], stagePerFrame[2],
                         s_lockBuckets[0], s_lockBuckets[1], s_lockBuckets[2], s_lockBuckets[3],
                         s_lockBuckets[4], s_lockBuckets[5]);

            PublishSnapshot(snap);

            s_cpuMs.clear();
            for (auto& v : s_gpuMs) v.clear();
            for (auto& b : s_lockBuckets) b = 0;
            s_windowStartQpc = now;
            s_windowStartPresents = s_presentCount;
            s_windowStartPaints = paintCount;
            s_windowStartDrops = dropTotal;
        }
    }

    void Configure() noexcept
    {
        if (s_configured.exchange(true)) return;
        LARGE_INTEGER freq{};
        QueryPerformanceFrequency(&freq);
        s_qpcToMs = freq.QuadPart ? 1000.0 / static_cast<double>(freq.QuadPart) : 0.0;

        const auto ini = PrismaUI::Utils::PluginIniPath().string();
        const bool on = GetPrivateProfileIntA("Perf", "Profiler", 0, ini.c_str()) != 0;
        const int secs = GetPrivateProfileIntA("Perf", "ReportSeconds", 5, ini.c_str());
        s_reportSeconds = secs > 0 ? static_cast<double>(secs) : 5.0;
        s_enabled.store(on && s_qpcToMs > 0.0);
        if (on) {
            logger::warn("[PrismaPerf] present profiler ENABLED (report every {}s) -- diagnostic only, "
                         "leave [Perf] Profiler=0 for normal play", secs > 0 ? secs : 5);
        }
    }

    bool Enabled() noexcept { return s_enabled.load(); }

    ScopedPresent::ScopedPresent() noexcept
    {
        if (!s_enabled.load()) return;
        active_ = true;
        start_ = NowQpc();
        ++s_presentCount;
        if (!s_haveWindowBaseline) {
            s_haveWindowBaseline = true;
            s_windowStartQpc = start_;
            s_windowStartPresents = s_presentCount - 1;
        }
    }

    void ScopedPresent::Finish() noexcept
    {
        if (!active_) return;
        active_ = false;
        s_cpuMs.push_back(static_cast<double>(NowQpc() - start_) * s_qpcToMs);
    }

    ScopedPresent::~ScopedPresent() noexcept
    {
        Finish();
    }

    void BeginStage(Stage s, ID3D11Device* dev, ID3D11DeviceContext* ctx) noexcept
    {
        if (!s_enabled.load() || !dev || !ctx) return;
        EnsureGpuQueries(dev);
        if (!s_gpuReady) return;

        StageRing& ring = s_stage[static_cast<int>(s)];
        GpuSlot& slot = ring.slots[ring.write];
        if (slot.inFlight) return;
        ctx->Begin(slot.disjoint.Get());
        ctx->End(slot.tsBegin.Get());
        ring.active = ring.write;
    }

    void EndStage(Stage s, ID3D11DeviceContext* ctx) noexcept
    {
        if (!s_enabled.load() || !ctx || !s_gpuReady) return;
        StageRing& ring = s_stage[static_cast<int>(s)];
        if (ring.active < 0) return;
        GpuSlot& slot = ring.slots[ring.active];
        ctx->End(slot.tsEnd.Get());
        ctx->End(slot.disjoint.Get());
        slot.inFlight = true;
        ring.active = -1;
        ring.write = (ring.write + 1) % kGpuSlots;
    }

    void RecordLockAcquire(double micros, bool dropped) noexcept
    {
        if (!s_enabled.load()) return;
        int bucket;
        if (dropped)              bucket = static_cast<int>(LockBucket::Dropped);
        else if (micros < 10.0)   bucket = static_cast<int>(LockBucket::Under10us);
        else if (micros < 100.0)  bucket = static_cast<int>(LockBucket::Us10to100);
        else if (micros < 500.0)  bucket = static_cast<int>(LockBucket::Us100to500);
        else if (micros < 1000.0) bucket = static_cast<int>(LockBucket::Ms05to1);
        else                      bucket = static_cast<int>(LockBucket::Ms1to2);
        ++s_lockBuckets[bucket];
    }

    void EndPresent(ID3D11DeviceContext* ctx, std::uint64_t paintCount, std::uint64_t dropTotal) noexcept
    {

        if (!s_enabled.load()) return;
        CollectGpu(ctx);
        Report(paintCount, dropTotal);
    }

    void GetSnapshot(Snapshot& out) noexcept
    {
        std::lock_guard<std::mutex> lock(s_snapMutex);
        out = s_snapshot;
    }
}
