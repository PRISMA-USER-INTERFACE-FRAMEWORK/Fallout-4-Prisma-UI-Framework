#include "FalloutGPUDriver.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "UltralightGPUUniforms.h"
#include "UltralightShaders.h"

namespace PrismaUI::FreezeDiagnostics {
    void QueueAcceleratedAvDump(const void* exceptionRecord, const void* context, std::uint32_t threadId,
                                const void* evidence, std::uint32_t evidenceBytes) noexcept;
}

namespace PrismaUI::GPU {

    namespace {

        constexpr UINT kRenderBufferSampleCount = 8;

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

    DXGI_FORMAT FalloutGPUDriver::TextureFormat(ultralight::BitmapFormat format) noexcept {
        switch (format) {
            case ultralight::BitmapFormat::BGRA8_UNORM_SRGB:
                return DXGI_FORMAT_B8G8R8A8_UNORM;
            case ultralight::BitmapFormat::A8_UNORM:
                return DXGI_FORMAT_A8_UNORM;
            default:
                return DXGI_FORMAT_UNKNOWN;
        }
    }

    void FalloutGPUDriver::CreateTexture(std::uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) {
        if (!bitmap || textureId == 0) {
            FailCommandTransport();
            return;
        }
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::CreateTexture;
        operation.id = textureId;
        operation.width = bitmap->width();
        operation.height = bitmap->height();
        operation.format = static_cast<std::uint32_t>(TextureFormat(bitmap->format()));
        operation.rowBytes = bitmap->row_bytes();
        operation.renderTarget = bitmap->IsEmpty();
        if (operation.width == 0 || operation.height == 0 || operation.format == DXGI_FORMAT_UNKNOWN) {
            FailCommandTransport();
            return;
        }
        if (!operation.renderTarget) {
            const std::size_t bytesPerPixel = operation.format == DXGI_FORMAT_A8_UNORM ? 1u : 4u;
            if (operation.width > (static_cast<std::size_t>(-1) / bytesPerPixel) ||
                operation.rowBytes < operation.width * bytesPerPixel ||
                operation.height > static_cast<std::size_t>(-1) / operation.rowBytes)
                {
                    FailCommandTransport();
                    return;
                }
            const auto size = static_cast<std::size_t>(operation.rowBytes) * operation.height;
            if (size != bitmap->size() || size > AcceleratedGenerationMailbox::kMaxPayloadBytes) {
                FailCommandTransport();
                return;
            }
            try {
                operation.bytes.resize(size);
            } catch (...) {
                FailCommandTransport();
                return;
            }
            const void* pixels = bitmap->LockPixels();
            if (!pixels) {
                FailCommandTransport();
                return;
            }
            std::memcpy(operation.bytes.data(), pixels, size);
            bitmap->UnlockPixels();
        }
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::UpdateTexture(std::uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) {
        if (!bitmap || textureId == 0 || bitmap->IsEmpty()) {
            FailCommandTransport();
            return;
        }
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::UpdateTexture;
        operation.id = textureId;
        operation.width = bitmap->width();
        operation.height = bitmap->height();
        operation.format = static_cast<std::uint32_t>(TextureFormat(bitmap->format()));
        operation.rowBytes = bitmap->row_bytes();
        if (operation.width == 0 || operation.height == 0 || operation.format == DXGI_FORMAT_UNKNOWN) {
            FailCommandTransport();
            return;
        }
        const std::size_t bytesPerPixel = operation.format == DXGI_FORMAT_A8_UNORM ? 1u : 4u;
        if (operation.width > (static_cast<std::size_t>(-1) / bytesPerPixel) ||
            operation.rowBytes < operation.width * bytesPerPixel ||
            operation.height > static_cast<std::size_t>(-1) / operation.rowBytes) {
            FailCommandTransport();
            return;
        }
        const auto size = static_cast<std::size_t>(operation.rowBytes) * operation.height;
        if (size != bitmap->size() || size > AcceleratedGenerationMailbox::kMaxPayloadBytes) {
            FailCommandTransport();
            return;
        }
        try {
            operation.bytes.resize(size);
        } catch (...) {
            FailCommandTransport();
            return;
        }
        const void* pixels = bitmap->LockPixels();
        if (!pixels) {
            FailCommandTransport();
            return;
        }
        std::memcpy(operation.bytes.data(), pixels, size);
        bitmap->UnlockPixels();
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::DestroyTexture(std::uint32_t textureId) {
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::DestroyTexture;
        operation.id = textureId;
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::CreateRenderBuffer(std::uint32_t renderBufferId, const ultralight::RenderBuffer& buffer) {
        if (renderBufferId == 0 || buffer.texture_id == 0 || buffer.width == 0 || buffer.height == 0 ||
            buffer.has_stencil_buffer || buffer.has_depth_buffer) {
            FailCommandTransport();
            return;
        }
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::CreateRenderBuffer;
        operation.id = renderBufferId;
        operation.auxiliaryId = buffer.texture_id;
        operation.width = buffer.width;
        operation.height = buffer.height;
        operation.hasStencilBuffer = buffer.has_stencil_buffer;
        operation.hasDepthBuffer = buffer.has_depth_buffer;
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::DestroyRenderBuffer(std::uint32_t renderBufferId) {
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::DestroyRenderBuffer;
        operation.id = renderBufferId;
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::CreateGeometry(std::uint32_t geometryId, const ultralight::VertexBuffer& vertices,
                                          const ultralight::IndexBuffer& indices) {
        if (geometryId == 0 || !vertices.data || !indices.data || vertices.size == 0 || indices.size == 0 ||
            indices.size % sizeof(ultralight::IndexType) != 0 ||
            vertices.size > AcceleratedGenerationMailbox::kMaxPayloadBytes ||
            indices.size > AcceleratedGenerationMailbox::kMaxPayloadBytes) {
            FailCommandTransport();
            return;
        }
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::CreateGeometry;
        operation.id = geometryId;
        operation.vertexFormat = static_cast<std::uint32_t>(vertices.format);
        operation.secondarySize = indices.size;
        try {
            operation.bytes.assign(vertices.data, vertices.data + vertices.size);
            operation.secondaryBytes.assign(indices.data, indices.data + indices.size);
        } catch (...) {
            FailCommandTransport();
            return;
        }
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::UpdateGeometry(std::uint32_t geometryId, const ultralight::VertexBuffer& vertices,
                                          const ultralight::IndexBuffer& indices) {
        if (geometryId == 0 || !vertices.data || !indices.data || vertices.size == 0 || indices.size == 0 ||
            indices.size % sizeof(ultralight::IndexType) != 0 ||
            vertices.size > AcceleratedGenerationMailbox::kMaxPayloadBytes ||
            indices.size > AcceleratedGenerationMailbox::kMaxPayloadBytes) {
            FailCommandTransport();
            return;
        }
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::UpdateGeometry;
        operation.id = geometryId;
        operation.vertexFormat = static_cast<std::uint32_t>(vertices.format);
        operation.secondarySize = indices.size;
        try {
            operation.bytes.assign(vertices.data, vertices.data + vertices.size);
            operation.secondaryBytes.assign(indices.data, indices.data + indices.size);
        } catch (...) {
            FailCommandTransport();
            return;
        }
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::DestroyGeometry(std::uint32_t geometryId) {
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::DestroyGeometry;
        operation.id = geometryId;
        (void)StageResourceOperation(std::move(operation));
    }

    bool FalloutGPUDriver::ExecuteCreateTexture(const AcceleratedGenerationOperation& operation) {
        if (!context_ || !context_->Ready()) {
            SetOperationFailure("create-texture-context-unavailable");
            return false;
        }
        if (operation.id == 0 || operation.width == 0 || operation.height == 0) {
            SetOperationFailure("create-texture-invalid-metadata");
            return false;
        }
        const auto format = static_cast<DXGI_FORMAT>(operation.format);
        if (format != DXGI_FORMAT_B8G8R8A8_UNORM && format != DXGI_FORMAT_A8_UNORM) {
            SetOperationFailure("create-texture-unsupported-format");
            return false;
        }
        if (operation.renderTarget) {
            if (!operation.bytes.empty()) {
                SetOperationFailure("create-render-target-unexpected-payload");
                return false;
            }
        } else {
            const std::size_t bytesPerPixel = format == DXGI_FORMAT_A8_UNORM ? 1u : 4u;
            if (operation.width > static_cast<std::size_t>(-1) / bytesPerPixel ||
                operation.rowBytes < operation.width * bytesPerPixel ||
                operation.height > static_cast<std::size_t>(-1) / operation.rowBytes) {
                SetOperationFailure("create-texture-invalid-payload-layout");
                return false;
            }
            const auto expectedSize = static_cast<std::size_t>(operation.rowBytes) * operation.height;
            if (operation.bytes.size() != expectedSize || operation.bytes.empty()) {
                SetOperationFailure("create-texture-payload-size-mismatch");
                return false;
            }
        }
        {
            std::lock_guard lock{resourcesMutex_};
            if (textures_.contains(operation.id)) {
                SetOperationFailure("create-texture-duplicate-id");
                return false;
            }
        }

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = operation.width;
        desc.Height = operation.height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = operation.renderTarget ? D3D11_USAGE_DEFAULT : D3D11_USAGE_DYNAMIC;
        desc.BindFlags = operation.renderTarget ? D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE
                                                : D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = operation.renderTarget ? 0 : D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA initial{};
        initial.pSysMem = operation.bytes.data();
        initial.SysMemPitch = operation.rowBytes;
        initial.SysMemSlicePitch = static_cast<UINT>(operation.bytes.size());
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        const auto hr = context_->Device()->CreateTexture2D(
            &desc, operation.renderTarget ? nullptr : &initial, texture.GetAddressOf());
        if (FAILED(hr)) {
            SetOperationFailure("create-texture2d", hr);
            return false;
        }
        if (!texture) {
            SetOperationFailure("create-texture2d-null");
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        const auto srvResult = context_->Device()->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf());
        if (FAILED(srvResult)) {
            SetOperationFailure("create-shader-resource-view", srvResult);
            return false;
        }
        if (!srv) {
            SetOperationFailure("create-shader-resource-view-null");
            return false;
        }

        std::lock_guard lock{resourcesMutex_};
        if (textures_.contains(operation.id)) {
            SetOperationFailure("create-texture-duplicate-at-commit");
            return false;
        }
        TextureEntry entry;
        entry.texture = std::move(texture);
        entry.srv = std::move(srv);
        entry.width = desc.Width;
        entry.height = desc.Height;
        entry.format = desc.Format;
        entry.dynamic = !operation.renderTarget;
        entry.epoch = resourceEpoch_;
        entry.rowBytes = operation.rowBytes;
        if (entry.dynamic) entry.pixels = operation.bytes;
        textures_.emplace(operation.id, std::move(entry));
        return true;
    }

    bool FalloutGPUDriver::ExecuteUpdateTexture(const AcceleratedGenerationOperation& operation) {
        if (!context_ || !context_->Ready() || operation.id == 0 || operation.width == 0 || operation.height == 0) {
            SetOperationFailure("update-texture-invalid-metadata");
            return false;
        }
        const auto format = static_cast<DXGI_FORMAT>(operation.format);
        const std::size_t bytesPerPixel = format == DXGI_FORMAT_A8_UNORM ? 1u : 4u;
        if ((format != DXGI_FORMAT_B8G8R8A8_UNORM && format != DXGI_FORMAT_A8_UNORM) ||
            operation.width > static_cast<std::size_t>(-1) / bytesPerPixel ||
            operation.rowBytes < operation.width * bytesPerPixel ||
            operation.height > static_cast<std::size_t>(-1) / operation.rowBytes ||
            operation.bytes.size() != static_cast<std::size_t>(operation.rowBytes) * operation.height) {
            SetOperationFailure("update-texture-invalid-payload");
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        {
            std::lock_guard lock{resourcesMutex_};
            const auto it = textures_.find(operation.id);
            if (it == textures_.end() || !it->second.dynamic || it->second.width != operation.width ||
                it->second.height != operation.height || it->second.format != format) {
                SetOperationFailure("update-texture-resource-mismatch");
                return false;
            }
            if (it->second.destroyPending) {
                SetOperationFailure("update-texture-destroy-pending");
                return false;
            }
            texture = it->second.texture;
        }
        if (!texture) {
            SetOperationFailure("update-texture-resource-null");
            return false;
        }
        auto* immediate = context_->Context();
        if (!immediate) {
            SetOperationFailure("update-texture-immediate-context-missing");
            return false;
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const auto mapResult = immediate->Map(texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(mapResult) || !mapped.pData || mapped.RowPitch < operation.width * bytesPerPixel) {
            if (mapped.pData) immediate->Unmap(texture.Get(), 0);
            SetOperationFailure("update-texture-map", mapResult);
            return false;
        }
        const auto rowSize = operation.width * bytesPerPixel;
        for (std::uint32_t row = 0; row < operation.height; ++row) {
            std::memcpy(static_cast<std::uint8_t*>(mapped.pData) + static_cast<std::size_t>(row) * mapped.RowPitch,
                        operation.bytes.data() + static_cast<std::size_t>(row) * operation.rowBytes, rowSize);
        }
        immediate->Unmap(texture.Get(), 0);
        {
            std::lock_guard lock{resourcesMutex_};
            const auto it = textures_.find(operation.id);
            if (it == textures_.end() || it->second.texture.Get() != texture.Get()) {
                SetOperationFailure("update-texture-changed-before-commit");
                return false;
            }
            it->second.pixels = operation.bytes;
            it->second.rowBytes = operation.rowBytes;
        }
        return true;
    }

    bool FalloutGPUDriver::ExecuteDestroyTexture(const AcceleratedGenerationOperation& operation) {
        if (operation.id == 0) {
            SetOperationFailure("destroy-texture-invalid-id");
            return false;
        }
        std::lock_guard lock{resourcesMutex_};
        const auto texture = textures_.find(operation.id);
        if (texture == textures_.end()) {
            SetOperationFailure("destroy-texture-missing");
            return false;
        }
        for (const auto& entry : renderTargets_)
            if (entry.second.textureId == operation.id) {
                texture->second.destroyPending = true;
                return true;
            }
        textures_.erase(texture);
        return true;
    }

    bool FalloutGPUDriver::ExecuteCreateRenderBuffer(const AcceleratedGenerationOperation& operation) {
        if (!context_ || !context_->Ready() || operation.id == 0 || operation.auxiliaryId == 0 || operation.width == 0 ||
            operation.height == 0 || operation.hasStencilBuffer || operation.hasDepthBuffer) {
            SetOperationFailure("create-render-buffer-invalid-metadata");
            return false;
        }
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        {
            std::lock_guard lock{resourcesMutex_};
            if (renderTargets_.contains(operation.id)) {
                SetOperationFailure("create-render-buffer-duplicate-id");
                return false;
            }
            const auto it = textures_.find(operation.auxiliaryId);
            if (it == textures_.end() || !it->second.texture || it->second.width != operation.width ||
                it->second.height != operation.height) {
                SetOperationFailure("create-render-buffer-backing-texture-mismatch");
                return false;
            }
            if (it->second.destroyPending) {
                SetOperationFailure("create-render-buffer-backing-texture-destroy-pending");
                return false;
            }
            texture = it->second.texture;
        }
        D3D11_TEXTURE2D_DESC textureDesc{};
        texture->GetDesc(&textureDesc);
        if (textureDesc.Width != operation.width || textureDesc.Height != operation.height ||
            textureDesc.SampleDesc.Count != 1 || textureDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
            SetOperationFailure("create-render-buffer-descriptor-mismatch");
            return false;
        }
        UINT qualityLevels = 0;
        const auto qualityResult = context_->Device()->CheckMultisampleQualityLevels(
            textureDesc.Format, kRenderBufferSampleCount, &qualityLevels);
        if (FAILED(qualityResult)) {
            SetOperationFailure("check-multisample-quality", qualityResult);
            return false;
        }
        if (qualityLevels == 0) {
            SetOperationFailure("no-multisample-quality");
            return false;
        }
        D3D11_TEXTURE2D_DESC multisampleDesc = textureDesc;
        multisampleDesc.SampleDesc.Count = kRenderBufferSampleCount;
        multisampleDesc.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN;
        multisampleDesc.Usage = D3D11_USAGE_DEFAULT;
        multisampleDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
        multisampleDesc.CPUAccessFlags = 0;
        multisampleDesc.MiscFlags = 0;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> multisampleTexture;
        const auto multisampleResult = context_->Device()->CreateTexture2D(
            &multisampleDesc, nullptr, multisampleTexture.GetAddressOf());
        if (FAILED(multisampleResult)) {
            SetOperationFailure("create-multisample-texture", multisampleResult);
            return false;
        }
        if (!multisampleTexture) {
            SetOperationFailure("create-multisample-texture-null");
            return false;
        }
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = textureDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        const auto rtvResult = context_->Device()->CreateRenderTargetView(
            multisampleTexture.Get(), &rtvDesc, rtv.GetAddressOf());
        if (FAILED(rtvResult)) {
            SetOperationFailure("create-render-target-view", rtvResult);
            return false;
        }
        if (!rtv) {
            SetOperationFailure("create-render-target-view-null");
            return false;
        }
        std::lock_guard lock{resourcesMutex_};
        if (renderTargets_.contains(operation.id)) {
            SetOperationFailure("create-render-buffer-duplicate-at-commit");
            return false;
        }
        const auto it = textures_.find(operation.auxiliaryId);
        if (it == textures_.end() || it->second.texture.Get() != texture.Get()) {
            SetOperationFailure("create-render-buffer-backing-changed-before-commit");
            return false;
        }
        RenderTargetEntry entry;
        entry.multisampleTexture = std::move(multisampleTexture);
        entry.rtv = std::move(rtv);
        entry.textureId = operation.auxiliaryId;
        entry.width = operation.width;
        entry.height = operation.height;
        entry.epoch = resourceEpoch_;
        renderTargets_.emplace(operation.id, std::move(entry));
        return true;
    }

    bool FalloutGPUDriver::ExecuteDestroyRenderBuffer(const AcceleratedGenerationOperation& operation) {
        if (operation.id == 0) {
            SetOperationFailure("destroy-render-buffer-invalid-id");
            return false;
        }
        std::lock_guard lock{resourcesMutex_};
        const auto target = renderTargets_.find(operation.id);
        if (target == renderTargets_.end()) {
            SetOperationFailure("destroy-render-buffer-missing");
            return false;
        }
        const auto textureId = target->second.textureId;
        renderTargets_.erase(target);
        const auto texture = textures_.find(textureId);
        if (texture != textures_.end() && texture->second.destroyPending) {
            const auto stillReferenced = std::any_of(
                renderTargets_.begin(), renderTargets_.end(),
                [textureId](const auto& entry) { return entry.second.textureId == textureId; });
            if (!stillReferenced) textures_.erase(texture);
        }
        return true;
    }

    bool FalloutGPUDriver::ExecuteCreateGeometry(const AcceleratedGenerationOperation& operation) {
        if (!context_ || !context_->Ready() || operation.id == 0 || operation.bytes.empty() ||
            operation.secondaryBytes.empty() || operation.bytes.size() > UINT_MAX ||
            operation.secondaryBytes.size() > UINT_MAX || operation.secondaryBytes.size() % sizeof(ultralight::IndexType) != 0 ||
            operation.secondarySize != operation.secondaryBytes.size() ||
            operation.vertexFormat > static_cast<std::uint32_t>(ultralight::VertexBufferFormat::_2f_4ub_2f_2f_28f)) {
            SetOperationFailure("create-geometry-invalid-payload-or-format");
            return false;
        }
        {
            std::lock_guard lock{resourcesMutex_};
            if (geometry_.contains(operation.id)) {
                SetOperationFailure("create-geometry-duplicate-id");
                return false;
            }
        }
        D3D11_BUFFER_DESC vertexDesc{};
        vertexDesc.Usage = D3D11_USAGE_DYNAMIC;
        vertexDesc.ByteWidth = static_cast<UINT>(operation.bytes.size());
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA vertexData{};
        vertexData.pSysMem = operation.bytes.data();
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        const auto vertexResult = context_->Device()->CreateBuffer(&vertexDesc, &vertexData, vertexBuffer.GetAddressOf());
        if (FAILED(vertexResult)) {
            SetOperationFailure("create-vertex-buffer", vertexResult);
            return false;
        }
        if (!vertexBuffer) {
            SetOperationFailure("create-vertex-buffer-null");
            return false;
        }
        D3D11_BUFFER_DESC indexDesc{};
        indexDesc.Usage = D3D11_USAGE_DYNAMIC;
        indexDesc.ByteWidth = static_cast<UINT>(operation.secondaryBytes.size());
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        indexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA indexData{};
        indexData.pSysMem = operation.secondaryBytes.data();
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        const auto indexResult = context_->Device()->CreateBuffer(&indexDesc, &indexData, indexBuffer.GetAddressOf());
        if (FAILED(indexResult)) {
            SetOperationFailure("create-index-buffer", indexResult);
            return false;
        }
        if (!indexBuffer) {
            SetOperationFailure("create-index-buffer-null");
            return false;
        }
        std::lock_guard lock{resourcesMutex_};
        if (geometry_.contains(operation.id)) {
            SetOperationFailure("create-geometry-duplicate-at-commit");
            return false;
        }
        GeometryEntry entry;
        entry.format = static_cast<ultralight::VertexBufferFormat>(operation.vertexFormat);
        entry.vertexBuffer = std::move(vertexBuffer);
        entry.indexBuffer = std::move(indexBuffer);
        entry.vertexCapacity = static_cast<std::uint32_t>(operation.bytes.size());
        entry.indexCapacity = static_cast<std::uint32_t>(operation.secondaryBytes.size());
        entry.vertexSize = entry.vertexCapacity;
        entry.indexSize = entry.indexCapacity;
        entry.epoch = resourceEpoch_;
        entry.vertexBytes = operation.bytes;
        entry.indexBytes = operation.secondaryBytes;
        geometry_.emplace(operation.id, std::move(entry));
        return true;
    }

    bool FalloutGPUDriver::ExecuteUpdateGeometry(const AcceleratedGenerationOperation& operation) {
        if (!context_ || !context_->Ready() || operation.id == 0 || operation.bytes.empty() ||
            operation.secondaryBytes.empty() || operation.bytes.size() > UINT_MAX ||
            operation.secondaryBytes.size() > UINT_MAX || operation.secondaryBytes.size() % sizeof(ultralight::IndexType) != 0 ||
            operation.secondarySize != operation.secondaryBytes.size() ||
            operation.vertexFormat > static_cast<std::uint32_t>(ultralight::VertexBufferFormat::_2f_4ub_2f_2f_28f)) {
            SetOperationFailure("update-geometry-invalid-payload-or-format");
            return false;
        }
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        std::uint32_t vertexCapacity = 0;
        std::uint32_t indexCapacity = 0;
        ultralight::VertexBufferFormat storedFormat{};
        {
            std::lock_guard lock{resourcesMutex_};
            const auto it = geometry_.find(operation.id);
            if (it == geometry_.end()) {
                SetOperationFailure("update-geometry-missing");
                return false;
            }
            vertexBuffer = it->second.vertexBuffer;
            indexBuffer = it->second.indexBuffer;
            vertexCapacity = it->second.vertexCapacity;
            indexCapacity = it->second.indexCapacity;
            storedFormat = it->second.format;
        }
        const auto requestedFormat = static_cast<ultralight::VertexBufferFormat>(operation.vertexFormat);
        if (!vertexBuffer || !indexBuffer) {
            SetOperationFailure("update-geometry-buffer-missing");
            return false;
        }
        if (operation.bytes.size() > vertexCapacity || operation.secondaryBytes.size() > indexCapacity ||
            requestedFormat != storedFormat) {
            AcceleratedGenerationOperation replacement = operation;
            replacement.type = AcceleratedGenerationOperationType::CreateGeometry;
            std::lock_guard lock{resourcesMutex_};
            if (!geometry_.contains(operation.id)) {
                SetOperationFailure("update-geometry-missing-before-replacement");
                return false;
            }
            D3D11_BUFFER_DESC vertexDesc{};
            vertexDesc.Usage = D3D11_USAGE_DYNAMIC;
            vertexDesc.ByteWidth = static_cast<UINT>(replacement.bytes.size());
            vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            D3D11_SUBRESOURCE_DATA vertexData{replacement.bytes.data(), 0, 0};
            Microsoft::WRL::ComPtr<ID3D11Buffer> replacementVertex;
            const auto vertexResult = context_->Device()->CreateBuffer(
                &vertexDesc, &vertexData, replacementVertex.GetAddressOf());
            if (FAILED(vertexResult)) {
                SetOperationFailure("replace-vertex-buffer", vertexResult);
                return false;
            }
            if (!replacementVertex) {
                SetOperationFailure("replace-vertex-buffer-null");
                return false;
            }
            D3D11_BUFFER_DESC indexDesc{};
            indexDesc.Usage = D3D11_USAGE_DYNAMIC;
            indexDesc.ByteWidth = static_cast<UINT>(replacement.secondaryBytes.size());
            indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            indexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            D3D11_SUBRESOURCE_DATA indexData{replacement.secondaryBytes.data(), 0, 0};
            Microsoft::WRL::ComPtr<ID3D11Buffer> replacementIndex;
            const auto indexResult = context_->Device()->CreateBuffer(
                &indexDesc, &indexData, replacementIndex.GetAddressOf());
            if (FAILED(indexResult)) {
                SetOperationFailure("replace-index-buffer", indexResult);
                return false;
            }
            if (!replacementIndex) {
                SetOperationFailure("replace-index-buffer-null");
                return false;
            }
            auto& entry = geometry_[operation.id];
            entry.format = requestedFormat;
            entry.vertexBuffer = std::move(replacementVertex);
            entry.indexBuffer = std::move(replacementIndex);
            entry.vertexCapacity = static_cast<std::uint32_t>(replacement.bytes.size());
            entry.indexCapacity = static_cast<std::uint32_t>(replacement.secondaryBytes.size());
            entry.vertexSize = entry.vertexCapacity;
            entry.indexSize = entry.indexCapacity;
            entry.epoch = resourceEpoch_;
            entry.vertexBytes = replacement.bytes;
            entry.indexBytes = replacement.secondaryBytes;
            return true;
        }

        auto* immediate = context_->Context();
        if (!immediate) {
            SetOperationFailure("update-geometry-immediate-context-missing");
            return false;
        }
        D3D11_MAPPED_SUBRESOURCE vertexMapped{};
        const auto vertexMapResult = immediate->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &vertexMapped);
        if (FAILED(vertexMapResult) || !vertexMapped.pData) {
            if (vertexMapped.pData) immediate->Unmap(vertexBuffer.Get(), 0);
            SetOperationFailure("update-vertex-buffer-map", vertexMapResult);
            return false;
        }
        std::memcpy(vertexMapped.pData, operation.bytes.data(), operation.bytes.size());
        immediate->Unmap(vertexBuffer.Get(), 0);
        D3D11_MAPPED_SUBRESOURCE indexMapped{};
        const auto indexMapResult = immediate->Map(indexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &indexMapped);
        if (FAILED(indexMapResult) || !indexMapped.pData) {
            if (indexMapped.pData) immediate->Unmap(indexBuffer.Get(), 0);
            SetOperationFailure("update-index-buffer-map", indexMapResult);
            return false;
        }
        std::memcpy(indexMapped.pData, operation.secondaryBytes.data(), operation.secondaryBytes.size());
        immediate->Unmap(indexBuffer.Get(), 0);
        std::lock_guard lock{resourcesMutex_};
        const auto it = geometry_.find(operation.id);
        if (it == geometry_.end() || it->second.vertexBuffer.Get() != vertexBuffer.Get() ||
            it->second.indexBuffer.Get() != indexBuffer.Get()) {
            SetOperationFailure("update-geometry-changed-before-commit");
            return false;
        }
        it->second.vertexSize = static_cast<std::uint32_t>(operation.bytes.size());
        it->second.indexSize = static_cast<std::uint32_t>(operation.secondaryBytes.size());
        it->second.vertexBytes = operation.bytes;
        it->second.indexBytes = operation.secondaryBytes;
        return true;
    }

    bool FalloutGPUDriver::ExecuteDestroyGeometry(const AcceleratedGenerationOperation& operation) {
        if (operation.id == 0) {
            SetOperationFailure("destroy-geometry-invalid-id");
            return false;
        }
        std::lock_guard lock{resourcesMutex_};
        if (geometry_.erase(operation.id) == 0) {
            SetOperationFailure("destroy-geometry-missing");
            return false;
        }
        return true;
    }

    bool FalloutGPUDriver::ExecuteResourceOperation(const AcceleratedGenerationOperation& operation) {
        switch (operation.type) {
            case AcceleratedGenerationOperationType::CreateTexture: return ExecuteCreateTexture(operation);
            case AcceleratedGenerationOperationType::UpdateTexture: return ExecuteUpdateTexture(operation);
            case AcceleratedGenerationOperationType::DestroyTexture: return ExecuteDestroyTexture(operation);
            case AcceleratedGenerationOperationType::CreateRenderBuffer: return ExecuteCreateRenderBuffer(operation);
            case AcceleratedGenerationOperationType::DestroyRenderBuffer: return ExecuteDestroyRenderBuffer(operation);
            case AcceleratedGenerationOperationType::CreateGeometry: return ExecuteCreateGeometry(operation);
            case AcceleratedGenerationOperationType::UpdateGeometry: return ExecuteUpdateGeometry(operation);
            case AcceleratedGenerationOperationType::DestroyGeometry: return ExecuteDestroyGeometry(operation);
            default:
                SetOperationFailure("unsupported-operation-type");
                return false;
        }
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FalloutGPUDriver::AcquireTextureSRV(
        std::uint32_t textureId) const {
        std::lock_guard lock{resourcesMutex_};
        const auto it = textures_.find(textureId);
        return it == textures_.end() || it->second.epoch != resourceEpoch_ || it->second.destroyPending ?
                   Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>{} : it->second.srv;
    }

    FalloutGPUDriver::TextureSnapshot FalloutGPUDriver::AcquireTextureSnapshot(std::uint32_t textureId) const {
        std::lock_guard lock{resourcesMutex_};
        const auto it = textures_.find(textureId);
        if (it == textures_.end()) return {};
        if (it->second.epoch != resourceEpoch_ || it->second.destroyPending) return {};
        return TextureSnapshot{it->second.srv, it->second.width, it->second.height, it->second.format,
                               it->second.epoch};
    }

    FalloutGPUDriver::TextureSnapshot FalloutGPUDriver::AcquirePresentationTextureSnapshot(
        std::uint32_t textureId) const {
        std::lock_guard lock{resourcesMutex_};
        const auto it = textures_.find(textureId);
        if (it == textures_.end() || it->second.epoch != resourceEpoch_) return {};
        if (it->second.destroyPending) {
            const bool retainedByRenderBuffer = std::any_of(
                renderTargets_.begin(), renderTargets_.end(), [textureId, this](const auto& entry) {
                    return entry.second.epoch == resourceEpoch_ && entry.second.textureId == textureId;
                });
            if (!retainedByRenderBuffer) return {};
        }
        return TextureSnapshot{it->second.srv, it->second.width, it->second.height, it->second.format,
                               it->second.epoch};
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FalloutGPUDriver::AcquireRenderBufferSRV(
        std::uint32_t renderBufferId) const {
        std::lock_guard lock{resourcesMutex_};
        const auto target = renderTargets_.find(renderBufferId);
        if (target == renderTargets_.end()) return {};
        const auto texture = textures_.find(target->second.textureId);
        return target->second.epoch != resourceEpoch_ || texture == textures_.end() ||
                       texture->second.epoch != resourceEpoch_ ?
                   Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>{} : texture->second.srv;
    }

    void FalloutGPUDriver::ResetPipelineForEpoch() noexcept {
        pipelineReady_ = false;
        fillVS_.Reset();
        fillPS_.Reset();
        fillPathVS_.Reset();
        fillPathPS_.Reset();
        fillLayout_.Reset();
        fillPathLayout_.Reset();
        constantBuffer_.Reset();
        if (context_) context_->ResetStates();
    }

    HRESULT FalloutGPUDriver::DeviceFailureResult() const noexcept {
        if (context_ && context_->Device()) {
            const auto reason = context_->Device()->GetDeviceRemovedReason();
            if (reason == DXGI_ERROR_DEVICE_REMOVED || reason == DXGI_ERROR_DEVICE_RESET) return reason;
        }
        return E_FAIL;
    }

    bool FalloutGPUDriver::RebuildForDeviceEpoch(std::uint64_t epoch) {
        if (!epoch || !context_ || !context_->Ready() || !context_->PrepareImmediateDevice()) return false;
        auto* device = context_->Device();
        if (!device) return false;
        ResetTransportForEpoch(epoch);
        ResetPipelineForEpoch();
        std::lock_guard lock{resourcesMutex_};

        for (auto& [_, entry] : textures_) {
            entry.texture.Reset();
            entry.srv.Reset();
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = entry.width;
            desc.Height = entry.height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = entry.format;
            desc.SampleDesc.Count = 1;
            desc.Usage = entry.dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
            desc.BindFlags = entry.dynamic ? D3D11_BIND_SHADER_RESOURCE
                                           : D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = entry.dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
            D3D11_SUBRESOURCE_DATA initial{};
            initial.pSysMem = entry.pixels.data();
            initial.SysMemPitch = entry.rowBytes;
            initial.SysMemSlicePitch = static_cast<UINT>(entry.pixels.size());
            if (entry.dynamic && entry.pixels.empty()) return false;
            if (FAILED(device->CreateTexture2D(&desc, entry.dynamic ? &initial : nullptr,
                                                entry.texture.GetAddressOf())) || !entry.texture)
                return false;
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;
            if (FAILED(device->CreateShaderResourceView(entry.texture.Get(), &srvDesc, entry.srv.GetAddressOf())) ||
                !entry.srv)
                return false;
            entry.epoch = epoch;
        }

        for (auto& [_, entry] : geometry_) {
            entry.vertexBuffer.Reset();
            entry.indexBuffer.Reset();
            if (entry.vertexBytes.empty() || entry.indexBytes.empty()) return false;
            D3D11_BUFFER_DESC vertexDesc{};
            vertexDesc.Usage = D3D11_USAGE_DYNAMIC;
            vertexDesc.ByteWidth = static_cast<UINT>(entry.vertexCapacity);
            vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            D3D11_SUBRESOURCE_DATA vertexData{entry.vertexBytes.data(), 0, 0};
            if (FAILED(device->CreateBuffer(&vertexDesc, &vertexData, entry.vertexBuffer.GetAddressOf())) ||
                !entry.vertexBuffer)
                return false;
            D3D11_BUFFER_DESC indexDesc{};
            indexDesc.Usage = D3D11_USAGE_DYNAMIC;
            indexDesc.ByteWidth = static_cast<UINT>(entry.indexCapacity);
            indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            indexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            D3D11_SUBRESOURCE_DATA indexData{entry.indexBytes.data(), 0, 0};
            if (FAILED(device->CreateBuffer(&indexDesc, &indexData, entry.indexBuffer.GetAddressOf())) ||
                !entry.indexBuffer)
                return false;
            entry.epoch = epoch;
        }

        for (auto& [_, entry] : renderTargets_) {
            entry.multisampleTexture.Reset();
            entry.rtv.Reset();
            const auto texture = textures_.find(entry.textureId);
            if (texture == textures_.end() || texture->second.epoch != epoch || !texture->second.texture) return false;
            D3D11_TEXTURE2D_DESC textureDesc{};
            texture->second.texture->GetDesc(&textureDesc);
            UINT qualityLevels = 0;
            if (FAILED(device->CheckMultisampleQualityLevels(textureDesc.Format, kRenderBufferSampleCount,
                                                              &qualityLevels)) || qualityLevels == 0)
                return false;
            textureDesc.SampleDesc.Count = kRenderBufferSampleCount;
            textureDesc.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN;
            textureDesc.Usage = D3D11_USAGE_DEFAULT;
            textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
            textureDesc.CPUAccessFlags = 0;
            textureDesc.MiscFlags = 0;
            if (FAILED(device->CreateTexture2D(&textureDesc, nullptr, entry.multisampleTexture.GetAddressOf())) ||
                !entry.multisampleTexture)
                return false;
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
            rtvDesc.Format = textureDesc.Format;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
            if (FAILED(device->CreateRenderTargetView(entry.multisampleTexture.Get(), &rtvDesc,
                                                       entry.rtv.GetAddressOf())) ||
                !entry.rtv)
                return false;
            entry.epoch = epoch;
            entry.dirty = false;
        }
        resourceEpoch_ = epoch;
        lastFailureResult_ = S_OK;
        return true;
    }

    bool FalloutGPUDriver::ClearRenderBufferCommand(std::uint32_t renderBufferId) {
        if (!context_ || !context_->Ready()) {
            SetOperationFailure("clear-context-unavailable");
            return false;
        }
        if (renderBufferId == 0) {
            SetOperationFailure("clear-default-render-target");
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        {
            std::lock_guard lock{resourcesMutex_};
            const auto it = renderTargets_.find(renderBufferId);
            if (it == renderTargets_.end()) {
                SetOperationFailure("clear-render-target-missing");
                return false;
            }
            if (it->second.epoch != resourceEpoch_) {
                SetOperationFailure("clear-render-target-epoch-mismatch");
                return false;
            }
            rtv = it->second.rtv;
            width = it->second.width;
            height = it->second.height;
        }
        if (!rtv) {
            SetOperationFailure("clear-render-target-view-missing");
            return false;
        }

        auto* immediate = context_->Context();
        if (!immediate) {
            SetOperationFailure("clear-immediate-context-missing");
            return false;
        }

        ID3D11ShaderResourceView* nullResources[3] = {nullptr, nullptr, nullptr};
        immediate->PSSetShaderResources(0, 3, nullResources);

        ID3D11RenderTargetView* raw = rtv.Get();
        immediate->OMSetRenderTargets(1, &raw, nullptr);
        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MinDepth = 0.0F;
        viewport.MaxDepth = 1.0F;
        immediate->RSSetViewports(1, &viewport);
        constexpr float clear[4] = {0.0F, 0.0F, 0.0F, 0.0F};
        immediate->ClearRenderTargetView(rtv.Get(), clear);
        std::lock_guard lock{resourcesMutex_};
        const auto it = renderTargets_.find(renderBufferId);
        if (it != renderTargets_.end() && it->second.rtv.Get() == rtv.Get()) it->second.dirty = true;
        return true;
    }

    bool FalloutGPUDriver::ResolveRenderBufferForTexture(std::uint32_t textureId) {
        if (!context_ || !context_->Ready() || textureId == 0) return true;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> destination;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> multisample;
        std::uint32_t renderBufferId = 0;
        {
            std::lock_guard lock{resourcesMutex_};
            for (const auto& [id, target] : renderTargets_) {
                if (target.textureId != textureId || !target.dirty) continue;
                const auto texture = textures_.find(textureId);
                if (target.epoch != resourceEpoch_ || texture == textures_.end() ||
                    texture->second.epoch != resourceEpoch_ || !texture->second.texture ||
                    !target.multisampleTexture)
                {
                    SetOperationFailure("resolve-resource-invalid");
                    return false;
                }
                renderBufferId = id;
                destination = texture->second.texture;
                multisample = target.multisampleTexture;
                break;
            }
        }
        if (!renderBufferId) return true;

        auto* immediate = context_->Context();
        if (!immediate || !destination || !multisample) {
            SetOperationFailure("resolve-resource-missing");
            return false;
        }
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::ResolveRenderBuffer);
        immediate->OMSetRenderTargets(0, nullptr, nullptr);
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::ResolveRenderBuffer);
        immediate->ResolveSubresource(destination.Get(), 0, multisample.Get(), 0, DXGI_FORMAT_B8G8R8A8_UNORM);

        std::lock_guard lock{resourcesMutex_};
        const auto it = renderTargets_.find(renderBufferId);
        if (it != renderTargets_.end() && it->second.textureId == textureId &&
            it->second.multisampleTexture.Get() == multisample.Get()) {
            it->second.dirty = false;
        }
        return true;
    }

    bool FalloutGPUDriver::ResolveDirtyRenderBuffers() {
        if (!context_ || !context_->Ready()) {
            SetOperationFailure("resolve-context-unavailable");
            return false;
        }

        struct PendingResolve {
            std::uint32_t renderBufferId = 0;
            std::uint32_t textureId = 0;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> destination;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> multisample;
        };
        std::vector<PendingResolve> pending;
        {
            std::lock_guard lock{resourcesMutex_};
            for (const auto& [id, target] : renderTargets_) {
                if (target.epoch != resourceEpoch_ || !target.dirty || !target.multisampleTexture) continue;
                const auto texture = textures_.find(target.textureId);
                if (texture == textures_.end()) {
                    SetOperationFailure("resolve-destination-texture-missing");
                    return false;
                }
                if (texture->second.epoch != resourceEpoch_) {
                    SetOperationFailure("resolve-destination-epoch-mismatch");
                    return false;
                }
                if (!texture->second.texture) {
                    SetOperationFailure("resolve-destination-resource-missing");
                    return false;
                }
                pending.push_back({id, target.textureId, texture->second.texture, target.multisampleTexture});
            }
        }
        if (pending.empty()) return true;

        auto* immediate = context_->Context();
        if (!immediate) {
            SetOperationFailure("resolve-immediate-context-missing");
            return false;
        }
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::ResolveRenderBuffer);
        immediate->OMSetRenderTargets(0, nullptr, nullptr);
        for (const auto& resolve : pending) {
            MarkBreadcrumb(AcceleratedBreadcrumbOperation::ResolveRenderBuffer);
            immediate->ResolveSubresource(resolve.destination.Get(), 0, resolve.multisample.Get(), 0,
                                          DXGI_FORMAT_B8G8R8A8_UNORM);
        }

        std::lock_guard lock{resourcesMutex_};
        for (const auto& resolve : pending) {
            const auto it = renderTargets_.find(resolve.renderBufferId);
            if (it != renderTargets_.end() && it->second.textureId == resolve.textureId &&
                it->second.multisampleTexture.Get() == resolve.multisample.Get()) {
                it->second.dirty = false;
            }
        }
        return true;
    }

    bool FalloutGPUDriver::InitializePipeline() {

        if (!context_ || !context_->Ready()) return false;
        if (!context_->EnsureStates()) return false;
        return EnsurePipeline();
    }

    bool FalloutGPUDriver::EnsurePipeline() {
        if (pipelineReady_) return true;
        if (!context_ || !context_->Ready()) return false;
        auto* device = context_->Device();
        if (!device) return false;

        const auto compile = [](const char* src, std::size_t len, const char* entry,
                                const char* target) -> Microsoft::WRL::ComPtr<ID3DBlob> {
            const UINT flags =
                D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL2 | D3DCOMPILE_PARTIAL_PRECISION;
            Microsoft::WRL::ComPtr<ID3DBlob> code;
            Microsoft::WRL::ComPtr<ID3DBlob> errors;
            if (FAILED(D3DCompile(src, len, nullptr, nullptr, nullptr, entry, target, flags, 0, code.GetAddressOf(),
                                  errors.GetAddressOf()))) {
                if (errors) OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
                return {};
            }
            return code;
        };

        const auto fillPathVsBlob = compile(kFillPathVS, sizeof(kFillPathVS) - 1, "VS", "vs_4_0");
        const auto fillPathPsBlob = compile(kFillPathPS, sizeof(kFillPathPS) - 1, "PS", "ps_4_0");
        const auto fillVsBlob = compile(kFillVS, sizeof(kFillVS) - 1, "VS", "vs_4_0");
        const auto fillPsBlob = compile(kFillPS, sizeof(kFillPS) - 1, "PS", "ps_4_0");
        if (!fillPathVsBlob || !fillPathPsBlob || !fillVsBlob || !fillPsBlob) return false;

        if (FAILED(device->CreateVertexShader(fillPathVsBlob->GetBufferPointer(), fillPathVsBlob->GetBufferSize(),
                                              nullptr, fillPathVS_.ReleaseAndGetAddressOf())) ||
            FAILED(device->CreatePixelShader(fillPathPsBlob->GetBufferPointer(), fillPathPsBlob->GetBufferSize(),
                                             nullptr, fillPathPS_.ReleaseAndGetAddressOf())) ||
            FAILED(device->CreateVertexShader(fillVsBlob->GetBufferPointer(), fillVsBlob->GetBufferSize(), nullptr,
                                              fillVS_.ReleaseAndGetAddressOf())) ||
            FAILED(device->CreatePixelShader(fillPsBlob->GetBufferPointer(), fillPsBlob->GetBufferSize(), nullptr,
                                             fillPS_.ReleaseAndGetAddressOf()))) {
            return false;
        }

        const D3D11_INPUT_ELEMENT_DESC pathLayout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        if (FAILED(device->CreateInputLayout(pathLayout, ARRAYSIZE(pathLayout), fillPathVsBlob->GetBufferPointer(),
                                             fillPathVsBlob->GetBufferSize(),
                                             fillPathLayout_.ReleaseAndGetAddressOf()))) {
            return false;
        }

        const D3D11_INPUT_ELEMENT_DESC fillLayout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,
             0},
            {"COLOR", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,
             0},
            {"COLOR", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,
             0},
            {"COLOR", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,
             0},
            {"COLOR", 5, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,
             0},
            {"COLOR", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,
             0},
            {"COLOR", 7, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,
             0},
        };
        if (FAILED(device->CreateInputLayout(fillLayout, ARRAYSIZE(fillLayout), fillVsBlob->GetBufferPointer(),
                                             fillVsBlob->GetBufferSize(), fillLayout_.ReleaseAndGetAddressOf()))) {
            return false;
        }

        D3D11_BUFFER_DESC cbDesc{};
        cbDesc.ByteWidth = sizeof(UltralightGPUUniformBlock);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateBuffer(&cbDesc, nullptr, constantBuffer_.ReleaseAndGetAddressOf()))) return false;

        pipelineReady_ = true;
        return true;
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

    bool FalloutGPUDriver::DrawGeometryCommand(std::uint32_t geometryId, std::uint32_t indicesCount,
                                               std::uint32_t indicesOffset, const ultralight::GPUState& state) {
        if (provenanceRejected_) {
            SetOperationFailure("provenance-rejected");
            return false;
        }
        if (!InitializePipeline()) {
            SetOperationFailure("pipeline-initialization");
            return false;
        }
        auto* drawContext = context_->Context();
        if (!drawContext) {
            SetOperationFailure("immediate-context-missing");
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv0;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv1;
        ultralight::VertexBufferFormat format{};
        std::uint32_t indexSize = 0;
        std::uint32_t targetTextureId = 0;
        {
            std::lock_guard lock{resourcesMutex_};
            const auto target = renderTargets_.find(state.render_buffer_id);
            if (target == renderTargets_.end()) {
                SetOperationFailure("render-target-missing");
                return false;
            }
            if (target->second.epoch != resourceEpoch_) {
                SetOperationFailure("render-target-epoch-mismatch");
                return false;
            }
            if (!target->second.rtv) {
                SetOperationFailure("render-target-view-missing");
                return false;
            }
            rtv = target->second.rtv;
            targetTextureId = target->second.textureId;

            const auto geo = geometry_.find(geometryId);
            if (geo == geometry_.end()) {
                SetOperationFailure("geometry-missing");
                return false;
            }
            if (geo->second.epoch != resourceEpoch_) {
                SetOperationFailure("geometry-epoch-mismatch");
                return false;
            }
            if (!geo->second.vertexBuffer || !geo->second.indexBuffer) {
                SetOperationFailure("geometry-buffer-missing");
                return false;
            }
            vertexBuffer = geo->second.vertexBuffer;
            indexBuffer = geo->second.indexBuffer;
            format = geo->second.format;
            indexSize = geo->second.indexSize;

            if (state.texture_1_id) {
                const auto it = textures_.find(state.texture_1_id);
                if (it == textures_.end()) {
                    SetOperationFailure("texture0-missing");
                    return false;
                }
                if (it->second.epoch != resourceEpoch_) {
                    SetOperationFailure("texture0-epoch-mismatch");
                    return false;
                }
                if (it->second.destroyPending) {
                    SetOperationFailure("texture0-destroy-pending");
                    return false;
                }
                if (!it->second.srv) {
                    SetOperationFailure("texture0-view-missing");
                    return false;
                }
                srv0 = it->second.srv;
            }
            if (state.texture_2_id) {
                const auto it = textures_.find(state.texture_2_id);
                if (it == textures_.end()) {
                    SetOperationFailure("texture1-missing");
                    return false;
                }
                if (it->second.epoch != resourceEpoch_) {
                    SetOperationFailure("texture1-epoch-mismatch");
                    return false;
                }
                if (it->second.destroyPending) {
                    SetOperationFailure("texture1-destroy-pending");
                    return false;
                }
                if (!it->second.srv) {
                    SetOperationFailure("texture1-view-missing");
                    return false;
                }
                srv1 = it->second.srv;
            }
        }

        UINT stride = 0;
        ID3D11InputLayout* inputLayout = nullptr;
        switch (format) {
            case ultralight::VertexBufferFormat::_2f_4ub_2f:
                stride = static_cast<UINT>(sizeof(ultralight::Vertex_2f_4ub_2f));
                inputLayout = fillPathLayout_.Get();
                break;
            case ultralight::VertexBufferFormat::_2f_4ub_2f_2f_28f:
                stride = static_cast<UINT>(sizeof(ultralight::Vertex_2f_4ub_2f_2f_28f));
                inputLayout = fillLayout_.Get();
                break;
        }
        if (stride == 0 || !inputLayout) {
            SetOperationFailure("unsupported-vertex-format");
            return false;
        }

        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        switch (state.shader_type) {
            case ultralight::ShaderType::Fill:
                vs = fillVS_.Get();
                ps = fillPS_.Get();
                break;
            case ultralight::ShaderType::FillPath:
                vs = fillPathVS_.Get();
                ps = fillPathPS_.Get();
                break;
        }
        if (!vs || !ps) {
            SetOperationFailure("unsupported-shader");
            return false;
        }

        const std::uint64_t indexEndBytes =
            (static_cast<std::uint64_t>(indicesOffset) + indicesCount) * sizeof(ultralight::IndexType);
        if (indicesCount == 0) {
            SetOperationFailure("empty-index-range");
            return false;
        }
        if (indexEndBytes > indexSize) {
            SetOperationFailure("index-range-out-of-bounds");
            return false;
        }

        ID3D11SamplerState* sampler = context_->Sampler();
        ID3D11BlendState* blendState = context_->BlendState(state.enable_blend);
        ID3D11DepthStencilState* depthStencilState = context_->DepthStencilState();
        ID3D11RasterizerState* rasterizerState = context_->RasterizerState(state.enable_scissor);
        activeBreadcrumb_ = {};
        activeBreadcrumb_.drawOrdinal = ++drawOrdinal_;
        activeBreadcrumb_.geometryId = geometryId;
        activeBreadcrumb_.renderBufferId = state.render_buffer_id;
        activeBreadcrumb_.texture0Id = state.texture_1_id;
        activeBreadcrumb_.texture1Id = state.texture_2_id;
        activeBreadcrumb_.indicesCount = indicesCount;
        activeBreadcrumb_.indicesOffset = indicesOffset;
        activeBreadcrumb_.indexSize = indexSize;
        activeBreadcrumb_.vertexFormat = static_cast<std::uint32_t>(format);
        activeBreadcrumb_.shaderType = static_cast<std::uint32_t>(state.shader_type);
        activeBreadcrumb_.viewportWidth = static_cast<float>(state.viewport_width);
        activeBreadcrumb_.viewportHeight = static_cast<float>(state.viewport_height);
        activeBreadcrumb_.renderTarget = reinterpret_cast<std::uintptr_t>(rtv.Get());
        activeBreadcrumb_.vertexBuffer = reinterpret_cast<std::uintptr_t>(vertexBuffer.Get());
        activeBreadcrumb_.indexBuffer = reinterpret_cast<std::uintptr_t>(indexBuffer.Get());
        activeBreadcrumb_.srv0 = reinterpret_cast<std::uintptr_t>(srv0.Get());
        activeBreadcrumb_.srv1 = reinterpret_cast<std::uintptr_t>(srv1.Get());
        activeBreadcrumb_.constantBuffer = reinterpret_cast<std::uintptr_t>(constantBuffer_.Get());
        activeBreadcrumb_.inputLayout = reinterpret_cast<std::uintptr_t>(inputLayout);
        activeBreadcrumb_.vertexShader = reinterpret_cast<std::uintptr_t>(vs);
        activeBreadcrumb_.pixelShader = reinterpret_cast<std::uintptr_t>(ps);
        activeBreadcrumb_.sampler = reinterpret_cast<std::uintptr_t>(sampler);
        activeBreadcrumb_.blendState = reinterpret_cast<std::uintptr_t>(blendState);
        activeBreadcrumb_.depthStencilState = reinterpret_cast<std::uintptr_t>(depthStencilState);
        activeBreadcrumb_.rasterizerState = reinterpret_cast<std::uintptr_t>(rasterizerState);
        activeBreadcrumb_.borrowedDevice = reinterpret_cast<std::uintptr_t>(context_->Device());
        activeBreadcrumb_.suppliedDevice = reinterpret_cast<std::uintptr_t>(context_->SuppliedDevice());
        activeBreadcrumb_.immediateContext = reinterpret_cast<std::uintptr_t>(context_->Context());
        if ((state.texture_1_id != 0 && state.texture_1_id == targetTextureId) ||
            (state.texture_2_id != 0 && state.texture_2_id == targetTextureId)) {
            MarkBreadcrumb(AcceleratedBreadcrumbOperation::LogicalRenderTargetSelfSample);
            if (!logicalSelfSampleLogged_) {
                logicalSelfSampleLogged_ = true;
                logger::warn(
                    "[GPU] logical render-target self-sample: generation={} draw={} target={} texture={} "
                    "texture0={} texture1={}",
                    activeBreadcrumb_.generation, activeBreadcrumb_.drawOrdinal, activeBreadcrumb_.renderBufferId,
                    targetTextureId, activeBreadcrumb_.texture0Id, activeBreadcrumb_.texture1Id);
            }
        }
        if (!ValidateDrawProvenance()) {
            provenanceRejected_ = true;
            SetOperationFailure("resource-provenance-mismatch");
            return false;
        }

        if (!ResolveRenderBufferForTexture(state.texture_1_id) ||
            !ResolveRenderBufferForTexture(state.texture_2_id)) {
            SetOperationFailure("render-buffer-resolve");
            return false;
        }

        const UltralightGPUUniformBlock uniforms = BuildUltralightGPUUniformBlock(state);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::MapConstantBuffer);
        const auto mapResult = drawContext->Map(constantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(mapResult) || !mapped.pData) {
            SetOperationFailure("constant-buffer-map", mapResult);
            return false;
        }
        std::memcpy(mapped.pData, &uniforms, sizeof(uniforms));
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::UnmapConstantBuffer);
        drawContext->Unmap(constantBuffer_.Get(), 0);

        ID3D11ShaderResourceView* const nullSrvs[3] = {nullptr, nullptr, nullptr};
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::ClearPixelShaderResources);
        drawContext->PSSetShaderResources(0, 3, nullSrvs);
        ID3D11RenderTargetView* rtvRaw = rtv.Get();
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetRenderTargets);
        drawContext->OMSetRenderTargets(1, &rtvRaw, nullptr);

        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(state.viewport_width);
        viewport.Height = static_cast<float>(state.viewport_height);
        viewport.MinDepth = 0.0F;
        viewport.MaxDepth = 1.0F;
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetViewports);
        drawContext->RSSetViewports(1, &viewport);

        ID3D11ShaderResourceView* const srvs[3] = {srv0.Get(), srv1.Get(), nullptr};
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetPixelShaderResources);
        drawContext->PSSetShaderResources(0, 3, srvs);

        ID3D11Buffer* cb = constantBuffer_.Get();
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetVertexConstantBuffers);
        drawContext->VSSetConstantBuffers(0, 1, &cb);
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetPixelConstantBuffers);
        drawContext->PSSetConstantBuffers(0, 1, &cb);

        const UINT offset = 0;
        ID3D11Buffer* vb = vertexBuffer.Get();
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetVertexBuffers);
        drawContext->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetIndexBuffer);
        drawContext->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetPrimitiveTopology);
        drawContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetInputLayout);
        drawContext->IASetInputLayout(inputLayout);

        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetSampler);
        drawContext->PSSetSamplers(0, 1, &sampler);

        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetVertexShader);
        drawContext->VSSetShader(vs, nullptr, 0);
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetPixelShader);
        drawContext->PSSetShader(ps, nullptr, 0);

        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetBlendState);
        if (!context_->BindBlend(state.enable_blend)) {
            SetOperationFailure("blend-state-bind");
            return false;
        }
        MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetRasterizerState);
        if (!context_->BindScissor(state.enable_scissor)) {
            SetOperationFailure("rasterizer-state-bind");
            return false;
        }
        if (state.enable_scissor) {
            const D3D11_RECT scissor{
                static_cast<LONG>(state.scissor_rect.left), static_cast<LONG>(state.scissor_rect.top),
                static_cast<LONG>(state.scissor_rect.right), static_cast<LONG>(state.scissor_rect.bottom)};
            MarkBreadcrumb(AcceleratedBreadcrumbOperation::SetScissorRects);
            drawContext->RSSetScissorRects(1, &scissor);
        }

        MarkBreadcrumb(AcceleratedBreadcrumbOperation::DrawIndexed);
        drawContext->DrawIndexed(indicesCount, indicesOffset, 0);
        std::lock_guard lock{resourcesMutex_};
        const auto target = renderTargets_.find(state.render_buffer_id);
        if (target != renderTargets_.end() && target->second.rtv.Get() == rtv.Get()) target->second.dirty = true;
        return true;
    }

}
