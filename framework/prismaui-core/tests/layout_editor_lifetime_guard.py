from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
INPUT = (SRC / "PrismaUI/WebRuntimeInput.cpp").read_text(encoding="utf-8")
VIEWS = (SRC / "PrismaUI/WebRuntimeViews.cpp").read_text(encoding="utf-8")
LAYOUT_API = (SRC / "API/LayoutAPI.cpp").read_text(encoding="utf-8")


def body(text, signature, indent="    "):
    start = text.index(signature)
    return text[start:text.index("\n" + indent + "}\n", start)]


hide = body(INPUT, "void HideLayoutEditor() {")
assert "st.layoutView.exchange(0" in hide and "TeardownView(view, false)" in hide, \
    "HideLayoutEditor must take the layout editor view out of layoutView and destroy it"
assert "Hide(view)" not in hide, "HideLayoutEditor must destroy the editor view, not only hide it"

teardown = body(VIEWS, "void Internal::TeardownView(")
assert "if (announceGone) LayoutHost::OnViewDestroyed(view);" in teardown and \
    "backend->DestroyView(view)" in teardown, \
    "TeardownView must destroy the view and report it gone only when asked"

stores = [m.group(0) for m in re.finditer(r"layoutView\.store\([^)]*\)", INPUT + VIEWS)]
assert sorted(stores) == sorted([
    "layoutView.store(0, std::memory_order_release)",
    "layoutView.store(view, std::memory_order_release)",
]), "layoutView may only be set to a freshly created editor view or cleared"

set_edit = body(LAYOUT_API, "Flat::LayoutResult SetEditMode(bool enabled) noexcept {")
off = set_edit[set_edit.index("} else {"):]
assert "WebRuntime::HideLayoutEditor();" in off, "closing edit mode must destroy the editor view"
push = body(LAYOUT_API, "void PushEditorState() {", "        ")
assert "if (!L::Instance().EditMode()) return;" in push, "no editor state may be pushed while edit mode is off"

print("layout editor lifetime guard ok")
