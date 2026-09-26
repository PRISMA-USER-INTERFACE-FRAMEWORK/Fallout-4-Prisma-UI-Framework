#pragma once

#include <cstdint>
#include <utility>

namespace PrismaUI::GPU::GenerationDrain {

enum class Activation { Unavailable, ContextMismatch, DrainFailed, Activated };

template <class Drain>
Activation ActivatePublished(uint64_t completedGeneration, uint64_t& activeGeneration, bool accelerated,
                             bool contextMatches, Drain&& drain)
{
    if (completedGeneration == 0 || activeGeneration != 0) return Activation::Unavailable;
    if (accelerated) {
        if (!contextMatches) return Activation::ContextMismatch;
        if (!std::forward<Drain>(drain)()) return Activation::DrainFailed;
    }
    activeGeneration = completedGeneration;
    return Activation::Activated;
}

inline void RetireFailed(uint64_t& completedGeneration, uint64_t& activeGeneration) noexcept
{
    completedGeneration = 0;
    activeGeneration = 0;
}

inline void EnterTerminalFailure(uint64_t& completedGeneration, uint64_t& activeGeneration, bool& acceptingFrames,
                                 bool& ready, bool& terminalGpuFailure) noexcept
{
    RetireFailed(completedGeneration, activeGeneration);
    acceptingFrames = false;
    ready = false;
    terminalGpuFailure = true;
}

[[nodiscard]] inline bool CanBeginPresent(bool acceptingFrames, bool ready, bool terminalGpuFailure) noexcept
{
    return acceptingFrames && ready && !terminalGpuFailure;
}

[[nodiscard]] inline bool OwnerMayWaitForRetirement(uint64_t completedGeneration, uint64_t activeGeneration,
                                                     bool terminalGpuFailure) noexcept
{
    return !terminalGpuFailure && (completedGeneration != 0 || activeGeneration != 0);
}

[[nodiscard]] inline bool HasPublishableGeneration(uint64_t generation) noexcept
{
    return generation != 0;
}

}
