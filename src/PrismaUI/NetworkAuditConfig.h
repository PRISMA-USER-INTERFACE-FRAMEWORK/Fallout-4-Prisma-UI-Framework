#pragma once

#include <windows.h>

#include <string>

namespace PrismaUI::NetworkAuditConfig {

inline bool LogRequests(const std::wstring& iniPath) {
    return ::GetPrivateProfileIntW(L"Network", L"bLogNetworkRequests", 0, iniPath.c_str()) != 0;
}

}
