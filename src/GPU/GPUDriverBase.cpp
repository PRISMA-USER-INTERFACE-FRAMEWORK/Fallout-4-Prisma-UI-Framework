#include "GPUDriverBase.h"

namespace PrismaUI::GPU {

namespace {

static_assert(AcceleratedGenerationMailbox::kMaxPayloadBytes ==
              AcceleratedGenerationMailbox::kMaxTexturePayloadBytes +
                  AcceleratedGenerationMailbox::kMaxNonTexturePayloadBytes);

[[nodiscard]] std::size_t OperationPayloadBytes(const AcceleratedGenerationOperation& operation) noexcept
{
    if (operation.bytes.size() > static_cast<std::size_t>(-1) - operation.secondaryBytes.size()) {
        return static_cast<std::size_t>(-1);
    }
    return operation.bytes.size() + operation.secondaryBytes.size();
}

[[nodiscard]] bool AccumulateOperationPayload(
    AcceleratedGenerationMailbox::PayloadBudgetSummary& summary,
    const AcceleratedGenerationOperation& operation) noexcept
{
    const auto payloadBytes = OperationPayloadBytes(operation);
    if (payloadBytes == static_cast<std::size_t>(-1)) {
        summary.withinLimit = false;
        return false;
    }
    return AcceleratedGenerationMailbox::AccumulatePayload(
        summary, operation.type, operation.width, operation.height, payloadBytes);
}

}

void GPUDriverBase::ClearStagingLocked() noexcept
{
    stagingOperations_.clear();
    stagingPayloadBytes_ = 0;
    stagingTexturePayloadBytes_ = 0;
    stagingNonTexturePayloadBytes_ = 0;
}

void GPUDriverBase::BeginSynchronize()
{
    std::lock_guard lock{commandMutex_};
    if (synchronizeDepth_++ == 0) ClearStagingLocked();
}

void GPUDriverBase::EndSynchronize()
{
    std::lock_guard lock{commandMutex_};
    if (synchronizeDepth_ == 0 || --synchronizeDepth_ != 0) return;
    if (commandTransportFailed_) {
        ClearStagingLocked();
        return;
    }
    PublishStagingLocked();
}

void GPUDriverBase::UpdateCommandList(const ultralight::CommandList& list)
{
    std::lock_guard lock{commandMutex_};
    if (commandTransportFailed_) return;
    if (list.size == 0) return;
    if (!list.commands) {
        ClearStagingLocked();
        commandTransportFailed_ = true;
        return;
    }
    if (list.size > AcceleratedGenerationMailbox::kMaxOperations) {
        ClearStagingLocked();
        commandTransportOverflowed_ = true;
        commandTransportFailed_ = true;
        return;
    }
    for (auto it = stagingOperations_.begin(); it != stagingOperations_.end();) {
        if (it->type == AcceleratedGenerationOperationType::Draw ||
            it->type == AcceleratedGenerationOperationType::ClearRenderBuffer)
            it = stagingOperations_.erase(it);
        else
            ++it;
    }

    AcceleratedGenerationMailbox::PayloadBudgetSummary summary;
    for (const auto& operation : stagingOperations_) {
        if (!AccumulateOperationPayload(summary, operation)) {
            ClearStagingLocked();
            commandTransportOverflowed_ = true;
            commandTransportFailed_ = true;
            return;
        }
    }
    stagingPayloadBytes_ = summary.bytes;
    stagingTexturePayloadBytes_ = summary.textureBytes;
    stagingNonTexturePayloadBytes_ = summary.nonTextureBytes;

    if (stagingOperations_.size() > AcceleratedGenerationMailbox::kMaxOperations - list.size) {
        ClearStagingLocked();
        commandTransportOverflowed_ = true;
        commandTransportFailed_ = true;
        return;
    }
    stagingOperations_.reserve(stagingOperations_.size() + list.size);
    for (std::uint32_t i = 0; i < list.size; ++i) {
        AcceleratedGenerationOperation operation;
        operation.epoch = lifecycleEpoch_;
        operation.type = list.commands[i].command_type == ultralight::CommandType::ClearRenderBuffer
                             ? AcceleratedGenerationOperationType::ClearRenderBuffer
                             : AcceleratedGenerationOperationType::Draw;
        operation.id = list.commands[i].geometry_id;
        operation.auxiliaryId = list.commands[i].gpu_state.render_buffer_id;
        operation.command = list.commands[i];
        stagingOperations_.push_back(std::move(operation));
    }
    if (synchronizeDepth_ == 0) PublishStagingLocked();
}

void GPUDriverBase::PublishStagingLocked()
{
    if (commandTransportFailed_ || stagingOperations_.empty()) return;
    AcceleratedCommandGeneration generation;
    generation.id = nextGeneration_++;
    generation.epoch = lifecycleEpoch_;
    if (!generation.id) {
        commandTransportFailed_ = true;
        ClearStagingLocked();
        return;
    }
    generation.operations = std::move(stagingOperations_);
    stagingPayloadBytes_ = 0;
    stagingTexturePayloadBytes_ = 0;
    stagingNonTexturePayloadBytes_ = 0;
    if (mailbox_.Publish(std::move(generation)) ==
        AcceleratedGenerationMailbox::PublishResult::RejectedForCapacity) {
        commandTransportOverflowed_ = true;
        commandTransportFailed_ = true;
    }
}

bool GPUDriverBase::StageResourceOperation(AcceleratedGenerationOperation operation)
{
    std::lock_guard lock{commandMutex_};
    if (commandTransportFailed_) return false;
    if (operation.id == 0 || operation.type == AcceleratedGenerationOperationType::Draw ||
        operation.type == AcceleratedGenerationOperationType::ClearRenderBuffer) {
        ClearStagingLocked();
        commandTransportFailed_ = true;
        return false;
    }

    if (stagingOperations_.size() >= AcceleratedGenerationMailbox::kMaxOperations) {
        ClearStagingLocked();
        commandTransportOverflowed_ = true;
        commandTransportFailed_ = true;
        return false;
    }

    AcceleratedGenerationMailbox::PayloadBudgetSummary summary;
    summary.textureBytes = stagingTexturePayloadBytes_;
    summary.nonTextureBytes = stagingNonTexturePayloadBytes_;
    summary.bytes = stagingPayloadBytes_;
    if (!AccumulateOperationPayload(summary, operation)) {
        ClearStagingLocked();
        commandTransportOverflowed_ = true;
        commandTransportFailed_ = true;
        return false;
    }

    operation.epoch = lifecycleEpoch_;
    stagingOperations_.push_back(std::move(operation));
    stagingPayloadBytes_ = summary.bytes;
    stagingTexturePayloadBytes_ = summary.textureBytes;
    stagingNonTexturePayloadBytes_ = summary.nonTextureBytes;
    if (synchronizeDepth_ == 0) PublishStagingLocked();
    return !commandTransportFailed_;
}

void GPUDriverBase::FailCommandTransport() noexcept
{
    std::lock_guard lock{commandMutex_};
    commandTransportFailed_ = true;
    ClearStagingLocked();
}

bool GPUDriverBase::HasCommandsPending() const noexcept
{
    {
        std::lock_guard lock{commandMutex_};
        if (!stagingOperations_.empty()) return true;
    }
    return mailbox_.HasPending();
}

std::size_t GPUDriverBase::PendingCommandCount() const noexcept
{
    std::lock_guard lock{commandMutex_};
    return mailbox_.PendingOperationCount() + stagingOperations_.size();
}

bool GPUDriverBase::HasCommandTransportOverflowed() const noexcept
{
    std::lock_guard lock{commandMutex_};
    return commandTransportOverflowed_;
}

bool GPUDriverBase::HasCommandTransportFailed() const noexcept
{
    std::lock_guard lock{commandMutex_};
    return commandTransportFailed_;
}

std::uint64_t GPUDriverBase::PendingGenerationId() const noexcept
{
    std::lock_guard lock{commandMutex_};
    if (commandTransportFailed_) return 0;
    return mailbox_.PendingId();
}

void GPUDriverBase::ResetTransportForEpoch(std::uint64_t epoch) noexcept
{
    std::lock_guard lock{commandMutex_};
    ClearStagingLocked();
    synchronizeDepth_ = 0;
    mailbox_.Reset();
    commandTransportOverflowed_ = false;
    commandTransportFailed_ = false;
    lifecycleEpoch_ = epoch ? epoch : 1;
}

std::uint64_t GPUDriverBase::LifecycleEpoch() const noexcept
{
    std::lock_guard lock{commandMutex_};
    return lifecycleEpoch_;
}

void GPUDriverBase::ExecuteGeneration()
{
    AcceleratedCommandGeneration generation;
    {
        std::lock_guard lock{commandMutex_};
        if (commandTransportFailed_ || !mailbox_.Consume(generation)) return;
    }
    if (generation.epoch != LifecycleEpoch()) {
        OnGenerationOperationFailure(generation, std::size_t(-1), std::size_t(-1), nullptr,
                                     "generation-epoch-mismatch");
        FailCommandTransport();
        return;
    }
    std::size_t lastSuccessfulOperation = std::size_t(-1);
    for (std::size_t operationIndex = 0; operationIndex < generation.operations.size(); ++operationIndex) {
        const auto& operation = generation.operations[operationIndex];
        OnGenerationOperationBegin(operationIndex, operation);
        if (operation.epoch != generation.epoch) {
            OnGenerationOperationFailure(generation, operationIndex, lastSuccessfulOperation, &operation,
                                         "operation-epoch-mismatch");
            FailCommandTransport();
            return;
        }
        if (operation.type == AcceleratedGenerationOperationType::ClearRenderBuffer) {
            if (!ClearRenderBufferCommand(operation.command.gpu_state.render_buffer_id)) {
                OnGenerationOperationFailure(generation, operationIndex, lastSuccessfulOperation, &operation,
                                             "operation-rejected");
                FailCommandTransport();
                return;
            }
        } else if (operation.type == AcceleratedGenerationOperationType::Draw) {
            if (!DrawGeometryCommand(operation.command.geometry_id, operation.command.indices_count,
                                     operation.command.indices_offset, operation.command.gpu_state)) {
                OnGenerationOperationFailure(generation, operationIndex, lastSuccessfulOperation, &operation,
                                             "operation-rejected");
                FailCommandTransport();
                return;
            }
        } else if (!ExecuteResourceOperation(operation)) {
            OnGenerationOperationFailure(generation, operationIndex, lastSuccessfulOperation, &operation,
                                         "operation-rejected");
            FailCommandTransport();
            return;
        }
        lastSuccessfulOperation = operationIndex;
    }
}

}
