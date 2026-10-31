#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_PARTS = (
    "WebRuntime.cpp",
    "WebRuntimeInternal.h",
    "WebRuntimeState.cpp",
    "WebRuntimePresentation.cpp",
    "WebRuntimePresent.cpp",
)
COMPOSITOR_PARTS = (
    "WebCompositor.cpp",
    "WebCompositorLayers.cpp",
)
POLICY = ROOT / "src/PrismaUI/PresentHoldPolicy.h"
MODELPREVIEW = sorted((ROOT / "src/PrismaUI").glob("ModelPreview*.cpp"))
TEXTUREOVERLAY = ROOT / "src/PrismaUI/TextureOverlay.cpp"
text = "\n".join((ROOT / "src/PrismaUI" / part).read_text(encoding="utf-8") for part in RUNTIME_PARTS)
compositor = "\n".join((ROOT / "src/PrismaUI" / part).read_text(encoding="utf-8") for part in COMPOSITOR_PARTS)
modelpreview = "\n".join(path.read_text(encoding="utf-8") for path in MODELPREVIEW)
textureoverlay = TEXTUREOVERLAY.read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(token: str, message: str, where: str = "runtime") -> None:
    if token not in (text if where == "runtime" else compositor):
        fail(message)


def reject(token: str, message: str, where: str = "runtime") -> None:
    if token in (text if where == "runtime" else compositor):
        fail(message)


if not POLICY.exists():
    fail("PresentHoldPolicy.h is missing; presentation must keep its pure, unit-tested policy core")
policy_text = POLICY.read_text(encoding="utf-8")
for banned in ("<d3d11.h>", "<windows.h>", "<Windows.h>", "RE::", "logger::"):
    if banned in policy_text:
        fail(f"PresentHoldPolicy.h must stay pure/Linux-buildable; found {banned}")

require('#include "PresentHoldPolicy.h"', "WebRuntime must drive presentation through PresentHoldPolicy")
reject("frame.Context() ? OrderedVisibleOnscreenViews()",
       "visibility must not be gated on the present lease (hide-during-stall would ghost)")
require("const auto orderedNativeViews = OrderedVisibleOnscreenViews();",
        "OnPresent must consult logical visibility unconditionally")

reject("kMaxHeldFrames",
       "the bounded held-frame cap returned; a starved visible UI must hold, never re-blank (strobe)")
reject("g_heldAlias", "the flat alias-capture machinery returned; held sources must be per-view")
reject("g_heldOwnedPool", "the in-place capture pool returned; capture failure must not corrupt held state")

require("CaptureHeldViewSource(", "per-view held sources must be captured (web texture + overlays per ViewId)")
require("Microsoft::WRL::ComPtr<ID3D11Texture2D> replacementTexture;",
        "held-copy replacement textures must be created outside the live slot")
require("Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> replacementSrv;",
        "held-copy replacement SRVs must be created outside the live slot")
require("slot.texture = std::move(replacementTexture);",
        "held-copy must commit a replacement texture only after creation and copy succeed")
require("slot.srv = std::move(replacementSrv);",
        "held-copy must commit a replacement SRV only after creation and copy succeed")
reject("slot.texture.Reset();\n                    slot.srv.Reset();\n                }\n            }\n            if (!slot.texture)",
       "a failed held-slot replacement must not clear the previous live held copy")
require("ctx->CopyResource(destination, source.Get());",
        "held-source capture must deep-copy the source texture (a ComPtr alias does not preserve pixels)")
require("std::swap(entry.web, entry.spareWeb);",
        "per-view capture must be transactional: build into spares and swap only on full success")
require("hp.presentPolicy.ShouldRefreshHeldSources(presentNowMs)",
        "held-source deep copies must be rate-limited on the wall clock, not a present count")

require("PrismaUI::PresentHold::Classify(viewSources, hp.committedViews)",
        "the freshness decision must be completeness (every displayed view has a source), not at-least-one-target")
reject("ChooseOverlaySource(liveAvailable, liveObservable, heldAvailable,",
       "the per-view all-or-nothing overlay hold returned; overlay continuity must be resolved per overlay")
reject("GetRegisteredOverlayKeys",
       "the registration-inference query returned; authoritative removal must be an explicit producer tombstone, not inferred from live availability")
require("PrismaUI::PresentHold::ReconcileOverlays(overlayInputs)",
        "overlays must resolve PER-OVERLAY (DrawLive / DrawHeld / Drop) in z-order, so one removed or hiccuping overlay never ghosts or drops the others")
require("ModelPreview::DrainRemovedOverlayKeys(removedOverlayKeys);",
        "ModelPreview must feed explicit removal tombstones (removal is never inferred from live availability)")
require("TextureOverlay::DrainRemovedOverlayKeys(removedOverlayKeys);",
        "TextureOverlay (local map) must feed explicit removal tombstones so a transient engine-texture gap is never treated as removal")
require("std::erase_if(held.overlays,",
        "drained removal tombstones must drop the held overlay copy every present, bypassing the no-lease hold")
require("PrismaUI::PresentHold::CommittedContainsRemoved(committedOverlayKeys, removedOverlayKeys)",
        "if a tombstoned overlay is baked into the committed frame it must invalidate that frame, so an incomplete-frame hold cannot silently preserve a removed overlay")
if "g_removedOverlayKeys.push_back(it->second.continuityKey);" not in modelpreview:
    fail("ModelPreview ErasePreview (authoritative teardown: Hide / whole-view / view destroy) must tombstone the preview's continuity key")
if "EvictToStub(victim->second);" not in modelpreview:
    fail("resource/LRU eviction must DEMOTE to a stub (retain held continuity), NOT tombstone -- only explicit teardown tombstones")
if "g_removedKeys.push_back(it->second.instanceKey);" not in textureoverlay:
    fail("TextureOverlay Hide/OnViewDestroyed must tombstone the departing episode instanceKey (immediate, for no-lease removal)")
if "instanceKey != 0) ? prev->second.instanceKey" not in textureoverlay:
    fail("TextureOverlay must mint a fresh instanceKey per Show episode (ABA-proof across Hide->Show), reused within an episode")
require("HeldOverlaysCurrent(liveOverlays, existing->second.overlays)",
        "held-overlay refresh must skip only when the held set exactly matches the live set (continuity key + content generation + geometry)")
require("!PrismaUI::PresentHold::OverlayContentClean(x.contentGeneration, y.contentGeneration)",
        "OverlayListEquals must gate recompose on overlay content generation so a stable-SRV animated overlay is not read as unchanged")
require("sourceSrv->GetDesc(&srvDesc)",
        "the held copy must read the source SRV's typed view desc so a typeless render target (ModelPreview B8G8R8A8_TYPELESS + UNORM SRV) keeps its format")
require("CreateShaderResourceView(replacementTexture.Get(), &srvDesc, &replacementSrv)",
        "the held SRV must be created with the source SRV's typed desc, not a nullptr desc that a typeless texture cannot resolve (capture would fail)")
reject("CreateShaderResourceView(replacementTexture.Get(), nullptr, &replacementSrv)",
       "the nullptr-desc held SRV returned; it fails on typeless ModelPreview render targets (held-source capture failed)")
require("HeldSlotReusable(",
        "held-copy pool reuse must validate the SRV view desc (format + dimension), not just the texture desc")
require("srvDesc.ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D",
        "held copy must fail closed on non-Texture2D SRVs -- the MiscFlags=0 clone and Texture2D subresource key only hold for ordinary Texture2D")
for field in ("srvFormat", "srvViewDim", "srvMostDetailedMip", "srvMipLevels", "sampleCount", "sampleQuality"):
    if field not in policy_text:
        fail(f"PresentHoldPolicy.h HeldSlotKey must include {field} for a complete texture+SRV reuse identity")
if "o.continuityKey = pv.continuityKey;" not in modelpreview:
    fail("ModelPreview overlays must carry a stable continuity key (RebuildOverlayCache)")
if "o.contentGeneration = pv.lastRenderFrame + 1;" not in modelpreview:
    fail("ModelPreview overlays must carry a content generation from the per-preview draw counter (lastRenderFrame)")
if "o.continuityKey = req.instanceKey;" not in textureoverlay:
    fail("TextureOverlay overlays must carry the per-episode instanceKey; local map contentGeneration stays 0 (unknown/always-dirty)")
require("const bool needsRecompose =",
        "a stable all-held stall must blit the committed frame, not recompose every layer every present")
require("hp.presentPolicy.OnPresent(presentNowMs, true, cls.complete, cls.committedFaithful,",
        "the present decision must come from the pure policy with timestamp + completeness + dirtiness inputs")
require("ClassifyReuseDiagnostic", "committed-frame reuse must carry a pure diagnostic classification")
require("ReuseReasonName", "committed-frame reuse logs must use stable diagnostic reason names")
require("freshTargets={}/{}, contentChanged={}, viewSetChanged={}, overlayChanged={},",
        "reuse diagnostics must include source, content, view-set, and overlay state")
reject("presenting committed frame without fresh content (producer starved;",
       "normal committed-frame reuse must not be labeled producer starvation")

require("st.compositor.ComposeAndCommitLayers(layers, effectiveOverlays,",
        "the complete frame must be composed offscreen with the effective (continuity-selected) overlays")
require("st.compositor.Draw(swapChain, committedSnapshot, noBlitOverlays, cursorX, cursorY, drawCursor);",
        "presentation must be exactly one operation: blit the committed frame through the audited Draw path")
if text.count("st.compositor.Draw(") != 1:
    fail("only the committed-frame blit may touch the backbuffer; extra per-layer backbuffer draws returned")

require("hp.presentPolicy.OnComposeFailed(", "compose failure must route through the policy fallback")
require("hp.presentPolicy.OnBlitSucceeded();", "blit success must be reported to the policy")
require("hp.presentPolicy.OnBlitSkipped(presentNowMs);",
        "SafeSkip must be reported with a timestamp so a skip after visible output can latch (anti-strobe)")
require("hp.presentPolicy.OnBlitFailed(presentNowMs);",
        "blit failure must arm the wall-clock dark latch via the policy")
require("st.compositor.InvalidateCommittedFrame();",
        "hiding the whole UI must invalidate the committed frame (no cross-episode replay)")
for banned in ("kDarkLatchPresents", "kHeldRefreshMinPresents"):
    if banned in text or banned in policy_text:
        fail(f"{banned}: frame-count safety windows returned; latch/refresh must be wall-clock (FPS-independent)")

if text.count("FreezeDiagnostics::NoteCompositeSuccess();") != 1:
    fail("NoteCompositeSuccess must be recorded exactly once, on the fresh-commit path only")
composed_gate = text.find("if (composedFresh) {")
note = text.find("FreezeDiagnostics::NoteCompositeSuccess();")
if min(composed_gate, note) < 0 or not (composed_gate < note):
    fail("composite health must be gated on a commit that contained fresh content")

require("const int composeIndex = m_committedIndex == 0 ? 1 : 0;",
        "compose must target the non-committed staging buffer (double-buffered)", "compositor")
require("m_context->ClearRenderTargetView(stage.rtv.Get(), clearColor);",
        "the staging frame must start transparent every compose", "compositor")
commit_flip = compositor.find("m_committedIndex = composeIndex;")
overlay_draw = compositor.rfind("DrawModelOverlays(overlays);")
if commit_flip < 0 or overlay_draw < 0 or overlay_draw > commit_flip:
    fail("the committed index must flip only after every layer and overlay draw (transactional commit)")

print("ok    transactional presentation: completeness, per-view sources, atomic commit, single blit, dark latch")
