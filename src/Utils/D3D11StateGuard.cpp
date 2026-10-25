#include "PCH.h"

#include "Utils/D3D11StateGuard.h"

namespace PrismaUI
{
    ScopedD3D11State::ScopedD3D11State(
        ID3D11DeviceContext* context) noexcept :
        context_(context)
    {
        if (!context_) {
            return;
        }

        context_->OMGetRenderTargets(
            static_cast<UINT>(renderTargets_.size()),
            renderTargets_.data(),
            &depthStencil_);

        context_->IAGetInputLayout(
            inputLayout_.GetAddressOf());
        context_->IAGetPrimitiveTopology(&topology_);
        context_->IAGetVertexBuffers(
            0,
            1,
            vertexBuffer_.GetAddressOf(),
            &vertexStride_,
            &vertexOffset_);
        context_->IAGetIndexBuffer(
            indexBuffer_.GetAddressOf(),
            &indexFormat_,
            &indexOffset_);

        context_->VSGetShader(
            vertexShader_.GetAddressOf(), nullptr, nullptr);
        context_->PSGetShader(
            pixelShader_.GetAddressOf(), nullptr, nullptr);
        context_->GSGetShader(
            geometryShader_.GetAddressOf(), nullptr, nullptr);
        context_->HSGetShader(
            hullShader_.GetAddressOf(), nullptr, nullptr);
        context_->DSGetShader(
            domainShader_.GetAddressOf(), nullptr, nullptr);

        context_->VSGetConstantBuffers(
            0,
            1,
            vertexConstantBuffer_.GetAddressOf());
        context_->PSGetConstantBuffers(
            0,
            1,
            pixelConstantBuffer_.GetAddressOf());
        context_->PSGetShaderResources(
            0,
            kShaderResourceSlots,
            pixelResources_.data());
        context_->PSGetSamplers(
            0,
            1,
            pixelSampler_.GetAddressOf());

        context_->OMGetBlendState(
            blendState_.GetAddressOf(),
            blendFactor_,
            &sampleMask_);
        context_->OMGetDepthStencilState(
            depthState_.GetAddressOf(),
            &stencilReference_);
        context_->RSGetState(rasterState_.GetAddressOf());

        viewportCount_ =
            static_cast<UINT>(viewports_.size());
        context_->RSGetViewports(
            &viewportCount_,
            viewports_.data());
        scissorCount_ =
            static_cast<UINT>(scissors_.size());
        context_->RSGetScissorRects(
            &scissorCount_,
            scissors_.data());
        context_->GetPredication(
            predicate_.GetAddressOf(),
            &predicateValue_);
    }

    ScopedD3D11State::~ScopedD3D11State() noexcept
    {
        if (!context_) {
            return;
        }

        context_->OMSetRenderTargets(
            static_cast<UINT>(renderTargets_.size()),
            renderTargets_.data(),
            depthStencil_);
        context_->IASetInputLayout(inputLayout_.Get());
        context_->IASetPrimitiveTopology(topology_);
        auto* vertexBuffer = vertexBuffer_.Get();
        context_->IASetVertexBuffers(
            0,
            1,
            &vertexBuffer,
            &vertexStride_,
            &vertexOffset_);
        context_->IASetIndexBuffer(
            indexBuffer_.Get(),
            indexFormat_,
            indexOffset_);

        context_->VSSetShader(
            vertexShader_.Get(), nullptr, 0);
        context_->PSSetShader(
            pixelShader_.Get(), nullptr, 0);
        context_->GSSetShader(
            geometryShader_.Get(), nullptr, 0);
        context_->HSSetShader(
            hullShader_.Get(), nullptr, 0);
        context_->DSSetShader(
            domainShader_.Get(), nullptr, 0);

        auto* vertexConstant =
            vertexConstantBuffer_.Get();
        context_->VSSetConstantBuffers(
            0,
            1,
            &vertexConstant);
        auto* pixelConstant =
            pixelConstantBuffer_.Get();
        context_->PSSetConstantBuffers(
            0,
            1,
            &pixelConstant);
        context_->PSSetShaderResources(
            0,
            kShaderResourceSlots,
            pixelResources_.data());
        auto* sampler = pixelSampler_.Get();
        context_->PSSetSamplers(0, 1, &sampler);

        context_->OMSetBlendState(
            blendState_.Get(),
            blendFactor_,
            sampleMask_);
        context_->OMSetDepthStencilState(
            depthState_.Get(),
            stencilReference_);
        context_->RSSetState(rasterState_.Get());
        context_->RSSetViewports(
            viewportCount_,
            viewports_.data());
        context_->RSSetScissorRects(
            scissorCount_,
            scissors_.data());
        context_->SetPredication(
            predicate_.Get(),
            predicateValue_);

        for (auto*& renderTarget : renderTargets_) {
            if (renderTarget) {
                renderTarget->Release();
                renderTarget = nullptr;
            }
        }
        if (depthStencil_) {
            depthStencil_->Release();
            depthStencil_ = nullptr;
        }
        for (auto*& resource : pixelResources_) {
            if (resource) {
                resource->Release();
                resource = nullptr;
            }
        }
    }
}
