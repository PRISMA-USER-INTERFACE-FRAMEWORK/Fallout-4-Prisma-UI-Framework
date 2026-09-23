#include "WebCompositor.h"

#include <DirectXTK/WICTextureLoader.h>
#include <d3dcompiler.h>
#include <d3d11.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <utility>
#include <vector>

#include "../Engine/EngineBackbuffer.h"
#include "Utils/ConflictChecker.h"
#include "Utils/D3D11StateGuard.h"
#include "Utils/DllLoader.h"
#include "Utils/ModulePath.h"

namespace PrismaUI {
    CompositeResult WebCompositor::ComposeAndCommitLayers(const std::vector<Web::RenderTargetSnapshot>& layers,
                                                          const std::vector<ModelPreview::Overlay>& overlays,
                                                          uint32_t width, uint32_t height) {
        if (!m_initialized) return CompositeResult::NotInitialized;
        if (layers.empty() || width == 0 || height == 0) return CompositeResult::InvalidArguments;
        if (auto* liveDevice = Engine::GetLiveRenderDevice()) {
            if (m_device && liveDevice != m_device.Get()) {
                InvalidateCommittedFrame();
                m_stage[0] = {};
                m_stage[1] = {};
                return CompositeResult::NotInitialized;
            }
        }
        for (const auto& target : layers) {
            if (!target.validForComposition(target.publishGeneration)) return CompositeResult::InvalidArguments;
            ID3D11ShaderResourceView* srv = target.srv.Get();
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srv->GetDesc(&srvDesc);
            if (srvDesc.Format != target.format) return CompositeResult::InvalidArguments;
            Microsoft::WRL::ComPtr<ID3D11Resource> resource;
            srv->GetResource(&resource);
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            if (!resource || FAILED(resource.As(&texture)) || !texture) return CompositeResult::InvalidArguments;
            D3D11_TEXTURE2D_DESC textureDesc{};
            texture->GetDesc(&textureDesc);
            if (textureDesc.Width != target.textureWidth || textureDesc.Height != target.textureHeight ||
                textureDesc.Format != target.format) {
                return CompositeResult::InvalidArguments;
            }
        }
        const int composeIndex = m_committedIndex == 0 ? 1 : 0;
        auto& stage = m_stage[composeIndex];
        if (stage.width != width || stage.height != height || !stage.rtv || !stage.srv) {
            stage = {};
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(m_device->CreateTexture2D(&desc, nullptr, &stage.texture)) ||
                FAILED(m_device->CreateRenderTargetView(stage.texture.Get(), nullptr, &stage.rtv)) ||
                FAILED(m_device->CreateShaderResourceView(stage.texture.Get(), nullptr, &stage.srv))) {
                stage = {};
                logger::error("[WebCompositor] staging frame allocation failed ({}x{})", width, height);
                return CompositeResult::CreateRtvFailed;
            }
            stage.width = width;
            stage.height = height;
        }
        ScopedD3D11State stateGuard(m_context);
        const float clearColor[4] = {0, 0, 0, 0};
        m_context->ClearRenderTargetView(stage.rtv.Get(), clearColor);
        ID3D11RenderTargetView* rtv = stage.rtv.Get();
        m_context->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT vp{};
        vp.Width = static_cast<FLOAT>(width);
        vp.Height = static_cast<FLOAT>(height);
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);
        const float blendFactor[4] = {0, 0, 0, 0};
        m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xFFFFFFFF);
        m_context->IASetInputLayout(nullptr);
        m_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->VSSetShader(m_vs.Get(), nullptr, 0);
        m_context->PSSetShader(m_ps.Get(), nullptr, 0);
        ID3D11SamplerState* sampler = m_sampler.Get();
        m_context->PSSetSamplers(0, 1, &sampler);
        ID3D11Buffer* sourceRegion = m_sourceRegion.Get();
        m_context->PSSetConstantBuffers(0, 1, &sourceRegion);
        for (const auto& target : layers) {
            ID3D11ShaderResourceView* srv = target.srv.Get();
            m_context->PSSetShaderResources(0, 1, &srv);
            const float sourceUv[] = {target.uvLeft, target.uvTop, target.uvRight, target.uvBottom};
            m_context->UpdateSubresource(m_sourceRegion.Get(), 0, nullptr, sourceUv, 0, 0);
            m_context->Draw(3, 0);
        }
        DrawModelOverlays(overlays);
        m_committedIndex = composeIndex;
        return CompositeResult::Success;
    }
    Web::RenderTargetSnapshot WebCompositor::CommittedSnapshot() const {
        Web::RenderTargetSnapshot snapshot;
        if (m_committedIndex < 0) return snapshot;
        const auto& stage = m_stage[m_committedIndex];
        snapshot.srv = stage.srv;
        snapshot.viewportWidth = stage.width;
        snapshot.viewportHeight = stage.height;
        snapshot.textureWidth = stage.width;
        snapshot.textureHeight = stage.height;
        snapshot.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        snapshot.publishGeneration = 1;
        return snapshot;
    }
}
