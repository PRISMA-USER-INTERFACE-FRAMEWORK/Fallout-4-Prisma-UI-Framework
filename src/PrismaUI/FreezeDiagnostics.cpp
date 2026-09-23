#include "PCH.h"

#include <windows.h>
#include <DbgHelp.h>

#include "PrismaUI/FreezeDiagnostics.h"
#include "PrismaUI/FreezeDiagnosticsCore.h"
#include "Utils/ModulePath.h"

#include <atomic>
#include <chrono>
#include <cwchar>
#include <filesystem>
#include <system_error>
#include <thread>

#pragma comment(lib, "Dbghelp.lib")

namespace PrismaUI::FreezeDiagnostics {

    namespace {

        struct AtomicLane {
            std::atomic<uint32_t> threadId{0};
            std::atomic<uint32_t> stage{static_cast<uint32_t>(Stage::Idle)};
            std::atomic<int64_t>  heartbeatMs{0};
            std::atomic<uint64_t> view{0};
            std::atomic<uint64_t> generation{0};
            std::atomic<int64_t>  detail{0};
        };

        AtomicLane            g_lanes[kLaneCount];
        std::atomic<bool>     g_everPresented{false};
        std::atomic<void*>    g_presentHwnd{nullptr};
        std::atomic<uint64_t> g_gpuCompleted{0};
        std::atomic<uint64_t> g_gpuActive{0};
        std::atomic<bool>     g_frameRequested{false};
        std::atomic<uint32_t> g_dsOp{0};
        std::atomic<int64_t>  g_dsSince{0};
        std::atomic<uint32_t> g_dsThread{0};
        std::atomic<uint64_t> g_dropView{0};
        std::atomic<int64_t>  g_dropSince{0};
        std::atomic<int64_t>  g_shutdownJoinSince{0};
        std::atomic<int32_t>  g_mpJobs{0};
        std::atomic<int32_t>  g_mpReads{0};
        std::atomic<int32_t>  g_mpClaimed{0};
        std::atomic<int32_t>  g_mpWorkers{0};
        std::atomic<int64_t>  g_lastRender{0};
        std::atomic<int64_t>  g_lastPresent{0};
        std::atomic<int64_t>  g_lastComposite{0};
        std::atomic<bool>     g_gameLoadActive{false};
        std::atomic<int64_t>  g_gameLoadSince{0};
        std::atomic<bool>     g_watchdogRunning{false};
        std::atomic<bool>     g_watchdogStarted{false};

        struct PendingAcceleratedAvDump {
            std::atomic<std::uint32_t> state{0};
            EXCEPTION_RECORD exception{};
            CONTEXT context{};
            std::uint32_t threadId = 0;
            std::uint32_t evidenceBytes = 0;
            BYTE evidence[16384]{};
        };

        PendingAcceleratedAvDump g_pendingAcceleratedAvDump;

        int64_t NowMs() noexcept {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                .count();
        }

        AtomicLane& LaneRef(Lane lane) noexcept { return g_lanes[static_cast<std::size_t>(lane)]; }

        bool IsMinimizedSafe() noexcept {
            HWND h = reinterpret_cast<HWND>(g_presentHwnd.load(std::memory_order_relaxed));
            return h != nullptr && ::IsIconic(h) != 0;
        }

        Snapshot Capture(int64_t /*now*/) {
            Snapshot s;
            for (std::size_t i = 0; i < kLaneCount; ++i) {
                AtomicLane& l = g_lanes[i];
                s.lanes[i].threadId = l.threadId.load(std::memory_order_relaxed);
                s.lanes[i].stage = static_cast<Stage>(l.stage.load(std::memory_order_relaxed));
                s.lanes[i].heartbeatMs = l.heartbeatMs.load(std::memory_order_acquire);
                s.lanes[i].view = l.view.load(std::memory_order_relaxed);
                s.lanes[i].generation = l.generation.load(std::memory_order_relaxed);
                s.lanes[i].detail = l.detail.load(std::memory_order_relaxed);
            }
            s.everPresented = g_everPresented.load(std::memory_order_relaxed);
            s.minimized = IsMinimizedSafe();
            s.gameLoadActive = g_gameLoadActive.load(std::memory_order_acquire);
            s.gameLoadSinceMs = g_gameLoadSince.load(std::memory_order_relaxed);
            s.completedGeneration = g_gpuCompleted.load(std::memory_order_relaxed);
            s.activeGeneration = g_gpuActive.load(std::memory_order_relaxed);
            s.frameRequested = g_frameRequested.load(std::memory_order_relaxed);
            s.dispatchSyncOp = static_cast<Stage>(g_dsOp.load(std::memory_order_relaxed));
            s.dispatchSyncSinceMs = g_dsSince.load(std::memory_order_relaxed);
            s.dispatchSyncThread = g_dsThread.load(std::memory_order_relaxed);
            s.dropViewView = g_dropView.load(std::memory_order_relaxed);
            s.dropViewSinceMs = g_dropSince.load(std::memory_order_relaxed);
            s.shutdownJoinSinceMs = g_shutdownJoinSince.load(std::memory_order_relaxed);
            s.mpJobs = g_mpJobs.load(std::memory_order_relaxed);
            s.mpReads = g_mpReads.load(std::memory_order_relaxed);
            s.mpClaimedRead = g_mpClaimed.load(std::memory_order_relaxed);
            s.mpWorkersActive = g_mpWorkers.load(std::memory_order_relaxed);
            s.lastRenderMs = g_lastRender.load(std::memory_order_relaxed);
            s.lastPresentMs = g_lastPresent.load(std::memory_order_relaxed);
            s.lastCompositeMs = g_lastComposite.load(std::memory_order_relaxed);
            return s;
        }

        bool MinidumpEnabled() {
            static const bool on = [] {
                const auto ini = PrismaUI::Utils::PluginIniPath().wstring();
                return ::GetPrivateProfileIntW(L"Debug", L"FreezeMinidump", 1, ini.c_str()) != 0;
            }();
            return on;
        }

        void WriteMiniDump() noexcept {
            try {
                std::error_code ec;
                const auto dir = PrismaUI::Utils::ThisModuleDir() / L"PrismaUI_F4" / L"FreezeDumps";
                std::filesystem::create_directories(dir, ec);
                if (ec) {
                    logger::error("[FREEZE] could not create dump directory: {}", ec.message());
                    return;
                }

                SYSTEMTIME st{};
                ::GetLocalTime(&st);
                static std::atomic<uint32_t> seq{0};
                wchar_t name[160];
                std::swprintf(name, 160, L"PrismaFreeze_%04u%02u%02u_%02u%02u%02u_%03u_%u.dmp", st.wYear,
                              st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                              seq.fetch_add(1, std::memory_order_relaxed));
                const auto dumpPath = dir / name;

                HANDLE file = ::CreateFileW(dumpPath.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                            FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE) {
                    logger::error("[FREEZE] could not open dump file (GLE={})", ::GetLastError());
                    return;
                }

                const auto type = static_cast<MINIDUMP_TYPE>(
                    MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules |
                    MiniDumpWithProcessThreadData);
                const BOOL ok = ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), file, type,
                                                    nullptr, nullptr, nullptr);
                ::CloseHandle(file);
                if (ok) {
                    logger::critical("[FREEZE] wrote minidump to PrismaUI_F4\\FreezeDumps\\{}",
                                     std::filesystem::path(name).string());
                } else {
                    logger::error("[FREEZE] MiniDumpWriteDump failed (GLE={})", ::GetLastError());
                    std::error_code rmEc;
                    std::filesystem::remove(dumpPath, rmEc);
                }
            } catch (const std::exception& e) {
                logger::error("[FREEZE] minidump write threw: {}", e.what());
            } catch (...) {
                logger::error("[FREEZE] minidump write threw");
            }
        }

        void WriteAcceleratedAvDump() noexcept {
            try {
                std::error_code ec;
                const auto dir = PrismaUI::Utils::ThisModuleDir() / L"PrismaUI_F4" / L"FreezeDumps";
                std::filesystem::create_directories(dir, ec);
                if (ec) {
                    logger::error("[GPU-AV] could not create dump directory: {}", ec.message());
                    return;
                }

                SYSTEMTIME st{};
                ::GetLocalTime(&st);
                wchar_t name[160];
                std::swprintf(name, 160, L"PrismaAccelAV_%04u%02u%02u_%02u%02u%02u_%03u.dmp", st.wYear,
                              st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
                const auto dumpPath = dir / name;
                HANDLE file = ::CreateFileW(dumpPath.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                            FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE) {
                    logger::error("[GPU-AV] could not open dump file (GLE={})", ::GetLastError());
                    return;
                }

                EXCEPTION_POINTERS pointers{&g_pendingAcceleratedAvDump.exception, &g_pendingAcceleratedAvDump.context};
                MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
                exceptionInfo.ThreadId = g_pendingAcceleratedAvDump.threadId;
                exceptionInfo.ExceptionPointers = &pointers;
                MINIDUMP_USER_STREAM stream{};
                stream.Type = static_cast<MINIDUMP_STREAM_TYPE>(0x50524953u);
                stream.BufferSize = g_pendingAcceleratedAvDump.evidenceBytes;
                stream.Buffer = g_pendingAcceleratedAvDump.evidence;
                MINIDUMP_USER_STREAM_INFORMATION streams{};
                streams.UserStreamCount = 1;
                streams.UserStreamArray = &stream;
                const auto type = static_cast<MINIDUMP_TYPE>(
                    MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules |
                    MiniDumpWithProcessThreadData);
                const BOOL ok = ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), file, type,
                                                    &exceptionInfo, &streams, nullptr);
                ::CloseHandle(file);
                if (ok) {
                    logger::critical("[GPU-AV] wrote accelerated AV minidump to PrismaUI_F4\\FreezeDumps\\{}",
                                     std::filesystem::path(name).string());
                } else {
                    logger::error("[GPU-AV] MiniDumpWriteDump failed (GLE={})", ::GetLastError());
                    std::error_code rmEc;
                    std::filesystem::remove(dumpPath, rmEc);
                }
            } catch (const std::exception& e) {
                logger::error("[GPU-AV] minidump write threw: {}", e.what());
            } catch (...) {
                logger::error("[GPU-AV] minidump write threw");
            }
        }

        void WatchdogMain() {
            const Thresholds thresholds;
            WatchdogState state;
            bool loadDumpDeferred = false;
            bool loadStallReported = false;
            while (g_watchdogRunning.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(750));
                std::uint32_t ready = 2;
                if (g_pendingAcceleratedAvDump.state.compare_exchange_strong(ready, 3, std::memory_order_acq_rel)) {
                    WriteAcceleratedAvDump();
                    g_pendingAcceleratedAvDump.state.store(4, std::memory_order_release);
                }
                const int64_t now = NowMs();
                const Snapshot snap = Capture(now);
                if (!snap.gameLoadActive) {
                    loadStallReported = false;
                } else if (!loadStallReported && snap.gameLoadSinceMs > 0 &&
                           now - snap.gameLoadSinceMs >= 60000) {
                    loadStallReported = true;
                    logger::warn("[LOAD] save-load state has remained active for {}ms without PostLoadGame/NewGame; "
                                 "ModelPreview engine reads remain suspended", now - snap.gameLoadSinceMs);
                }
                const Verdict verdict = EvaluateFreeze(snap, thresholds, state, now);
                if (verdict.shouldReport) {
                    for (const auto& line : FormatSnapshotLines(snap, verdict, now)) {
                        logger::critical("{}", line);
                    }
                }
                const bool loadDumpAllowed = !snap.gameLoadActive ||
                    (snap.gameLoadSinceMs > 0 && now - snap.gameLoadSinceMs >= thresholds.loadDumpMs);
                const bool releaseDeferredDump = loadDumpDeferred && verdict.frozen && loadDumpAllowed;
                if ((verdict.shouldWriteDump || releaseDeferredDump) && loadDumpAllowed && MinidumpEnabled()) {
                    WriteMiniDump();
                    loadDumpDeferred = false;
                } else if (verdict.shouldWriteDump && snap.gameLoadActive) {
                    loadDumpDeferred = true;
                    logger::warn("[FREEZE] minidump deferred while save loading is active (load age={}ms)",
                                 snap.gameLoadSinceMs > 0 ? now - snap.gameLoadSinceMs : 0);
                }
                if (verdict.episodeEnd) {
                    loadDumpDeferred = false;
                    logger::info("[FREEZE] progress resumed; freeze episode cleared");
                }
            }
        }

    }

    void MarkStage(Lane lane, Stage stage, uint64_t view, uint64_t generation, int64_t detail) noexcept {
        AtomicLane& l = LaneRef(lane);
        l.threadId.store(::GetCurrentThreadId(), std::memory_order_relaxed);
        l.stage.store(static_cast<uint32_t>(stage), std::memory_order_relaxed);
        l.view.store(view, std::memory_order_relaxed);
        l.generation.store(generation, std::memory_order_relaxed);
        l.detail.store(detail, std::memory_order_relaxed);
        l.heartbeatMs.store(NowMs(), std::memory_order_release);
    }

    void MarkIdle(Lane lane) noexcept {
        AtomicLane& l = LaneRef(lane);
        l.stage.store(static_cast<uint32_t>(Stage::Idle), std::memory_order_relaxed);
        l.heartbeatMs.store(NowMs(), std::memory_order_release);
    }

    void MarkPresent(Stage stage, uint64_t generation) noexcept {
        g_everPresented.store(true, std::memory_order_relaxed);
        if (stage == Stage::Present) g_lastPresent.store(NowMs(), std::memory_order_relaxed);
        MarkStage(Lane::Present, stage, 0, generation, 0);
    }

    void SetPresentWindow(void* hwnd) noexcept { g_presentHwnd.store(hwnd, std::memory_order_relaxed); }

    void SetGpuState(uint64_t completed, uint64_t active, bool frameRequested) noexcept {
        g_gpuCompleted.store(completed, std::memory_order_relaxed);
        g_gpuActive.store(active, std::memory_order_relaxed);
        g_frameRequested.store(frameRequested, std::memory_order_relaxed);
    }

    void BeginDispatchSync(Stage op, uint32_t threadId) noexcept {
        g_dsThread.store(threadId, std::memory_order_relaxed);
        g_dsSince.store(NowMs(), std::memory_order_relaxed);
        g_dsOp.store(static_cast<uint32_t>(op), std::memory_order_release);
    }

    void EndDispatchSync() noexcept {
        g_dsOp.store(static_cast<uint32_t>(Stage::Idle), std::memory_order_relaxed);
        g_dsSince.store(0, std::memory_order_relaxed);
        g_dsThread.store(0, std::memory_order_relaxed);
    }

    void BeginDropViewWait(uint64_t view) noexcept {
        g_dropSince.store(NowMs(), std::memory_order_relaxed);
        g_dropView.store(view, std::memory_order_release);
    }

    void EndDropViewWait() noexcept {
        g_dropView.store(0, std::memory_order_relaxed);
        g_dropSince.store(0, std::memory_order_relaxed);
    }

    void BeginShutdownJoin() noexcept { g_shutdownJoinSince.store(NowMs(), std::memory_order_release); }
    void EndShutdownJoin() noexcept { g_shutdownJoinSince.store(0, std::memory_order_relaxed); }

    void SetModelPreviewState(int jobs, int reads, int claimedRead, int workersActive) noexcept {
        g_mpJobs.store(jobs, std::memory_order_relaxed);
        g_mpReads.store(reads, std::memory_order_relaxed);
        g_mpClaimed.store(claimedRead, std::memory_order_relaxed);
        g_mpWorkers.store(workersActive, std::memory_order_relaxed);
    }

    void SetGameLoadActive(bool active) noexcept {
        if (active) {
            g_gameLoadSince.store(NowMs(), std::memory_order_relaxed);
            g_gameLoadActive.store(true, std::memory_order_release);
        } else {
            g_gameLoadActive.store(false, std::memory_order_release);
            g_gameLoadSince.store(0, std::memory_order_relaxed);
        }
    }

    void NoteRenderSuccess() noexcept { g_lastRender.store(NowMs(), std::memory_order_relaxed); }
    void NoteCompositeSuccess() noexcept { g_lastComposite.store(NowMs(), std::memory_order_relaxed); }

    void QueueAcceleratedAvDump(const void* exceptionRecord, const void* context, std::uint32_t threadId,
                                const void* evidence, std::uint32_t evidenceBytes) noexcept {
        std::uint32_t idle = 0;
        if (!g_pendingAcceleratedAvDump.state.compare_exchange_strong(idle, 1, std::memory_order_acq_rel)) return;
        if (!exceptionRecord || !context || !evidence || evidenceBytes > sizeof(g_pendingAcceleratedAvDump.evidence)) {
            g_pendingAcceleratedAvDump.state.store(4, std::memory_order_release);
            return;
        }
        ::CopyMemory(&g_pendingAcceleratedAvDump.exception, exceptionRecord, sizeof(EXCEPTION_RECORD));
        g_pendingAcceleratedAvDump.exception.ExceptionRecord = nullptr;
        ::CopyMemory(&g_pendingAcceleratedAvDump.context, context, sizeof(CONTEXT));
        g_pendingAcceleratedAvDump.threadId = threadId;
        g_pendingAcceleratedAvDump.evidenceBytes = evidenceBytes;
        ::CopyMemory(g_pendingAcceleratedAvDump.evidence, evidence, evidenceBytes);
        g_pendingAcceleratedAvDump.state.store(2, std::memory_order_release);
    }

    ScopedStage::ScopedStage(Lane lane, Stage stage, uint64_t view, uint64_t generation) noexcept : lane_(lane) {
        previous_ = static_cast<Stage>(LaneRef(lane).stage.load(std::memory_order_relaxed));
        MarkStage(lane, stage, view, generation, 0);
    }

    ScopedStage::~ScopedStage() { MarkStage(lane_, previous_); }

    void StartWatchdog() noexcept {
        if (g_watchdogStarted.exchange(true, std::memory_order_acq_rel)) return;
        g_watchdogRunning.store(true, std::memory_order_release);
        try {
            std::thread(WatchdogMain).detach();
            logger::info("[FREEZE] freeze-diagnostics watchdog started (process-lifetime, diagnostic-only)");
        } catch (...) {
            g_watchdogRunning.store(false, std::memory_order_release);
        }
    }

    void StopWatchdog() noexcept { g_watchdogRunning.store(false, std::memory_order_release); }

}
