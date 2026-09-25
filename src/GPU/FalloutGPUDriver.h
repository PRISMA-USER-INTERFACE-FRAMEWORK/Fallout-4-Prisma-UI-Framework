#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

#include "FalloutD3DContext.h"
#include "AcceleratedDeviceLifecycle.h" 
#include "GPUDriverBase.h"
#include "Utils/D3D11StateGuard.h"

namespace PrismaUI::GPU {

    enum class AcceleratedBreadcrumbOperation : std::uint32_t {
        None,
        CaptureEnvironment,
        DeviceFeatureLevel,
        DeviceCreationFlags,
        ImmediateContextType,
        DeviceDispatchModule,
        ImmediateContextDispatchModule,
        ProvenanceRenderTarget,
        ProvenanceVertexBuffer,
        ProvenanceIndexBuffer,
        ProvenanceSrv0,
        ProvenanceSrv1,
        ProvenanceConstantBuffer,
        ProvenanceVertexShader,
        ProvenancePixelShader,
        ProvenanceInputLayout,
        ProvenanceSampler,
        ProvenanceBlendState,
        ProvenanceDepthStencilState,
        ProvenanceRasterizerState,
        ProvenanceImmediateContext,
        LogicalRenderTargetSelfSample,
        ResolveRenderBuffer,
        MapConstantBuffer,
        UnmapConstantBuffer,
        ClearPixelShaderResources,
        SetRenderTargets,
        SetViewports,
        SetPixelShaderResources,
        SetVertexConstantBuffers,
        SetPixelConstantBuffers,
        SetVertexBuffers,
        SetIndexBuffer,
        SetPrimitiveTopology,
        SetInputLayout,
        SetSampler,
        SetVertexShader,
        SetPixelShader,
        SetBlendState,
        SetRasterizerState,
        SetScissorRects,
        DrawIndexed,
        CaptureImmediateState,
        ExecuteGeneration,
    };

    struct AcceleratedBreadcrumb {
        std::uint64_t sequence = 0;
        std::uint64_t generation = 0;
        std::uint32_t operation = 0;
        std::uint32_t provenance = 0;
        std::uint32_t drawOrdinal = 0;
        std::uint32_t geometryId = 0;
        std::uint32_t renderBufferId = 0;
        std::uint32_t texture0Id = 0;
        std::uint32_t texture1Id = 0;
        std::uint32_t indicesCount = 0;
        std::uint32_t indicesOffset = 0;
        std::uint32_t indexSize = 0;
        std::uint32_t vertexFormat = 0;
        std::uint32_t shaderType = 0;
        float viewportWidth = 0.0F;
        float viewportHeight = 0.0F;
        std::uintptr_t renderTarget = 0;
        std::uintptr_t vertexBuffer = 0;
        std::uintptr_t indexBuffer = 0;
        std::uintptr_t srv0 = 0;
        std::uintptr_t srv1 = 0;
        std::uintptr_t constantBuffer = 0;
        std::uintptr_t inputLayout = 0;
        std::uintptr_t vertexShader = 0;
        std::uintptr_t pixelShader = 0;
        std::uintptr_t sampler = 0;
        std::uintptr_t blendState = 0;
        std::uintptr_t depthStencilState = 0;
        std::uintptr_t rasterizerState = 0;
        std::uintptr_t borrowedDevice = 0;
        std::uintptr_t suppliedDevice = 0;
        std::uintptr_t immediateContext = 0;
    };

    struct AcceleratedAvEvidence {
        static constexpr std::uint32_t kBreadcrumbCapacity = 32;

        std::uint64_t lastSequence = 0;
        std::uint64_t generation = 0;
        std::uint32_t faultThreadId = 0;
        std::uint32_t breadcrumbCount = 0;
        std::uint32_t deviceFeatureLevel = 0;
        std::uint32_t deviceCreationFlags = 0;
        std::uint32_t immediateContextType = 0;
        std::uintptr_t borrowedDevice = 0;
        std::uintptr_t suppliedDevice = 0;
        std::uintptr_t immediateContext = 0;
        wchar_t proxyD3d11Module[260]{};
        wchar_t systemD3d11Module[260]{};
        wchar_t deviceDispatchModule[260]{};
        wchar_t immediateContextDispatchModule[260]{};
        AcceleratedBreadcrumb breadcrumbs[kBreadcrumbCapacity]{};
    };

    struct DrainResult {
        HRESULT result = E_FAIL;

        [[nodiscard]] explicit operator bool() const noexcept { return SUCCEEDED(result); }
    };

    class FalloutGPUDriver final : public GPUDriverBase {
    public:
        explicit FalloutGPUDriver(FalloutD3DContext* context) noexcept : context_(context) {}
        ~FalloutGPUDriver() override = default;

        bool InitializePipeline();

        void CreateTexture(std::uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) override;
        void UpdateTexture(std::uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) override;
        void DestroyTexture(std::uint32_t textureId) override;

        void CreateRenderBuffer(std::uint32_t renderBufferId, const ultralight::RenderBuffer& buffer) override;
        void DestroyRenderBuffer(std::uint32_t renderBufferId) override;

        void CreateGeometry(std::uint32_t geometryId, const ultralight::VertexBuffer& vertices,
                            const ultralight::IndexBuffer& indices) override;
        void UpdateGeometry(std::uint32_t geometryId, const ultralight::VertexBuffer& vertices,
                            const ultralight::IndexBuffer& indices) override;
        void DestroyGeometry(std::uint32_t geometryId) override;

        [[nodiscard]] DrainResult DrainPendingGeneration();
        void SetDiagnosticGeneration(std::uint64_t generation) noexcept;
        [[nodiscard]] bool RebuildForDeviceEpoch(std::uint64_t epoch);
        [[nodiscard]] std::uint64_t ResourceEpoch() const noexcept { return resourceEpoch_; }
        [[nodiscard]] HRESULT LastFailureResult() const noexcept { return lastFailureResult_; }

        struct TextureSnapshot {
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            std::uint64_t epoch = 0;
        };

        [[nodiscard]] Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> AcquireTextureSRV(std::uint32_t textureId) const;
        [[nodiscard]] TextureSnapshot AcquireTextureSnapshot(std::uint32_t textureId) const;
        [[nodiscard]] TextureSnapshot AcquirePresentationTextureSnapshot(std::uint32_t textureId) const;
        [[nodiscard]] Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> AcquireRenderBufferSRV(
            std::uint32_t renderBufferId) const;

    protected:
        [[nodiscard]] bool ClearRenderBufferCommand(std::uint32_t renderBufferId) override;
        [[nodiscard]] bool DrawGeometryCommand(std::uint32_t geometryId, std::uint32_t indicesCount,
                                               std::uint32_t indicesOffset,
                                               const ultralight::GPUState& state) override;
        [[nodiscard]] bool ExecuteResourceOperation(const AcceleratedGenerationOperation& operation) override;

    private:
        void OnGenerationOperationBegin(std::size_t operationIndex,
                                        const AcceleratedGenerationOperation& operation) override;
        void OnGenerationOperationFailure(const AcceleratedCommandGeneration& generation,
                                          std::size_t operationIndex, std::size_t lastSuccessfulOperation,
                                          const AcceleratedGenerationOperation* operation,
                                          const char* fallbackReason) override;
        void SetOperationFailure(const char* reason, HRESULT result = S_OK) noexcept;
        void LogDrainStageFailure(const char* stage, const char* reason, HRESULT result) const;

        struct TextureEntry {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            bool dynamic = false;
            bool destroyPending = false;
            std::uint64_t epoch = 1;
            std::uint32_t rowBytes = 0;
            std::vector<std::uint8_t> pixels;
        };

        struct RenderTargetEntry {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> multisampleTexture;
            Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
            std::uint32_t textureId = 0;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            bool dirty = false;
            std::uint64_t epoch = 1;
        };

        struct GeometryEntry {
            ultralight::VertexBufferFormat format{};
            Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
            Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
            std::uint32_t vertexCapacity = 0;
            std::uint32_t indexCapacity = 0;
            std::uint32_t vertexSize = 0;
            std::uint32_t indexSize = 0;
            std::uint64_t epoch = 1;
            std::vector<std::uint8_t> vertexBytes;
            std::vector<std::uint8_t> indexBytes;
        };

        [[nodiscard]] static DXGI_FORMAT TextureFormat(ultralight::BitmapFormat format) noexcept;

        bool EnsurePipeline();

        void MarkBreadcrumb(AcceleratedBreadcrumbOperation operation) noexcept;
        void CaptureEnvironment() noexcept;
        bool ValidateDrawProvenance() noexcept;
        bool ValidateDeviceChild(ID3D11DeviceChild* child, AcceleratedBreadcrumbOperation operation) noexcept;
        int CaptureAccessViolation(EXCEPTION_POINTERS* pointers) noexcept;
        bool ResolveRenderBufferForTexture(std::uint32_t textureId);
        bool ResolveDirtyRenderBuffers();
        bool ExecuteCreateTexture(const AcceleratedGenerationOperation& operation);
        bool ExecuteUpdateTexture(const AcceleratedGenerationOperation& operation);
        bool ExecuteDestroyTexture(const AcceleratedGenerationOperation& operation);
        bool ExecuteCreateRenderBuffer(const AcceleratedGenerationOperation& operation);
        bool ExecuteDestroyRenderBuffer(const AcceleratedGenerationOperation& operation);
        bool ExecuteCreateGeometry(const AcceleratedGenerationOperation& operation);
        bool ExecuteUpdateGeometry(const AcceleratedGenerationOperation& operation);
        bool ExecuteDestroyGeometry(const AcceleratedGenerationOperation& operation);

        DrainResult DrainInner();
        DrainResult SehGuardedDrain() noexcept;
        [[nodiscard]] HRESULT DeviceFailureResult() const noexcept;
        void ResetPipelineForEpoch() noexcept;

        FalloutD3DContext* context_ = nullptr;
        mutable std::mutex resourcesMutex_;
        std::map<std::uint32_t, TextureEntry> textures_;
        std::map<std::uint32_t, RenderTargetEntry> renderTargets_;
        std::map<std::uint32_t, GeometryEntry> geometry_;

        bool pipelineReady_ = false;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> fillVS_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> fillPS_;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> fillPathVS_;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> fillPathPS_;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> fillLayout_;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> fillPathLayout_;
        Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;
        AcceleratedBreadcrumb activeBreadcrumb_{};
        AcceleratedAvEvidence evidence_{};
        AcceleratedAvEvidence capturedEvidence_{};
        EXCEPTION_RECORD capturedException_{};
        CONTEXT capturedContext_{};
        std::uint64_t breadcrumbSequence_ = 0;
        std::uint32_t breadcrumbWriteIndex_ = 0;
        std::uint32_t drawOrdinal_ = 0;
        bool provenanceRejected_ = false;
        bool logicalSelfSampleLogged_ = false;
        bool avCaptured_ = false;
        std::uint64_t resourceEpoch_ = 1;
        HRESULT lastFailureResult_ = S_OK;
        const char* operationFailureReason_ = nullptr;
        HRESULT operationFailureResult_ = S_OK;
    };

}
