#pragma once

#include <cstdint>
#include <string>

namespace PrismaUI::Engine::ViewNetworkPolicy {

enum Policy : int {
    kUnrestricted = 0,
    kLocalOnly = 1,
    kRemoteNoFile = 2,
};

inline constexpr int kDefaultPolicy = kRemoteNoFile;

void Set(std::uint64_t view, int policy);
[[nodiscard]] int Get(std::uint64_t view);

void SetOriginHost(std::uint64_t view, std::string host);
[[nodiscard]] std::string GetOriginHost(std::uint64_t view);

void Forget(std::uint64_t view);

}
