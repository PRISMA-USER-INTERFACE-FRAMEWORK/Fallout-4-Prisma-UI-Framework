#pragma once

#include "FreezeDiagnostics.h"

#include <cstdint>

#include <d3d11.h>

namespace PrismaUI::PresentProfiler
{

    void Configure() noexcept;
    bool Enabled() noexcept;

    enum class Stage : int { UpdateOffscreen = 0, TickCore = 1, Draw = 2 };
    inline constexpr int kStageCount = 3;

    class ScopedPresent final
    {
    public:
        ScopedPresent() noexcept;
        ~ScopedPresent() noexcept;
        ScopedPresent(const ScopedPresent&) = delete;
        ScopedPresent& operator=(const ScopedPresent&) = delete;

        void Finish() noexcept;

    private:
        std::int64_t start_ = 0;
        bool active_ = false;
        PrismaUI::FreezeDiagnostics::ScopedStage freezePresent_{
            PrismaUI::FreezeDiagnostics::Lane::Present,
            PrismaUI::FreezeDiagnostics::Stage::Present};
    };

    void BeginStage(Stage s, ID3D11Device* dev, ID3D11DeviceContext* ctx) noexcept;
    void EndStage(Stage s, ID3D11DeviceContext* ctx) noexcept;

    void RecordLockAcquire(double micros, bool dropped) noexcept;

    void EndPresent(ID3D11DeviceContext* ctx,
                    std::uint64_t paintCount,
                    std::uint64_t dropTotal) noexcept;

    enum class LockBucket : int {
        Under10us = 0, Us10to100 = 1, Us100to500 = 2, Ms05to1 = 3, Ms1to2 = 4, Dropped = 5
    };
    inline constexpr int kLockBucketCount = 6;

    struct Snapshot
    {
        bool          enabled = false;
        bool          gpuAvailable = false;
        double        windowSeconds = 0.0;
        std::uint64_t windowPresents = 0;
        double        presentsPerSec = 0.0;
        double        webPaintsPerSec = 0.0;
        std::uint64_t droppedCtx = 0;

        double cpuAvgMs = 0.0, cpuP50Ms = 0.0, cpuP95Ms = 0.0, cpuP99Ms = 0.0, cpuMaxMs = 0.0;

        double gpuStageAvgMs[kStageCount] = {};
        double gpuTotalAvgMs = 0.0;

        std::uint64_t lockBuckets[kLockBucketCount] = {};
    };

    void GetSnapshot(Snapshot& out) noexcept;
}
