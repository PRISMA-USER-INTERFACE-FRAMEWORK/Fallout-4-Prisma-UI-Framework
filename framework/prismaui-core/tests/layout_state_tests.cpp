#include "PrismaUI/LayoutState.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {
    int failures = 0;

    void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    }

    using namespace PrismaUI::Layout;
}

int main() {
    Check(NormalizeKey("\\Interface\\MyHud//Index.HTML") == "interface/myhud/index.html",
          "keys are lower case, forward slash and collapsed");

    const Rect clamped = Clamp({-5000.0f, 5000.0f, 1.0f, 99999.0f});
    Check(clamped.w == kMinSize && clamped.h == kStageHeight * 2.0f, "size is clamped to its limits");
    Check(clamped.x >= kMinSize - clamped.w && clamped.y <= kStageHeight - kMinSize,
          "some part of the rectangle stays on the stage");
    Check(IsFullStage(Rect{}) && !IsFullStage(Rect{0, 0, 960, 1080}), "full stage detection");

    const Dest dest = ToDest({480, 270, 960, 540});
    Check(std::fabs(dest.left - 0.25f) < 1e-6f && std::fabs(dest.bottom - 0.75f) < 1e-6f,
          "stage rectangle converts to window fractions");

    int mx = 0, my = 0;
    Check(MapPoint({480, 270, 960, 540}, 960, 540, 1920, 1080, mx, my) && mx == 960 && my == 540,
          "the middle of the rectangle maps to the middle of the page");
    Check(MapPoint({480, 270, 960, 540}, 480, 270, 1920, 1080, mx, my) && mx == 0 && my == 0,
          "the rectangle corner maps to the page origin");
    Check(!MapPoint({0, 0, 0, 0}, 1, 1, 1920, 1080, mx, my), "a degenerate rectangle refuses to map");
    Check(!MapPoint({}, 1, 1, 0, 0, mx, my), "an unknown window size refuses to map");

    State state;
    Check(!state.SetRect(1, {}) && !state.SetHidden(1, true) && !state.Reset(1), "unknown views are refused");
    state.Track(1, "interface/a/index.html", "A.dll", "A");
    state.Track(2, "interface/b/index.html", "B.dll", "");
    Entry entry;
    Check(state.Get(2, entry) && entry.title == "interface/b/index.html", "an empty title falls back to the key");
    const auto before = state.Generation();
    Check(state.SetRect(1, {100, 50, 640, 360}) && state.Generation() > before, "a move bumps the generation");
    const auto afterMove = state.Generation();
    Check(state.SetRect(1, {100, 50, 640, 360}) && state.Generation() == afterMove, "an unchanged move does not");
    Check(!state.SetRect(1, {std::nanf(""), 0, 100, 100}), "non-finite rectangles are rejected");
    Check(state.SetHidden(2, true) && state.UserHidden(2) && !state.UserHidden(1), "hide is per view");
    Rect placed;
    Check(state.Placement(1, placed) && placed.w == 640.0f && !state.Placement(2, placed),
          "only moved views have a placement");

    const Rect registeredDefault{0, 0, 800, 600};
    Check(state.Register(1, "Alpha", &registeredDefault) && state.Get(1, entry) && entry.registered &&
              entry.title == "Alpha" && entry.defaultRect.w == 800.0f && entry.rect.w == 640.0f,
          "registering keeps a player's moved rectangle and sets the default");
    Check(state.Reset(1) && state.Get(1, entry) && entry.rect.w == 800.0f, "reset returns to the registered default");
    Check(state.Unregister(1) && !state.Unregister(1) && !state.Register(99, "x", nullptr),
          "unregister and unknown register behave");

    state.SetRect(1, {300, 200, 500, 400});
    state.SetSettings({20, true});
    const std::string saved = state.Serialize();
    Check(saved.find("\"version\":1") != std::string::npos && saved.find("interface/a/index.html") != std::string::npos &&
              saved.find("interface/b/index.html") != std::string::npos,
          "serialization writes the version and every changed view");

    State fresh;
    Check(fresh.Load(saved), "a saved file loads");
    Check(fresh.GetSettings().snap == 20 && fresh.GetSettings().grid, "editor settings round trip");
    fresh.Track(7, "interface/a/index.html", "A.dll", "A");
    fresh.Track(8, "interface/b/index.html", "B.dll", "B");
    Check(fresh.Get(7, entry) && entry.rect.x == 300.0f && entry.rect.h == 400.0f,
          "a layout saved for a view applies when the view is created later");
    Check(fresh.UserHidden(8) && !fresh.UserHidden(7), "a hidden view stays hidden after a reload");
    fresh.Untrack(8);
    fresh.Track(9, "interface/b/index.html", "B.dll", "B");
    Check(fresh.UserHidden(9), "the hide survives a view being destroyed and created again");

    Check(!fresh.Load("{\"views\":{}}"), "a file without a version is refused");
    Check(!fresh.Load("{\"version\":1,"), "a truncated file is refused");
    Check(!fresh.Load(std::string(kMaxFileBytes + 1, ' ')), "an oversized file is refused");
    Check(fresh.Get(7, entry) && entry.rect.x == 300.0f, "a refused file leaves the layout unchanged");

    Check(fresh.Load("{\"version\":9,\"future\":[1,2,{\"a\":null}],\"views\":{\"Interface\\\\A\\\\Index.html\":"
                     "{\"x\":10,\"y\":20,\"w\":400,\"h\":300,\"hidden\":false,\"extra\":true}}}"),
          "a newer additive file still loads");
    Check(fresh.Get(7, entry) && entry.rect.x == 10.0f && entry.rect.w == 400.0f && !fresh.UserHidden(9),
          "known fields apply from a newer file and views missing from it return to default");
    Check(fresh.Serialize().find("future") == std::string::npos, "unknown fields are dropped on the next write");

    Check(fresh.Load("{\"version\":1,\"views\":{\"interface/a/index.html\":{\"x\":0,\"y\":0,\"w\":99999,\"h\":-4}}}") &&
              fresh.Get(7, entry) && entry.rect.w == kStageWidth * 2.0f && entry.rect.h == kMinSize,
          "loaded rectangles are clamped");

    fresh.SetRect(7, {10, 10, 100, 100});
    fresh.ClearDirty();
    fresh.ResetAll();
    Check(fresh.Dirty(), "reset all marks the layout dirty so closing the editor saves it");
    Check(fresh.Get(7, entry) && IsFullStage(entry.rect) && fresh.Serialize().find("interface/a") == std::string::npos,
          "reset all clears live and saved layouts");

    Check(fresh.Load("{\"version\":1,\"views\":{\"interface/a/index.html\":{\"x\":1e300,\"y\":0,\"w\":10,\"h\":10,\"hidden\":true}}}") &&
              fresh.UserHidden(7) && fresh.Get(7, entry) && IsFullStage(entry.rect),
          "an out of range number drops the rectangle instead of casting it");
    Check(!fresh.FutureVersionLoaded() && fresh.Load("{\"version\":2,\"views\":{}}") && fresh.FutureVersionLoaded() &&
              fresh.Load("{\"version\":1,\"views\":{}}") && !fresh.FutureVersionLoaded(),
          "a file from a newer version is reported so the writer can keep a backup");

    Check(TruncateUtf8("abÃ©", 3) == "ab" && TruncateUtf8("abc", 3) == "abc" && TruncateUtf8("Ã©", 1).empty(),
          "truncation never cuts a UTF-8 sequence");
    State capped;
    capped.Track(1, std::string(kMaxKeyBytes + 50, 'k'), "O", "");
    Check(capped.Get(1, entry) && entry.key.size() == kMaxKeyBytes, "an over long key is capped");
    for (std::uint64_t i = 0; i < kMaxSavedViews + 20; ++i) {
        capped.Track(100 + i, "interface/v" + std::to_string(i), "O", "");
        capped.SetHidden(100 + i, true);
        capped.Untrack(100 + i);
    }
    capped.Track(9999, "interface/live", "O", "");
    Check(capped.SetHidden(9999, true) && capped.Serialize().find("interface/live") != std::string::npos,
          "a live view can still be saved when the saved table is full of dormant entries");

    Check(!state.EditMode() && state.SetEditMode(true) && state.EditMode() && !state.SetEditMode(true),
          "edit mode reports only real transitions");

    if (failures) {
        std::cerr << failures << " layout state check(s) failed\n";
        return 1;
    }
    std::cout << "layout state tests passed\n";
    return 0;
}
