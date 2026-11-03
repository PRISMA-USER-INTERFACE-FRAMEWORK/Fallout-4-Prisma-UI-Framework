#pragma once

#include <cstdint>

namespace PrismaUI::VirtualPointer {

void OnStick(bool rightStick, float x, float y) noexcept;
bool OnButton(std::uint32_t buttonCode, bool pressed, bool released) noexcept;
void CancelHeldClick() noexcept;
void DiscardHeldClick(std::uint64_t view) noexcept;
void ResetSession() noexcept;

}
