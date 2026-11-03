#include "ModelPreviewInternal.h"

#include <windows.h>
#include <psapi.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace PrismaUI::ModelPreview {

static IDXGIAdapter3* VramAdapter() {
    if (g_vramAdapter) return g_vramAdapter.Get();
    if (!g_dev) return nullptr;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDev;
    if (FAILED(g_dev->QueryInterface(IID_PPV_ARGS(&dxgiDev)))) return nullptr;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDev->GetAdapter(&adapter))) return nullptr;
    if (FAILED(adapter.As(&g_vramAdapter))) return nullptr;
    return g_vramAdapter.Get();
}

static std::atomic<bool>        g_sampleRequested{ false };
static std::atomic<const char*> g_sampleReason{ nullptr };
static std::atomic<bool>        g_burstPending{ false };
static std::atomic<long long>   g_lastViewCreateMs{ 0 };

constexpr long long kBurstQuietMs = 750;

static long long SteadyNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

void RequestMemorySample(const char* a_reason) {
    g_sampleReason.store(a_reason ? a_reason : "unspecified");
    g_sampleRequested.store(true);
}

void NoteViewCreated() {
    g_lastViewCreateMs.store(SteadyNowMs());
    g_burstPending.store(true);
    RequestMemorySample("view-created");
}

void LogMemoryTelemetry() {
    using clock = std::chrono::steady_clock;
    static clock::time_point s_last{};
    const auto now = clock::now();

    const char* trigger = "timer";
    bool forced = false;
    if (g_burstPending.load() && SteadyNowMs() - g_lastViewCreateMs.load() >= kBurstQuietMs) {
        g_burstPending.store(false);
        forced = true;
        trigger = "view-burst-complete";
    } else if (g_sampleRequested.exchange(false)) {
        forced = true;
        const char* r = g_sampleReason.load();
        trigger = r ? r : "requested";
    }

    if (!forced && s_last != clock::time_point{} && now - s_last < std::chrono::seconds(30)) return;
    s_last = now;

    DXGI_QUERY_VIDEO_MEMORY_INFO vmi{};
    bool gotVram = false;
    if (auto* adapter = VramAdapter())
        gotVram = SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &vmi));

    constexpr std::uint64_t kMB = 1024ull * 1024ull;
    const std::uint64_t vramUsedMB   = gotVram ? vmi.CurrentUsage / kMB : 0;
    const std::uint64_t vramBudgetMB = gotVram ? vmi.Budget / kMB : 0;
    const std::uint64_t vramResMB    = gotVram ? vmi.CurrentReservation / kMB : 0;
    const std::int64_t  vramHeadMB   = static_cast<std::int64_t>(vramBudgetMB) -
                                       static_cast<std::int64_t>(vramUsedMB);

    const std::string vramField =
        gotVram
            ? std::string("vramLocal used=") + std::to_string(vramUsedMB) +
              " MB budget=" + std::to_string(vramBudgetMB) +
              " MB reserved=" + std::to_string(vramResMB) +
              " MB headroom=" + std::to_string(vramHeadMB) +
              " MB" + (vramHeadMB < 0 ? " OVER-BUDGET" : "")
            : std::string("vramLocal unavailable");

    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    const bool gotProc = GetProcessMemoryInfo(GetCurrentProcess(),
                                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                                              sizeof(pmc)) != FALSE;

    std::size_t modelBytes = 0, modelEntries = 0;
    LoadCacheUsage(modelBytes, modelEntries);

    std::size_t texInFlight = 0, texCached = 0;
    {
        std::lock_guard<std::mutex> lock(g_texMutex);
        texInFlight = g_texInFlight.size();
        texCached   = g_texDone.size();
    }

    logger::info("[ModelPreview][mem] trigger={} | process private={} MB working={} MB "
                 "| modelCache={} MB ({} entries) "

                 "| previewEntries={} liveRT={}/{} pooledRT={} evicted={} (rt {}x{}) "
                 "| textures: engine-owned, requested={} resolved={} inFlight={} cached={}{} "
                 "| {}",
                 trigger,
                 gotProc ? pmc.PrivateUsage / (1024 * 1024) : 0,
                 gotProc ? pmc.WorkingSetSize / (1024 * 1024) : 0,
                 modelBytes / (1024 * 1024), modelEntries,
                 g_previews.size(), g_rtLive, g_cfg.maxLivePreviews, g_rtFreeList.size(), g_evictions,
                 g_cfg.rtSize, g_cfg.rtSize,
                 g_texRequested.load(), g_texResolved.load(), texInFlight, texCached,
                 g_texFailClosed.load() ? " FAIL-CLOSED" : "",
                 vramField);
}

}
