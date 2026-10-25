#include "ModelPreview.h"

#include "FreezeDiagnostics.h"
#include "ModelPreviewInternal.h"
#include "ModelPreviewShutdownGate.h"
#include "Utils/ModulePath.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

namespace PrismaUI::ModelPreview {

static void LoadConfigOnce() {

    static std::once_flag s_once;
    std::call_once(s_once, [] {
    g_cfg.loaded = true;

    const std::wstring ini =
        (PrismaUI::Utils::ThisModuleDir() / L"PrismaUI_ModelPreview.ini").wstring();
    g_cfg.enabled = GetPrivateProfileIntW(L"General", L"bEnabled", 1, ini.c_str()) != 0;
    int sz = GetPrivateProfileIntW(L"General", L"iRTSize", 512, ini.c_str());
    g_cfg.rtSize = sz < 128 ? 128 : (sz > 2048 ? 2048 : sz);
    g_cfg.spinDegPerSec = (float)GetPrivateProfileIntW(L"General", L"iSpinDegPerSec", 45, ini.c_str());

    g_cfg.workerThreads = GetPrivateProfileIntW(L"General", L"bWorkerThreads", 1, ini.c_str()) != 0;
    int cap = GetPrivateProfileIntW(L"General", L"iMaxLivePreviews", 192, ini.c_str());
    g_cfg.maxLivePreviews = cap < 32 ? 32 : (cap > 4096 ? 4096 : cap);
    wchar_t solo[256]{};
    GetPrivateProfileStringW(L"Debug", L"sSoloShape", L"", solo, 256, ini.c_str());
    if (solo[0]) { int n = WideCharToMultiByte(CP_UTF8, 0, solo, -1, nullptr, 0, nullptr, nullptr);
                   std::string s(n > 0 ? n - 1 : 0, '\0');
                   if (n > 0) WideCharToMultiByte(CP_UTF8, 0, solo, -1, s.data(), n, nullptr, nullptr);
                   g_cfg.soloShape = s; }
    logger::info("[ModelPreview] enabled={} rtSize={} spin={}deg/s maxLivePreviews={} soloShape='{}'",
                 g_cfg.enabled, g_cfg.rtSize, g_cfg.spinDegPerSec, g_cfg.maxLivePreviews, g_cfg.soloShape);
    });
}

bool Enabled() {
    LoadConfigOnce();
    return g_cfg.enabled;
}

bool WorkerThreadsEnabled() noexcept { return g_cfg.workerThreads; }

static uint64_t g_nextPreviewContinuity = 1;
static std::vector<uint64_t> g_removedOverlayKeys;
static ViewGateFn g_viewGate = nullptr;
static std::mutex g_reqMutex;
static std::map<std::string, Request> g_pendingShows;
static std::vector<std::string> g_pendingHides;

static constexpr int kMaxStaticRendersPerFrame = 8;

static constexpr int kMaxRendersPerFrame = 8;

static std::vector<Preview*> g_staticScratch;
static std::vector<Preview*> g_spinScratch;

static uint64_t g_touchClock = 0;

static uint64_t g_loadGenClock = 0;

static void RetractImage(Preview& pv) {
    if (ImageSourcePolicy::Retire(pv.image)) g_retractedImageKeys.push_back(pv.continuityKey);
}

static void ErasePreview(std::map<std::string, Preview>::iterator& it) {
    if (it->second.continuityKey != 0) g_removedOverlayKeys.push_back(it->second.continuityKey);
    if (!it->second.image.url.empty()) g_releasedImageKeys.push_back(it->second.continuityKey);
    ReleaseTarget(it->second);
    it = g_previews.erase(it);
}

static void EvictToStub(Preview& pv) {
    ReleaseTarget(pv);
    RetractImage(pv);
    pv.model.reset();
    pv.drawn = false;
    pv.statusTexSent = false;
}

void Show(ViewId viewId, const std::string& json) {
    if (!Enabled()) return;
    Request r;
    r.viewId = viewId;
    r.id = JsonStr(json, "id");

    r.nifPaths = JsonStrArray(json, "nifPaths");
    if (r.nifPaths.empty()) {
        std::string single = JsonStr(json, "nifPath");
        if (!single.empty()) r.nifPaths.push_back(std::move(single));
    }
    r.formType = JsonStr(json, "formType");
    r.matSwap = JsonStr(json, "matSwap");
    r.rect = { (float)JsonNum(json, "x", 0), (float)JsonNum(json, "y", 0),
               (float)JsonNum(json, "w", 0), (float)JsonNum(json, "h", 0) };
    r.clip = { (float)JsonNum(json, "clipX", 0), (float)JsonNum(json, "clipY", 0),
               (float)JsonNum(json, "clipW", 0), (float)JsonNum(json, "clipH", 0) };
    r.zoom = (float)JsonNum(json, "zoom", 1.0);
    r.panX = (float)JsonNum(json, "panX", 0.0);
    r.panY = (float)JsonNum(json, "panY", 0.0);
    r.roll = (float)JsonNum(json, "roll", 0.0);
    r.pitch = (float)JsonNum(json, "pitch", 0.0);
    r.brightness = (float)JsonNum(json, "brightness", 1.0);
    r.spin = (float)JsonNum(json, "spin", -10000.0);
    r.yaw = (float)JsonNum(json, "yaw", -10000.0);
    r.flip = JsonNum(json, "flip", 0.0) != 0.0;
    r.imageSource = JsonBool(json, "imageSource");
    if (r.nifPaths.empty()) return;
    std::lock_guard<std::mutex> lock(g_reqMutex);
    const std::string key = MakeFullKey(viewId, r.id);

    std::erase(g_pendingHides, key);
    g_pendingShows[key] = std::move(r);
}

void Hide(ViewId viewId, const std::string& json) {
    std::string id = JsonStr(json, "id");
    if (id.empty()) {
        {
            std::lock_guard<std::mutex> lock(g_reqMutex);
            const std::string prefix = std::to_string(viewId) + "\x1f";
            std::erase_if(g_pendingShows, [&](const auto& kv) { return kv.first.rfind(prefix, 0) == 0; });
            g_pendingHides.push_back(prefix + "*");
        }
        CancelQueuedJobsForView(viewId);
    } else {
        const std::string key = MakeFullKey(viewId, id);
        {
            std::lock_guard<std::mutex> lock(g_reqMutex);
            g_pendingShows.erase(key);
            g_pendingHides.push_back(key);
        }
        CancelQueuedJob(viewId, key);
    }
}

void OnViewDestroyed(ViewId viewId) {
    {
        std::lock_guard<std::mutex> lock(g_reqMutex);

        const std::string prefix = std::to_string(viewId) + "\x1f";
        std::erase_if(g_pendingShows, [&](const auto& kv) { return kv.first.rfind(prefix, 0) == 0; });
        g_pendingHides.push_back(prefix + "*");
    }
    CancelQueuedJobsForView(viewId);
}

void GetOverlays(ViewId viewId, std::vector<Overlay>& out) {
    if (!g_d3dReady || viewId == 0) return;
    RebuildOverlayCache();
    auto it = g_overlayCache.find(viewId);
    if (it == g_overlayCache.end()) return;
    out.insert(out.end(), it->second.begin(), it->second.end());
}

void GetActiveViews(std::vector<ViewId>& out) {
    out.clear();
    if (!g_d3dReady) return;
    RebuildOverlayCache();
    out.reserve(g_overlayCache.size());
    for (const auto& [viewId, overlays] : g_overlayCache) {
        if (!overlays.empty()) out.push_back(viewId);
    }
}

void DrainRemovedOverlayKeys(std::vector<uint64_t>& out) {
    out.insert(out.end(), g_removedOverlayKeys.begin(), g_removedOverlayKeys.end());
    g_removedOverlayKeys.clear();
}

bool HasPendingWork() {
    if (!g_previews.empty()) return true;
    if (!g_retractedImageKeys.empty() || !g_releasedImageKeys.empty()) return true;
    std::lock_guard<std::mutex> lock(g_reqMutex);
    return !g_pendingShows.empty() || !g_pendingHides.empty();
}

void SetViewGate(ViewGateFn fn) { g_viewGate = fn; }

static void TickCoreImpl(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
    static std::vector<std::string> hides;
    static std::vector<std::pair<std::string, Request>> shows;
    hides.clear();
    shows.clear();
    {
        std::lock_guard<std::mutex> lock(g_reqMutex);
        hides.swap(g_pendingHides);
        shows.reserve(g_pendingShows.size());
        for (auto& kv : g_pendingShows) shows.emplace_back(kv.first, std::move(kv.second));
        g_pendingShows.clear();
    }
    for (auto& h : hides) {
        bool wholeView = h.size() >= 2 && h.back() == '*' && h[h.size() - 2] == '\x1f';
        if (wholeView) {
            std::string prefix = h.substr(0, h.size() - 1);
            for (auto it = g_previews.begin(); it != g_previews.end();) {
                if (it->first.rfind(prefix, 0) == 0) ErasePreview(it); else ++it;
            }
        } else {
            auto it = g_previews.find(h);
            if (it != g_previews.end()) ErasePreview(it);
        }
    }

    if (!EnsureRenderResources(dev)) return;

    static std::vector<std::pair<ViewId, bool>> viewActive;
    viewActive.clear();
    auto viewIsActive = [&](ViewId v) -> bool {
        if (!g_viewGate) return true;
        for (const auto& [id, active] : viewActive)
            if (id == v) return active;
        const bool a = g_viewGate(v);
        viewActive.emplace_back(v, a);
        return a;
    };

    auto evictOneVictim = [&](const std::string& keepKey) -> bool {
        auto victim = g_previews.end();
        bool victimInactive = false;
        uint64_t victimTouch = 0;
        for (auto it = g_previews.begin(); it != g_previews.end(); ++it) {
            if (it->first == keepKey) continue;

            const Preview& cand = it->second;
            if (!(cand.tex && cand.rtv && cand.srv)) continue;
            const bool inactive = !viewIsActive(it->second.viewId);
            const uint64_t t = it->second.touch;
            bool better;
            if (victim == g_previews.end())      better = true;
            else if (inactive != victimInactive) better = inactive;
            else                                 better = t < victimTouch;
            if (better) { victim = it; victimInactive = inactive; victimTouch = t; }
        }
        if (victim == g_previews.end()) return false;
        EvictToStub(victim->second);
        ++g_evictions;
        return true;
    };

    auto ensureTargetCapped = [&](Preview& pv, const std::string& keepKey) -> bool {

        const bool hasCompleteTarget = pv.tex && pv.rtv && pv.srv;
        if (!hasCompleteTarget)
            while (g_rtLive >= (size_t)g_cfg.maxLivePreviews)
                if (!evictOneVictim(keepKey)) break;
        return EnsurePreviewTarget(pv);
    };

    static std::vector<std::pair<std::string, Request>> requeue;
    requeue.clear();

    for (auto& [fullKey, r] : shows) {
        Preview& pv = g_previews[fullKey];
        if (pv.continuityKey == 0) pv.continuityKey = g_nextPreviewContinuity++;
        pv.viewId = r.viewId; pv.id = r.id; pv.rect = r.rect; pv.clip = r.clip;
        bool contentChanged = pv.model && (pv.zoom != r.zoom || pv.panX != r.panX || pv.panY != r.panY ||
            pv.roll != r.roll || pv.pitch != r.pitch || pv.brightness != r.brightness ||
            pv.flip != r.flip || pv.spin != r.spin);
        pv.zoom = r.zoom; pv.panX = r.panX; pv.panY = r.panY; pv.roll = r.roll; pv.pitch = r.pitch;
        pv.brightness = r.brightness; pv.flip = r.flip;
        if (!r.imageSource) RetractImage(pv);
        pv.imageSource = r.imageSource;

        std::string itemKey = JoinNifPathsKey(r.nifPaths) + "\x1f" + LowerStr(r.matSwap);
        const bool sameItem = pv.modelKey == itemKey && pv.model;
        const bool yawProvided = r.yaw > -9999.0f;
        const bool wasSpinning = EffSpin(pv.spin) != 0.0f, willSpin = EffSpin(r.spin) != 0.0f;
        float oldYaw = pv.yaw;
        if (yawProvided) pv.yaw = r.yaw;
        else if (sameItem && wasSpinning && !willSpin) pv.yaw = pv.lastAngleDeg;
        else if (sameItem && !wasSpinning && willSpin) { pv.yaw = pv.lastAngleDeg;
            pv.spinStart = std::chrono::steady_clock::now(); pv.spinStarted = true; }
        else if (!sameItem) pv.yaw = 0.0f;
        pv.spin = r.spin;
        if (pv.model && (contentChanged || pv.yaw != oldYaw)) pv.dirty = true;

        if (!sameItem && pv.wantKey != itemKey) {
            if (GameLoadActive()) {
                requeue.emplace_back(fullKey, r);
                pv.touch = ++g_touchClock;
                continue;
            }
            pv.nifPaths = r.nifPaths; pv.wantKey = itemKey; pv.loadGen = ++g_loadGenClock;
            pv.statusTexSent = false;
            ensureTargetCapped(pv, fullKey);
            Request jobReq = r;
            if (!PostJob(fullKey, pv.loadGen, jobReq)) {

                pv.wantKey.clear();
                requeue.emplace_back(fullKey, r);
            }
        }
        pv.touch = ++g_touchClock;
    }

    if (!requeue.empty()) {
        std::lock_guard<std::mutex> lock(g_reqMutex);

        auto cancelledByHide = [&](const std::string& key) {
            for (const auto& h : g_pendingHides) {
                if (h == key) return true;
                if (h.size() >= 2 && h.back() == '*' && h[h.size() - 2] == '\x1f' &&
                    key.rfind(h.substr(0, h.size() - 1), 0) == 0) return true;
            }
            return false;
        };

        for (auto& [k, req] : requeue)
            if (!cancelledByHide(k)) g_pendingShows.emplace(std::move(k), std::move(req));
    }

    requeue.clear();
    shows.clear();
    for (auto& res : TakeLoadResults()) {
        FinishLoadResult(dev, res);
        auto it = g_previews.find(res.fullKey);
        if (it == g_previews.end()) continue;
        Preview& pv = it->second;

        if (res.loadGen != pv.loadGen) continue;
        pv.wantKey.clear();
        if (res.ok && res.model && ensureTargetCapped(pv, res.fullKey) && ComputePose(pv, res.model, res.key, res.kind)) {
            pv.dirty = true;
            pv.statusTexSent = false;

            logger::info("[ModelPreview] preview ready: view={} nif='{}' rect=({},{},{},{}) items={}",
                         pv.viewId, JoinNifPathsDisplay(pv.nifPaths), (int)pv.rect.x, (int)pv.rect.y, (int)pv.rect.w,
                         (int)pv.rect.h, res.model->items.size());

            EmitStatus(pv, true, "", "loaded");
        } else { pv.model = nullptr; pv.modelKey.clear();
            logger::warn("[ModelPreview] no drawable preview for '{}'", res.key);
            EmitStatus(pv, false, "mesh not found, unreadable, or no drawable shapes", "error"); }
    }

    ++g_previewFrame;

    bool any = false;
    for (auto& [k, pv] : g_previews)
        if (pv.model && pv.rtv && (pv.dirty || EffSpin(pv.spin) != 0.0f) && viewIsActive(pv.viewId)) {
            any = true; break;
        }
    if (any) {
        ScopedStateBackup backup(ctx);

        int totalBudget  = kMaxRendersPerFrame;
        int staticBudget = kMaxStaticRendersPerFrame;

        auto renderOne = [&](Preview& pv) {
            pv.dirty = RenderPreviewInner(ctx, pv);
            pv.lastRenderFrame = g_previewFrame;
            if (pv.model && !pv.dirty && !pv.statusTexSent) {
                EmitStatus(pv, true, "", "textured");
                pv.statusTexSent = true;
            }
        };

        g_staticScratch.clear();
        for (auto& [k, pv] : g_previews) {
            if (totalBudget <= 0) break;
            if (!pv.model || !pv.rtv) continue;
            if (EffSpin(pv.spin) != 0.0f) continue;
            if (!pv.dirty) continue;
            if (!viewIsActive(pv.viewId)) continue;
            g_staticScratch.push_back(&pv);
        }
        std::stable_sort(g_staticScratch.begin(), g_staticScratch.end(),
                         [](const Preview* a, const Preview* b) { return a->lastRenderFrame < b->lastRenderFrame; });
        for (Preview* pvp : g_staticScratch) {
            if (totalBudget <= 0 || staticBudget <= 0) break;
            staticBudget -= 1;
            totalBudget -= 1;
            renderOne(*pvp);
        }

        if (totalBudget > 0) {
            g_spinScratch.clear();
            for (auto& [k, pv] : g_previews) {
                if (!pv.model || !pv.rtv) continue;
                if (EffSpin(pv.spin) == 0.0f) continue;
                if (!viewIsActive(pv.viewId)) continue;
                g_spinScratch.push_back(&pv);
            }
            std::sort(g_spinScratch.begin(), g_spinScratch.end(),
                      [](const Preview* a, const Preview* b) { return a->lastRenderFrame < b->lastRenderFrame; });
            for (Preview* pvp : g_spinScratch) {
                if (totalBudget <= 0) break;
                --totalBudget;
                renderOne(*pvp);
            }
        }
    }
}

void TickCore(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
    const auto tick = g_shutdownGate.EnterTick();
    if (!tick.owns_lock() || !Enabled() || !dev || !ctx) return;
    MarkTickCoreSeen();
    SetLoadDevice(dev);
    int queuedJobs = 0, workerCount = 0;
    LoadQueueUsage(queuedJobs, workerCount);
    FreezeDiagnostics::SetModelPreviewState(queuedJobs, ReadQueueDepth(), WorkersWaitingRead(), workerCount);
    FreezeDiagnostics::ScopedStage tickStage(FreezeDiagnostics::Lane::ModelPreviewServicing,
                                             FreezeDiagnostics::Stage::ModelPreviewTick);
    try {
        RequestEngineReadService();
        ServiceTextureWatchdog();
        TickCoreImpl(dev, ctx);
        RequestEngineReadService();
        LogMemoryTelemetry();
    } catch (const std::exception& e) {
        logger::error("[ModelPreview] TickCore exception: {}", e.what());
    } catch (...) {
        logger::error("[ModelPreview] TickCore unknown exception");
    }
}

void Shutdown() {
    static std::atomic<bool> done{false};
    if (done.exchange(true)) return;
    const auto shutdownLock = g_shutdownGate.BeginShutdown();
    BeginReadShutdown();
    ClearMissingReadPaths();
    ServiceEngineReadsOnGameThread();
    ShutdownLoadWorkers();
    for (auto it = g_previews.begin(); it != g_previews.end();) ErasePreview(it);
    g_overlayCache.clear();
    g_overlayCacheFrame = ~uint64_t{0};
    g_rtFreeList.clear();
    g_rtLive = 0;
    ResetLoadState();

    g_texShutdown.store(true, std::memory_order_release);
    { std::lock_guard<std::mutex> lock(g_texMutex); g_texDone.clear(); g_texOrder.clear(); }
    g_layouts.clear();
    for (auto& vs : g_vs) vs.Reset();
    for (auto& b : g_vsBlob) b.Reset();
    g_ps.Reset(); g_graySrv.Reset(); g_blendState.Reset(); g_depthState.Reset();
    g_blendAlpha.Reset(); g_blendAdditive.Reset(); g_depthStateNoWrite.Reset();
    g_rasterizer.Reset(); g_sampler.Reset(); g_cb.Reset(); g_dsv.Reset(); g_depthTex.Reset();
    g_d3dReady = false; g_dev = nullptr; SetLoadDevice(nullptr);
    g_vramAdapter.Reset();
    logger::info("[ModelPreview] shutdown complete");
}

}
