#include "ViewNetworkPolicy.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <mutex>
#include <utility>

namespace PrismaUI::Engine::ViewNetworkPolicy {

namespace {

struct Entry {
    int policy = kDefaultPolicy;
    std::string originHost;
};

std::mutex g_mutex;
std::map<std::uint64_t, Entry> g_entries;

}

void Set(std::uint64_t view, int policy) {
    if (view == 0 || policy < kUnrestricted || policy > kRemoteNoFile) return;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_entries[view].policy = policy;
}

int Get(std::uint64_t view) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto it = g_entries.find(view);
    return it == g_entries.end() ? kDefaultPolicy : it->second.policy;
}

void SetOriginHost(std::uint64_t view, std::string host) {
    if (view == 0 || host.empty()) return;
    std::transform(host.begin(), host.end(), host.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::lock_guard<std::mutex> lock(g_mutex);
    Entry& entry = g_entries[view];
    if (entry.originHost.empty()) entry.originHost = std::move(host);
}

std::string GetOriginHost(std::uint64_t view) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto it = g_entries.find(view);
    return it == g_entries.end() ? std::string{} : it->second.originHost;
}

void Forget(std::uint64_t view) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_entries.erase(view);
}

}
