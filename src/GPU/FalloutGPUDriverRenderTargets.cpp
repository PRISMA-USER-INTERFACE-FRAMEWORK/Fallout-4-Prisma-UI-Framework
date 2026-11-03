#include "FalloutGPUDriver.h"

#include <algorithm>

namespace PrismaUI::GPU {

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

}
