#!/usr/bin/env python3
"""Accelerated-v2 proxy/borrowed-device ownership invariants."""

from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening, semicolon = source.find("{", start), source.find(";", start)
    while 0 <= semicolon < opening:
        start = source.index(signature, start + len(signature))
        opening, semicolon = source.find("{", start), source.find(";", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unclosed function: {signature}")


policy = read("src/GPU/AcceleratedProxyOwnershipPolicy.h")
context = read("src/GPU/FalloutD3DContext.cpp")
context_h = read("src/GPU/FalloutD3DContext.h")
identity = read("src/Utils/EnbProxyIdentity.cpp")
width = read("src/Utils/SwapchainWidthPolicy.cpp")
width_policy = read("src/Utils/SwapchainWidthPolicy.h")
driver = "".join(path.read_text(encoding="utf-8") for path in sorted(Path("src/GPU").glob("FalloutGPUDriver*.cpp")))
lifecycle = read("src/GPU/AcceleratedDeviceLifecycle.h")
hooks = read("src/Hooks/Hooks.cpp") + "\n" + read("src/Hooks/HooksLifecycle.inl")
hooks_lifecycle = read("src/Hooks/HooksLifecycle.inl")
backend = read("src/PrismaUI/UltralightBackend.cpp")

gpu_sources = "\n".join(
    path.read_text(encoding="utf-8", errors="ignore")
    for path in sorted(Path("src/GPU").glob("*"))
    if path.suffix in {".cpp", ".h"}
)

# One policy owns the proxy verdict. A module-path check is retained for diagnostics, but the old
# boolean bypass must never again be the execution boundary.
assert "EvaluateAcceleratedProxyOwnership" in policy
assert "ProxyRequiresRuntimeAcceptance" in policy
assert "EligibleControlledProxyTest" in policy
assert "if (!allowProxiedDevice && IsD3D11ProxyLoaded()) return false;" not in context
assert "ownershipResult_ = EvaluateAcceleratedProxyOwnership(evidence);" in context
assert "return ownershipResult_.eligible;" in context

# Immediate-context ownership is authoritative. The supplied device remains diagnostic-only.
assert "current->GetDevice(actualDevice.GetAddressOf());" in context
assert "current->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE" in context
assert "suppliedDevice_ = device;" in context
assert "device_ = device;" not in context
assert "OwnershipAllowsExecution" in context_h

# Module presence is evidence, not ownership. Proxy testing additionally requires resolvable,
# coherent device/context dispatch provenance.
assert "GetModuleHandleExW" in context
assert "deviceDispatchModule == contextDispatchModule" in context
assert "deviceDispatchMatchesRuntime" in context
assert "contextDispatchMatchesRuntime" in context
assert "deviceDispatchMatchesRuntime" in policy
assert "contextDispatchMatchesRuntime" in policy
assert "d3d11ModuleEvidenceKnown" in policy
assert "ProxyDispatchEvidenceUnavailable" in policy
assert "DispatchOwnershipInconsistent" in policy

assert "GetFileVersionInfoW" in identity
assert "IsKnownEnbProxyModule" in identity and "IsKnownEnbProxyAddress" in identity
assert "SameModuleIdentity" in identity
assert "ProductName" not in identity
assert "ContainsInsensitive(value, L\"ENBSeries\") && ContainsInsensitive(value, L\"Fallout 4\")" in identity
assert "IsKnownBaseOnlyFrameGenAddress" in width
assert "return 18;" in width_policy
assert "return 0;" in width_policy
width_body = function_body(width, "unsigned int DetermineShadowSlotCount")
assert width_body.index("ProbeInterface") < width_body.index("SelectShadowSlotCount")
assert "SwapchainInterfaceProbe::AliasesBase" in width
assert "SwapchainInterfaceProbe::DistinctInterface" in width
assert "SwapchainInterfaceProbe::NotSupported" in width
assert "unrecognizedProxy" in width_policy
assert "SwapchainInterfaceProbe::AliasesBase" in width_policy
assert "SwapchainInterfaceProbe::Failed" in width_policy
assert "EvaluateKnownInterposerSwapchainCoherence" in policy
assert "EvaluateKnownInterposerSwapchainCoherence" in context
assert "OwnsActiveSwapchainChain" in context
assert "OwnsActiveSwapchainChain" in hooks_lifecycle
assert "\n    }\n\n    HRESULT APIENTRY D3DHooks::HookResizeBuffersA" in hooks
assert "ChainOwnsDispatch" in hooks_lifecycle
active_chain = function_body(hooks_lifecycle, "bool D3DHooks::OwnsActiveSwapchainChain")
assert "SwapchainChain" in active_chain and "s_activePresentSlot" in active_chain and \
    "s_activeResizeSlot" in active_chain and "ChainOwnsDispatch" in active_chain
assert "ownerAllowed = EqualsIgnoreCase(owner, \"dxgi.dll\") || IsFrameGenModule(target);" in read(
    "src/Utils/ConflictCheckerHookTarget.cpp"
)

# Prisma-owned context states prove device-child provenance before they can be bound.
for token in (
    "BelongsToDevice(blendEnabled_.Get(), device_.Get())",
    "BelongsToDevice(blendDisabled_.Get(), device_.Get())",
    "BelongsToDevice(rasterizer_.Get(), device_.Get())",
    "BelongsToDevice(scissoredRasterizer_.Get(), device_.Get())",
    "BelongsToDevice(sampler_.Get(), device_.Get())",
):
    assert token in context, f"missing D3D state provenance gate: {token}"

# The driver keeps the stronger device-child proof for textures, views, render targets, buffers,
# shaders, layouts, and render state. Do not weaken it to accommodate a proxy.
assert "child->GetDevice(owner.GetAddressOf());" in driver
assert "owner.Get() == context_->Device()" in driver
assert "provenance mismatch" in driver

# Swapchain identity stays owned by the device-lifecycle epoch. This slice consumes that contract;
# it does not replace it with a module-name guess.
assert "swapChain" in lifecycle
assert "inputs == inputs_" in lifecycle
assert "epoch == epoch_" in lifecycle

# Existing hook retirement remains fail-safe: foreign ownership is quarantined before MinHook is
# allowed to remove a retired slot. Proxy eligibility must not turn into hook reclamation.
retire = function_body(hooks, "RetirementResult RemoveRetiredSlot")
foreign = retire.index("if (!PrologueOwnedBy(target, expected))")
remove = retire.index("MH_DisableHook(target)")
assert foreign < remove
assert "MH_RemoveHook" not in retire, (
    "retirement must never hand a trampoline back to MinHook while the process runs"
)
assert "s_minHookOrphaned.store(true" in retire
assert "externally owned; leaving MinHook entry orphaned" in retire

# Accepted immediate-context architecture stays pinned.
for forbidden in ("CreateDeferredContext(", "RecordingContext", "FinishCommandList(", "ExecuteCommandList("):
    assert forbidden not in gpu_sources, f"retired D3D11 execution path returned: {forbidden}"

assert "const bool allowAccelerated = ::GetPrivateProfileIntW(" in backend
assert 'L"GPU", L"bAcceleratedRendering", 1, Utils::PluginIniPath()' in backend
assert "PRISMA_ACCELERATED_RUNTIME_ACCEPTANCE" in backend
assert "allowAccelerated && device_" in backend
assert "gpuContext_.Initialize(device_.Get(), immediateContext_.Get(), true)" in backend
assert 'L"bAccelerated"' not in backend

print("accelerated proxy ownership guard passed")
