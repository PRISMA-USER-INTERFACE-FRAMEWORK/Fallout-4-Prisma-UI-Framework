from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
PIPELINE = (ROOT / "src/GPU/FalloutGPUDriverPipeline.cpp").read_text(encoding="utf-8")
RESOURCES = (ROOT / "src/GPU/FalloutGPUDriverResources.cpp").read_text(encoding="utf-8")
DRIVER = "".join(path.read_text(encoding="utf-8") for path in sorted((ROOT / "src/GPU").glob("FalloutGPUDriver*.cpp")))
BACKEND = (ROOT / "src/PrismaUI/UltralightBackend.cpp").read_text(encoding="utf-8")
REGISTRY = (ROOT / "src/GPU/ExternalTextureRegistry.h").read_text(encoding="utf-8")
NOOP = "if (externalTextures_ && externalTextures_->Contains(operation.id)) return true;"
COLLISION = "if (externalTextures_ && externalTextures_->Contains(operation.id)) {"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    if start == -1:
        fail(f"missing {signature}")
    end = source.find("\n    }\n", start)
    if end == -1:
        fail(f"unterminated {signature}")
    return source[start:end]


for texture, slot in (("texture_1_id", "srv0"), ("texture_2_id", "srv1")):
    branch_start = PIPELINE.find(
        f"if (state.{texture} && !textures_.contains(state.{texture}) && externalTextures_ &&")
    branch_end = PIPELINE.find(f"}} else if (state.{texture}) {{", branch_start)
    if branch_start == -1 or branch_end == -1:
        fail(f"external lookup branch for {texture} is missing")
    branch = PIPELINE[branch_start:branch_end]
    if "SetOperationFailure" in branch or "return false" in branch:
        fail(f"external lookup branch for {texture} must not fail the draw")
    if f"{slot} = AcquireExternalTextureSRV(state.{texture});" not in branch:
        fail(f"external lookup branch for {texture} must bind {slot} through AcquireExternalTextureSRV")

acquire = function_body(
    PIPELINE, "Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FalloutGPUDriver::AcquireExternalTextureSRV(")
if "SetOperationFailure" in acquire or "return false" in acquire:
    fail("AcquireExternalTextureSRV must soft-fall back to a null SRV, not fail the draw")
if "externalTextures_->Acquire(textureId)" not in acquire:
    fail("AcquireExternalTextureSRV must take the SRV through ExternalTextureRegistry::Acquire")
if "owner.Get() == context_->Device()" not in acquire:
    fail("AcquireExternalTextureSRV must bind only an SRV owned by the driver's device")

if "->Release()" in DRIVER:
    fail("the driver must not Release() an SRV from ExternalTextureRegistry::Acquire; hold it in a ComPtr")

rebuild = function_body(RESOURCES, "bool FalloutGPUDriver::RebuildForDeviceEpoch(std::uint64_t epoch)")
if "externalTextures_->ReleaseViews();" not in rebuild:
    fail("RebuildForDeviceEpoch must release the external views and keep the ids")
if "externalTextures_->Clear();" in rebuild:
    fail("RebuildForDeviceEpoch must not clear the external ids on a device epoch change")

unbind_start = REGISTRY.find("void Unbind(std::uint32_t textureId) {")
unbind_end = REGISTRY.find("\n        }\n", unbind_start)
if unbind_start == -1 or unbind_end == -1:
    fail("ExternalTextureRegistry::Unbind is missing")
unbind = REGISTRY[unbind_start:unbind_end]
if "erase" in unbind or "found->second = {};" not in unbind:
    fail("ExternalTextureRegistry::Unbind must leave a tombstone instead of erasing the id")

for name, first in (
    ("ExecuteCreateTexture", COLLISION),
    ("ExecuteUpdateTexture", NOOP),
    ("ExecuteDestroyTexture", NOOP),
):
    body = function_body(RESOURCES, f"bool FalloutGPUDriver::{name}(const AcceleratedGenerationOperation& operation)")
    if body.splitlines()[1].strip() != first:
        fail(f"{name} must check the external id before any other check: {first}")

if 'L"GPU", L"bAcceleratedRendering", 1,' not in BACKEND:
    fail("UltralightBackend.cpp must attempt accelerated rendering by default behind bAcceleratedRendering")
if "gpuContext_.Initialize(device_.Get(), immediateContext_.Get(), true)" not in BACKEND:
    fail("UltralightBackend.cpp must pass the controlled proxy allowance to the accelerated context")

print("external texture guard passed")
