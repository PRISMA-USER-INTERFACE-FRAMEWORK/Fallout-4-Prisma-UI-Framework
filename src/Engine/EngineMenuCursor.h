#pragma once

#include <cstdint>

#include "RE/M/MenuCursor.h"

namespace PrismaUI::Engine {

struct MenuCursorSnapshot {
    bool valid = false;
    std::int32_t cursorPosX = 0;
    std::int32_t cursorPosY = 0;
    std::int32_t minCursorX = 0;
    std::int32_t minCursorY = 0;
    std::int32_t maxCursorX = 0;
    std::int32_t maxCursorY = 0;
    float leftConstraintPct = 0.0F;
    float rightConstraintPct = 0.0F;
    float topConstraintPct = 0.0F;
    float bottomConstraintPct = 0.0F;
    std::uint32_t registeredCursors = 0;
    bool forceOSCursorPos = false;
    bool allowGamepadCursorOverride = false;
};

static_assert(sizeof(RE::MenuCursor) == 0x58, "MenuCursor EngineCompat layout changed");

[[nodiscard]] inline MenuCursorSnapshot SnapshotMenuCursor() noexcept
{
    const auto* cursor = RE::MenuCursor::GetSingleton();
    if (!cursor) return {};

    MenuCursorSnapshot snapshot;
    snapshot.valid = true;
    snapshot.cursorPosX = cursor->cursorPosX;
    snapshot.cursorPosY = cursor->cursorPosY;
    snapshot.minCursorX = cursor->minCursorX;
    snapshot.minCursorY = cursor->minCursorY;
    snapshot.maxCursorX = cursor->maxCursorX;
    snapshot.maxCursorY = cursor->maxCursorY;
    snapshot.leftConstraintPct = cursor->leftConstraintPct;
    snapshot.rightConstraintPct = cursor->rightConstraintPct;
    snapshot.topConstraintPct = cursor->topConstraintPct;
    snapshot.bottomConstraintPct = cursor->bottomConstraintPct;
    snapshot.registeredCursors = cursor->registeredCursors;
    snapshot.forceOSCursorPos = cursor->forceOSCursorPos;
    snapshot.allowGamepadCursorOverride = cursor->allowGamepadCursorOverride;
    return snapshot;
}

}
