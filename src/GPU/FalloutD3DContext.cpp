#include "FalloutD3DContext.h"

#ifndef PRISMAUI_FO4VR
#include "Hooks/Hooks.h"
#endif
#include "Utils/ConflictChecker.h"

#include <string>
#include <utility>

#include <atomic>

namespace PrismaUI::GPU {

namespace {

constexpr DWORD kModulePathCapacity = 32768;

std::wstring ModulePath(HMODULE module) noexcept
{
    if (!module) return {};
    std::wstring path(kModulePathCapacity, L'\0');
    const auto length = ::GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || static_cast<std::size_t>(length) >= path.size()) return {};
    path.resize(length);
    return path;
}

std::wstring SystemD3D11Path() noexcept
{
    std::wstring path(kModulePathCapacity, L'\0');
    const auto length = ::GetSystemDirectoryW(path.data(), static_cast<UINT>(path.size()));
    if (!length || static_cast<std::size_t>(length) >= path.size()) return {};
    path.resize(length);
    if (!path.empty() && path.back() != L'\\') path.push_back(L'\\');
    path.append(L"d3d11.dll");
    return path;
}

bool PathsEqualInsensitive(const std::wstring& lhs, const std::wstring& rhs) noexcept
{
    if (lhs.empty() || rhs.empty()) return false;
    return ::CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_EQUAL;
}

std::string NarrowPath(const std::wstring& value)
{
    if (value.empty()) return "<unavailable>";
    const auto size = ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0,
                                            nullptr, nullptr);
    if (size <= 0) return "<unavailable>";
    std::string result(static_cast<std::size_t>(size), '\0');
    if (::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr,
                              nullptr) <= 0)
        return "<unavailable>";
    return result;
}

HMODULE DispatchModule(IUnknown* object) noexcept
{
    if (!object) return nullptr;
    auto** vtable = *reinterpret_cast<void***>(object);
    if (!vtable || !vtable[0]) return nullptr;
    HMODULE module = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(vtable[0]), &module))
        return nullptr;
    return module;
}

bool BelongsToDevice(ID3D11DeviceChild* child, ID3D11Device* expected) noexcept
{
    if (!child || !expected) return false;
    Microsoft::WRL::ComPtr<ID3D11Device> owner;
    child->GetDevice(owner.GetAddressOf());
    return owner && owner.Get() == expected;
}

bool KnownEnbSwapchainChain() noexcept
{
#ifdef PRISMAUI_FO4VR
    return false;
#else
    auto* rendererData = RE::BSGraphics::GetRendererData();
    auto* swapChain = rendererData ? rendererData->renderWindow[0].swapChain : nullptr;
    if (!swapChain) return false;
    auto** dispatch = *reinterpret_cast<void***>(swapChain);
    if (!dispatch) return false;
    void* present = dispatch[8];
    void* resize = dispatch[13];
    const bool livePresentIsKnown = PrismaUI::ConflictChecker::IsKnownEnbProxyAddress(present);
    const bool liveResizeIsKnown = PrismaUI::ConflictChecker::IsKnownEnbProxyAddress(resize);
    const auto isPresentDetour = [](void* target) {
        return target == reinterpret_cast<void*>(&::Hooks::D3DHooks::HookPresentA) ||
               target == reinterpret_cast<void*>(&::Hooks::D3DHooks::HookPresentB);
    };
    const auto isResizeDetour = [](void* target) {
        return target == reinterpret_cast<void*>(&::Hooks::D3DHooks::HookResizeBuffersA) ||
               target == reinterpret_cast<void*>(&::Hooks::D3DHooks::HookResizeBuffersB);
    };
    const void* recordedPresent = ::Hooks::D3DHooks::s_hookedPresentTarget.load(std::memory_order_acquire);
    const void* recordedResize = ::Hooks::D3DHooks::s_hookedResizeTarget.load(std::memory_order_acquire);
    const bool prismaOwnsCurrentChain = ::Hooks::D3DHooks::OwnsActiveSwapchainChain(swapChain);
    const PrismaUI::GPU::KnownInterposerSwapchainEvidence evidence{
        livePresentIsKnown,
        liveResizeIsKnown,
        isPresentDetour(present),
        isResizeDetour(resize),
        PrismaUI::ConflictChecker::IsKnownEnbProxyAddress(const_cast<void*>(recordedPresent)),
        PrismaUI::ConflictChecker::IsKnownEnbProxyAddress(const_cast<void*>(recordedResize)),
        prismaOwnsCurrentChain,
        livePresentIsKnown && liveResizeIsKnown &&
            PrismaUI::ConflictChecker::SameModuleIdentity(present, resize),
        PrismaUI::ConflictChecker::SameModuleIdentity(const_cast<void*>(recordedPresent),
                                                       const_cast<void*>(recordedResize))};
    return PrismaUI::GPU::EvaluateKnownInterposerSwapchainCoherence(evidence);
#endif
}

}

bool FalloutD3DContext::IsD3D11ProxyLoaded() noexcept
{
    const auto module = ::GetModuleHandleW(L"d3d11.dll");
    const auto loadedPath = ModulePath(module);
    const auto systemPath = SystemD3D11Path();
    return loadedPath.empty() || systemPath.empty() || !PathsEqualInsensitive(loadedPath, systemPath);
}

bool FalloutD3DContext::Initialize(ID3D11Device* device, ID3D11DeviceContext* immediateContext,
                                   bool allowControlledProxyTest)
{
    Reset();
    controlledProxyTest_ = allowControlledProxyTest;
    suppliedDevice_ = device;
    immediateContext_ = immediateContext;
    return PrepareImmediateDevice(immediateContext);
}

bool FalloutD3DContext::PrepareImmediateDevice(ID3D11DeviceContext* current) noexcept
{
    immediateContext_ = current;

    Microsoft::WRL::ComPtr<ID3D11Device> actualDevice;
    if (current) current->GetDevice(actualDevice.GetAddressOf());
    device_ = actualDevice;

    const auto loadedD3D11Module = ::GetModuleHandleW(L"d3d11.dll");
    const auto loadedD3D11Path = ModulePath(loadedD3D11Module);
    const auto systemD3D11Path = SystemD3D11Path();
    const bool moduleEvidenceKnown = !loadedD3D11Path.empty() && !systemD3D11Path.empty();
    const bool proxyModuleDetected = moduleEvidenceKnown && !PathsEqualInsensitive(loadedD3D11Path, systemD3D11Path);

    const auto deviceDispatchModule = DispatchModule(actualDevice.Get());
    const auto contextDispatchModule = DispatchModule(current);
    const auto deviceDispatchPath = ModulePath(deviceDispatchModule);
    const auto contextDispatchPath = ModulePath(contextDispatchModule);

    AcceleratedProxyOwnershipEvidence evidence;
    evidence.canonicalDevicePresent = actualDevice.Get() != nullptr;
    evidence.immediateContextPresent = current != nullptr;
    evidence.immediateContextType = current && current->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
    evidence.contextDeviceMatchesCanonical = actualDevice.Get() != nullptr;
    evidence.d3d11ModuleEvidenceKnown = moduleEvidenceKnown;
    evidence.proxyModuleDetected = proxyModuleDetected;
    evidence.deviceDispatchKnown = deviceDispatchModule != nullptr;
    evidence.contextDispatchKnown = contextDispatchModule != nullptr;
    evidence.dispatchModulesAgree = deviceDispatchModule && contextDispatchModule &&
                                    deviceDispatchModule == contextDispatchModule;
    evidence.deviceDispatchMatchesRuntime = deviceDispatchModule && deviceDispatchModule == loadedD3D11Module;
    evidence.contextDispatchMatchesRuntime = contextDispatchModule && contextDispatchModule == loadedD3D11Module;
    evidence.controlledProxyTest = controlledProxyTest_;
    evidence.knownInterposerChain = KnownEnbSwapchainChain();
    evidence.deviceDispatchIsSystem = PathsEqualInsensitive(deviceDispatchPath, systemD3D11Path);
    evidence.contextDispatchIsKnownInterposer =
        PrismaUI::ConflictChecker::IsKnownEnbProxyModule(reinterpret_cast<void*>(contextDispatchModule));
    evidence.swapchainOwnershipCoherent = evidence.knownInterposerChain;
    ownershipResult_ = EvaluateAcceleratedProxyOwnership(evidence);

    const bool diagnosticChanged =
        !ownershipDiagnosticLogged_ || lastLoggedCanonicalDevice_ != actualDevice.Get() || lastLoggedContext_ != current ||
        lastLoggedD3D11Module_ != loadedD3D11Module || lastLoggedDeviceDispatchModule_ != deviceDispatchModule ||
        lastLoggedContextDispatchModule_ != contextDispatchModule ||
        lastLoggedKnownInterposerChain_ != evidence.knownInterposerChain ||
        lastLoggedDeviceDispatchIsSystem_ != evidence.deviceDispatchIsSystem ||
        lastLoggedContextDispatchIsKnownInterposer_ != evidence.contextDispatchIsKnownInterposer ||
        lastLoggedOwnershipClass_ != ownershipResult_.classification ||
        lastLoggedEligibilityReason_ != ownershipResult_.reason;

    if (diagnosticChanged) {
        const auto supplied = suppliedDevice_.Get();
        const bool suppliedMatchesCanonical = !supplied || supplied == actualDevice.Get();
        const auto flags = actualDevice ? actualDevice->GetCreationFlags() : 0u;
        const auto contextType = current ? static_cast<unsigned>(current->GetType()) : 0xFFFFFFFFu;
        const auto ownershipClass = AcceleratedProxyOwnershipClassName(ownershipResult_.classification);
        const auto reason = AcceleratedProxyEligibilityReasonName(ownershipResult_.reason);
        const auto loadedPathUtf8 = NarrowPath(loadedD3D11Path);
        const auto systemPathUtf8 = NarrowPath(systemD3D11Path);
        const auto deviceDispatchUtf8 = NarrowPath(deviceDispatchPath);
        const auto contextDispatchUtf8 = NarrowPath(contextDispatchPath);

        if (ownershipResult_.eligible) {
            logger::info(
                "[WebRuntime] [GPU] accelerated-v2 ownership: class={} eligible=true reason={} "
                "d3d11='{}' systemD3D11='{}' supplied={:#x} canonical={:#x} context={:#x} "
                "suppliedMatchesCanonical={} deviceDispatch='{}' contextDispatch='{}' "
                "deviceDispatchMatchesRuntime={} contextDispatchMatchesRuntime={} knownInterposerChain={} "
                "deviceDispatchIsSystem={} contextDispatchIsKnownInterposer={} flags={:#x} contextType={}",
                ownershipClass, reason, loadedPathUtf8, systemPathUtf8,
                reinterpret_cast<std::uintptr_t>(supplied), reinterpret_cast<std::uintptr_t>(actualDevice.Get()),
                reinterpret_cast<std::uintptr_t>(current), suppliedMatchesCanonical, deviceDispatchUtf8,
                contextDispatchUtf8, evidence.deviceDispatchMatchesRuntime, evidence.contextDispatchMatchesRuntime,
                evidence.knownInterposerChain, evidence.deviceDispatchIsSystem,
                evidence.contextDispatchIsKnownInterposer, flags, contextType);
        } else {
            logger::warn(
                "[WebRuntime] [GPU] accelerated-v2 ownership rejected: class={} reason={} "
                "d3d11='{}' systemD3D11='{}' supplied={:#x} canonical={:#x} context={:#x} "
                "suppliedMatchesCanonical={} deviceDispatch='{}' contextDispatch='{}' "
                "deviceDispatchMatchesRuntime={} contextDispatchMatchesRuntime={} knownInterposerChain={} "
                "deviceDispatchIsSystem={} contextDispatchIsKnownInterposer={} flags={:#x} contextType={}",
                ownershipClass, reason, loadedPathUtf8, systemPathUtf8,
                reinterpret_cast<std::uintptr_t>(supplied), reinterpret_cast<std::uintptr_t>(actualDevice.Get()),
                reinterpret_cast<std::uintptr_t>(current), suppliedMatchesCanonical, deviceDispatchUtf8,
                contextDispatchUtf8, evidence.deviceDispatchMatchesRuntime, evidence.contextDispatchMatchesRuntime,
                evidence.knownInterposerChain, evidence.deviceDispatchIsSystem,
                evidence.contextDispatchIsKnownInterposer, flags, contextType);
        }

        ownershipDiagnosticLogged_ = true;
        lastLoggedCanonicalDevice_ = actualDevice.Get();
        lastLoggedContext_ = current;
        lastLoggedD3D11Module_ = loadedD3D11Module;
        lastLoggedDeviceDispatchModule_ = deviceDispatchModule;
        lastLoggedContextDispatchModule_ = contextDispatchModule;
        lastLoggedKnownInterposerChain_ = evidence.knownInterposerChain;
        lastLoggedDeviceDispatchIsSystem_ = evidence.deviceDispatchIsSystem;
        lastLoggedContextDispatchIsKnownInterposer_ = evidence.contextDispatchIsKnownInterposer;
        lastLoggedOwnershipClass_ = ownershipResult_.classification;
        lastLoggedEligibilityReason_ = ownershipResult_.reason;
    }

    return ownershipResult_.eligible;
}

void FalloutD3DContext::ResetStates() noexcept
{
    sampler_.Reset();
    scissoredRasterizer_.Reset();
    rasterizer_.Reset();
    blendDisabled_.Reset();
    blendEnabled_.Reset();
}

void FalloutD3DContext::Reset()
{
    ResetStates();
    immediateContext_.Reset();
    suppliedDevice_.Reset();
    device_.Reset();
    ownershipResult_ = {};
    controlledProxyTest_ = false;
    ownershipDiagnosticLogged_ = false;
    lastLoggedCanonicalDevice_ = nullptr;
    lastLoggedContext_ = nullptr;
    lastLoggedD3D11Module_ = nullptr;
    lastLoggedDeviceDispatchModule_ = nullptr;
    lastLoggedContextDispatchModule_ = nullptr;
    lastLoggedOwnershipClass_ = AcceleratedProxyOwnershipClass::UnknownProxyChain;
    lastLoggedEligibilityReason_ = AcceleratedProxyEligibilityReason::D3D11ModuleEvidenceUnavailable;
}

bool FalloutD3DContext::EnsureStates()
{
    if (!Ready()) return false;
    if (blendEnabled_ && blendDisabled_ && rasterizer_ && scissoredRasterizer_ && sampler_) return true;

    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(device_->CreateBlendState(&blend, blendEnabled_.ReleaseAndGetAddressOf())) ||
        !BelongsToDevice(blendEnabled_.Get(), device_.Get())) {
        ResetStates();
        return false;
    }

    blend.RenderTarget[0].BlendEnable = FALSE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    if (FAILED(device_->CreateBlendState(&blend, blendDisabled_.ReleaseAndGetAddressOf())) ||
        !BelongsToDevice(blendDisabled_.Get(), device_.Get())) {
        ResetStates();
        return false;
    }

    D3D11_RASTERIZER_DESC raster{};
    raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE;
    raster.FrontCounterClockwise = FALSE;

    raster.DepthClipEnable = FALSE;
    raster.ScissorEnable = FALSE;
    if (FAILED(device_->CreateRasterizerState(&raster, rasterizer_.ReleaseAndGetAddressOf())) ||
        !BelongsToDevice(rasterizer_.Get(), device_.Get())) {
        ResetStates();
        return false;
    }

    raster.ScissorEnable = TRUE;
    if (FAILED(device_->CreateRasterizerState(&raster, scissoredRasterizer_.ReleaseAndGetAddressOf())) ||
        !BelongsToDevice(scissoredRasterizer_.Get(), device_.Get())) {
        ResetStates();
        return false;
    }

    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler.MinLOD = 0.0F;

    sampler.MaxLOD = 0.0F;
    if (FAILED(device_->CreateSamplerState(&sampler, sampler_.ReleaseAndGetAddressOf())) ||
        !BelongsToDevice(sampler_.Get(), device_.Get())) {
        ResetStates();
        return false;
    }

    return true;
}

bool FalloutD3DContext::BindBlend(bool enabled)
{
    if (!EnsureStates() || !immediateContext_) return false;
    ID3D11BlendState* state = enabled ? blendEnabled_.Get() : blendDisabled_.Get();
    constexpr float factors[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    immediateContext_->OMSetBlendState(state, factors, 0xFFFFFFFFu);
    return true;
}

bool FalloutD3DContext::BindScissor(bool enabled)
{
    if (!EnsureStates() || !immediateContext_) return false;
    immediateContext_->RSSetState(enabled ? scissoredRasterizer_.Get() : rasterizer_.Get());
    return true;
}

}
