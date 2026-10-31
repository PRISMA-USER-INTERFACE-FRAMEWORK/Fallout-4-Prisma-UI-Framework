#!/usr/bin/env python3
"""Engine-3D mutation stays in one file.

src/API/MeshBinding*.cpp is the ONLY path in the framework that writes into engine-owned 3D --
geometry cull flags, shader-property flags, and the bound texture's SRV slot. It used to hand-build
a private NiTexture by calloc + memcpy over a donor; that path is gone and a bind that would need
one is refused. Concentrating that in one translation unit is what makes `[Debug] bMeshBinding=0` a
clean one-launch bisect for heap corruption, and what keeps API.cpp reviewable as a plugin-facing
forwarding layer.

API.cpp used to hold all 520 lines of it inline. This stops it drifting back.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
API = ROOT / "src" / "API" / "API.cpp"
MESH = ROOT / "src" / "API" / "MeshBinding.cpp"
MESH_PARTS = (MESH, ROOT / "src" / "API" / "MeshBindingDiag.cpp", ROOT / "src" / "API" / "MeshBindingInternal.h")

if not all(part.exists() for part in MESH_PARTS):
    print("FAIL: src/API/MeshBinding*.cpp is gone; the mesh-binding subsystem must stay isolated.")
    raise SystemExit(1)

api = API.read_text(encoding="utf-8", errors="replace")

# Writes into engine-owned 3D, and the private-texture forgery. None of this belongs in API.cpp.
FORBIDDEN = {
    "->pSRView =": "writing a D3D SRV into a BSGraphics texture slot",
    "diffuseTexture =": "reassigning a material's diffuse texture",
    "flags.flags |=": "setting engine geometry cull flags",
    "flags.flags &=": "clearing engine geometry cull flags",
    "EShaderPropertyFlag": "patching shader-property flags",
    "lastRenderPassState": "invalidating engine render passes",
    "displayGeometry": "mutating an Interface3D renderer's geometry list",
    "netimmerse_cast": "walking engine shader properties",
    "rendererTexture": "reaching into BSGraphics texture internals",
}

hits = [(tok, why) for tok, why in FORBIDDEN.items() if tok in api]
if hits:
    print("FAIL: engine-3D mutation reappeared in src/API/API.cpp:")
    for tok, why in hits:
        print(f"  {tok!r} -- {why}")
    print("It belongs in src/API/MeshBinding*.cpp behind the PrismaUI::MeshBinding interface.")
    raise SystemExit(1)

# API.cpp must still route the three plugin entry points there, or the extraction was bypassed
# rather than kept.
for call in ("PrismaUI::MeshBinding::Enabled()",
             "PrismaUI::MeshBinding::BindToGeometry(",
             "PrismaUI::MeshBinding::BindToScreenTexture(",
             "PrismaUI::MeshBinding::Unbind(",
             "PrismaUI::MeshBinding::DiagTick(&PrismaUI::WebRuntime::AcquireViewSRV)"):
    if call not in api:
        print(f"FAIL: src/API/API.cpp no longer forwards to {call}")
        raise SystemExit(1)

# The traversal must stay a template. Reintroducing std::function here means an allocation and an
# indirect call at every node of a full character scenegraph, four times over.
mesh = MESH.read_text(encoding="utf-8", errors="replace")
subsystem = "\n".join(part.read_text(encoding="utf-8", errors="replace") for part in MESH_PARTS)
if "std::function<void(RE::NiAVObject" in subsystem:
    print("FAIL: src/API/MeshBinding*.cpp reintroduced a std::function scenegraph walker.")
    print("Use the ForEachNode template so the visit inlines.")
    raise SystemExit(1)

for token in ("diffuseTexture =", "calloc", "EnginePrivateTexture", "AcquirePrivateTexture", "matStomps"):
    if token in subsystem:
        print(f"FAIL: src/API/MeshBinding*.cpp still contains {token!r}; the private-texture path was removed (fail closed).")
        raise SystemExit(1)
if (ROOT / "src" / "Engine" / "EnginePrivateTexture.h").exists():
    print("FAIL: src/Engine/EnginePrivateTexture.h must stay deleted (fail closed).")
    raise SystemExit(1)
bind = mesh[mesh.index("bool BindToGeometry("):mesh.index("bool BindToScreenTexture(")]
donor_at = bind.index("FindDonorTexture(root)")
make_at = bind.index("MakeBinding(")
if donor_at > make_at or "return false;" not in bind[donor_at:make_at]:
    print("FAIL: BindToGeometry must refuse a donor-shared geometry before MakeBinding().")
    raise SystemExit(1)


def function_body(code, signature):
    start = code.index(signature)
    opening = code.index("{", start)
    depth = 0
    for index in range(opening, len(code)):
        depth += {"{": 1, "}": -1}.get(code[index], 0)
        if depth == 0:
            return code[opening + 1:index]
    raise SystemExit(f"FAIL: could not read the body of {signature}")


enabled = function_body(mesh, "bool Enabled()")
availability = function_body(mesh, "Availability GetAvailability()")
if "GetAvailability()" not in enabled or "Availability::Available" not in enabled:
    print("FAIL: Enabled() must be derived from GetAvailability() and the Available state (fail closed).")
    raise SystemExit(1)
resolve = availability.find("Readiness::ResolveAvailability(")
if resolve < 0:
    print("FAIL: GetAvailability() must resolve the config flag through Readiness::ResolveAvailability().")
    raise SystemExit(1)
call_end = availability.index(");", resolve)
call = availability[resolve:call_end]
if "Engine::TextureLayoutBuildVerified()" not in call:
    print("FAIL: ResolveAvailability() must receive Engine::TextureLayoutBuildVerified() as its layout "
          "argument; without it an unverified engine texture layout could enable mesh binding.")
    raise SystemExit(1)
if "bMeshBinding" not in availability:
    print("FAIL: GetAvailability() must read the [Debug] bMeshBinding config flag.")
    raise SystemExit(1)
if "return availability;" not in availability:
    print("FAIL: GetAvailability() must return the resolved availability.")
    raise SystemExit(1)

print("mesh_binding_isolation_guard ok (engine-3D mutation confined to MeshBinding*.cpp)")
