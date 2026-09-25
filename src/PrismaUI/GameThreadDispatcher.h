#pragma once

#include <Windows.h>

#include <cstdint>
#include <functional>

namespace PrismaUI::GameThreadDispatcher {

void CaptureCurrentThread() noexcept;
bool AttachWindow(HWND hwnd) noexcept;
void DetachWindow(HWND hwnd) noexcept;

bool Dispatch(std::function<void()> task, uint64_t view = 0);

bool DispatchSafety(std::function<void()> task);
void DropView(uint64_t view) noexcept;

bool IsGameThread() noexcept;
bool IsReady() noexcept;

bool HandleWindowMessage(HWND hwnd, UINT message) noexcept;
UINT MessageId() noexcept;

}
