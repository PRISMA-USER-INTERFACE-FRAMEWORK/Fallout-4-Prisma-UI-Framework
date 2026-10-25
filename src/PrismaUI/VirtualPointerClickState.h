#pragma once

#include <cstdint>

namespace PrismaUI::VirtualPointerClick {

struct Owner {
    std::uint64_t view = 0;
    std::uint64_t generation = 0;
};

constexpr bool operator==(Owner a, Owner b) noexcept { return a.view == b.view && a.generation == b.generation; }
constexpr bool ValidOwner(Owner owner) noexcept { return owner.view != 0 && owner.generation != 0; }

struct State {
    Owner owner{};
    bool held = false;
    bool hasSample = false;
    std::uint32_t movesLogged = 0;
};

enum class Begin : std::uint8_t { kRejected, kAdmitted };
enum class End : std::uint8_t { kNothing, kReleaseOwner, kDiscarded };

constexpr std::uint32_t kMoveLogLimit = 12;

inline Begin BeginClick(State& state, bool routingAdmitted, Owner current) noexcept
{
    if (state.held) return Begin::kRejected;
    if (!routingAdmitted) return Begin::kRejected;
    if (!ValidOwner(current)) return Begin::kRejected;
    state.owner = current;
    state.held = true;
    return Begin::kAdmitted;
}

inline End EndClick(State& state, Owner current) noexcept
{
    if (!state.held) return End::kNothing;
    const bool stillOwner = ValidOwner(current) && state.owner == current;
    state.held = false;
    state.owner = Owner{};
    return stillOwner ? End::kReleaseOwner : End::kDiscarded;
}

inline bool CancelClick(State& state, Owner current) noexcept
{
    if (!state.held) return false;
    const bool ownsTarget = ValidOwner(state.owner) && state.owner == current;
    state.held = false;
    state.owner = Owner{};
    return ownsTarget;
}

inline bool DiscardIfOwnerView(State& state, std::uint64_t view) noexcept
{
    if (!state.held || view == 0 || state.owner.view != view) return false;
    state.held = false;
    state.owner = Owner{};
    return true;
}

inline bool RecordMove(State& state) noexcept
{
    if (state.movesLogged >= kMoveLogLimit) return false;
    ++state.movesLogged;
    return true;
}

inline void ResetSession(State& state) noexcept
{
    state.owner = Owner{};
    state.held = false;
    state.hasSample = false;
    state.movesLogged = 0;
}

}
