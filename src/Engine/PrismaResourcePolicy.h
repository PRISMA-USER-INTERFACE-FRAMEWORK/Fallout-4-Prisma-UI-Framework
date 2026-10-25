#pragma once

#include <string_view>

namespace PrismaUI::Engine::PrismaResourcePolicy {

inline bool IsAllowedInitialShellChildNavigation(const bool isInitialUnownedShellChild,
                                                 const std::string_view targetHost) {
    return isInitialUnownedShellChild && targetHost == "shell";
}

}
