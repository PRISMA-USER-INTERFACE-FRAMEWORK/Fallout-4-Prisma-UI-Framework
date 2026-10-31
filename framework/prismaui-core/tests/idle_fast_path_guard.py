#!/usr/bin/env python3
"""The idle gate must stay AHEAD of the per-frame work it exists to avoid.

It originally sat at the bottom of OnPresent and skipped only the composite, so an idle
frame still paid for the mesh-binding diagnostic, both spikes, ModelPreview::TickCore and
two sorted view walks. Moving any of that above the gate silently restores that cost, and
nothing about the rendered output changes -- so only this check would notice.

After the #736 split the gather work lives in WebRuntimePresentation.cpp's GatherViewOverlays,
called from OnPresent below the gate; this guard pins both halves.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PRESENT = ROOT / "src" / "PrismaUI" / "WebRuntimePresent.cpp"
PRESENTATION = ROOT / "src" / "PrismaUI" / "WebRuntimePresentation.cpp"

text = PRESENT.read_text(encoding="utf-8", errors="replace")
presentation = PRESENTATION.read_text(encoding="utf-8", errors="replace")

GATE = "if (idleFrame) return;"
# Everything the gate is supposed to be able to skip, in the present translation unit.
MUST_FOLLOW = (
    "GatherViewOverlays(frame.Device(), frame.Context(), *st.backend, perViewOverlays);",
    "st.compositor.Draw(",
)
# The per-frame gather bodies, which must remain inside the helper the gate precedes.
GATHERED = (
    "PluginAPI::DiagTickMeshBindings();",
    "LocalMapSpike::Tick(dev, ctx);",
    "CameraSpike::Tick(dev, ctx);",
    "ModelPreview::TickCore(dev, ctx);",
    "ModelPreview::GetActiveViews(activeViews);",
    "TextureOverlay::GetActiveViews(texViews);",
)

if text.count(GATE) != 1:
    print(f"FAIL: expected exactly one '{GATE}' in src/PrismaUI/WebRuntimePresent.cpp, found {text.count(GATE)}")
    raise SystemExit(1)

gate_at = text.index(GATE)

early = []
for token in MUST_FOLLOW:
    if token not in text:
        print(f"FAIL: idle-gate guard is stale -- '{token}' is gone from OnPresent")
        raise SystemExit(1)
    if text.index(token) < gate_at:
        early.append(token)

if early:
    print("FAIL: per-frame work moved ABOVE the idle gate in src/PrismaUI/WebRuntimePresent.cpp:")
    for token in early:
        print(f"  {token}")
    print("An idle frame would pay for it again. Keep it below 'if (idleFrame) return;'.")
    raise SystemExit(1)

if "void GatherViewOverlays(" not in presentation:
    print("FAIL: the per-frame gather helper left src/PrismaUI/WebRuntimePresentation.cpp")
    raise SystemExit(1)
for token in GATHERED:
    if token not in presentation:
        print(f"FAIL: skippable per-frame work '{token}' is gone from GatherViewOverlays")
        raise SystemExit(1)

# The gate must stay conservative: dropping any of these terms widens it into frames that
# still have something to draw.
for term in ("SkipIdleCompositeEnabled()", "st.focusedView.load() == 0", "!st.dockVisible.load()",
             "!drawCursor", "ModelPreview::HasPendingWork()", "TextureOverlay::HasPendingWork()",
             "!HasVisibleOnscreenView()"):
    if term not in text:
        print(f"FAIL: the idle gate no longer requires '{term}' -- it can now skip a frame that draws.")
        raise SystemExit(1)

print("idle_fast_path_guard ok (gate precedes all skippable per-frame work)")
