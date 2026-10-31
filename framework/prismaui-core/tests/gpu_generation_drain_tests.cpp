#include <cassert>
#include <cstdint>
#include <vector>

#include "GPU/GenerationDrainState.h"
#include "GPU/AcceleratedDeviceLifecycle.h" // invariant coverage
#include "GPU/GPUDriverBase.h"

namespace ultralight {

GPUDriver::~GPUDriver() = default;

}

namespace {

class RecordingGPUDriver final : public PrismaUI::GPU::GPUDriverBase {
public:
    using PrismaUI::GPU::GPUDriverBase::ExecuteGeneration;
    using PrismaUI::GPU::GPUDriverBase::StageResourceOperation;

    std::vector<std::uint32_t> drawnGeometryIds;
    std::vector<std::uint32_t> clearedRenderBufferIds;
    std::vector<std::uint32_t> operationEvents;
    std::vector<PrismaUI::GPU::AcceleratedGenerationOperation> resourceOperations;
    bool rejectResources = false;
    bool rejectDraws = false;
    bool rejectClears = false;

    void CreateTexture(std::uint32_t, ultralight::RefPtr<ultralight::Bitmap>) override {}
    void UpdateTexture(std::uint32_t, ultralight::RefPtr<ultralight::Bitmap>) override {}
    void DestroyTexture(std::uint32_t) override {}
    void CreateRenderBuffer(std::uint32_t, const ultralight::RenderBuffer&) override {}
    void DestroyRenderBuffer(std::uint32_t) override {}
    void CreateGeometry(std::uint32_t, const ultralight::VertexBuffer&,
                        const ultralight::IndexBuffer&) override {}
    void UpdateGeometry(std::uint32_t, const ultralight::VertexBuffer&,
                        const ultralight::IndexBuffer&) override {}
    void DestroyGeometry(std::uint32_t) override {}

protected:
    bool ExecuteResourceOperation(const PrismaUI::GPU::AcceleratedGenerationOperation& operation) override
    {
        operationEvents.push_back(1000u + static_cast<std::uint32_t>(operation.type));
        resourceOperations.push_back(operation);
        return !rejectResources;
    }

    bool ClearRenderBufferCommand(std::uint32_t renderBufferId) override
    {
        clearedRenderBufferIds.push_back(renderBufferId);
        operationEvents.push_back(2000u + renderBufferId);
        return !rejectClears;
    }

    bool DrawGeometryCommand(std::uint32_t geometryId, std::uint32_t, std::uint32_t,
                             const ultralight::GPUState&) override
    {
        drawnGeometryIds.push_back(geometryId);
        operationEvents.push_back(3000u + geometryId);
        return !rejectDraws;
    }
};

struct FakeCommand {
    int value;
};

struct FakeGeneration {
    bool textureUpdate = false;
    bool geometryUpdate = false;
    bool drawCommands = false;
    bool executionSucceeds = true;
    unsigned drains = 0;

    bool Drain()
    {
        assert(textureUpdate || geometryUpdate || drawCommands);
        ++drains;
        return executionSucceeds;
    }
};

void ActivateAndCheck(FakeGeneration& generation)
{
    uint64_t completed = 17;
    uint64_t active = 0;
    const auto result = PrismaUI::GPU::GenerationDrain::ActivatePublished(
        completed, active, true, true, [&generation] { return generation.Drain(); });
    assert(result == PrismaUI::GPU::GenerationDrain::Activation::Activated);
    assert(active == completed);
    assert(generation.drains == 1);

    const auto second = PrismaUI::GPU::GenerationDrain::ActivatePublished(
        completed, active, true, true, [&generation] { return generation.Drain(); });
    assert(second == PrismaUI::GPU::GenerationDrain::Activation::Unavailable);
    assert(generation.drains == 1);
}

void VerifyNoOpAcceleratedContinuity()
{
    uint64_t visibleGeneration = 41;
    const uint64_t noOpGeneration = 0;
    if (PrismaUI::GPU::GenerationDrain::HasPublishableGeneration(noOpGeneration))
        visibleGeneration = noOpGeneration;
    assert(visibleGeneration == 41);
    assert(!PrismaUI::GPU::GenerationDrain::OwnerMayWaitForRetirement(0, 0, false));

    const uint64_t laterGeneration = 42;
    if (PrismaUI::GPU::GenerationDrain::HasPublishableGeneration(laterGeneration))
        visibleGeneration = laterGeneration;
    assert(visibleGeneration == laterGeneration);
}

}  // namespace

int main()
{
    PrismaUI::GPU::AcceleratedDeviceLifecycle lifecycle;
    const PrismaUI::GPU::AcceleratedLifecycleInputs firstInputs{1, 2, 3};
    const PrismaUI::GPU::AcceleratedLifecycleInputs secondInputs{4, 2, 3};
    assert(lifecycle.Observe(firstInputs) ==
           PrismaUI::GPU::AcceleratedDeviceLifecycle::Transition::Stable);
    assert(lifecycle.Epoch() == 1 && lifecycle.Accepts(1, firstInputs));
    assert(lifecycle.Observe(secondInputs) ==
           PrismaUI::GPU::AcceleratedDeviceLifecycle::Transition::Changed);
    assert(lifecycle.Epoch() == 2 && !lifecycle.Accepts(1, firstInputs) && lifecycle.Accepts(2, secondInputs));
    lifecycle.BeginResize();
    assert(!lifecycle.Accepts(2, secondInputs));
    assert(lifecycle.CompleteResize(false, secondInputs) ==
           PrismaUI::GPU::AcceleratedDeviceLifecycle::Transition::Stable);
    assert(lifecycle.Epoch() == 2 && lifecycle.Accepts(2, secondInputs));
    lifecycle.BeginResize();
    assert(lifecycle.CompleteResize(true, firstInputs) ==
           PrismaUI::GPU::AcceleratedDeviceLifecycle::Transition::Changed);
    assert(lifecycle.Epoch() == 3 && lifecycle.Accepts(3, firstInputs));
    lifecycle.MarkDeviceLost();
    assert(lifecycle.Dead() && !lifecycle.Accepts(3, secondInputs));
    assert(lifecycle.Observe(firstInputs) ==
           PrismaUI::GPU::AcceleratedDeviceLifecycle::Transition::Changed);
    assert(lifecycle.Epoch() == 4 && lifecycle.Accepts(4, firstInputs));

    VerifyNoOpAcceleratedContinuity();

    FakeGeneration textureOnly{.textureUpdate = true};
    ActivateAndCheck(textureOnly);

    FakeGeneration geometryOnly{.geometryUpdate = true};
    ActivateAndCheck(geometryOnly);

    FakeGeneration normal{.textureUpdate = true, .geometryUpdate = true, .drawCommands = true};
    ActivateAndCheck(normal);

    FakeGeneration failed{.textureUpdate = true, .executionSucceeds = false};
    uint64_t completed = 23;
    uint64_t active = 0;
    bool acceptingFrames = true;
    bool ready = true;
    bool terminalGpuFailure = false;
    const auto failedResult = PrismaUI::GPU::GenerationDrain::ActivatePublished(
        completed, active, true, true, [&failed] { return failed.Drain(); });
    assert(failedResult == PrismaUI::GPU::GenerationDrain::Activation::DrainFailed);
    assert(active == 0);
    assert(failed.drains == 1);
    PrismaUI::GPU::GenerationDrain::EnterTerminalFailure(completed, active, acceptingFrames, ready,
                                                          terminalGpuFailure);
    assert(completed == 0 && active == 0);
    assert(!acceptingFrames && !ready && terminalGpuFailure);
    assert(!PrismaUI::GPU::GenerationDrain::CanBeginPresent(acceptingFrames, ready, terminalGpuFailure));
    assert(!PrismaUI::GPU::GenerationDrain::OwnerMayWaitForRetirement(completed, active, terminalGpuFailure));

    FakeGeneration laterGeneration{.drawCommands = true};
    completed = 24;
    if (PrismaUI::GPU::GenerationDrain::CanBeginPresent(acceptingFrames, ready, terminalGpuFailure)) {
        assert(PrismaUI::GPU::GenerationDrain::ActivatePublished(
                   completed, active, true, true, [&laterGeneration] { return laterGeneration.Drain(); }) ==
               PrismaUI::GPU::GenerationDrain::Activation::Activated);
    }
    assert(active == 0 && laterGeneration.drains == 0);

    FakeGeneration wrongContext{.drawCommands = true};
    completed = 31;
    active = 0;
    assert(PrismaUI::GPU::GenerationDrain::ActivatePublished(
               completed, active, true, false, [&wrongContext] { return wrongContext.Drain(); }) ==
           PrismaUI::GPU::GenerationDrain::Activation::ContextMismatch);
    assert(active == 0 && completed == 31 && wrongContext.drains == 0);

    PrismaUI::GPU::AcceleratedGenerationMailbox mailbox;
    PrismaUI::GPU::AcceleratedCommandGeneration first;
    first.id = 1;
    PrismaUI::GPU::AcceleratedGenerationOperation texture;
    texture.type = PrismaUI::GPU::AcceleratedGenerationOperationType::UpdateTexture;
    texture.id = 7;
    texture.bytes = {1, 2, 3, 4};
    first.operations.push_back(texture);
    PrismaUI::GPU::AcceleratedGenerationOperation draw;
    draw.type = PrismaUI::GPU::AcceleratedGenerationOperationType::Draw;
    draw.command.command_type = ultralight::CommandType::DrawGeometry;
    draw.command.geometry_id = 11;
    first.operations.push_back(draw);
    assert(mailbox.Publish(first) ==
           PrismaUI::GPU::AcceleratedGenerationMailbox::PublishResult::Published);
    assert(mailbox.HasPending() && mailbox.PendingOperationCount() == 2);

    PrismaUI::GPU::AcceleratedCommandGeneration consumed;
    assert(mailbox.Consume(consumed) && consumed.id == 1 && consumed.operations.size() == 2);
    assert(consumed.operations[0].bytes.size() == 4);
    assert(!mailbox.HasPending() && mailbox.PendingOperationCount() == 0);

    PrismaUI::GPU::AcceleratedCommandGeneration second;
    second.id = 2;
    second.operations.push_back(draw);
    assert(mailbox.Publish(first) ==
           PrismaUI::GPU::AcceleratedGenerationMailbox::PublishResult::Published);
    assert(mailbox.Publish(second) ==
           PrismaUI::GPU::AcceleratedGenerationMailbox::PublishResult::Published);
    assert(mailbox.Consume(consumed) && consumed.id == 2 && consumed.operations.size() == 3);
    assert(consumed.operations[0].type == PrismaUI::GPU::AcceleratedGenerationOperationType::UpdateTexture);
    assert(consumed.operations[2].command.geometry_id == 11);

    PrismaUI::GPU::AcceleratedGenerationMailbox operationBound(1, 4);
    PrismaUI::GPU::AcceleratedCommandGeneration operationFirst;
    operationFirst.id = 3;
    operationFirst.operations.push_back(draw);
    assert(operationBound.Publish(operationFirst) ==
           PrismaUI::GPU::AcceleratedGenerationMailbox::PublishResult::Published);
    PrismaUI::GPU::AcceleratedCommandGeneration operationSecond;
    operationSecond.id = 4;
    operationSecond.operations.push_back(draw);
    assert(operationBound.Publish(operationSecond) ==
           PrismaUI::GPU::AcceleratedGenerationMailbox::PublishResult::RejectedForCapacity);
    assert(operationBound.HasCapacityViolation());
    assert(operationBound.PendingOperationCount() == 1 && operationBound.PendingPayloadBytes() == 0);
    assert(operationBound.Consume(consumed) && consumed.id == 3);

    PrismaUI::GPU::AcceleratedGenerationMailbox payloadBound(4, 4);
    PrismaUI::GPU::AcceleratedCommandGeneration payloadFirst;
    payloadFirst.id = 5;
    payloadFirst.operations.push_back(texture);
    assert(payloadBound.Publish(payloadFirst) ==
           PrismaUI::GPU::AcceleratedGenerationMailbox::PublishResult::Published);
    PrismaUI::GPU::AcceleratedCommandGeneration payloadSecond;
    payloadSecond.id = 6;
    payloadSecond.operations.push_back(texture);
    payloadSecond.operations[0].bytes = {1};
    assert(payloadBound.Publish(payloadSecond) ==
           PrismaUI::GPU::AcceleratedGenerationMailbox::PublishResult::RejectedForCapacity);
    assert(payloadBound.HasCapacityViolation());
    assert(payloadBound.PendingOperationCount() == 1 && payloadBound.PendingPayloadBytes() == 4);
    assert(payloadBound.Consume(consumed) && consumed.id == 5);

    RecordingGPUDriver driver;
    ultralight::Command firstCommand{};
    firstCommand.command_type = ultralight::CommandType::DrawGeometry;
    firstCommand.geometry_id = 101;
    ultralight::Command secondCommand{};
    secondCommand.command_type = ultralight::CommandType::DrawGeometry;
    secondCommand.geometry_id = 202;
    const ultralight::CommandList firstList{1, &firstCommand};
    const ultralight::CommandList secondList{1, &secondCommand};

    driver.BeginSynchronize();
    driver.UpdateCommandList(firstList);
    driver.UpdateCommandList(secondList);
    driver.EndSynchronize();
    assert(driver.PendingCommandCount() == 1);
    driver.ExecuteGeneration();
    assert(driver.drawnGeometryIds.size() == 1 && driver.drawnGeometryIds[0] == 202);

    driver.UpdateCommandList(firstList);
    driver.UpdateCommandList(secondList);
    assert(driver.PendingCommandCount() == 2);
    driver.ExecuteGeneration();
    assert(driver.drawnGeometryIds.size() == 3);
    assert(driver.drawnGeometryIds[1] == 101 && driver.drawnGeometryIds[2] == 202);

    RecordingGPUDriver drawFailureDriver;
    drawFailureDriver.rejectDraws = true;
    drawFailureDriver.UpdateCommandList(firstList);
    drawFailureDriver.ExecuteGeneration();
    assert(drawFailureDriver.HasCommandTransportFailed());

    ultralight::Command clearCommand{};
    clearCommand.command_type = ultralight::CommandType::ClearRenderBuffer;
    const ultralight::CommandList clearList{1, &clearCommand};
    RecordingGPUDriver clearFailureDriver;
    clearFailureDriver.rejectClears = true;
    clearFailureDriver.UpdateCommandList(clearList);
    clearFailureDriver.ExecuteGeneration();
    assert(clearFailureDriver.HasCommandTransportFailed());

    RecordingGPUDriver stagedCountDriver;
    stagedCountDriver.BeginSynchronize();
    PrismaUI::GPU::AcceleratedGenerationOperation countOperation;
    countOperation.type = PrismaUI::GPU::AcceleratedGenerationOperationType::DestroyTexture;
    countOperation.id = 1;
    for (std::size_t i = 0; i < PrismaUI::GPU::AcceleratedGenerationMailbox::kMaxOperations; ++i)
        assert(stagedCountDriver.StageResourceOperation(countOperation));
    assert(!stagedCountDriver.StageResourceOperation(countOperation));
    assert(stagedCountDriver.HasCommandTransportOverflowed());
    assert(stagedCountDriver.PendingCommandCount() == 0);
    stagedCountDriver.EndSynchronize();

    RecordingGPUDriver combinedCountDriver;
    combinedCountDriver.BeginSynchronize();
    for (std::size_t i = 0; i + 1 < PrismaUI::GPU::AcceleratedGenerationMailbox::kMaxOperations; ++i)
        assert(combinedCountDriver.StageResourceOperation(countOperation));
    std::vector<ultralight::Command> twoCommands(2);
    twoCommands[0].command_type = ultralight::CommandType::DrawGeometry;
    twoCommands[1].command_type = ultralight::CommandType::DrawGeometry;
    combinedCountDriver.UpdateCommandList({2, twoCommands.data()});
    assert(combinedCountDriver.HasCommandTransportOverflowed());
    assert(combinedCountDriver.PendingCommandCount() == 0);
    combinedCountDriver.EndSynchronize();

    // Exact 64/256/320 MiB payload boundaries are exercised by large_texture_transport_policy_test
    // through the pure aggregate policy. Keep this integration test allocation-small so CI does not
    // materialize a 256-320 MiB vector merely to test arithmetic.

    RecordingGPUDriver resourceDriver;
    PrismaUI::GPU::AcceleratedGenerationOperation createTexture;
    createTexture.type = PrismaUI::GPU::AcceleratedGenerationOperationType::CreateTexture;
    createTexture.id = 41;
    createTexture.width = 2;
    createTexture.height = 1;
    createTexture.format = 87;
    createTexture.rowBytes = 8;
    createTexture.bytes = {1, 2, 3, 4, 5, 6, 7, 8};
    assert(resourceDriver.StageResourceOperation(createTexture));
    createTexture.bytes[0] = 99;

    PrismaUI::GPU::AcceleratedGenerationOperation createGeometry;
    createGeometry.type = PrismaUI::GPU::AcceleratedGenerationOperationType::CreateGeometry;
    createGeometry.id = 42;
    createGeometry.vertexFormat = static_cast<std::uint32_t>(ultralight::VertexBufferFormat::_2f_4ub_2f);
    createGeometry.bytes = {10, 11, 12, 13};
    createGeometry.secondaryBytes = {14, 15, 16, 17};
    createGeometry.secondarySize = 4;
    assert(resourceDriver.StageResourceOperation(createGeometry));
    createGeometry.bytes[0] = 98;
    createGeometry.secondaryBytes[0] = 97;

    PrismaUI::GPU::AcceleratedGenerationOperation destroyTexture;
    destroyTexture.type = PrismaUI::GPU::AcceleratedGenerationOperationType::DestroyTexture;
    destroyTexture.id = 41;
    assert(resourceDriver.StageResourceOperation(destroyTexture));
    assert(resourceDriver.PendingGenerationId() != 0);
    resourceDriver.ExecuteGeneration();
    assert(resourceDriver.resourceOperations.size() == 3);
    assert(resourceDriver.resourceOperations[0].bytes[0] == 1);
    assert(resourceDriver.resourceOperations[1].bytes[0] == 10);
    assert(resourceDriver.resourceOperations[1].secondaryBytes[0] == 14);
    assert(resourceDriver.operationEvents[0] ==
           1000u + static_cast<std::uint32_t>(PrismaUI::GPU::AcceleratedGenerationOperationType::CreateTexture));
    assert(resourceDriver.operationEvents[2] ==
           1000u + static_cast<std::uint32_t>(PrismaUI::GPU::AcceleratedGenerationOperationType::DestroyTexture));
    assert(resourceDriver.resourceOperations[0].epoch == resourceDriver.LifecycleEpoch());
    assert(!resourceDriver.HasCommandsPending());
    assert(!resourceDriver.HasCommandTransportFailed());

    RecordingGPUDriver epochTransport;
    assert(epochTransport.StageResourceOperation(createTexture));
    assert(epochTransport.HasCommandsPending());
    epochTransport.ResetTransportForEpoch(9);
    assert(!epochTransport.HasCommandsPending());
    assert(epochTransport.LifecycleEpoch() == 9);
    assert(epochTransport.StageResourceOperation(createTexture));
    epochTransport.ExecuteGeneration();
    assert(!epochTransport.resourceOperations.empty() && epochTransport.resourceOperations.back().epoch == 9);

    RecordingGPUDriver orderedDriver;
    orderedDriver.BeginSynchronize();
    assert(orderedDriver.StageResourceOperation(createTexture));
    orderedDriver.UpdateCommandList(firstList);
    assert(orderedDriver.StageResourceOperation(destroyTexture));
    orderedDriver.UpdateCommandList(secondList);
    orderedDriver.EndSynchronize();
    orderedDriver.ExecuteGeneration();
    assert(orderedDriver.operationEvents.size() == 3);
    assert(orderedDriver.operationEvents[0] ==
           1000u + static_cast<std::uint32_t>(PrismaUI::GPU::AcceleratedGenerationOperationType::CreateTexture));
    assert(orderedDriver.operationEvents[1] == 1000u +
                                                   static_cast<std::uint32_t>(
                                                       PrismaUI::GPU::AcceleratedGenerationOperationType::DestroyTexture));
    assert(orderedDriver.drawnGeometryIds.size() == 1 && orderedDriver.drawnGeometryIds[0] == 202);

    RecordingGPUDriver reuseDriver;
    reuseDriver.BeginSynchronize();
    assert(reuseDriver.StageResourceOperation(destroyTexture));
    assert(reuseDriver.StageResourceOperation(createTexture));
    reuseDriver.EndSynchronize();
    reuseDriver.ExecuteGeneration();
    assert(reuseDriver.resourceOperations.size() == 2);
    assert(reuseDriver.resourceOperations[0].type ==
           PrismaUI::GPU::AcceleratedGenerationOperationType::DestroyTexture);
    assert(reuseDriver.resourceOperations[1].type ==
           PrismaUI::GPU::AcceleratedGenerationOperationType::CreateTexture);

    RecordingGPUDriver failingResourceDriver;
    failingResourceDriver.rejectResources = true;
    assert(failingResourceDriver.StageResourceOperation(createTexture));
    failingResourceDriver.UpdateCommandList(firstList);
    failingResourceDriver.ExecuteGeneration();
    assert(failingResourceDriver.HasCommandTransportFailed());
    assert(failingResourceDriver.drawnGeometryIds.empty());

    RecordingGPUDriver invalidDriver;
    PrismaUI::GPU::AcceleratedGenerationOperation invalid;
    invalid.type = PrismaUI::GPU::AcceleratedGenerationOperationType::CreateTexture;
    assert(!invalidDriver.StageResourceOperation(invalid));
    assert(invalidDriver.HasCommandTransportFailed());
    assert(invalidDriver.PendingCommandCount() == 0);

    std::vector<ultralight::Command> full(PrismaUI::GPU::AcceleratedGenerationMailbox::kMaxOperations);
    full[0].command_type = ultralight::CommandType::DrawGeometry;
    full[0].geometry_id = 303;
    driver.UpdateCommandList({static_cast<std::uint32_t>(full.size()), full.data()});
    assert(driver.PendingCommandCount() == PrismaUI::GPU::AcceleratedGenerationMailbox::kMaxOperations);
    driver.UpdateCommandList(secondList);
    assert(driver.HasCommandTransportOverflowed());
    assert(driver.PendingCommandCount() == PrismaUI::GPU::AcceleratedGenerationMailbox::kMaxOperations);
    driver.UpdateCommandList(firstList);
    driver.ExecuteGeneration();
    assert(driver.drawnGeometryIds.size() == 3);
    assert(driver.PendingCommandCount() == PrismaUI::GPU::AcceleratedGenerationMailbox::kMaxOperations);
}
