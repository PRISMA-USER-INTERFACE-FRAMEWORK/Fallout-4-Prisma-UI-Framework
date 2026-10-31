#pragma once

#include "PrismaUI_F4VR_API.h"
#include "VR/SpatialPointerProtocol.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace PrismaUI::VR
{
    using PrismaViewId = std::uint64_t;

    inline constexpr std::size_t kMaximumFrameworkViews = 64;

    struct SpatialRuntimeState
    {
        PRISMA_UI_VR_API::SpatialUpdateV1 pending{};
        PRISMA_UI_VR_API::SpatialUpdateV1 active{};
        PRISMA_UI_VR_API::SpatialUpdateV1 applied{};
        std::uint64_t acceptedSequence = 0;
        std::uint64_t appliedSequence = 0;
        std::uint32_t replacedPendingUpdateCount = 0;
        PRISMA_UI_VR_API::SpatialResult lastApplyResult =
            PRISMA_UI_VR_API::SpatialResult::NotReady;
        bool hasPending = false;
        bool hasActive = false;
        bool hasApplied = false;
        bool backendReady = false;
    };

    struct SpatialPointerRuntimeState
    {
        static constexpr std::size_t kSampleCapacity = 16;

        std::array<
            PRISMA_UI_VR_API::SpatialPointerUpdateV1,
            kSampleCapacity>
            samples{};
        std::size_t sampleHead = 0;
        std::size_t sampleCount = 0;
        PRISMA_UI_VR_API::SpatialPointerUpdateV1 active{};
        std::uint64_t acceptedSequence = 0;
        std::uint64_t appliedSequence = 0;
        std::uint32_t replacedPendingUpdateCount = 0;
        PRISMA_UI_VR_API::SpatialResult lastApplyResult =
            PRISMA_UI_VR_API::SpatialResult::NotReady;
        std::chrono::steady_clock::time_point lastSubmissionTime{};
        bool hasActive = false;
        bool backendReady = false;
        bool routed = false;
        bool hasHit = false;
        SpatialPointerProtocol::State interaction{};
        float hitDistance = 0.0f;
        float hitUv[2]{};
    };

    struct VRView
    {
        explicit VRView(PrismaViewId viewId) noexcept :
            id(viewId)
        {}

        PrismaViewId id = 0;
        std::atomic<bool> destroying = false;
        std::atomic<bool> hidden = false;
        std::atomic<bool> focused = false;
        std::atomic<bool> focusRequestPending = false;

        std::mutex spatialMutex;
        SpatialRuntimeState spatial;

        std::mutex spatialPointerMutex;
        SpatialPointerRuntimeState spatialPointer;
    };

    struct Registry
    {
        std::map<PrismaViewId, std::shared_ptr<VRView>> views;
        mutable std::shared_mutex viewsMutex;
    };

    [[nodiscard]] Registry& GetRuntime() noexcept;

    [[nodiscard]] std::shared_ptr<VRView> FindView(
        PrismaViewId viewId) noexcept;

    [[nodiscard]] std::shared_ptr<VRView> AcquireView(
        PrismaViewId viewId) noexcept;

    void ReleaseView(PrismaViewId viewId) noexcept;

    void PruneDeadViews() noexcept;

    void SyncFlags(const std::shared_ptr<VRView>& view) noexcept;

    [[nodiscard]] bool IsShuttingDown() noexcept;
    void SetShuttingDown(bool shuttingDown) noexcept;

    [[nodiscard]] bool IsRenderBackendOperational() noexcept;
    void SetRenderBackendOperational(bool operational) noexcept;
}
