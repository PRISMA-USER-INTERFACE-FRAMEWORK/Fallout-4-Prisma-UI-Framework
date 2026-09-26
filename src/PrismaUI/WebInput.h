#pragma once

#include <windows.h>

#include <cstdint>

namespace PrismaUI::WebInput {

bool Install(HWND gameHwnd);
bool QueueInstall(HWND gameHwnd);
bool IsInstalled();
#ifndef PRISMAUI_FO4VR
bool QueueDispatcherReattach();
#endif
void Uninstall();

bool GetClientScreenRect(int& outX, int& outY, int& outWidth, int& outHeight);

void SetCaptureActive(bool active, bool swallow);
bool IsCaptureActive();
bool IsCursorOwned();

bool SetViewCursorPolicy(std::uint64_t view, std::uint32_t policy);
std::uint32_t GetViewCursorPolicy(std::uint64_t view);

void SetEscapeOwned(bool owned);

int GetLastCursorX();
int GetLastCursorY();

int GetTrackedCursorX();
int GetTrackedCursorY();

}
