import json
from pathlib import Path

RUNTIME_PARTS = (
    "WebRuntime.cpp",
        "WebRuntimeInternal.h",
        "WebRuntimeState.cpp",
        "WebRuntimeLifecycle.cpp",
        "WebRuntimeFrame.cpp",
        "WebRuntimePresent.cpp",
        "WebRuntimePresentation.cpp",
        "WebRuntimeFocus.cpp",
        "WebRuntimeInput.cpp",
        "WebRuntimeViews.cpp",
        "WebRuntimeInspector.cpp",
        "WebRuntimeCallbacks.cpp",
)


def require(source: str, needle: str, message: str) -> None:
    if needle not in source:
        raise AssertionError(message)


def forbid(source: str, needle: str, message: str) -> None:
    if needle in source:
        raise AssertionError(message)


backend = Path("src/PrismaUI/UltralightBackend.cpp").read_text(encoding="utf-8")
backend_implementation = "\n".join(
    Path(path).read_text(encoding="utf-8")
    for path in (
        "src/PrismaUI/UltralightBackend.cpp",
        "src/PrismaUI/UltralightBackendCallbacks.cpp",
        "src/PrismaUI/UltralightBackendDispatch.cpp",
        "src/PrismaUI/UltralightBackendFrames.cpp",
        "src/PrismaUI/UltralightBackendInput.cpp",
        "src/PrismaUI/UltralightBackendPresentation.cpp",
        "src/PrismaUI/UltralightBackendViewState.cpp",
        "src/PrismaUI/UltralightBackendViews.cpp",
    )
)
document_callbacks = Path("src/PrismaUI/UltralightDocumentCallbacks.cpp").read_text(encoding="utf-8")
inspector = Path("src/PrismaUI/UltralightInspector.cpp").read_text(encoding="utf-8")
file_system = "\n".join(
    Path(path).read_text(encoding="utf-8")
    for path in ("src/PrismaUI/PrismaFileSystem.h", "src/PrismaUI/PrismaFileSystem.cpp")
)
ultralight_runtime = "\n".join(
    Path(path).read_text(encoding="utf-8")
    for path in ("src/PrismaUI/UltralightRuntime.h", "src/PrismaUI/UltralightRuntime.cpp")
)
backend_surface = "\n".join(
    (backend_implementation, document_callbacks, inspector, file_system, ultralight_runtime)
)
dispatch_queue = Path("src/PrismaUI/UltralightDispatchQueue.h").read_text(encoding="utf-8")
runtime = "\n".join(
    (Path("src/PrismaUI") / part).read_text(encoding="utf-8") for part in RUNTIME_PARTS
)

PINNED_HEADER = Path("src/PrismaUI/UltralightRuntimeManifest.g.h")
pinned = PINNED_HEADER.read_text(encoding="utf-8")
manifest = json.loads(Path("runtime/ultralight-1.4.0.manifest.json").read_text(encoding="utf-8"))

# Official release-candidate creation must still pin every vendor SDK runtime byte.
for required_release_file in (
    "libs/AppCore.dll",
    "libs/WebCore.dll",
    "libs/Ultralight.dll",
    "libs/UltralightCore.dll",
    "resources/cacert.pem",
    "resources/icudt67l.dat",
):
    if required_release_file not in manifest.get("files", {}):
        raise AssertionError(f"release manifest lost required SDK pin: {required_release_file}")

# Installed runtime integrity intentionally excludes LGPL shared libraries so an end user may replace
# AppCore/WebCore with an interface-compatible build. Proprietary runtime DLLs and resources remain
# compiled trust anchors.
for needle, message in (
    ("resources/cacert.pem", "the pinned certificate resource must be in the compiled trust anchor"),
    ("resources/icudt67l.dat", "the pinned ICU resource must be in the compiled trust anchor"),
    ("libs/Ultralight.dll", "Ultralight.dll must remain in the compiled trust anchor"),
    ("libs/UltralightCore.dll", "UltralightCore.dll must remain in the compiled trust anchor"),
    ("kPinnedRuntimeFiles", "the header must expose the pinned runtime table"),
):
    require(pinned, needle, message)
for replaceable in ("libs/AppCore.dll", "libs/WebCore.dll"):
    forbid(pinned, replaceable, f"{replaceable} must remain user-replaceable at installed runtime")

backend_checks = (
    ("const bool allowAccelerated = ::GetPrivateProfileIntW(", "accelerated startup must be attempted by default"),
    ('L"GPU", L"bAcceleratedRendering", 1,', "the GPU kill switch must be read from [GPU] bAcceleratedRendering with a default of on"),
    ("ThisModuleDir()", "the FileSystem root must follow the provider module"),
    ("parent_path().parent_path() / L\"PrismaUI_F4\"", "the package root must resolve from the provider module layout"),
    ("resourcesRoot_", "Ultralight resources must resolve below the packaged resources root"),
    ("LoadLibraryExW", "the packaged Ultralight runtime must be loaded from an explicit path"),
    ("ValidateRuntimePackage", "startup must preflight the pinned DLL and resource manifest"),
    ("kPinnedRuntimeFiles", "runtime preflight must hash against the generated pinned trust anchor"),
    ("UltralightRuntimeManifest.g.h", "the compiled trust anchor must be the generated manifest header"),
    ("~UltralightBackend", "the backend must stop and join its owner thread before static teardown"),
    ("BCryptFinishHash", "runtime preflight must verify SHA-256, not only DLL loadability"),
    ("fileSystem_ = std::make_unique<PrismaFileSystem>",
     "the Ultralight FileSystem must not be constructed before runtime preflight"),
    ("platform.set_file_system(nullptr)",
     "owner-thread teardown must detach the FileSystem from Platform before releasing the runtime"),
    ("platform.set_clipboard(&SystemClipboard::UltralightClipboard())",
     "Ultralight must read the OS clipboard so pasting works in Prisma text fields"),
    ("platform.set_clipboard(nullptr)",
     "owner-thread teardown must detach the Clipboard from Platform"),
    ("fileSystem_.reset()",
     "the FileSystem must be destroyed on the owner thread while the runtime DLLs are still loaded"),
    ("__pfnDliNotifyHook2", "delay-loaded Ultralight imports must use the packaged runtime handles"),
    ("dliNotePreLoadLibrary", "delay-loaded Ultralight imports must resolve before the default DLL search"),
    ("Ultralight runtime DLLs were not found", "missing Ultralight runtime files must fail closed"),
    ("InjectContentSecurityPolicyIntoHtml", "HTML must receive the generated Ultralight CSP"),
    ("CreateSession(false, \"prisma\")", "the backend must use a non-persistent Ultralight session"),
    ("platform.set_gpu_driver(accelerated_ ? gpuDriver_.get() : nullptr)",
     "GPU activation must register only a fully initialized borrowed-device driver"),
    ("gpuContext_.Initialize(device_.Get(), immediateContext_.Get(), true)",
     "accelerated startup must use the centralized ownership policy with the controlled proxy allowance"),
    ("gpuDriver_.reset();", "a failed accelerated startup must drop the GPU driver and keep the CPU backend"),
    ("config.is_accelerated = accelerated_", "View acceleration must follow backend GPU readiness"),
    ("gpuDriver_->DrainPendingGeneration()", "Present must drain every deferred GPU generation before publication"),
    ("GenerationDrain::Activation::DrainFailed", "a failed deferred drain must retire without granting a lease"),
    ("stateCv_.wait_for", "Present must use a bounded wait for the next completed generation"),
    ("record.view->render_target()", "GPU Views must publish their native render target"),
    ("AcquirePresentationTextureSnapshot(frame.textureId)", "native texture IDs must resolve through the borrowed-device driver"),
    ("target.uv_coords", "native padded render-target UVs must cross the presentation seam"),
    ("renderer_->RefreshDisplay(0)", "the owner loop must refresh display before rendering"),
    ("renderer_->Render()", "the owner loop must render each completed generation"),
    ("set_view_listener(record->callbacks.get())", "native View lifecycle callbacks must be installed"),
    ("set_load_listener(record->callbacks.get())", "native load callbacks must be installed"),
    ("OnChangeURL", "browser-triggered URL changes must cross the backend policy boundary"),
    ("WebNetworkPolicy::IsTrustedLocalDocumentUrl", "top-level documents must remain trusted local assets"),
    ("BlockNavigation", "remote top-level navigation must fail closed at the callback boundary"),
    ("trustedDocumentReady", "native listeners must only install for a trusted local document"),
    ("KeyEvent::kType_Char", "character input must reach Ultralight as a character event"),
    ("event.text = ultralight::String(\"\\n\")", "Enter character input must normalize to newline"),
    ("BitmapSurface", "GPU initialization failure must retain the CPU BitmapSurface fallback"),
    ("DXGI_FORMAT_B8G8R8A8_UNORM", "CPU fallback frames must upload as BGRA8 textures"),
)

for needle, message in backend_checks:
    require(backend_surface, needle, message)

# The FileSystem containment policy lives in the module that owns the resolve call, not in the
# backend root. Anchoring it to file_system keeps the token from being satisfied by an unrelated
# mention elsewhere in the backend surface while the real call site goes unguarded. The namespace
# is part of the needle: the policy moved out of the retired CefHost tree, and matching a bare
# "LogicalPathPolicy::ResolveBelow" would accept a qualified name that no longer compiles.
require(file_system, "PrismaUI::Engine::LogicalPathPolicy::ResolveBelow",
        "FileSystem paths must use the logical containment policy")
forbid(file_system, "CefHost", "the FileSystem must not depend on the retired CEF host tree")

require(dispatch_queue, "std::atomic_bool stopping_{false};", "the owner lifecycle stop state must be atomic")

fail_start = document_callbacks.find("void UltralightDocumentCallbacks::OnFailLoading(")
fail_end = document_callbacks.find("void UltralightDocumentCallbacks::OnWindowObjectReady(", fail_start)
if fail_start == -1 or fail_end == -1:
    raise AssertionError("Ultralight load-failure handler is missing")
fail_body = document_callbacks[fail_start:fail_end]
subframe_gate = fail_body.find("if (!mainFrame)")
subframe_return = fail_body.find("return;", subframe_gate)
fatal_health = fail_body.find("host_.SetDocumentHealth(*record, PrismaUI::Web::ViewHealth::LoadFailed)")
fatal_report = fail_body.find("ReportError(id, PrismaUI::Web::ViewHealth::LoadFailed")
if min(subframe_gate, subframe_return, fatal_health, fatal_report) == -1:
    raise AssertionError("load-failure frame-scope contract is incomplete")
if not subframe_gate < subframe_return < fatal_health < fatal_report:
    raise AssertionError("subframe failures must return before whole-view LoadFailed health and error reporting")

for needle, message in (
    ("FalloutD3DContext::IsD3D11ProxyLoaded", "backend startup must not make module presence its ownership gate"),
    ("allowProxiedDevice", "the legacy filename-only proxy override must not remain in backend startup"),
    ("d3d11.dll proxy detected", "backend startup must not reject based only on a proxy filename"),
):
    forbid(backend_surface, needle, message)

if "HasCommandsPending()" in backend_surface:
    raise AssertionError("Present must not gate deferred generation execution on draw-command presence")
if 'GetPrivateProfileIntW(L"GPU", L"bAllowProxiedDevice"' in backend_surface:
    raise AssertionError("the unsupported proxied-device override must not be runtime-configurable")
if 'GetPrivateProfileIntW(L"GPU", L"bAccelerated"' in backend_surface:
    raise AssertionError("the retired unprefixed bAccelerated key must not return")
if "bAccelerated=1" in backend_surface:
    raise AssertionError("runtime logs must not advertise the removed accelerated INI switch")
require(Path("release-assets/PrismaUI_F4.ini").read_text(encoding="utf-8"), "bAcceleratedRendering=1",
        "the shipped ini must document the GPU kill switch with its default")
if "PackageRoot()," in backend_surface or "ThisModuleDir()," in backend_surface:
    raise AssertionError("runtime loading must not fall back to loose package or plugin DLL copies")

_dtor = ultralight_runtime.find("UltralightRuntime::~UltralightRuntime()")
if _dtor == -1:
    raise AssertionError("the Ultralight runtime wrapper must define a destructor")
if "FreeLibrary" in ultralight_runtime[_dtor:_dtor + 300]:
    raise AssertionError(
        "the runtime destructor must not FreeLibrary at process-exit static teardown "
        "(loader-lock deadlock risk); leave the modules resident for the process lifetime")

runtime_checks = (
    ("EnsureSystemViews();", "WebRuntime must allocate system Views after backend readiness"),
    ("CreateView(path, {}, \"PrismaUI\")", "system Views must use the managed WebRuntime lifecycle"),
    ("system/boot.html", "the boot system View must use a managed asset"),
    ("system/dock.html", "the dock system View must use a managed asset"),
    ("SetOrder(view, -100000)", "boot ordering must remain WebRuntime-owned"),
    ("SetOrder(view, 100000)", "dock ordering must remain WebRuntime-owned"),
    ("bool SupportsViewNetworkPolicy() { return false; }", "Free native request interception must fail closed"),
)

for needle, message in runtime_checks:
    require(runtime, needle, message)

for path in sorted(Path("src").rglob("*")):
    if path.suffix not in (".cpp", ".h"):
        continue
    source = path.read_text(encoding="utf-8")
    name = path.as_posix()
    if name != "src/PrismaUI/UltralightBackendFrames.cpp":
        forbid(source, "render_target()", f"{name} must not call View::render_target(); only UltralightBackendFrames may")
    if name not in ("src/PrismaUI/UltralightBackend.cpp", "src/PrismaUI/UltralightBackend.h"):
        forbid(source, "PublishFrames();", f"{name} must not call PublishFrames(); only the owner loop may")

for forbidden in ("NetworkListener", "NetworkRequest", "set_network_listener"):
    if forbidden in backend:
        raise AssertionError(f"Free backend must not depend on Pro-only {forbidden} enforcement")
if "LoadURL" in backend:
    raise AssertionError("Free backend must not expose native top-level remote navigation")

owner_start = backend.find("void UltralightBackend::OwnerMain(")
owner_driver = backend.find("gpuDriver_ = std::make_unique<PrismaUI::GPU::FalloutGPUDriver>(&gpuContext_);", owner_start)
owner_proof = backend.find("accelerated_ = accelerated_ && gpuDriver_->InitializePipeline();", owner_start)
owner_fallback = backend.find("if (!accelerated_) {", owner_start)
owner_register = backend.find("platform.set_gpu_driver(accelerated_ ? gpuDriver_.get() : nullptr)", owner_start)
if not -1 < owner_driver < owner_proof < owner_fallback < owner_register:
    raise AssertionError("InitializePipeline must run after driver construction and before the CPU fallback and set_gpu_driver")

present_start = backend_implementation.find("PrismaUI::Web::PresentLease UltralightBackend::BeginPresentFrame(")
present_end = backend_implementation.find(
    "PrismaUI::Web::RenderTargetSnapshot UltralightBackend::ViewRenderTarget(", present_start)
present_gate = backend_implementation.find("CanBeginPresent(acceptingFrames_", present_start)
present_observe = backend_implementation.find("ObserveAcceleratedLifecycleLocked(immediate", present_start)
if not -1 < present_start < present_gate < present_observe < present_end:
    raise AssertionError("BeginPresentFrame must check acceptingFrames_ before observing the accelerated lifecycle")

teardown = backend.find('currentOp_.store("teardown", std::memory_order_release);')
teardown_lock = backend.find("std::lock_guard lock(stateMutex_);", teardown)
teardown_reset = backend.find("gpuDriver_.reset();", teardown)
if not -1 < teardown < teardown_lock < teardown_reset:
    raise AssertionError("owner teardown must hold stateMutex_ while releasing the GPU driver")

print("ultralight backend guard passed")
