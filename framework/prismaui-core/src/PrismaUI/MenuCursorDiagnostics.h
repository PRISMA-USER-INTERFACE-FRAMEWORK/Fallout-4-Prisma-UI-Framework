#pragma once

#include <cstdint>

namespace PrismaUI::MenuCursorDiagnostics {

void OnFocusedCaptureArmed(std::uint64_t view);
void OnFocusedCaptureReleased();
void OnPresentCursorSample();

void OnFocusEpisodeBegan(std::uint64_t view, std::uint64_t generation);
void OnFocusRegistered(std::uint64_t view, std::uint64_t generation);
void OnFocusedMouseMove();
void OnFocusEpisodeEnded();

void OnSuppressedCursorMenuOpened(const char* menuName);

}
