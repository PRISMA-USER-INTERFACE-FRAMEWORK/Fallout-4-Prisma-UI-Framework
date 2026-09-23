#pragma once

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

}
