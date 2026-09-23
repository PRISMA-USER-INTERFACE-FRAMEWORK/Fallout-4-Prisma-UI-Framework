#include "PCH.h"

#include "VR/VRNetworkPolicy.h"

#include "PrismaUI/WebRuntime.h"

#include <map>
#include <mutex>

namespace PrismaUI::VRNetworkPolicy
{
    namespace
    {
        using PRISMA_UI_VR_API::NetworkAccessPolicy;

        std::mutex g_mutex;
        std::map<VR::PrismaViewId, NetworkAccessPolicy> g_policies;

        [[nodiscard]] bool Apply(
            VR::PrismaViewId viewId,
            NetworkAccessPolicy policy) noexcept
        {
            if (!WebRuntime::IsValid(viewId)) {
                return false;
            }
            return WebRuntime::SetViewNetworkPolicy(viewId, static_cast<int>(policy));
        }
    }

    bool Set(
        VR::PrismaViewId viewId,
        NetworkAccessPolicy policy) noexcept
    {
        if (viewId == 0 ||
            policy < NetworkAccessPolicy::Unrestricted ||
            policy > NetworkAccessPolicy::RemoteNoFile) {
            return false;
        }
        if (!Apply(viewId, policy)) {
            return false;
        }
        try {
            std::lock_guard lock(g_mutex);
            g_policies[viewId] = policy;
        } catch (...) {
            return false;
        }
        return true;
    }

    bool Get(
        VR::PrismaViewId viewId,
        NetworkAccessPolicy* outPolicy) noexcept
    {
        if (!outPolicy) {
            return false;
        }
        int hostPolicy = 0;
        if (WebRuntime::GetViewNetworkPolicy(viewId, hostPolicy)) {
            *outPolicy = static_cast<NetworkAccessPolicy>(hostPolicy);
            return true;
        }
        try {
            std::lock_guard lock(g_mutex);
            const auto it = g_policies.find(viewId);
            *outPolicy = it != g_policies.end() ?
                it->second :
                NetworkAccessPolicy::RemoteNoFile;
        } catch (...) {
            return false;
        }
        return true;
    }

    void ReapplyOnDomReady(VR::PrismaViewId viewId) noexcept
    {
        NetworkAccessPolicy policy = NetworkAccessPolicy::RemoteNoFile;
        {
            try {
                std::lock_guard lock(g_mutex);
                const auto it = g_policies.find(viewId);
                if (it == g_policies.end()) {
                    return;
                }
                policy = it->second;
            } catch (...) {
                return;
            }
        }
        (void)Apply(viewId, policy);
    }

    void Forget(VR::PrismaViewId viewId) noexcept
    {
        try {
            std::lock_guard lock(g_mutex);
            g_policies.erase(viewId);
        } catch (...) {
        }
    }

    bool IsEnforceable() noexcept
    {
        return WebRuntime::IsHostLoaded() && WebRuntime::SupportsViewNetworkPolicy();
    }
}
