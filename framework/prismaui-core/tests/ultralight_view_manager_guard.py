from pathlib import Path


header = Path("src/PrismaUI/UltralightViewManager.h").read_text(encoding="utf-8")
source = Path("src/PrismaUI/UltralightViewManager.cpp").read_text(encoding="utf-8")
backend = Path("src/PrismaUI/UltralightBackend.cpp").read_text(encoding="utf-8")
views = Path("src/PrismaUI/UltralightBackendViews.cpp").read_text(encoding="utf-8")

assert "class UltralightViewManager final" in header
for field in (
    "std::map<PrismaUI::Web::ViewId, std::unique_ptr<ViewRecord>> records_;",
    "std::map<PrismaUI::Web::ViewId, ViewReservationState> states_;",
    "std::map<PrismaUI::Web::ViewId, PrismaUI::Web::ViewHealth> health_;",
    "std::map<PrismaUI::Web::ViewId, std::pair<std::string, std::string>> metadata_;",
):
    assert field in header

assert "std::function<bool()> canAccept" in header
assert "if (canAccept && !canAccept()) return 0;" in source
assert "states_[id] = ViewReservationState::Reserved;" in source
assert "metadata_.erase(id);" in source
assert "if (report) report(id, PrismaUI::Web::ViewHealth::LoadFailed" in source
assert "if (onReady) onReady(0);" in source
assert source.index("if (report) report") < source.index("if (onReady) onReady(0);")

failure = views[views.index("if (!record->view)"):views.index("if (IsViewDestroyRequested(id))", views.index("if (!record->view)"))]
assert "FailViewCreation(id, std::move(record->domReady)," in failure
assert "FailViewCreation(id, std::move(onReady)," not in failure

assert "struct ViewRecord" not in backend
assert "viewStates_" not in backend
assert "publishedHealth_" not in backend
assert "publishedMetadata_" not in backend
assert "std::map<ViewId, std::unique_ptr<ViewRecord>> records_" not in backend

print("Ultralight view manager boundary guard passed")
