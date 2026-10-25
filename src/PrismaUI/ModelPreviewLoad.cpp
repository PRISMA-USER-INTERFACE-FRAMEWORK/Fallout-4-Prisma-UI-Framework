#include "ModelPreviewLoad.h"

#include <atomic>
#include <condition_variable>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace PrismaUI::ModelPreview {

static std::mutex g_resultMutex;
static std::deque<LoadResult> g_results;

static std::mutex g_lruMutex;
static std::list<std::pair<std::string, std::shared_ptr<PreviewModel>>> g_lru;

static constexpr size_t LRU_MAX = 160;
static constexpr std::size_t kMeshBudgetBytes = 192ull * 1024 * 1024;
static std::size_t g_lruBytes = 0;
static std::unordered_set<std::string> g_missing;
static std::atomic<ID3D11Device*> g_devForWorker{ nullptr };

static std::atomic<bool> g_devSingleThreaded{ false };

void SetLoadDevice(ID3D11Device* dev) { g_devForWorker.store(dev); }
void SetDeviceSingleThreaded(bool singleThreaded) { g_devSingleThreaded.store(singleThreaded); }

static bool LookupLoadedModel(const std::string& key, std::shared_ptr<PreviewModel>& out) {
    std::lock_guard<std::mutex> lock(g_lruMutex);
    for (auto it = g_lru.begin(); it != g_lru.end(); ++it)
        if (it->first == key) { g_lru.splice(g_lru.begin(), g_lru, it); out = g_lru.front().second; return true; }
    return g_missing.count(key) != 0;
}

std::shared_ptr<PreviewModel> CacheLoadedModel(std::string key, std::shared_ptr<PreviewModel> pm) {
    std::lock_guard<std::mutex> lock(g_lruMutex);
    for (auto& e : g_lru) if (e.first == key) return e.second;
    g_lruBytes += pm->gpuBytes;
    g_lru.emplace_front(std::move(key), pm);

    while ((g_lru.size() > LRU_MAX || g_lruBytes > kMeshBudgetBytes) && g_lru.size() > 1) {
        const auto& victim = g_lru.back();
        g_lruBytes -= (victim.second ? victim.second->gpuBytes : 0);
        g_lru.pop_back();
    }
    return pm;
}

void MarkLoadMissing(const std::string& key) {
    std::lock_guard<std::mutex> lock(g_lruMutex);
    g_missing.insert(key);
}

static std::shared_ptr<PreviewModel> LoadModel(ID3D11Device* dev, const std::vector<std::string>& nifPaths,
                                                const std::string& matSwap) {
    if (nifPaths.empty()) return nullptr;
    std::string key = JoinNifPathsKey(nifPaths) + "\x1f" + LowerStr(matSwap);
    std::shared_ptr<PreviewModel> cached;
    if (LookupLoadedModel(key, cached)) return cached;
    std::shared_ptr<CpuModel> cpu = LoadCpuModel(key, nifPaths, matSwap);
    return cpu ? FinishModel(dev, std::move(*cpu)) : nullptr;
}

struct Job { std::string fullKey; uint64_t loadGen = 0; Request req; };
static std::mutex g_jobMutex;
static std::condition_variable g_jobCv;

static std::map<ViewId, std::deque<Job>> g_jobsByView;
static ViewId g_rrCursor = 0;

constexpr size_t kWorkerCount = 3;

static constexpr size_t kMaxJobsPerView = 256;
static std::vector<std::thread> g_workers;
static bool g_workerStarted = false;
static std::atomic<bool> g_workerExit{ false };

static bool AnyJobs() { for (auto& [v, q] : g_jobsByView) if (!q.empty()) return true; return false; }

static bool PopNextJob(Job& out) {
    if (g_jobsByView.empty()) return false;
    auto it = g_jobsByView.upper_bound(g_rrCursor);
    for (size_t n = 0; n <= g_jobsByView.size(); ++n) {
        if (it == g_jobsByView.end()) it = g_jobsByView.begin();
        if (!it->second.empty()) {
            g_rrCursor = it->first;
            out = std::move(it->second.front());
            it->second.pop_front();
            if (it->second.empty()) g_jobsByView.erase(it);
            return true;
        }
        ++it;
    }
    return false;
}

void CancelQueuedJobsForView(ViewId viewId) {
    std::lock_guard<std::mutex> lock(g_jobMutex);
    g_jobsByView.erase(viewId);
}

void CancelQueuedJob(ViewId viewId, const std::string& fullKey) {
    std::lock_guard<std::mutex> lock(g_jobMutex);
    auto it = g_jobsByView.find(viewId);
    if (it == g_jobsByView.end()) return;
    std::erase_if(it->second, [&](const Job& job) { return job.fullKey == fullKey; });
    if (it->second.empty()) g_jobsByView.erase(it);
}

static int KindFromFormType(const std::string& ft) {
    if (ft == "WEAP") return kKindElongate;
    if (ft == "AMMO") return kKindAmmo;
    if (ft == "ARMO") return kKindArmor;

    if (ft == "STAT" || ft == "FURN" || ft == "WORLD") return kKindStatic;
    return kKindGeneric;
}

static void WorkerMain() {
    SetWorkerThread(true);
    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(g_jobMutex);
            g_jobCv.wait(lock, [] {
                return g_workerExit.load() ||
                       (!GameLoadActive() && AnyJobs());
            });
            if (g_workerExit.load()) return;
            if (!PopNextJob(job)) continue;
        }
        LoadResult res; res.fullKey = job.fullKey; res.loadGen = job.loadGen;
        res.key = JoinNifPathsKey(job.req.nifPaths) + "\x1f" + LowerStr(job.req.matSwap);
        res.kind = KindFromFormType(job.req.formType);
        try {
            if (g_devSingleThreaded.load()) {
                if (!job.req.nifPaths.empty() && !LookupLoadedModel(res.key, res.model))
                    res.cpu = LoadCpuModel(res.key, job.req.nifPaths, job.req.matSwap);
                res.ok = res.model != nullptr;
            } else {
                ID3D11Device* dev = g_devForWorker.load();
                if (dev && !job.req.nifPaths.empty()) { res.model = LoadModel(dev, job.req.nifPaths, job.req.matSwap);
                    res.ok = res.model != nullptr; }
            }
        } catch (...) {
            logger::error("[ModelPreview] exception during model load"); res.ok = false; res.model = nullptr;
        }
        { std::lock_guard<std::mutex> lock(g_resultMutex); g_results.push_back(std::move(res)); }
    }
}

static bool EnsureWorkersStarted(std::unique_lock<std::mutex>& lock) {
    if (g_workerStarted) return true;
    if (g_workerExit.load()) return false;
    try {
        g_workers.reserve(kWorkerCount);
        for (size_t i = 0; i < kWorkerCount; ++i) g_workers.emplace_back(WorkerMain);
        g_workerStarted = true;
        return true;
    } catch (const std::exception& e) {
        logger::error("[ModelPreview] worker pool startup failed: {}", e.what());
    } catch (...) {
        logger::error("[ModelPreview] worker pool startup failed with an unknown exception");
    }

    g_workerExit.store(true);
    lock.unlock();
    g_jobCv.notify_all();
    for (auto& worker : g_workers) if (worker.joinable()) worker.join();
    lock.lock();
    g_workers.clear();
    g_workerExit.store(false);
    return false;
}

bool PostJob(const std::string& fullKey, uint64_t loadGen, const Request& req) {

    if (GameLoadActive()) return false;

    if (!WorkerThreadsEnabled()) {
        LoadResult res; res.fullKey = fullKey; res.loadGen = loadGen;
        res.key = JoinNifPathsKey(req.nifPaths) + "\x1f" + LowerStr(req.matSwap);
        res.kind = KindFromFormType(req.formType);
        try {
            ID3D11Device* dev = g_devForWorker.load();
            if (dev && !req.nifPaths.empty()) {
                res.model = LoadModel(dev, req.nifPaths, req.matSwap);
                res.ok = res.model != nullptr;
            }
        } catch (...) {
            logger::error("[ModelPreview] exception during inline (bWorkerThreads=0) model load");
            res.ok = false; res.model = nullptr;
        }
        { std::lock_guard<std::mutex> lock(g_resultMutex); g_results.push_back(std::move(res)); }
        return true;
    }

    std::unique_lock<std::mutex> lock(g_jobMutex);
    if (g_workerExit.load() || !EnsureWorkersStarted(lock)) return false;

    auto& queue = g_jobsByView[req.viewId];
    for (auto& queued : queue) {
        if (queued.fullKey == fullKey) {
            queued = Job{ fullKey, loadGen, req };
            lock.unlock();
            g_jobCv.notify_one();
            return true;
        }
    }
    if (queue.size() >= kMaxJobsPerView) return false;
    queue.push_back(Job{ fullKey, loadGen, req });
    lock.unlock();
    g_jobCv.notify_one();
    return true;
}

std::deque<LoadResult> TakeLoadResults() {
    std::deque<LoadResult> taken;
    { std::lock_guard<std::mutex> lock(g_resultMutex); taken.swap(g_results); }
    return taken;
}

void FinishLoadResult(ID3D11Device* dev, LoadResult& res) {
    if (res.cpu && !LookupLoadedModel(res.key, res.model)) {
        try {
            res.model = FinishModel(dev, std::move(*res.cpu));
        } catch (...) {
            logger::error("[ModelPreview] exception finishing model on render thread");
            res.model = nullptr;
        }
    }
    res.cpu.reset();
    res.ok = res.model != nullptr;
}

void LoadCacheUsage(std::size_t& bytes, std::size_t& entries) {
    std::lock_guard<std::mutex> lock(g_lruMutex);
    bytes = g_lruBytes;
    entries = g_lru.size();
}

void LoadQueueUsage(int& queuedJobs, int& workerCount) {
    std::lock_guard<std::mutex> lock(g_jobMutex);
    for (const auto& [view, jobs] : g_jobsByView) queuedJobs += static_cast<int>(jobs.size());
    workerCount = g_workerStarted ? static_cast<int>(kWorkerCount) : 0;
}

void ShutdownLoadWorkers() {
    {
        std::lock_guard<std::mutex> lock(g_jobMutex);
        g_workerExit.store(true);
        g_jobsByView.clear();
    }
    g_jobCv.notify_all();
    for (auto& worker : g_workers) if (worker.joinable()) worker.join();
    {
        std::lock_guard<std::mutex> lock(g_jobMutex);
        g_workers.clear();
        g_workerStarted = false;
    }
}

void SetGameLoadActive(bool active) noexcept {
    SetReadsGameLoadActive(active);
    if (!active) g_jobCv.notify_all();
}

void ResetLoadState() {
    { std::lock_guard<std::mutex> lock(g_lruMutex); g_lru.clear(); g_missing.clear(); g_lruBytes = 0; }
    { std::lock_guard<std::mutex> lock(g_resultMutex); g_results.clear(); }
}

namespace {
struct WorkerExitBackstop {
    ~WorkerExitBackstop() {
        g_workerExit.store(true);
        BeginReadShutdown();
        g_jobCv.notify_all();
        for (auto& worker : g_workers) {
            if (worker.joinable()) worker.join();
        }
    }
};
WorkerExitBackstop g_workerExitBackstop;
}

}
