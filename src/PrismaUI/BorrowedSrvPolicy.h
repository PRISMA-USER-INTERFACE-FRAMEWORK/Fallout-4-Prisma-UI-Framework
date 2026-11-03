#pragma once

namespace PrismaUI::BorrowedSrvPolicy {

    enum class Owner { PrismaOwned, EngineOwned };
    enum class Decision { Retain, Reject };

    [[nodiscard]] constexpr Decision Decide(Owner owner, bool sameDevice) noexcept {
        return owner == Owner::PrismaOwned && sameDevice ? Decision::Retain : Decision::Reject;
    }

}
