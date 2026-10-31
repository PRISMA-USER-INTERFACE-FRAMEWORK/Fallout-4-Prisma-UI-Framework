#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = ROOT / "src/PrismaUI"
text = "\n".join(path.read_text(encoding="utf-8") for path in sorted(SRC_DIR.glob("ModelPreview*.cpp")) + sorted(SRC_DIR.glob("ModelPreview*.h")))


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(token: str, message: str) -> None:
    if token not in text:
        fail(message)


def reject(token: str, message: str) -> None:
    if token in text:
        fail(message)


# The pre-Tick diagnostic must not share a non-atomic std::thread::id between render and worker
# threads. It is diagnostic-only, so a release/acquire boolean is the full synchronization needed.
reject("g_engineThreadId", "ModelPreview reintroduced the racy shared engine thread id")
require("static std::atomic<bool>     g_tickCoreSeen{ false };",
        "ModelPreview must track first TickCore with an atomic boolean")
require("g_tickCoreSeen.store(true, std::memory_order_release);",
        "TickCore must publish first-service state with release ordering")
require("g_tickCoreSeen.load(std::memory_order_acquire)",
        "model workers must read first-service state with acquire ordering")

# ReadReq is stack-owned by the worker. Once ServiceEngineReads claims it, shutdown may not let the
# worker unwind until the engine-thread read marks it done.
require("bool claimed = false;", "ReadReq must track render-thread ownership")
require("static std::deque<ReadReq*>    g_readQueue;",
        "engine read queue must use O(1) front removal")
require("static std::atomic<bool>       g_readServiceQueued{ false };",
        "engine read servicing must coalesce dispatcher wakeups")
require("\nvoid ServiceEngineReadsOnGameThread() {",
        "engine reads must be serviced outside the Present hook")
require("GameThreadDispatcher::Dispatch([]",
        "engine read servicing must be deferred through the game-thread dispatcher")
reject("ServiceEngineReads();", "Present must not service blocking engine reads inline")
reject("g_readQueue.erase(g_readQueue.begin())",
       "engine read queue regressed to vector-style front erasure")
claim = text.find("req->claimed = true;")
pop = text.find("g_readQueue.pop_front();")
read = text.find("ReadGameFileOnEngineThread(*req->path, *req->out)")
if min(claim, pop, read) < 0 or not (claim < pop < read):
    fail("ServiceEngineReads must claim the stack request before removing/using it")
require("!req.claimed || !g_readServicingActive.load",
        "shutdown may release a claimed worker read only when nothing is servicing it (g_readServicingActive)")
require("g_readServicingActive.store(true",
        "servicing-active must be marked when a read is claimed, before the engine read")
require("g_readServicingActive.store(false",
        "servicing-active must be cleared when the read completes")
sa_set = text.find("g_readServicingActive.store(true")
if not (claim < sa_set < read):
    fail("g_readServicingActive must be set at claim time (under the read lock), before the engine read")
shutdown = text.find("void Shutdown() {")
if shutdown < 0:
    fail("ModelPreview Shutdown is missing")
tick = text.find("void TickCore(")
direct_tick_service = text.find("ServiceEngineReadsOnGameThread();", tick, shutdown)
if direct_tick_service >= 0:
    fail("TickCore must request deferred engine-read servicing rather than call it inline")
shutdown_call = text.find("ShutdownLoadWorkers();", shutdown)
shutdown_service = text.find("ServiceEngineReadsOnGameThread();", shutdown)
shutdown_workers = text.find("void ShutdownLoadWorkers() {")
shutdown_join = text.find("for (auto& worker : g_workers) if (worker.joinable()) worker.join();", shutdown_workers)
if min(shutdown_call, shutdown_service, shutdown_workers, shutdown_join) < 0 or not (shutdown < shutdown_service < shutdown_call):
    fail("Shutdown must ServiceEngineReads claimed/queued engine files before joining workers")

# Worker objects own their threads directly. Startup must be rollback-safe if construction of a later
# thread fails after earlier workers have already started.
require("static std::vector<std::thread> g_workers;",
        "worker pool must own std::thread values")
reject("std::vector<std::thread*>", "heap-owned worker thread pointers returned")
require("static bool EnsureWorkersStarted(std::unique_lock<std::mutex>& lock)",
        "worker startup rollback helper is missing")
require("for (auto& worker : g_workers) if (worker.joinable()) worker.join();",
        "worker shutdown/startup rollback must join owned threads")

# A newer request for the same preview must replace queued stale work before the queue-cap check.
coalesce = text.find("if (queued.fullKey == fullKey)")
cap = text.find("if (queue.size() >= kMaxJobsPerView) return false;")
if coalesce < 0 or cap < 0 or coalesce > cap:
    fail("queued ModelPreview work must be coalesced before enforcing the per-view cap")
require("queued = Job{ fullKey, loadGen, req };",
        "queued superseded work must be replaced with the newest request")
require("CancelQueuedJobsForView(viewId);",
        "whole-view hide/destroy must cancel queued model jobs")
require("CancelQueuedJob(viewId, key);",
        "single-preview hide must cancel its queued model job")

# Overlay discovery must share one per-frame scan rather than GetActiveViews plus one full scan per
# view. ModelPreview knows the RT dimensions and should provide them to the compositor directly.
require("inline std::map<ViewId, std::vector<Overlay>> g_overlayCache;",
        "ModelPreview grouped overlay cache is missing")
require("void RebuildOverlayCache() {", "ModelPreview overlay cache builder is missing")
require("o.sourceWidth = static_cast<uint32_t>(g_cfg.rtSize);",
        "ModelPreview must publish known overlay source width")
require("o.sourceHeight = static_cast<uint32_t>(g_cfg.rtSize);",
        "ModelPreview must publish known overlay source height")
reject("std::set<ViewId> seen;", "GetActiveViews regressed to a separate set-building scan")

# Reconstruction safety: this line is easy to silently mistype and changes quaternion rotation math.
require("const float yy = y * ys, yz = y * zs, zz = z * zs;",
        "QuatToXf yy term is not y*ys")
reject("const float yy = y * xs, yz = y * zs, zz = z * zs;",
       "QuatToXf reconstruction typo returned")

# Single-threaded devices: CPU parse runs on workers, GPU buffers are created on the render thread.
post_job = text.find("\nbool PostJob(")
inline_gate = text.find("if (!WorkerThreadsEnabled()) {", post_job)
if post_job < 0 or inline_gate < 0 or "g_devSingleThreaded" in text[post_job:inline_gate]:
    fail("PostJob's inline branch must be gated on bWorkerThreads alone")

worker_main = text.find("static void WorkerMain()")
worker_end = text.find("static bool EnsureWorkersStarted(", worker_main)
single_branch = text.find("if (g_devSingleThreaded.load()) {", worker_main, worker_end)
single_end = text.find("} else {", single_branch, worker_end)
if min(worker_main, worker_end, single_branch, single_end) < 0:
    fail("WorkerMain must branch on g_devSingleThreaded")
single_body = text[single_branch:single_end]
if "LoadCpuModel(" not in single_body:
    fail("WorkerMain's single-threaded branch must run only the CPU stage")
for token in ("LoadModel(", "BuildGpuModel(", "g_devForWorker"):
    if token in single_body:
        fail(f"WorkerMain's single-threaded branch must not call or touch {token}")

tick_core = text.find("static void TickCoreImpl(")
drain = text.find("for (auto& res : TakeLoadResults()) {", tick_core)
stale = text.find("if (res.loadGen != pv.loadGen) continue;", drain)
finish = text.find("FinishLoadResult(dev, res);", drain, stale)
if min(tick_core, drain, stale, finish) < 0:
    fail("TickCoreImpl must finish CPU-built models while draining results")
require("if (res.cpu && !LookupLoadedModel(res.key, res.model)) {",
        "render-thread finish must reuse an already-cached model before BuildGpuModel")
finish_helper = text.find("void FinishLoadResult(")
helper_try = text.find("try {", finish_helper)
helper_call = text.find("res.model = FinishModel(dev, std::move(*res.cpu));", helper_try)
helper_catch = text.find("} catch (...) {", helper_call)
if min(finish_helper, helper_try, helper_call, helper_catch) < 0:
    fail("render-thread FinishModel must sit inside try/catch so one failure cannot abandon the drain batch")

internal = (SRC_DIR / "ModelPreviewInternal.h").read_text(encoding="utf-8")
for line in internal.splitlines():
    if (line.startswith("static ") and not line.startswith("static_assert")) or "namespace {" in line:
        fail(f"ModelPreviewInternal.h shared state must be inline in a named namespace, not per-TU: {line.strip()}")

print("ok    ModelPreview queue lifetime, worker ownership, coalescing, and overlay scan invariants")
