#pragma once

#include "WebInputWindow.h"
#include "Utf16Pairing.h"

#include <atomic>
#include <cstdint>

#include <windows.h>

namespace PrismaUI::WebInput {

inline Detail::WindowState g_window;
inline std::atomic<bool> g_capture{false};
inline std::atomic<bool> g_swallow{false};
inline std::atomic<bool> g_mouseInsideWebView{false};
inline std::atomic<uint32_t> g_mouseCaptureMask{0};
inline std::atomic<bool> g_escapeOwned{false};
inline std::atomic<int> g_lastCursorX{0};
inline std::atomic<int> g_lastCursorY{0};
inline PrismaUI::Utf16Pairing::Pending g_pendingSurrogate;

void OnWindowDestroyed(HWND hwnd);
void SyncImeAssociation(HWND hwnd);
bool PageOwnsImeUi();
LRESULT CALLBACK SubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

}
