#include "FalloutGPUDriver.h"

#include <d3dcompiler.h>

#include <cstring>

#include "UltralightGPUUniforms.h"
#include "UltralightPipelineLayout.h"
#include "UltralightShaders.h"

namespace PrismaUI::GPU {

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
            Microsoft::WRL::ComPtr<ID3DBlob> code;
            Microsoft::WRL::ComPtr<ID3DBlob> errors;
            if (FAILED(D3DCompile(src, len, nullptr, nullptr, nullptr, entry, target, kUltralightShaderCompileFlags, 0,
                                  code.GetAddressOf(), errors.GetAddressOf()))) {
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

        if (FAILED(device->CreateInputLayout(kFillPathLayout, ARRAYSIZE(kFillPathLayout),
                                             fillPathVsBlob->GetBufferPointer(), fillPathVsBlob->GetBufferSize(),
                                             fillPathLayout_.ReleaseAndGetAddressOf()))) {
            return false;
        }

        if (FAILED(device->CreateInputLayout(kFillLayout, ARRAYSIZE(kFillLayout), fillVsBlob->GetBufferPointer(),
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

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FalloutGPUDriver::AcquireExternalTextureSRV(
        std::uint32_t textureId) {
        auto srv = externalTextures_->Acquire(textureId);
        if (srv) {
            Microsoft::WRL::ComPtr<ID3D11Device> owner;
            srv->GetDevice(owner.GetAddressOf());
            if (owner.Get() == context_->Device()) return srv;
        }
        if (staleExternalTextureIds_.insert(textureId).second) {
            logger::warn("[GPU] external texture {} is unbound or on another device; its slot samples nothing",
                         textureId);
        }
        return {};
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

            if (state.texture_1_id && !textures_.contains(state.texture_1_id) && externalTextures_ &&
                externalTextures_->Contains(state.texture_1_id)) {
                srv0 = AcquireExternalTextureSRV(state.texture_1_id);
            } else if (state.texture_1_id) {
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
            if (state.texture_2_id && !textures_.contains(state.texture_2_id) && externalTextures_ &&
                externalTextures_->Contains(state.texture_2_id)) {
                srv1 = AcquireExternalTextureSRV(state.texture_2_id);
            } else if (state.texture_2_id) {
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
