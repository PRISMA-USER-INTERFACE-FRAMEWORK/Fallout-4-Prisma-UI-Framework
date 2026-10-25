#pragma once

#include <cmath>
#include <cstdint>

namespace PrismaUI::OffscreenFrameState {

struct Snapshot {
    std::uint64_t epoch = 0;
    std::uint64_t lastContentGen = 0;
    bool haveContent = false;
    bool hasTexture = false;
};

inline bool NeedsCopy(const Snapshot& state, bool haveGenerationApi, std::uint64_t generation) noexcept {
    if (!haveGenerationApi) return true;
    return !state.hasTexture || !state.haveContent || state.lastContentGen != generation;
}

inline bool CanPublish(std::uint64_t liveEpoch, std::uint64_t snapshotEpoch) noexcept {
    return liveEpoch != 0 && liveEpoch == snapshotEpoch;
}

inline bool ShouldReleaseKeyedMutex(bool haveMutex, bool acquired) noexcept {
    return haveMutex && acquired;
}

inline bool ResolveSourceCoordinate(float uv, std::uint32_t extent, std::uint32_t& coordinate) noexcept {
    const double scaled = static_cast<double>(uv) * static_cast<double>(extent);
    if (!std::isfinite(scaled)) return false;
    const long long rounded = std::llround(scaled);
    const double tolerance = std::fmax(0.0001, std::ldexp(static_cast<double>(extent), -23));
    if (rounded < 0 || rounded > static_cast<long long>(extent) ||
        std::abs(scaled - static_cast<double>(rounded)) > tolerance) {
        return false;
    }
    coordinate = static_cast<std::uint32_t>(rounded);
    return true;
}

}
