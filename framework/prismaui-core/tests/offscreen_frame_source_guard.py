#!/usr/bin/env python3
"""Pin the offscreen-frame render policy against the source that implements it.

The policy this guards -- nonblocking keyed-mutex acquire, epoch-checked publish, and no D3D work
inside g_offscreenMutex -- used to live in WebRuntime.cpp's UpdateOffscreenFrames. It now lives in
OffscreenFrames.cpp as UpdateImpl, with the map lifetime in Track(). The guard follows the code
rather than the file: each function is looked up across the candidate sources and MUST be found in
exactly one, so a future move fails loudly here instead of silently guarding nothing.
"""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
CANDIDATES = (
    "src/PrismaUI/OffscreenFrames.cpp",
    "src/PrismaUI/WebRuntime.cpp",
    "src/PrismaUI/WebRuntimeViews.cpp",
)
SOURCES = {rel: (ROOT / rel).read_text(encoding="utf-8") for rel in CANDIDATES}


def function_body(signature: str) -> str:
    """Body of `signature`, located across CANDIDATES. Ambiguous or missing is a hard failure."""
    found = [rel for rel, text in SOURCES.items() if signature in text]
    if len(found) != 1:
        raise AssertionError(
            f"expected {signature!r} in exactly one of {list(CANDIDATES)}, found it in {found}. "
            "If it moved again, update CANDIDATES; do not delete the assertions below."
        )
    source = SOURCES[found[0]]
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unclosed function: {signature}")


def lock_blocks(body: str):
    needle = "std::lock_guard lock(g_offscreenMutex);"
    pos = 0
    while True:
        hit = body.find(needle, pos)
        if hit < 0:
            return
        # lock_guard is normally inside an already-open block; find the containing block by walking
        # backward to the nearest opening brace, then forward until that block closes.
        opening = body.rfind("{", 0, hit)
        if opening < 0:
            raise AssertionError("could not locate g_offscreenMutex critical section")
        depth = 0
        for index in range(opening, len(body)):
            if body[index] == "{":
                depth += 1
            elif body[index] == "}":
                depth -= 1
                if depth == 0:
                    yield body[opening:index + 1]
                    pos = index + 1
                    break
        else:
            raise AssertionError("unclosed g_offscreenMutex critical section")


body = function_body("void UpdateImpl(")

assert "AcquireSync(0, 0)" in body, "source keyed mutex must be acquired nonblocking"
assert "AcquireSync(0, 8)" not in body, "render thread regained an 8 ms per-view wait"
assert "OffscreenFrameState::NeedsCopy" in body, "generation skip must use the tested state policy"
assert "OffscreenFrameState::CanPublish" in body, "publish must verify the live view epoch"
assert "snapshot.epoch" in body, "copy path lost its view epoch snapshot"
assert "destinationTexture" in body and "destinationSrv" in body, "stable resources must be held strongly outside the map lock"
assert "context->CopyResource(destinationTexture.Get(), sourceTexture.Get());" not in body
assert "context->CopySubresourceRegion(destinationTexture.Get()" in body
assert "&sourceBox" in body
assert "sourceRight - sourceLeft != renderTarget.viewportWidth" in body
assert "sourceBottom - sourceTop != renderTarget.viewportHeight" in body
assert "desc.Width = renderTarget.viewportWidth" in body
assert "desc.Height = renderTarget.viewportHeight" in body
assert "OffscreenFrameState::ShouldReleaseKeyedMutex(haveMutex, mutexAcquired)" in body
frames = SOURCES["src/PrismaUI/OffscreenFrames.cpp"]
assert "using PrismaUI::OffscreenFrameState::ResolveSourceCoordinate;" in frames, "copy-box UVs must resolve through the host-tested OffscreenFrameState policy"
assert "bool ResolveSourceCoordinate(" not in frames, "OffscreenFrames.cpp must not keep its own ResolveSourceCoordinate"

for block in lock_blocks(body):
    assert "AcquireSync" not in block, "AcquireSync must stay outside g_offscreenMutex"
    assert "CopyResource" not in block, "CopyResource must stay outside g_offscreenMutex"
    assert "CopySubresourceRegion" not in block, "CopySubresourceRegion must stay outside g_offscreenMutex"
    assert "CreateTexture2D" not in block, "stable texture allocation must stay outside g_offscreenMutex"
    assert "CreateShaderResourceView" not in block, "stable SRV allocation must stay outside g_offscreenMutex"

# Map lifetime. SetViewOffscreen used to assign the epoch inline; it now delegates, so assert both
# halves -- the delegation, and that the epoch is still assigned only on insertion.
track = function_body("void Track(")
assert "NextOffscreenEpochLocked" in track, "new offscreen lifetimes must receive a fresh epoch"
assert "inserted" in track, "epoch must be assigned on map insertion, not on every Track()"

set_offscreen = function_body("void SetViewOffscreen(")
assert "OffscreenFrames::Track(" in set_offscreen, "SetViewOffscreen(true) must register the view"
assert "OffscreenFrames::Forget(" in set_offscreen, "SetViewOffscreen(false) must drop the view"

print("offscreen_frame_source_guard ok")
