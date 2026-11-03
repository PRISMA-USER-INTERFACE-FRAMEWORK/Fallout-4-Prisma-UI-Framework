#pragma once

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/platform/GPUDriver.h>
#pragma warning(pop)

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace PrismaUI::GPU {

enum class AcceleratedGenerationOperationType : std::uint8_t {
    CreateTexture,
    UpdateTexture,
    CreateGeometry,
    UpdateGeometry,
    CreateRenderBuffer,
    DestroyTexture,
    DestroyGeometry,
    DestroyRenderBuffer,
    Draw,
    ClearRenderBuffer,
};

struct AcceleratedGenerationOperation {
    std::uint64_t epoch = 1;
    AcceleratedGenerationOperationType type = AcceleratedGenerationOperationType::Draw;
    std::uint32_t id = 0;
    std::uint32_t auxiliaryId = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t format = 0;
    std::uint32_t vertexFormat = 0;
    std::uint32_t rowBytes = 0;
    std::uint32_t secondarySize = 0;
    bool renderTarget = false;
    bool hasStencilBuffer = false;
    bool hasDepthBuffer = false;
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> secondaryBytes;
    ultralight::Command command{};
};

struct AcceleratedCommandGeneration {
    std::uint64_t id = 0;
    std::uint64_t epoch = 1;
    std::vector<AcceleratedGenerationOperation> operations;

    [[nodiscard]] bool empty() const noexcept { return operations.empty(); }
};

class AcceleratedGenerationMailbox {
public:
    static constexpr std::size_t kMaxOperations = 4096;

    static constexpr std::size_t kMaxNonTexturePayloadBytes = 64u * 1024u * 1024u;
    static constexpr std::size_t kMaxTexturePayloadBytes = 256u * 1024u * 1024u;
    static constexpr std::size_t kMaxPayloadBytes =
        kMaxTexturePayloadBytes + kMaxNonTexturePayloadBytes;

    struct PayloadBudgetSummary {
        std::size_t textureBytes = 0;
        std::size_t nonTextureBytes = 0;
        std::size_t bytes = 0;
        bool withinLimit = true;
    };

    [[nodiscard]] static constexpr bool IsTexturePayloadOperation(
        AcceleratedGenerationOperationType type) noexcept
    {
        return type == AcceleratedGenerationOperationType::CreateTexture ||
               type == AcceleratedGenerationOperationType::UpdateTexture;
    }

    [[nodiscard]] static constexpr bool PayloadShapeWithinBudget(
        AcceleratedGenerationOperationType type,
        std::uint32_t,
        std::uint32_t,
        std::size_t payloadBytes) noexcept
    {
        if (payloadBytes > kMaxPayloadBytes) return false;
        return IsTexturePayloadOperation(type) ? payloadBytes <= kMaxTexturePayloadBytes
                                               : payloadBytes <= kMaxNonTexturePayloadBytes;
    }

    [[nodiscard]] static constexpr bool AccumulatePayload(
        PayloadBudgetSummary& summary,
        AcceleratedGenerationOperationType type,
        std::uint32_t width,
        std::uint32_t height,
        std::size_t payloadBytes,
        std::size_t maxPayloadBytes = kMaxPayloadBytes) noexcept
    {
        if (!summary.withinLimit || !PayloadShapeWithinBudget(type, width, height, payloadBytes)) {
            summary.withinLimit = false;
            return false;
        }
        if (summary.bytes > maxPayloadBytes || payloadBytes > maxPayloadBytes - summary.bytes) {
            summary.withinLimit = false;
            return false;
        }

        if (IsTexturePayloadOperation(type)) {
            if (summary.textureBytes > kMaxTexturePayloadBytes ||
                payloadBytes > kMaxTexturePayloadBytes - summary.textureBytes) {
                summary.withinLimit = false;
                return false;
            }
            summary.textureBytes += payloadBytes;
        } else {
            if (summary.nonTextureBytes > kMaxNonTexturePayloadBytes ||
                payloadBytes > kMaxNonTexturePayloadBytes - summary.nonTextureBytes) {
                summary.withinLimit = false;
                return false;
            }
            summary.nonTextureBytes += payloadBytes;
        }
        summary.bytes += payloadBytes;
        return true;
    }

    enum class PublishResult : std::uint8_t {
        Empty,
        Published,
        RejectedForCapacity,
    };

    explicit AcceleratedGenerationMailbox(
        std::size_t maxOperations = kMaxOperations,
        std::size_t maxPayloadBytes = kMaxPayloadBytes) noexcept
        : maxOperations_(maxOperations), maxPayloadBytes_(maxPayloadBytes)
    {}

    [[nodiscard]] PublishResult Publish(AcceleratedCommandGeneration generation)
    {
        if (generation.empty()) return PublishResult::Empty;
        const auto generationPayload = PayloadBytes(generation);
        std::lock_guard lock{mutex_};
        if (!generationPayload.withinLimit || !Fits(generation.operations.size(), generationPayload.bytes)) {
            capacityViolation_ = true;
            return PublishResult::RejectedForCapacity;
        }
        if (pendingReady_) {
            const auto operationCount = pending_.operations.size() + generation.operations.size();
            if (generationPayload.textureBytes > kMaxTexturePayloadBytes - pendingTexturePayloadBytes_ ||
                generationPayload.nonTextureBytes > kMaxNonTexturePayloadBytes - pendingNonTexturePayloadBytes_ ||
                generationPayload.bytes > maxPayloadBytes_ - pendingPayloadBytes_) {
                capacityViolation_ = true;
                return PublishResult::RejectedForCapacity;
            }
            const auto payloadBytes = pendingPayloadBytes_ + generationPayload.bytes;
            if (!Fits(operationCount, payloadBytes)) {
                capacityViolation_ = true;
                return PublishResult::RejectedForCapacity;
            }
            AcceleratedCommandGeneration merged;
            merged.id = generation.id;
            merged.epoch = generation.epoch;
            merged.operations.reserve(operationCount);
            for (auto& operation : pending_.operations) {
                merged.operations.push_back(std::move(operation));
            }
            for (auto& operation : generation.operations) {
                merged.operations.push_back(std::move(operation));
            }
            pending_ = std::move(merged);
            pendingPayloadBytes_ = payloadBytes;
            pendingTexturePayloadBytes_ += generationPayload.textureBytes;
            pendingNonTexturePayloadBytes_ += generationPayload.nonTextureBytes;
        } else {
            pending_ = std::move(generation);
            pendingPayloadBytes_ = generationPayload.bytes;
            pendingTexturePayloadBytes_ = generationPayload.textureBytes;
            pendingNonTexturePayloadBytes_ = generationPayload.nonTextureBytes;
        }
        pendingReady_ = true;
        return PublishResult::Published;
    }

    [[nodiscard]] bool Consume(AcceleratedCommandGeneration& generation)
    {
        std::lock_guard lock{mutex_};
        if (!pendingReady_) return false;
        generation = std::move(pending_);
        pending_ = {};
        pendingPayloadBytes_ = 0;
        pendingTexturePayloadBytes_ = 0;
        pendingNonTexturePayloadBytes_ = 0;
        pendingReady_ = false;
        return true;
    }

    [[nodiscard]] bool HasPending() const noexcept
    {
        std::lock_guard lock{mutex_};
        return pendingReady_;
    }

    [[nodiscard]] std::size_t PendingOperationCount() const noexcept
    {
        std::lock_guard lock{mutex_};
        return pendingReady_ ? pending_.operations.size() : 0;
    }

    [[nodiscard]] std::size_t PendingPayloadBytes() const noexcept
    {
        std::lock_guard lock{mutex_};
        return pendingReady_ ? pendingPayloadBytes_ : 0;
    }

    [[nodiscard]] std::size_t PendingTexturePayloadBytes() const noexcept
    {
        std::lock_guard lock{mutex_};
        return pendingReady_ ? pendingTexturePayloadBytes_ : 0;
    }

    [[nodiscard]] std::size_t PendingNonTexturePayloadBytes() const noexcept
    {
        std::lock_guard lock{mutex_};
        return pendingReady_ ? pendingNonTexturePayloadBytes_ : 0;
    }

    [[nodiscard]] bool HasCapacityViolation() const noexcept
    {
        std::lock_guard lock{mutex_};
        return capacityViolation_;
    }

    [[nodiscard]] std::uint64_t PendingId() const noexcept
    {
        std::lock_guard lock{mutex_};
        return pendingReady_ ? pending_.id : 0;
    }

    void Reset() noexcept
    {
        std::lock_guard lock{mutex_};
        pending_ = {};
        pendingPayloadBytes_ = 0;
        pendingTexturePayloadBytes_ = 0;
        pendingNonTexturePayloadBytes_ = 0;
        pendingReady_ = false;
        capacityViolation_ = false;
    }

private:
    [[nodiscard]] PayloadBudgetSummary PayloadBytes(const AcceleratedCommandGeneration& generation) const noexcept
    {
        PayloadBudgetSummary summary;
        for (const auto& operation : generation.operations) {
            if (operation.bytes.size() > maxPayloadBytes_ ||
                operation.secondaryBytes.size() > maxPayloadBytes_ - operation.bytes.size()) {
                summary.withinLimit = false;
                return summary;
            }
            const auto operationPayloadBytes = operation.bytes.size() + operation.secondaryBytes.size();
            if (!AccumulatePayload(summary, operation.type, operation.width, operation.height,
                                   operationPayloadBytes, maxPayloadBytes_)) {
                return summary;
            }
        }
        return summary;
    }

    [[nodiscard]] bool Fits(std::size_t operationCount, std::size_t payloadBytes) const noexcept
    {
        return operationCount <= maxOperations_ && payloadBytes <= maxPayloadBytes_;
    }

    mutable std::mutex mutex_;
    bool pendingReady_ = false;
    bool capacityViolation_ = false;
    std::size_t pendingPayloadBytes_ = 0;
    std::size_t pendingTexturePayloadBytes_ = 0;
    std::size_t pendingNonTexturePayloadBytes_ = 0;
    std::size_t maxOperations_;
    std::size_t maxPayloadBytes_;
    AcceleratedCommandGeneration pending_;
};

class GPUDriverBase : public ultralight::GPUDriver {
public:
    GPUDriverBase() = default;
    ~GPUDriverBase() override = default;

    void BeginSynchronize() override;
    void EndSynchronize() override;

    std::uint32_t NextTextureId() override { return nextTextureId_.fetch_add(1, std::memory_order_relaxed); }
    std::uint32_t NextRenderBufferId() override {
        return nextRenderBufferId_.fetch_add(1, std::memory_order_relaxed);
    }
    std::uint32_t NextGeometryId() override { return nextGeometryId_.fetch_add(1, std::memory_order_relaxed); }

    void UpdateCommandList(const ultralight::CommandList& list) override;

    [[nodiscard]] bool HasCommandsPending() const noexcept;
    [[nodiscard]] std::size_t PendingCommandCount() const noexcept;
    [[nodiscard]] bool HasCommandTransportOverflowed() const noexcept;
    [[nodiscard]] bool HasCommandTransportFailed() const noexcept;
    [[nodiscard]] std::uint64_t PendingGenerationId() const noexcept;

    void ResetTransportForEpoch(std::uint64_t epoch) noexcept;
    [[nodiscard]] std::uint64_t LifecycleEpoch() const noexcept;

protected:
    [[nodiscard]] bool StageResourceOperation(AcceleratedGenerationOperation operation);
    void FailCommandTransport() noexcept;
    void ExecuteGeneration();

    virtual void OnGenerationOperationBegin(std::size_t, const AcceleratedGenerationOperation&) {}
    virtual void OnGenerationOperationFailure(const AcceleratedCommandGeneration&, std::size_t, std::size_t,
                                              const AcceleratedGenerationOperation*, const char*) {}

    [[nodiscard]] virtual bool ExecuteResourceOperation(const AcceleratedGenerationOperation&) { return false; }

    [[nodiscard]] virtual bool ClearRenderBufferCommand(std::uint32_t renderBufferId) = 0;
    [[nodiscard]] virtual bool DrawGeometryCommand(std::uint32_t geometryId, std::uint32_t indicesCount,
                                                   std::uint32_t indicesOffset,
                                                   const ultralight::GPUState& state) = 0;

private:
    void ClearStagingLocked() noexcept;
    void PublishStagingLocked();

    std::atomic<std::uint32_t> nextTextureId_{1};
    std::atomic<std::uint32_t> nextRenderBufferId_{1};
    std::atomic<std::uint32_t> nextGeometryId_{1};
    mutable std::mutex commandMutex_;
    std::vector<AcceleratedGenerationOperation> stagingOperations_;
    std::size_t stagingPayloadBytes_ = 0;
    std::size_t stagingTexturePayloadBytes_ = 0;
    std::size_t stagingNonTexturePayloadBytes_ = 0;
    std::uint32_t synchronizeDepth_ = 0;
    std::uint64_t nextGeneration_ = 1;
    bool commandTransportOverflowed_ = false;
    bool commandTransportFailed_ = false;
    std::uint64_t lifecycleEpoch_ = 1;
    AcceleratedGenerationMailbox mailbox_;
};

}
