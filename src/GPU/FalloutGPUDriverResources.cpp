#include "FalloutGPUDriver.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace PrismaUI::GPU {

    namespace {

        constexpr UINT kRenderBufferSampleCount = 8;

    }

    bool FalloutGPUDriver::ExecuteCreateTexture(const AcceleratedGenerationOperation& operation) {
        if (externalTextures_ && externalTextures_->Contains(operation.id)) {
            SetOperationFailure("create-texture-external-id-collision");
            return false;
        }
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
        if (externalTextures_ && externalTextures_->Contains(operation.id)) return true;
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
        if (externalTextures_ && externalTextures_->Contains(operation.id)) return true;
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

    bool FalloutGPUDriver::RebuildForDeviceEpoch(std::uint64_t epoch) {
        if (!epoch || !context_ || !context_->Ready() || !context_->PrepareImmediateDevice()) return false;
        auto* device = context_->Device();
        if (!device) return false;
        ResetTransportForEpoch(epoch);
        ResetPipelineForEpoch();
        if (externalTextures_) externalTextures_->ReleaseViews();
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

}
