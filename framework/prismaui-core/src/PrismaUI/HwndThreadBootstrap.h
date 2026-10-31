#pragma once

#include <Windows.h>

namespace PrismaUI::HwndThreadBootstrap {

using InstallCallback = bool (*)(HWND);

bool Queue(HWND hwnd, InstallCallback callback) noexcept;
void Cancel(HWND hwnd = nullptr) noexcept;

}
