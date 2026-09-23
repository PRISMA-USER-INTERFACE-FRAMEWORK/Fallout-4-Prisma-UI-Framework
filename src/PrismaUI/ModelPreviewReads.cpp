#ifdef PRISMAUI_MODELPREVIEW_READS_TEST
#include "modelpreview_reads_test_env.h"
#else
#include "ModelPreviewReads.h"

#include "FreezeDiagnostics.h"
#include "GameThreadDispatcher.h"
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace PrismaUI::ModelPreview {

#ifndef PRISMAUI_MODELPREVIEW_READS_TEST
static bool DrainStream(RE::BSResourceNiBinaryStream& s, std::vector<uint8_t>& out) {
    if (!s) return false;
    out.clear();
    uint8_t buf[8192];
    for (;;) {
        std::size_t got = s.DoRead(buf, sizeof(buf));
        if (got == 0) break;
        out.insert(out.end(), buf, buf + got);
        if (out.size() > (64u << 20)) return false;
    }
    return out.size() > 0x40;
}

static std::string NormAssetPath(const std::string& in) {
    std::string out; out.reserve(in.size());
    for (char c : in) {
        if (c == '\\') c = '/';
        if (c == '/' && !out.empty() && out.back() == '/') continue;
        out.push_back(c);
    }
    return out;
}
#endif

std::string LowerStr(const std::string& in) {
    std::string r = in;
    for (auto& ch : r) ch = (char)tolower((unsigned char)ch);
    return r;
}

static thread_local bool     t_isModelWorker = false;
static std::atomic<bool>     g_tickCoreSeen{ false };
static std::atomic<uint64_t> g_preTickWorkerReads{0};
struct ReadReq {
    const std::string* path = nullptr;
    std::vector<uint8_t>* out = nullptr;
    bool claimed = false;
    bool ok = false;
    bool done = false;
};
static std::mutex              g_readMutex;
static std::condition_variable g_readCv;
static std::deque<ReadReq*>    g_readQueue;
static std::atomic<bool>       g_readShutdown{ false };
static std::atomic<int>        g_readQueueDepth{ 0 };
static std::atomic<int>        g_workersWaitingRead{ 0 };
static std::atomic<bool>       g_readServicingActive{ false };
static std::atomic<bool>       g_readServiceQueued{ false };
static std::atomic<bool>       g_gameLoadActive{ false };
static constexpr auto kMissingReadRetry = std::chrono::seconds(30);
static std::unordered_map<std::string, std::chrono::steady_clock::time_point> g_missingReadPaths;

static constexpr int    kMaxEngineReadsPerService = 4;
static constexpr double kEngineReadBudgetMs       = 0.5;
static void RequestEngineReadService();
static bool ReadGameFileOnEngineThread(const std::string& rawPath, std::vector<uint8_t>& out);

static void ServiceEngineReadsOnGameThread() {
    FreezeDiagnostics::ScopedStage readStage(FreezeDiagnostics::Lane::ModelPreviewServicing,
                                             FreezeDiagnostics::Stage::ModelPreviewRead);
    if (g_gameLoadActive.load(std::memory_order_acquire)) return;
    const auto start = std::chrono::steady_clock::now();
    int serviced = 0;
    for (;;) {
        ReadReq* req = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_readMutex);
            if (g_readQueue.empty()) return;
            req = g_readQueue.front();
            req->claimed = true;
            g_readServicingActive.store(true, std::memory_order_release);
            g_readQueue.pop_front();
            g_readQueueDepth.store(static_cast<int>(g_readQueue.size()), std::memory_order_relaxed);
        }
        bool ok = false;
        try {
            ok = ReadGameFileOnEngineThread(*req->path, *req->out);
        } catch (...) {
            req->out->clear();
        }
        {
            std::lock_guard<std::mutex> lock(g_readMutex);
            req->ok = ok;
            req->done = true;
            g_readServicingActive.store(false, std::memory_order_release);
        }
        g_readCv.notify_all();

        if (++serviced >= kMaxEngineReadsPerService) return;
        const double elapsedMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        if (elapsedMs >= kEngineReadBudgetMs) return;
    }
}

void RequestEngineReadService() {
    if (g_gameLoadActive.load(std::memory_order_acquire)) return;
    {
        std::lock_guard<std::mutex> lock(g_readMutex);
        if (g_readQueue.empty()) return;
    }
    bool expected = false;
    if (!g_readServiceQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
    if (!GameThreadDispatcher::Dispatch([] {
            ServiceEngineReadsOnGameThread();
            g_readServiceQueued.store(false, std::memory_order_release);
            RequestEngineReadService();
        })) {
        g_readServiceQueued.store(false, std::memory_order_release);
    }
}

bool ReadGameFile(const std::string& rawPath, std::vector<uint8_t>& out) {
    if (!t_isModelWorker) {
        if (g_gameLoadActive.load(std::memory_order_acquire)) return false;
        return ReadGameFileOnEngineThread(rawPath, out);
    }

    if (!g_tickCoreSeen.load(std::memory_order_acquire)) {
        const uint64_t n = ++g_preTickWorkerReads;
        if (n <= 3 || n % 50 == 0) {
            logger::warn("[ModelPreview] worker read BEFORE TickCore #{} '{}' -- the old guard would "
                         "have run this engine call on this worker thread (heap corruption)",
                         n, rawPath);
        }
    }

    ReadReq req;
    req.path = &rawPath;
    req.out = &out;
    {
        std::lock_guard<std::mutex> lock(g_readMutex);
        if (g_readShutdown.load()) return false;
        g_readQueue.push_back(&req);
        g_readQueueDepth.store(static_cast<int>(g_readQueue.size()), std::memory_order_relaxed);
    }
    g_workersWaitingRead.fetch_add(1, std::memory_order_relaxed);
    RequestEngineReadService();
    struct WaitingGuard {
        ~WaitingGuard() { g_workersWaitingRead.fetch_sub(1, std::memory_order_relaxed); }
    } waitingGuard;
    std::unique_lock<std::mutex> lk(g_readMutex);
    g_readCv.wait(lk, [&req] {
        return req.done || (g_readShutdown.load() &&
                            (!req.claimed || !g_readServicingActive.load(std::memory_order_acquire)));
    });
    if (!req.done) {
        auto it = std::find(g_readQueue.begin(), g_readQueue.end(), &req);
        if (it != g_readQueue.end()) g_readQueue.erase(it);
        return false;
    }
    return req.ok;
}

#ifndef PRISMAUI_MODELPREVIEW_READS_TEST
static bool ReadGameFileOnEngineThread(const std::string& rawPath, std::vector<uint8_t>& out) {
    const std::string path = NormAssetPath(rawPath);
    const auto lookupNow = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(g_readMutex);
        const auto key = LowerStr(path);
        const auto it = g_missingReadPaths.find(key);
        if (it != g_missingReadPaths.end()) {
            if (lookupNow < it->second) return false;
            g_missingReadPaths.erase(it);
        }
    }
    const auto readStart = std::chrono::steady_clock::now();
    bool plainValid; uint32_t plainErr;
    { RE::BSResourceNiBinaryStream s(path.c_str());
      if (DrainStream(s, out)) {
          const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count();
          if (elapsed >= 5.0) logger::warn("[ModelPreview][SLOWREAD] path='{}' totalMs={:.2f} bytes={} rescan=0", path, elapsed, out.size());
          return true;
      }
      plainValid = static_cast<bool>(s); plainErr = static_cast<uint32_t>(s.lastError); }
    std::unique_ptr<RE::BSResourceNiBinaryStream> rs(RE::BSResourceNiBinaryStream::BinaryStreamWithRescan(path.c_str()));
    if (rs && DrainStream(*rs, out)) {
        const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count();
        if (elapsed >= 5.0) logger::warn("[ModelPreview][SLOWREAD] path='{}' totalMs={:.2f} bytes={} rescan=1", path, elapsed, out.size());
        return true;
    }

    logger::warn("[ModelPreview] read FAIL '{}': plain(valid={},err={}) rescan(ptr={},valid={},err={}) bytes={}",
                 path, plainValid, plainErr, rs != nullptr, rs ? static_cast<bool>(*rs) : false,
                 rs ? static_cast<uint32_t>(rs->lastError) : 0u, out.size());
    {
        std::lock_guard<std::mutex> lock(g_readMutex);
        const auto failureNow = std::chrono::steady_clock::now();
        g_missingReadPaths[LowerStr(path)] = failureNow + kMissingReadRetry;
    }
    return false;
}
#endif

bool GameLoadActive() noexcept {
    return g_gameLoadActive.load(std::memory_order_acquire);
}

void SetReadsGameLoadActive(bool active) noexcept {
    g_gameLoadActive.store(active, std::memory_order_release);
    if (!active) g_readCv.notify_all();
}

void SetWorkerThread(bool isWorker) noexcept {
    t_isModelWorker = isWorker;
}

void MarkTickCoreSeen() noexcept {
    g_tickCoreSeen.store(true, std::memory_order_release);
}

void BeginReadShutdown() noexcept {
    g_readShutdown.store(true);
    g_readCv.notify_all();
}

void ClearMissingReadPaths() noexcept {
    std::lock_guard<std::mutex> lock(g_readMutex);
    g_missingReadPaths.clear();
}

void NotifyReadWaiters() noexcept {
    g_readCv.notify_all();
}

int ReadQueueDepth() noexcept {
    return g_readQueueDepth.load(std::memory_order_relaxed);
}

int WorkersWaitingRead() noexcept {
    return g_workersWaitingRead.load(std::memory_order_relaxed);
}

}
