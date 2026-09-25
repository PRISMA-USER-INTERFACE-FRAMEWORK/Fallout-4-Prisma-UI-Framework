#pragma once

#include "AcceleratedProxyOwnershipPolicy.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace PrismaUI::GPU {

class FalloutD3DContext {
public:
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* immediateContext,
                    bool allowControlledProxyTest = false);
    [[nodiscard]] static bool IsD3D11ProxyLoaded() noexcept;
    void Reset();
    [[nodiscard]] bool PrepareImmediateDevice(ID3D11DeviceContext* current) noexcept;
    [[nodiscard]] bool PrepareImmediateDevice() noexcept { return PrepareImmediateDevice(immediateContext_.Get()); }
    void ResetStates() noexcept;

    [[nodiscard]] bool Ready() const noexcept {
        return immediateContext_ && device_ && ownershipResult_.eligible;
    }
    [[nodiscard]] ID3D11Device* Device() const noexcept { return device_.Get(); }
    [[nodiscard]] ID3D11Device* SuppliedDevice() const noexcept { return suppliedDevice_.Get(); }
    [[nodiscard]] ID3D11DeviceContext* Context() const noexcept { return immediateContext_.Get(); }
    [[nodiscard]] const AcceleratedProxyOwnershipResult& OwnershipResult() const noexcept { return ownershipResult_; }
    [[nodiscard]] bool OwnershipAllowsExecution() const noexcept { return ownershipResult_.eligible; }
    bool EnsureStates();
    [[nodiscard]] bool BindBlend(bool enabled);
    [[nodiscard]] bool BindScissor(bool enabled);
    [[nodiscard]] ID3D11SamplerState* Sampler() const noexcept { return sampler_.Get(); }
    [[nodiscard]] ID3D11BlendState* BlendState(bool enabled) const noexcept {
        return enabled ? blendEnabled_.Get() : blendDisabled_.Get();
    }
    [[nodiscard]] ID3D11RasterizerState* RasterizerState(bool enabled) const noexcept {
        return enabled ? scissoredRasterizer_.Get() : rasterizer_.Get();
    }
    [[nodiscard]] ID3D11DepthStencilState* DepthStencilState() const noexcept { return nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11Device> suppliedDevice_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediateContext_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendEnabled_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendDisabled_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> scissoredRasterizer_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    AcceleratedProxyOwnershipResult ownershipResult_{};
    bool controlledProxyTest_ = false;
    bool ownershipDiagnosticLogged_ = false;
    ID3D11Device* lastLoggedCanonicalDevice_ = nullptr;
    ID3D11DeviceContext* lastLoggedContext_ = nullptr;
    HMODULE lastLoggedD3D11Module_ = nullptr;
    HMODULE lastLoggedDeviceDispatchModule_ = nullptr;
    HMODULE lastLoggedContextDispatchModule_ = nullptr;
    bool lastLoggedKnownInterposerChain_ = false;
    bool lastLoggedDeviceDispatchIsSystem_ = false;
    bool lastLoggedContextDispatchIsKnownInterposer_ = false;
    AcceleratedProxyOwnershipClass lastLoggedOwnershipClass_ = AcceleratedProxyOwnershipClass::UnknownProxyChain;
    AcceleratedProxyEligibilityReason lastLoggedEligibilityReason_ =
        AcceleratedProxyEligibilityReason::D3D11ModuleEvidenceUnavailable;
};

}
