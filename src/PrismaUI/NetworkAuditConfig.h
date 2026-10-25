#pragma once

#include <windows.h>

#include <atomic>
#include <string>
#include <string_view>

namespace PrismaUI::NetworkAuditConfig {

inline std::atomic<bool> g_enabled{false};

inline bool Enabled() { return g_enabled.load(std::memory_order_relaxed); }

inline void SetEnabled(bool on) { g_enabled.store(on, std::memory_order_relaxed); }

inline bool LogRequests(const std::wstring& iniPath) {
    return ::GetPrivateProfileIntW(L"Network", L"bLogNetworkRequests", 0, iniPath.c_str()) != 0;
}

inline std::string_view WithoutQuery(std::string_view url) {
    return url.substr(0, url.find_first_of("?#"));
}

}
