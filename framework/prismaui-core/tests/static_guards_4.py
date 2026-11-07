from pathlib import Path
import sys

from static_guard_support import check, failures, function_body, read, read_web_runtime

@check("accelerated-v2 device epochs guard resize and publication")
def _accelerated_device_lifecycle():
    lifecycle = read("src/GPU/AcceleratedDeviceLifecycle.h")
    base = read("src/GPU/GPUDriverBase.h") + read("src/GPU/GPUDriverBase.cpp")
    driver = read("src/GPU/FalloutGPUDriver.h") + "".join(
        read(path.as_posix()) for path in sorted(Path("src/GPU").glob("FalloutGPUDriver*.cpp")))
    backend = read("src/PrismaUI/UltralightBackendPresentation.cpp")
    backend_api = read("src/PrismaUI/IWebBackend.h")
    runtime = read_web_runtime()
    hooks = read("src/Hooks/Hooks.cpp")
    snapshot = read("src/PrismaUI/RenderTargetSnapshot.h")
    assert "class AcceleratedDeviceLifecycle" in lifecycle
    assert "ResetTransportForEpoch" in base and "generation.epoch" in base
    assert "RebuildForDeviceEpoch" in driver
    # RebuildForDeviceEpoch() reconstructs the GPU resources from the CPU-owned shadows after a
    # device epoch change, so pin the shadows themselves -- the members, the upload paths that
    # fill them, and their use during the rebuild -- rather than an #include that outlives them.
    shadows = (("TextureEntry", "pixels", "operation.bytes",
                ("ExecuteCreateTexture", "ExecuteUpdateTexture")),
               ("GeometryEntry", "vertexBytes", "operation.bytes",
                ("ExecuteCreateGeometry", "ExecuteUpdateGeometry")),
               ("GeometryEntry", "indexBytes", "operation.secondaryBytes",
                ("ExecuteCreateGeometry", "ExecuteUpdateGeometry")))
    for struct, member, source, owners in shadows:
        fields = function_body(driver, f"struct {struct}")
        assert f"std::vector<std::uint8_t> {member};" in fields, \
            f"{struct}::{member} is no longer a CPU-owned resource shadow"
        for owner in owners:
            upload = function_body(driver, f"FalloutGPUDriver::{owner}(")
            assert f".{member} = {source};" in upload, \
                f"{owner}() no longer fills the {struct}::{member} shadow from {source}"
    rebuild = function_body(driver, "bool FalloutGPUDriver::RebuildForDeviceEpoch(std::uint64_t epoch)")
    for _, member, _, _ in shadows:
        assert f"entry.{member}.data()" in rebuild, \
            f"RebuildForDeviceEpoch() no longer rebuilds from the {member} shadow"
    # A missing shadow must fail the rebuild instead of publishing an empty resource.
    assert "entry.pixels.empty()" in rebuild, "RebuildForDeviceEpoch() no longer fails closed on an empty pixels shadow"
    assert "entry.vertexBytes.empty() || entry.indexBytes.empty()" in rebuild, \
        "RebuildForDeviceEpoch() no longer fails closed on empty geometry shadows"
    assert "deviceEpoch" in backend and "frame.deviceEpoch != deviceEpoch_" in backend
    assert "PresentationStateValid" in backend_api and "PresentationStateValid" in backend
    assert "HeldSourceEpochValid" in read("src/PrismaUI/PresentHoldPolicy.h")
    assert "validForEpoch" in snapshot
    assert "OnResizeBegin" in runtime and "OnResizeComplete" in runtime
    advance = function_body(runtime, "FrameAdvanceGuard AdvanceFrame(IDXGISwapChain* swapChain)")
    assert "!st.backend->PresentationStateValid()" in advance
    assert advance.index("ClearHeldPresentState();") < advance.index("st.compositor.InvalidateCommittedFrame();")
    assert "!frame.Context() ||" not in runtime
    held = runtime[runtime.index("source.hasHeld ="):runtime.index("outViewSources.push_back(source);", runtime.index("source.hasHeld ="))]
    assert "HeldSourceEpochValid" in held and "backend.PresentationStateValid()" in held
    for hook in (function_body(hooks, "HRESULT APIENTRY D3DHooks::HookResizeBuffersA"),
                 function_body(hooks, "HRESULT APIENTRY D3DHooks::HookResizeBuffersB")):
        assert "OnResizeBegin" in hook and "OnResizeComplete" in hook
        assert hook.index("OnResizeBegin") < hook.index("fn(pSwapChain") < hook.index("OnResizeComplete")

if __name__ == "__main__":
    if failures:
        print(f"{len(failures)} guard(s) failed")
        sys.exit(1)
