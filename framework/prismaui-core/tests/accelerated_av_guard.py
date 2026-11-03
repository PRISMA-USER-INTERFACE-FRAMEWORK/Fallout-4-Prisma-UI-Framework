from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
GPU = "".join(path.read_text(encoding="utf-8") for path in sorted((ROOT / "src/GPU").glob("FalloutGPUDriver*.cpp")))
GPU_HEADER = (ROOT / "src/GPU/FalloutGPUDriver.h").read_text(encoding="utf-8")
FREEZE = (ROOT / "src/PrismaUI/FreezeDiagnostics.cpp").read_text(encoding="utf-8")
CONTEXT = (ROOT / "src/GPU/FalloutD3DContext.cpp").read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(text: str, needle: str, message: str) -> None:
    if needle not in text:
        fail(message)


def require_before(operation: str, call: str) -> None:
    call_at = GPU.find(call)
    if call_at < 0:
        fail(f"missing D3D call {call}")
    marker = f"MarkBreadcrumb(AcceleratedBreadcrumbOperation::{operation})"
    marker_at = GPU.rfind(marker, 0, call_at)
    if marker_at < 0:
        fail(f"{call} has no preceding {operation} breadcrumb")


for operation in (
    "MapConstantBuffer", "UnmapConstantBuffer", "ClearPixelShaderResources", "SetRenderTargets",
    "SetViewports", "SetPixelShaderResources", "SetVertexConstantBuffers", "SetPixelConstantBuffers",
    "SetVertexBuffers", "SetIndexBuffer", "SetPrimitiveTopology", "SetInputLayout", "SetSampler",
    "SetVertexShader", "SetPixelShader", "SetBlendState", "SetRasterizerState", "SetScissorRects",
    "DrawIndexed", "ResolveRenderBuffer", "CaptureImmediateState", "ExecuteGeneration",
    "DeviceFeatureLevel", "DeviceCreationFlags", "ImmediateContextType",
):
    require(GPU_HEADER, operation, f"missing accelerated breadcrumb operation {operation}")

for operation, call in (
    ("MapConstantBuffer", "drawContext->Map("), ("UnmapConstantBuffer", "drawContext->Unmap("),
    ("ClearPixelShaderResources", "drawContext->PSSetShaderResources(0, 3, nullSrvs)"),
    ("SetRenderTargets", "drawContext->OMSetRenderTargets("), ("SetViewports", "drawContext->RSSetViewports("),
    ("SetPixelShaderResources", "drawContext->PSSetShaderResources(0, 3, srvs)"),
    ("SetVertexConstantBuffers", "drawContext->VSSetConstantBuffers("),
    ("SetPixelConstantBuffers", "drawContext->PSSetConstantBuffers("),
    ("SetVertexBuffers", "drawContext->IASetVertexBuffers("), ("SetIndexBuffer", "drawContext->IASetIndexBuffer("),
    ("SetPrimitiveTopology", "drawContext->IASetPrimitiveTopology("), ("SetInputLayout", "drawContext->IASetInputLayout("),
    ("SetSampler", "drawContext->PSSetSamplers("), ("SetVertexShader", "drawContext->VSSetShader("),
    ("SetPixelShader", "drawContext->PSSetShader("), ("SetBlendState", "context_->BindBlend("),
    ("SetRasterizerState", "context_->BindScissor("), ("SetScissorRects", "drawContext->RSSetScissorRects("),
    ("DrawIndexed", "drawContext->DrawIndexed("), ("ResolveRenderBuffer", "immediate->ResolveSubresource("),
):
    require_before(operation, call)

for field in (
    "generation", "drawOrdinal", "geometryId", "renderBufferId", "texture0Id", "texture1Id",
    "indicesCount", "indicesOffset", "indexSize", "vertexFormat", "shaderType", "viewportWidth",
    "viewportHeight", "renderTarget", "vertexBuffer", "indexBuffer", "srv0", "srv1", "constantBuffer",
    "inputLayout", "vertexShader", "pixelShader", "sampler", "blendState", "depthStencilState",
    "rasterizerState", "borrowedDevice", "immediateContext",
    "suppliedDevice",
):
    require(GPU_HEADER, field, f"accelerated breadcrumb is missing {field}")

require(GPU, "child->GetDevice(owner.GetAddressOf())", "resource provenance checks must call GetDevice")
require(GPU, "activeBreadcrumb_.provenance = child ? 3u : 0u;",
        "GetDevice must be marked as in-progress before the COM call")
require(GPU, "provenanceRejected_ = true;", "device provenance mismatch must fail the draw closed")
require(GPU, "ProvenanceOperationName", "provenance rejection must name the failed resource")
require(GPU, "[GPU-AV] provenance mismatch:", "provenance rejection must log its captured breadcrumb")
require(GPU, "if (provenanceRejected_) {", "first provenance rejection must stop later draw recording")
require(GPU, 'SetOperationFailure("texture0-missing")',
        "draws must fail closed when the first nonzero texture ID is missing")
require(GPU, 'SetOperationFailure("texture1-missing")',
        "draws must fail closed when the second nonzero texture ID is missing")
require(GPU_HEADER, "LogicalRenderTargetSelfSample",
        "accelerated diagnostics must identify logical render-target self-sampling")
require(GPU, "state.texture_1_id == targetTextureId",
        "accelerated diagnostics must test the first source texture against the render target")
require(GPU, "state.texture_2_id == targetTextureId",
        "accelerated diagnostics must test the second source texture against the render target")
require(GPU, "[GPU] logical render-target self-sample:",
        "accelerated diagnostics must report a detected logical render-target self-sample")
require(GPU_HEADER, "multisampleTexture", "render buffers must keep a separate drawing surface")
require(GPU, "D3D11_RTV_DIMENSION_TEXTURE2DMS", "render buffers must draw into a multisample RTV")
require(GPU, "ResolveSubresource", "render buffers must resolve before publication or sampling")
require(GPU, "ResolveRenderBufferForTexture", "render-buffer sources must resolve before sampling")
require(GPU, "ResolveDirtyRenderBuffers", "dirty render buffers must resolve before generation completion")
require(GPU_HEADER, "destroyPending", "deferred texture retirement must track pending destruction")
require(GPU, "AcquirePresentationTextureSnapshot", "presentation needs a dependency-aware texture snapshot")
require(GPU, "retainedByRenderBuffer", "presentation must retain pending backing textures while referenced")
require(GPU, "texture->second.destroyPending = true", "texture-first destruction must defer while referenced")
require(GPU, "stillReferenced", "shared backing textures must wait for the last render-buffer dependency")
require(CONTEXT, "current->GetDevice(actualDevice.GetAddressOf());",
        "accelerated context must obtain the canonical device on the render thread")
require(CONTEXT, "device_ = actualDevice;",
        "the immediate-context device must become the canonical device")
if "device_ = device;" in CONTEXT:
    fail("owner-thread initialization must not make the supplied device authoritative")
require(CONTEXT, "IsD3D11ProxyLoaded", "accelerated context must retain proxied d3d11 diagnostics")
require(CONTEXT, "allowControlledProxyTest", "accelerated context must expose only an explicit controlled proxy test gate")
require(CONTEXT, "EvaluateAcceleratedProxyOwnership", "accelerated proxy eligibility must use the centralized ownership policy")
require(CONTEXT, "return ownershipResult_.eligible;", "accelerated ownership policy must gate immediate-context readiness")
require(GPU, "PrepareImmediateDevice", "generation drain must prepare the immediate-context device")
require(GPU, "capturedException_ = *pointers->ExceptionRecord;", "SEH filter must copy EXCEPTION_RECORD")
require(GPU, "capturedException_.ExceptionRecord = nullptr;", "copied exception record must clear chained pointer")
require(GPU, "capturedContext_ = *pointers->ContextRecord;", "SEH filter must copy CONTEXT")
require(GPU, "CaptureAccessViolation(GetExceptionInformation())", "SEH filter must capture while pointers are live")
require(GPU, "QueueAcceleratedAvDump(", "fault handler must queue a dump instead of writing inline")
require(GPU, "device->GetCreationFlags()", "accelerated evidence must record D3D11 creation flags")
require(FREEZE, "QueueAcceleratedAvDump", "watchdog must own queued accelerated AV dumps")
require(FREEZE, "PrismaAccelAV_", "accelerated dump needs a distinct diagnostic filename")
require(FREEZE, "MINIDUMP_EXCEPTION_INFORMATION", "accelerated dump must receive reconstructed exception pointers")
require(FREEZE, "g_pendingAcceleratedAvDump.state.compare_exchange_strong(ready, 3",
        "watchdog must claim the AV request before DbgHelp writes it")
require(FREEZE, "WriteAcceleratedAvDump();", "watchdog must be the accelerated dump writer")
require(FREEZE, "WriteMiniDump();", "watchdog must retain freeze dump support")

print("ok    accelerated AV breadcrumbs, copied SEH context, and serialized watchdog dump ownership")
