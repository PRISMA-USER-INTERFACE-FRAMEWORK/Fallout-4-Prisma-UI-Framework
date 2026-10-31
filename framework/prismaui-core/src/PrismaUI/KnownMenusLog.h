#pragma once

#include "KnownMenus.h"

#include <mutex>
#include <set>
#include <string>
#include <string_view>

namespace PrismaUI::KnownMenus
{
    inline void LogMenuPolicyOnce(std::string_view a_api, std::string_view a_menu, Mechanism a_used)
    {
        static std::mutex            s_mutex;
        static std::set<std::string> s_seen;
        {
            std::lock_guard lock{ s_mutex };
            if (!s_seen.insert(std::string{ a_api } + ":" + std::string{ a_menu }).second) {
                return;
            }
        }

        const auto* policy = Find(a_menu);
        if (!policy) {
            logger::info("VanillaUISuppressor: {}('{}'): no KnownMenus policy entry; applying anyway. "
                         "Check the MENU_NAME spelling against menu-suppression-policy.md.",
                         a_api, a_menu);
        } else if (policy->mechanism != a_used) {
            logger::warn("VanillaUISuppressor: {}('{}') uses {} but the policy expects {}: {}",
                         a_api, a_menu, MechanismName(a_used), MechanismName(policy->mechanism), policy->why);
        }
    }
}
