#pragma once

#include <cstdint>

namespace PrismaUI::MenuCursorDiagnostics {

#if defined(PRISMAUI_FO4VR)
inline void OnFocusedCaptureArmed(std::uint64_t) {}
inline void OnFocusedCaptureReleased() {}
inline void OnPresentCursorSample() {}
#else
void OnFocusedCaptureArmed(std::uint64_t view);
void OnFocusedCaptureReleased();
void OnPresentCursorSample();
#endif

}
