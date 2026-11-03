#pragma once

#include <cstdint>

namespace PrismaUI::MenuCursorDiagnostics {

void OnFocusedCaptureArmed(std::uint64_t view);
void OnFocusedCaptureReleased();
void OnPresentCursorSample();

}
