from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BRIDGE = (ROOT / "src/PrismaUI/PapyrusBridge.cpp").read_text(encoding="utf-8")
VIEWS = (ROOT / "src/PrismaUI/WebRuntimeViews.cpp").read_text(encoding="utf-8")
VR_POLICY = (ROOT / "src/VR/VRNetworkPolicy.cpp").read_text(encoding="utf-8")

forget_start = BRIDGE.index("void ForgetViewOwner(WebRuntime::ViewId viewId) {")
forget = BRIDGE[forget_start:BRIDGE.index("\n}", forget_start)]
assert "std::lock_guard lock(g_ownerMutex);" in forget and "g_viewOwners.erase(viewId);" in forget, (
    "ForgetViewOwner must erase the owner entry under g_ownerMutex"
)
assert "PapyrusBridge::ForgetViewOwner(view);" in VIEWS, "view Destroy must call ForgetViewOwner"

get = VR_POLICY[VR_POLICY.index("    bool Get("):]
guard = get.find("if (!WebRuntime::IsValid(viewId))")
assert guard != -1 and guard < get.find("WebRuntime::GetViewNetworkPolicy("), (
    "VRNetworkPolicy Get must reject an invalid view before the host query"
)

print("papyrus view lifecycle guard passed")
