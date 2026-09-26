#include "WebCompositor.h"

#include <DirectXTK/WICTextureLoader.h>
#include <d3dcompiler.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>

#include "../Engine/EngineBackbuffer.h"  
#include "Utils/ConflictChecker.h"
#include "Utils/D3D11StateGuard.h"
#include "Utils/DllLoader.h"
#include "Utils/ModulePath.h"

namespace PrismaUI {

    namespace {

        const char* kShaderSrc = R"(
struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VSOut VSMain(uint id : SV_VertexID) {
    VSOut o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv.x * 2.0 - 1.0, 1.0 - o.uv.y * 2.0, 0.0, 1.0);
    return o;
}

Texture2D shellTex : register(t0);
SamplerState shellSampler : register(s0);
cbuffer SourceRegion : register(b0) { float4 sourceUv; };

float4 PSMain(VSOut i) : SV_TARGET {
    return shellTex.Sample(shellSampler, lerp(sourceUv.xy, sourceUv.zw, i.uv));
}
)";

        constexpr uint64_t kOverlaySourceCacheKeepTicks = 64;
        constexpr size_t kOverlaySourceCacheMaxEntries = 128;

        const char* ReleasePathLabel(Backbuffer::PresentationMode mode) {
            switch (mode) {
                case Backbuffer::PresentationMode::Borrowed:
                    return "borrowed";
                case Backbuffer::PresentationMode::LegacyOwned:
                    return "legacy-owned";
                default:
                    return "owned";
            }
        }

    }

    bool WebCompositor::Init(ID3D11Device* device, ID3D11DeviceContext* context) {
        m_device = device;
        m_context = context;

        const auto ini = Utils::PluginIniPath().string();
        const int ov = GetPrivateProfileIntA("Compatibility", "iBackbufferOwnership", 0, ini.c_str());
        if (ov == 1) {
            m_userOverride = Backbuffer::OwnershipOverride::Owned;
        } else if (ov == 2) {
            m_userOverride = Backbuffer::OwnershipOverride::Borrowed;
        } else {
            m_userOverride = Backbuffer::OwnershipOverride::Auto;
        }
        logger::info("[WebCompositor] iBackbufferOwnership={} ({})", ov,
                     ov == 1   ? "forced-Owned"
                     : ov == 2 ? "forced-Borrowed"
                               : "Auto");

        Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

        HRESULT hr = D3DCompile(kShaderSrc, strlen(kShaderSrc), nullptr, nullptr, nullptr, "VSMain", "vs_5_0",
                                D3DCOMPILE_ENABLE_STRICTNESS, 0, &vsBlob, &errBlob);
        if (FAILED(hr)) {
            logger::error("[WebCompositor] VS compile failed: {}",
                          errBlob ? static_cast<const char*>(errBlob->GetBufferPointer()) : "(no error blob)");
            return false;
        }
        hr = D3DCompile(kShaderSrc, strlen(kShaderSrc), nullptr, nullptr, nullptr, "PSMain", "ps_5_0",
                        D3DCOMPILE_ENABLE_STRICTNESS, 0, &psBlob, &errBlob);
        if (FAILED(hr)) {
            logger::error("[WebCompositor] PS compile failed: {}",
                          errBlob ? static_cast<const char*>(errBlob->GetBufferPointer()) : "(no error blob)");
            return false;
        }

        if (FAILED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs))) {
            logger::error("[WebCompositor] CreateVertexShader failed");
            return false;
        }
        if (FAILED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_ps))) {
            logger::error("[WebCompositor] CreatePixelShader failed");
            return false;
        }

        D3D11_SAMPLER_DESC sampDesc{};
        sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        if (FAILED(device->CreateSamplerState(&sampDesc, &m_sampler))) {
            logger::error("[WebCompositor] CreateSamplerState failed");
            return false;
        }

        D3D11_BLEND_DESC blendDesc{};
        auto& rt = blendDesc.RenderTarget[0];
        rt.BlendEnable = true;
        rt.SrcBlend = D3D11_BLEND_ONE;
        rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(device->CreateBlendState(&blendDesc, &m_blendState))) {
            logger::error("[WebCompositor] CreateBlendState failed");
            return false;
        }

        D3D11_BUFFER_DESC sourceRegionDesc{};
        sourceRegionDesc.ByteWidth = sizeof(float) * 4;
        sourceRegionDesc.Usage = D3D11_USAGE_DEFAULT;
        sourceRegionDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateBuffer(&sourceRegionDesc, nullptr, &m_sourceRegion))) {
            logger::error("[WebCompositor] CreateBuffer failed");
            return false;
        }

        m_initialized = true;
        logger::info("[WebCompositor] initialized (fullscreen-triangle blit)");
        return true;
    }

    void WebCompositor::InvalidateRenderTarget() {
        m_rtv.Reset();
        m_bbDesc = {};

        m_overlaySourceSizes.clear();

        m_ownership = Backbuffer::Ownership::Unclassified;
        m_presentationMode = Backbuffer::PresentationMode::SafeSkip;
        m_backbufferBorrowed = false;

        m_classifiedGetBufferFn = nullptr;
    }

    CompositeResult WebCompositor::Draw(IDXGISwapChain* swapChain, const Web::RenderTargetSnapshot& target,
                                        const std::vector<ModelPreview::Overlay>& overlays, int cursorX, int cursorY,
                                        bool drawCursor) {
        if (!m_initialized) return CompositeResult::NotInitialized;
        if (!target.validForComposition(target.publishGeneration) || !swapChain) {
            return CompositeResult::InvalidArguments;
        }

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

        if (auto* liveDevice = Engine::GetLiveRenderDevice()) {
            if (m_device && liveDevice != m_device.Get()) {
                static bool s_logged = false;
                if (!s_logged) {
                    s_logged = true;
                    logger::warn(
                        "[WebCompositor] game D3D11 device changed (cached={} live={}) -- renderer "
                        "recreated; dropping stale resources and skipping until re-init",
                        static_cast<void*>(m_device.Get()), static_cast<void*>(liveDevice));
                }
                InvalidateRenderTarget();
                InvalidateCommittedFrame();
                m_stage[0] = {};
                m_stage[1] = {};
                m_swapChainIdentity = nullptr;
                return CompositeResult::NotInitialized;
            }
        }

        const Engine::EngineBackbuffer eb = Engine::ResolveEngineBackbuffer();
        ID3D11RenderTargetView* engineRtv = eb.rtv;
        const bool haveEngineTarget = eb.valid;
        D3D11_TEXTURE2D_DESC engineDesc{};
        engineDesc.Width = eb.width;
        engineDesc.Height = eb.height;

        if (haveEngineTarget) {

            if (m_rtv) InvalidateRenderTarget();
            m_swapChainIdentity = nullptr;
        } else if (swapChain != m_swapChainIdentity) {

            InvalidateRenderTarget();
            m_swapChainIdentity = swapChain;
        }

        if (!haveEngineTarget && m_ownership == Backbuffer::Ownership::Unclassified) {
            Backbuffer::Observations o;
            o.haveSwapChain = swapChain != nullptr;
            o.userOverride = m_userOverride;
            o.frameGenModuleLoaded = ConflictChecker::AnyKnownFrameGenModuleLoaded();

            const auto gb = ConflictChecker::InspectGetBufferOwner(swapChain);
            o.vtableOwnerIsKnownFrameGen = gb.owner == ConflictChecker::GetBufferOwnerClass::KnownFrameGen;
            o.getBufferOwnerForeign = gb.owner == ConflictChecker::GetBufferOwnerClass::Foreign;
            m_classifiedGetBufferFn = gb.getBufferFn;
            if (swapChain) {
                DXGI_SWAP_CHAIN_DESC desc{};
                if (SUCCEEDED(swapChain->GetDesc(&desc))) {
                    o.swapChainDescWidth = desc.BufferDesc.Width;
                    o.swapChainDescHeight = desc.BufferDesc.Height;
                    RECT rc{};
                    if (desc.OutputWindow && GetClientRect(desc.OutputWindow, &rc) && rc.right > rc.left &&
                        rc.bottom > rc.top) {

                        o.haveEngineBackbufferSize = true;
                        o.engineBackbufferWidth = static_cast<std::uint32_t>(rc.right - rc.left);
                        o.engineBackbufferHeight = static_cast<std::uint32_t>(rc.bottom - rc.top);
                    }
                }
            }
            const auto classified = Backbuffer::ClassifyBackbufferOwnership(o);
            m_presentationMode = Backbuffer::ResolveBackbufferPresentationMode(classified.ownership, o);
            m_backbufferBorrowed = m_presentationMode == Backbuffer::PresentationMode::Borrowed;

            const bool retryNextFrame = m_presentationMode == Backbuffer::PresentationMode::SafeSkip &&
                                        classified.ownership == Backbuffer::Ownership::Unknown &&
                                        !o.haveEngineBackbufferSize;
            m_ownership = retryNextFrame ? Backbuffer::Ownership::Unclassified : classified.ownership;

            if (retryNextFrame) {
                static std::atomic<int64_t> s_lastRetryLogMs{0};
                const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count();
                if (now - s_lastRetryLogMs.load() > 1000) {
                    s_lastRetryLogMs.store(now);
                    logger::warn(
                        "[WebCompositor] backbuffer policy defer: Unknown with no client size yet "
                        "-- next frame re-asks (no GetBuffer this frame)");
                }
            } else {
                logger::info(
                    "[WebCompositor] backbuffer policy ownership={} reason={} mode={} "
                    "swapchain={}x{} client={}x{} (client size is window pixels, not a GetBuffer)",
                    Backbuffer::ToString(classified.ownership), Backbuffer::ToString(classified.reason),
                    Backbuffer::ToString(m_presentationMode), o.swapChainDescWidth, o.swapChainDescHeight,
                    o.engineBackbufferWidth, o.engineBackbufferHeight);
                if (m_presentationMode == Backbuffer::PresentationMode::LegacyOwned) {
                    logger::warn(
                        "[WebCompositor] LegacyOwned: GetBuffer+Release on an UNPROVEN chain "
                        "(FG loaded, GetBuffer dxgi-owned, client size agrees with swapchain desc; a "
                        "genuine foreign GetBuffer owner would SafeSkip instead). Override with "
                        "[Compatibility] iBackbufferOwnership=2 if the UI dies across resizes");
                }
            }
        }

        if (!haveEngineTarget && m_ownership != Backbuffer::Ownership::Unclassified && m_classifiedGetBufferFn) {
            void* live = ConflictChecker::CurrentGetBufferFn(swapChain);
            if (live != m_classifiedGetBufferFn) {
                logger::warn(
                    "[WebCompositor] GetBuffer dispatch changed since classification (classified={} "
                    "live={}) -- dropping verdict, reclassifying next frame",
                    m_classifiedGetBufferFn, live);
                InvalidateRenderTarget();
                m_swapChainIdentity = nullptr;
                return CompositeResult::SafeSkip;
            }
        }

        if (!haveEngineTarget && m_presentationMode == Backbuffer::PresentationMode::SafeSkip) {
            return CompositeResult::SafeSkip;
        }

        if (!haveEngineTarget && !m_rtv) {

            void* live = ConflictChecker::CurrentGetBufferFn(swapChain);
            if (live != m_classifiedGetBufferFn) {
                logger::warn(
                    "[WebCompositor] GetBuffer dispatch changed since classification (classified={} "
                    "live={}) -- skipping this frame and reclassifying next",
                    m_classifiedGetBufferFn, live);
                InvalidateRenderTarget();
                m_swapChainIdentity = nullptr;
                return CompositeResult::SafeSkip;
            }
            Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
            if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&backbuffer))) || !backbuffer) {
                logger::error("[WebCompositor] swapChain->GetBuffer(0) failed -- overlay not drawn this frame");
                return CompositeResult::GetBufferFailed;
            }

            ID3D11Texture2D* bb = m_backbufferBorrowed ? backbuffer.Detach() : backbuffer.Get();
            if (FAILED(m_device->CreateRenderTargetView(bb, nullptr, &m_rtv))) {
                logger::error("[WebCompositor] CreateRenderTargetView(backbuffer) failed -- overlay not drawn");
                m_rtv.Reset();
                return CompositeResult::CreateRtvFailed;
            }
            bb->GetDesc(&m_bbDesc);
            logger::info("[WebCompositor] backbuffer RTV (re)built: {}x{} ({})", m_bbDesc.Width, m_bbDesc.Height,
                         ReleasePathLabel(m_presentationMode));
        }

        const D3D11_TEXTURE2D_DESC bbDesc = haveEngineTarget ? engineDesc : m_bbDesc;
        if (bbDesc.Width == 0 || bbDesc.Height == 0) return CompositeResult::InvalidBackbuffer;

        ScopedD3D11State stateGuard(m_context);

        ID3D11RenderTargetView* rtv = haveEngineTarget ? engineRtv : m_rtv.Get();
        m_context->OMSetRenderTargets(1, &rtv, nullptr);

        D3D11_VIEWPORT vp{};
        vp.Width = static_cast<FLOAT>(bbDesc.Width);
        vp.Height = static_cast<FLOAT>(bbDesc.Height);
        vp.MaxDepth = 1.0f;
        m_context->RSSetViewports(1, &vp);

        const float blendFactor[4] = {0, 0, 0, 0};
        m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xFFFFFFFF);
        m_context->IASetInputLayout(nullptr);
        m_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->VSSetShader(m_vs.Get(), nullptr, 0);
        m_context->PSSetShader(m_ps.Get(), nullptr, 0);
        m_context->PSSetShaderResources(0, 1, &srv);
        ID3D11SamplerState* sampler = m_sampler.Get();
        m_context->PSSetSamplers(0, 1, &sampler);
        const float sourceUv[] = {target.uvLeft, target.uvTop, target.uvRight, target.uvBottom};
        m_context->UpdateSubresource(m_sourceRegion.Get(), 0, nullptr, sourceUv, 0, 0);
        ID3D11Buffer* sourceRegion = m_sourceRegion.Get();
        m_context->PSSetConstantBuffers(0, 1, &sourceRegion);

        m_context->Draw(3, 0);

        DrawModelOverlays(overlays);

        DrawCursor(cursorX, cursorY, drawCursor);

        return CompositeResult::Success;
    }

}
