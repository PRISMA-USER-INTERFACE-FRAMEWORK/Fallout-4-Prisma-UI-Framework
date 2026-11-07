#!/usr/bin/env python3
from pathlib import Path

xmake = Path("xmake.lua").read_text(encoding="utf-8")
marker = 'target("PrismaUI_F4VR")'
start = xmake.index(marker)
end = xmake.index('target("PrismaUI_PapyrusJsonTests")', start)
vr = xmake[start:end]

assert '"src/Utils/**.cpp"' in vr, "VR target no longer compiles ConflictChecker/Utils"
assert '"src/Hooks/InlineHookClassifier.cpp"' in vr, (
    "VR target compiles ConflictChecker.cpp, which calls ClassifyHookTarget; "
    "InlineHookClassifier.cpp must stay linked"
)
assert '"src/Hooks/Hooks.cpp"' not in vr, "flat hook installation must not leak into VR"
assert '"src/PrismaUI/GameThreadDispatcher.cpp"' not in vr, "flat dispatcher must not leak into VR"

print("vr_link_source_guard ok")
