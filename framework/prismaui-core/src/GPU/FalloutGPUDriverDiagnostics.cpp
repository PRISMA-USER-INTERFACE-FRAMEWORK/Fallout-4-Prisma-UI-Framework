#include "FalloutGPUDriver.h"

#include <algorithm>

namespace PrismaUI::FreezeDiagnostics {
    void QueueAcceleratedAvDump(const void* exceptionRecord, const void* context, std::uint32_t threadId,
                                const void* evidence, std::uint32_t evidenceBytes) noexcept;
}

namespace PrismaUI::GPU {

    namespace {

        void CaptureModulePath(void* address, wchar_t (&destination)[260]) noexcept {
            if (!address) return;
            MEMORY_BASIC_INFORMATION memory{};
            if (!::VirtualQuery(address, &memory, sizeof(memory)) || !memory.AllocationBase) return;
            ::GetModuleFileNameW(static_cast<HMODULE>(memory.AllocationBase), destination, 260);
        }

        void CaptureDispatchModule(void* object, wchar_t (&destination)[260]) noexcept {
            if (!object) return;
            auto* const* dispatch = *reinterpret_cast<void* const* const*>(object);
            if (!dispatch) return;
            CaptureModulePath(dispatch[0], destination);
        }

        const char* ProvenanceOperationName(AcceleratedBreadcrumbOperation operation) noexcept {
            switch (operation) {
                case AcceleratedBreadcrumbOperation::ProvenanceRenderTarget: return "render-target";
                case AcceleratedBreadcrumbOperation::ProvenanceVertexBuffer: return "vertex-buffer";
                case AcceleratedBreadcrumbOperation::ProvenanceIndexBuffer: return "index-buffer";
                case AcceleratedBreadcrumbOperation::ProvenanceSrv0: return "srv0";
                case AcceleratedBreadcrumbOperation::ProvenanceSrv1: return "srv1";
                case AcceleratedBreadcrumbOperation::ProvenanceConstantBuffer: return "constant-buffer";
                case AcceleratedBreadcrumbOperation::ProvenanceVertexShader: return "vertex-shader";
                case AcceleratedBreadcrumbOperation::ProvenancePixelShader: return "pixel-shader";
                case AcceleratedBreadcrumbOperation::ProvenanceInputLayout: return "input-layout";
                case AcceleratedBreadcrumbOperation::ProvenanceSampler: return "sampler";
                case AcceleratedBreadcrumbOperation::ProvenanceBlendState: return "blend-state";
                case AcceleratedBreadcrumbOperation::ProvenanceDepthStencilState: return "depth-stencil-state";
                case AcceleratedBreadcrumbOperation::ProvenanceRasterizerState: return "rasterizer-state";
                case AcceleratedBreadcrumbOperation::ProvenanceImmediateContext: return "immediate-context";
                default: return "unknown";
            }
        }

        const char* GenerationOperationName(AcceleratedGenerationOperationType operation) noexcept {
            switch (operation) {
                case AcceleratedGenerationOperationType::CreateTexture: return "create-texture";
                case AcceleratedGenerationOperationType::UpdateTexture: return "update-texture";
                case AcceleratedGenerationOperationType::CreateGeometry: return "create-geometry";
                case AcceleratedGenerationOperationType::UpdateGeometry: return "update-geometry";
                case AcceleratedGenerationOperationType::CreateRenderBuffer: return "create-render-buffer";
                case AcceleratedGenerationOperationType::DestroyTexture: return "destroy-texture";
                case AcceleratedGenerationOperationType::DestroyGeometry: return "destroy-geometry";
                case AcceleratedGenerationOperationType::DestroyRenderBuffer: return "destroy-render-buffer";
                case AcceleratedGenerationOperationType::Draw: return "draw-geometry";
                case AcceleratedGenerationOperationType::ClearRenderBuffer: return "clear-render-buffer";
                default: return "unknown";
            }
        }

    }

    HRESULT FalloutGPUDriver::DeviceFailureResult() const noexcept {
        if (context_ && context_->Device()) {
            const auto reason = context_->Device()->GetDeviceRemovedReason();
            if (reason == DXGI_ERROR_DEVICE_REMOVED || reason == DXGI_ERROR_DEVICE_RESET) return reason;
        }
        return E_FAIL;
    }

    void FalloutGPUDriver::SetDiagnosticGeneration(std::uint64_t generation) noexcept {
        evidence_ = {};
        evidence_.generation = generation;
        activeBreadcrumb_ = {};
        breadcrumbSequence_ = 0;
        breadcrumbWriteIndex_ = 0;
        drawOrdinal_ = 0;
        provenanceRejected_ = false;
        avCaptured_ = false;
    }

    void FalloutGPUDriver::OnGenerationOperationBegin(std::size_t operationIndex,
                                                      const AcceleratedGenerationOperation& operation) {
        (void)operationIndex;
        (void)operation;
        operationFailureReason_ = nullptr;
        operationFailureResult_ = S_OK;
    }

    void FalloutGPUDriver::SetOperationFailure(const char* reason, HRESULT result) noexcept {
        if (operationFailureReason_) return;
        operationFailureReason_ = reason;
        operationFailureResult_ = result;
    }

    void FalloutGPUDriver::OnGenerationOperationFailure(
        const AcceleratedCommandGeneration& generation, std::size_t operationIndex, std::size_t lastSuccessfulOperation,
        const AcceleratedGenerationOperation* operation, const char* fallbackReason) {
        const auto generationId = generation.id;
        const auto generationEpoch = generation.epoch;
        const auto reason = operationFailureReason_ ? operationFailureReason_ : fallbackReason;
        const auto result = operationFailureResult_ == S_OK ? E_FAIL : operationFailureResult_;
        const auto* const device = context_ ? context_->Device() : nullptr;
        const auto* const immediate = context_ ? context_->Context() : nullptr;
        if (!operation) {
            logger::error(
                "[GPU] generation failure: generation={} opIndex=none lastSuccessful=none opCount=unknown "
                "reason={} hr={:#010x} generationEpoch={} resourceEpoch={} device={:#x} context={:#x}",
                generationId, reason, static_cast<unsigned long>(result), generationEpoch, resourceEpoch_,
                reinterpret_cast<std::uintptr_t>(device), reinterpret_cast<std::uintptr_t>(immediate));
            return;
        }
        logger::error("[GPU] generation sequence: generation={} epoch={} operations={}", generationId,
                      generationEpoch, generation.operations.size());
        for (std::size_t index = 0; index < generation.operations.size(); ++index) {
            const auto& recorded = generation.operations[index];
            const auto& recordedCommand = recorded.command;
            logger::error(
                "[GPU] generation operation: generation={} index={} type={} epoch={} id={} auxiliary={} "
                "geometry={} target={} texture0={} texture1={} indicesCount={} indicesOffset={} vertexFormat={} "
                "shaderType={}",
                generationId, index, GenerationOperationName(recorded.type), recorded.epoch, recorded.id,
                recorded.auxiliaryId, recordedCommand.geometry_id, recordedCommand.gpu_state.render_buffer_id,
                recordedCommand.gpu_state.texture_1_id, recordedCommand.gpu_state.texture_2_id,
                recordedCommand.indices_count, recordedCommand.indices_offset, recorded.vertexFormat,
                static_cast<std::uint32_t>(recordedCommand.gpu_state.shader_type));
        }
        const auto& command = operation->command;
        logger::error(
            "[GPU] generation failure: generation={} opIndex={} lastSuccessful={} op={} opEpoch={} "
            "generationEpoch={} resourceEpoch={} id={} auxiliary={} geometry={} target={} texture0={} texture1={} "
            "indicesCount={} indicesOffset={} indexSize={} vertexFormat={} shaderType={} viewport={}x{} "
            "reason={} hr={:#010x} device={:#x} context={:#x}",
            generationId, operationIndex, lastSuccessfulOperation == std::size_t(-1) ? -1 : lastSuccessfulOperation,
            GenerationOperationName(operation->type), operation->epoch, generationEpoch, resourceEpoch_, operation->id,
            operation->auxiliaryId, command.geometry_id, command.gpu_state.render_buffer_id,
            command.gpu_state.texture_1_id, command.gpu_state.texture_2_id, command.indices_count,
            command.indices_offset, activeBreadcrumb_.indexSize, operation->vertexFormat,
            static_cast<std::uint32_t>(command.gpu_state.shader_type), command.gpu_state.viewport_width,
            command.gpu_state.viewport_height, reason, static_cast<unsigned long>(result),
            reinterpret_cast<std::uintptr_t>(device), reinterpret_cast<std::uintptr_t>(immediate));
    }

    void FalloutGPUDriver::LogDrainStageFailure(const char* stage, const char* reason, HRESULT result) const {
        const auto* const device = context_ ? context_->Device() : nullptr;
        const auto* const immediate = context_ ? context_->Context() : nullptr;
        logger::error(
            "[GPU] generation drain-stage failure: stage={} generation={} reason={} hr={:#010x} "
            "resourceEpoch={} device={:#x} context={:#x}",
            stage, evidence_.generation, reason, static_cast<unsigned long>(result), resourceEpoch_,
            reinterpret_cast<std::uintptr_t>(device), reinterpret_cast<std::uintptr_t>(immediate));
    }

    void FalloutGPUDriver::MarkBreadcrumb(AcceleratedBreadcrumbOperation operation) noexcept {
        activeBreadcrumb_.sequence = ++breadcrumbSequence_;
        activeBreadcrumb_.operation = static_cast<std::uint32_t>(operation);
        activeBreadcrumb_.generation = evidence_.generation;
        evidence_.lastSequence = activeBreadcrumb_.sequence;
        const auto slot = breadcrumbWriteIndex_ % AcceleratedAvEvidence::kBreadcrumbCapacity;
        evidence_.breadcrumbs[slot] = activeBreadcrumb_;
        ++breadcrumbWriteIndex_;
        evidence_.breadcrumbCount = std::min(breadcrumbWriteIndex_, AcceleratedAvEvidence::kBreadcrumbCapacity);
    }

    void FalloutGPUDriver::CaptureEnvironment() noexcept {
        activeBreadcrumb_ = {};
        activeBreadcrumb_.borrowedDevice = reinterpret_cast<std::uintptr_t>(context_ ? context_->Device() : nullptr);
        activeBreadcrumb_.suppliedDevice =
            reinterpret_cast<std::uintptr_t>(context_ ? context_->SuppliedDevice() : nullptr);
        activeBreadcrumb_.immediateContext = reinterpret_cast<std::uintptr_t>(context_ ? context_->Context() : nullptr);
        evidence_.borrowedDevice = activeBreadcrumb_.borrowedDevice;
        evidence_.suppliedDevice = activeBreadcrumb_.suppliedDevice;
        evidence_.immediateContext = activeBreadcrumb_.immediateContext;
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::CaptureEnvironment);

        const auto proxyModule = ::GetModuleHandleW(L"d3d11.dll");
        if (proxyModule) ::GetModuleFileNameW(proxyModule, evidence_.proxyD3d11Module, 260);
        const UINT systemLength = ::GetSystemDirectoryW(evidence_.systemD3d11Module, 260);
        if (systemLength && systemLength + 9 < 260) ::lstrcatW(evidence_.systemD3d11Module, L"\\d3d11.dll");

        auto* const device = context_ ? context_->Device() : nullptr;
        auto* const immediate = context_ ? context_->Context() : nullptr;
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::DeviceDispatchModule);
        CaptureDispatchModule(device, evidence_.deviceDispatchModule);
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::ImmediateContextDispatchModule);
        CaptureDispatchModule(immediate, evidence_.immediateContextDispatchModule);
        if (device) {
            MarkBreadcrumb(AcceleratedBreadcrumbOperation::DeviceFeatureLevel);
            evidence_.deviceFeatureLevel = static_cast<std::uint32_t>(device->GetFeatureLevel());
            MarkBreadcrumb(AcceleratedBreadcrumbOperation::DeviceCreationFlags);
            evidence_.deviceCreationFlags = device->GetCreationFlags();
        }
        if (immediate) {
            MarkBreadcrumb(AcceleratedBreadcrumbOperation::ImmediateContextType);
            evidence_.immediateContextType = static_cast<std::uint32_t>(immediate->GetType());
        }
    }

    bool FalloutGPUDriver::ValidateDeviceChild(ID3D11DeviceChild* child,
                                               AcceleratedBreadcrumbOperation operation) noexcept {
        activeBreadcrumb_.provenance = child ? 3u : 0u;
        MarkBreadcrumb(operation);
        if (!child) return true;
        Microsoft::WRL::ComPtr<ID3D11Device> owner;
        child->GetDevice(owner.GetAddressOf());
        const bool matches = owner && context_ && owner.Get() == context_->Device();
        activeBreadcrumb_.provenance = matches ? 1u : 2u;
        const auto slot = (breadcrumbWriteIndex_ - 1u) % AcceleratedAvEvidence::kBreadcrumbCapacity;
        evidence_.breadcrumbs[slot].provenance = activeBreadcrumb_.provenance;
        if (!matches) {
            logger::error(
                "[GPU-AV] provenance mismatch: operation={} seq={} generation={} draw={} child={:#x} owner={:#x} "
                "borrowedDevice={:#x} suppliedDevice={:#x} geometry={} target={} texture0={} texture1={} indexSize={}",
                ProvenanceOperationName(operation), activeBreadcrumb_.sequence, activeBreadcrumb_.generation,
                activeBreadcrumb_.drawOrdinal, reinterpret_cast<std::uintptr_t>(child),
                reinterpret_cast<std::uintptr_t>(owner.Get()), activeBreadcrumb_.borrowedDevice,
                activeBreadcrumb_.suppliedDevice,
                activeBreadcrumb_.geometryId, activeBreadcrumb_.renderBufferId, activeBreadcrumb_.texture0Id,
                activeBreadcrumb_.texture1Id, activeBreadcrumb_.indexSize);
        }
        return matches;
    }

    bool FalloutGPUDriver::ValidateDrawProvenance() noexcept {
        bool valid = true;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.renderTarget),
                                    AcceleratedBreadcrumbOperation::ProvenanceRenderTarget) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.vertexBuffer),
                                    AcceleratedBreadcrumbOperation::ProvenanceVertexBuffer) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.indexBuffer),
                                    AcceleratedBreadcrumbOperation::ProvenanceIndexBuffer) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.srv0),
                                    AcceleratedBreadcrumbOperation::ProvenanceSrv0) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.srv1),
                                    AcceleratedBreadcrumbOperation::ProvenanceSrv1) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.constantBuffer),
                                    AcceleratedBreadcrumbOperation::ProvenanceConstantBuffer) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.vertexShader),
                                    AcceleratedBreadcrumbOperation::ProvenanceVertexShader) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.pixelShader),
                                    AcceleratedBreadcrumbOperation::ProvenancePixelShader) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.inputLayout),
                                    AcceleratedBreadcrumbOperation::ProvenanceInputLayout) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.sampler),
                                    AcceleratedBreadcrumbOperation::ProvenanceSampler) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.blendState),
                                    AcceleratedBreadcrumbOperation::ProvenanceBlendState) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.depthStencilState),
                                    AcceleratedBreadcrumbOperation::ProvenanceDepthStencilState) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.rasterizerState),
                                    AcceleratedBreadcrumbOperation::ProvenanceRasterizerState) && valid;
        valid = ValidateDeviceChild(reinterpret_cast<ID3D11DeviceChild*>(activeBreadcrumb_.immediateContext),
                                    AcceleratedBreadcrumbOperation::ProvenanceImmediateContext) && valid;
        return valid;
    }

    int FalloutGPUDriver::CaptureAccessViolation(EXCEPTION_POINTERS* pointers) noexcept {
        if (!pointers || !pointers->ExceptionRecord || !pointers->ContextRecord ||
            pointers->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        capturedException_ = *pointers->ExceptionRecord;
        capturedException_.ExceptionRecord = nullptr;
        capturedContext_ = *pointers->ContextRecord;
        capturedEvidence_ = evidence_;
        capturedEvidence_.faultThreadId = ::GetCurrentThreadId();
        capturedEvidence_.lastSequence = breadcrumbSequence_;
        avCaptured_ = true;
        return EXCEPTION_EXECUTE_HANDLER;
    }

    DrainResult FalloutGPUDriver::DrainInner() {
        lastFailureResult_ = S_OK;
        provenanceRejected_ = false;
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::CaptureImmediateState);
        PrismaUI::ScopedD3D11State stateGuard{context_->Context()};
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::ExecuteGeneration);
        ExecuteGeneration();
        if (HasCommandTransportFailed()) {
            lastFailureResult_ = DeviceFailureResult();
            LogDrainStageFailure("execute-generation", "command-transport-failed", lastFailureResult_);
            return {lastFailureResult_};
        }
        if (provenanceRejected_) {
            LogDrainStageFailure("provenance-check", "provenance-rejected", E_ACCESSDENIED);
            return {E_ACCESSDENIED};
        }
        operationFailureReason_ = nullptr;
        operationFailureResult_ = S_OK;
        if (!ResolveDirtyRenderBuffers()) {
            lastFailureResult_ = DeviceFailureResult();
            LogDrainStageFailure("resolve-dirty-render-buffers",
                                 operationFailureReason_ ? operationFailureReason_ : "resolve-rejected",
                                 operationFailureResult_ == S_OK ? lastFailureResult_ : operationFailureResult_);
            return {lastFailureResult_};
        }
        return {S_OK};
    }

    DrainResult FalloutGPUDriver::SehGuardedDrain() noexcept {
        __try {
            CaptureEnvironment();
            return DrainInner();
        } __except (CaptureAccessViolation(GetExceptionInformation())) {
            if (avCaptured_) {
                PrismaUI::FreezeDiagnostics::QueueAcceleratedAvDump(
                    &capturedException_, &capturedContext_, capturedEvidence_.faultThreadId, &capturedEvidence_,
                    static_cast<std::uint32_t>(sizeof(capturedEvidence_)));
            }
            lastFailureResult_ = DeviceFailureResult();
            return DrainResult{lastFailureResult_};
        }
    }

    DrainResult FalloutGPUDriver::DrainPendingGeneration() {
        if (HasCommandTransportFailed()) {
            const auto result = HasCommandTransportOverflowed() ? E_OUTOFMEMORY : E_FAIL;
            LogDrainStageFailure("execute-generation", "command-transport-already-failed", result);
            return DrainResult{result};
        }
        if (!context_ || !context_->Ready()) return {};
        if (!context_->PrepareImmediateDevice()) {
            LogDrainStageFailure("prepare-immediate-device", "prepare-rejected", E_ACCESSDENIED);
            return DrainResult{E_ACCESSDENIED};
        }
        return SehGuardedDrain();
    }

}
