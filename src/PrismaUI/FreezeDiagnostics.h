#pragma once

#include "PrismaUI/FreezeDiagnosticsCore.h"

#include <cstdint>

namespace PrismaUI::FreezeDiagnostics {

    void MarkStage(Lane lane, Stage stage, uint64_t view = 0, uint64_t generation = 0, int64_t detail = 0) noexcept;
    void MarkIdle(Lane lane) noexcept;

    void MarkPresent(Stage stage, uint64_t generation = 0) noexcept;
    void SetPresentWindow(void* hwnd) noexcept;

    void SetGpuState(uint64_t completed, uint64_t active, bool frameRequested) noexcept;

    void BeginDispatchSync(Stage op, uint32_t threadId) noexcept;
    void EndDispatchSync() noexcept;

    void BeginDropViewWait(uint64_t view) noexcept;
    void EndDropViewWait() noexcept;

    void BeginShutdownJoin() noexcept;
    void EndShutdownJoin() noexcept;

    void SetModelPreviewState(int jobs, int reads, int claimedRead, int workersActive) noexcept;
    void SetGameLoadActive(bool active) noexcept;

    void NoteRenderSuccess() noexcept;
    void NoteCompositeSuccess() noexcept;
    void QueueAcceleratedAvDump(const void* exceptionRecord, const void* context, std::uint32_t threadId,
                                const void* evidence, std::uint32_t evidenceBytes) noexcept;

    class ScopedStage {
    public:
        ScopedStage(Lane lane, Stage stage, uint64_t view = 0, uint64_t generation = 0) noexcept;
        ~ScopedStage();
        ScopedStage(const ScopedStage&) = delete;
        ScopedStage& operator=(const ScopedStage&) = delete;

    private:
        Lane   lane_;
        Stage  previous_;
    };

    void StartWatchdog() noexcept;
    void StopWatchdog() noexcept;

}
