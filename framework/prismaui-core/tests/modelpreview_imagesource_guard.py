from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
PREVIEW_DIR = ROOT / "src" / "PrismaUI"
MODEL_PREVIEW = (PREVIEW_DIR / "ModelPreview.cpp").read_text(encoding="utf-8")
POLICY = (PREVIEW_DIR / "ModelPreviewImageSourcePolicy.h").read_text(encoding="utf-8")
PIPELINE = (PREVIEW_DIR / "ModelPreviewPipeline.cpp").read_text(encoding="utf-8")
PRESENTATION = (PREVIEW_DIR / "WebRuntimePresentation.cpp").read_text(encoding="utf-8")
SYNC = (PREVIEW_DIR / "ModelPreviewImageSources.cpp").read_text(encoding="utf-8")
STUBS = (ROOT / "src" / "VR" / "VRUnportedStubs.cpp").read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def function_body(source: str, signature: str, closer: str = "\n}\n") -> str:
    start = source.find(signature)
    if start == -1:
        fail(f"missing {signature}")
    end = source.find(closer, start)
    if end == -1:
        fail(f"unterminated {signature}")
    return source[start:end]


presentation = function_body(PRESENTATION, "void GatherViewOverlays(", "\n    }\n")
tick = presentation.find("ModelPreview::TickCore(dev, ctx);")
sync = presentation.find("ModelPreview::SyncImageSources(backend);")
overlays = presentation.find("ModelPreview::GetOverlays(view, overlays);")
if tick == -1 or not tick < sync < overlays:
    fail("GatherViewOverlays must call ModelPreview::SyncImageSources after TickCore and before GetOverlays")

if "void SyncImageSources(PrismaUI::Web::IWebBackend& backend) {" not in SYNC:
    fail("ModelPreviewImageSources.cpp must define SyncImageSources")
if "pv.image.live = backend.IsExternalImageLive(pv.continuityKey);" not in SYNC:
    fail("SyncImageSources must refresh the live bind from IsExternalImageLive")
if "const bool hasTarget = pv.tex && pv.rtv && pv.srv && pv.model && pv.drawn;" not in SYNC:
    fail("SyncImageSources must publish only a target that has a model and has drawn")
if "for (const auto key : g_releasedImageKeys) backend.ReleaseExternalImage(key);" not in SYNC:
    fail("SyncImageSources must drain g_releasedImageKeys through ReleaseExternalImage")
if "pv.lastRenderFrame + 1))" not in SYNC:
    fail("SyncImageSources must publish with the generation lastRenderFrame + 1")
if "const auto size = static_cast<uint32_t>(g_cfg.rtSize);" not in SYNC or "pv.srv, size, size," not in SYNC:
    fail("SyncImageSources must reserve and publish the g_cfg.rtSize target")
if "if (!r.imageSource) RetractImage(pv);" not in MODEL_PREVIEW:
    fail("ModelPreview must retract the image when a show turns imageSource off")
if "void SyncImageSources(PrismaUI::Web::IWebBackend&) {}" not in STUBS:
    fail("src/VR/VRUnportedStubs.cpp must stub SyncImageSources for the VR target")

reconcile = function_body(POLICY, "bool Reconcile(")
gate = reconcile.find("if (state.url.empty() ||")
publish = reconcile.find("backend.PublishExternalImage(")
if gate == -1 or publish == -1 or not gate < publish:
    fail("PublishExternalImage must be gated on a non-empty URL")

evict = function_body(MODEL_PREVIEW, "static void EvictToStub(Preview& pv) {")
if "RetractImage(pv);" not in evict:
    fail("EvictToStub must retract the preview's image")
retract = function_body(MODEL_PREVIEW, "static void RetractImage(Preview& pv) {", "\n}\n")
if "ImageSourcePolicy::Retire(pv.image)" not in retract or "g_retractedImageKeys.push_back(" not in retract:
    fail("RetractImage must queue the retract key")
erase = function_body(MODEL_PREVIEW, "static void ErasePreview(")
if "g_releasedImageKeys.push_back(" not in erase:
    fail("ErasePreview must queue the release key")
view_destroyed = function_body(MODEL_PREVIEW, "void OnViewDestroyed(ViewId viewId) {")
if "g_pendingHides.push_back(" not in view_destroyed:
    fail("OnViewDestroyed must queue a hide so ErasePreview releases the image on the present thread")
shutdown = function_body(MODEL_PREVIEW, "void Shutdown() {")
if "ErasePreview(it)" not in shutdown:
    fail("Shutdown must erase previews through ErasePreview")

exclusion = function_body(POLICY, "inline bool ExcludesHoleOverlay(")
if "!state.url.empty() && state.published && state.live" not in exclusion:
    fail("hole-overlay exclusion must require a non-empty URL, an issued publish and a live bind")
rebuild = function_body(PIPELINE, "void RebuildOverlayCache() {")
excluded = rebuild.find("ImageSourcePolicy::ExcludesHoleOverlay(pv.image))")
pushed = rebuild.find("g_overlayCache[pv.viewId].push_back(o);")
if not 0 < excluded < pushed:
    fail("RebuildOverlayCache must skip excluded previews before building an overlay")

OWNER_FILES = sorted(PREVIEW_DIR.glob("UltralightBackend*.cpp")) + [PREVIEW_DIR / "UltralightImageSources.cpp"]
for path in OWNER_FILES:
    text = path.read_text(encoding="utf-8")
    for name in ("g_previews", "g_overlayCache", "g_rtFreeList"):
        if re.search(rf"\b{name}\b", text):
            fail(f"{path.name} touches {name}: owner-thread code must not reach present-thread ModelPreview state")

print("modelpreview imagesource guard passed")
