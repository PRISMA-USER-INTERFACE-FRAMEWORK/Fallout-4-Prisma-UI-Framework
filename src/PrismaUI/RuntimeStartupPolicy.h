#pragma once

#include "../PrismaUI_F4_Modern_API.h"
#include <atomic>

namespace PrismaUI::RuntimeStartupPolicy {
    inline PRISMA_UI_FLAT_API::RuntimeState State(bool failed, bool active, bool dispatcher, bool requested) noexcept {
        if (failed) return PRISMA_UI_FLAT_API::RuntimeState::Failed;
        if (active && dispatcher) return PRISMA_UI_FLAT_API::RuntimeState::Ready;
        return requested || active ? PRISMA_UI_FLAT_API::RuntimeState::Starting : PRISMA_UI_FLAT_API::RuntimeState::Waiting;
    }
    class Request {
    public:
        bool Claim(bool eligible) noexcept { return eligible && !claimed_.exchange(true, std::memory_order_acq_rel); }
        bool Claimed() const noexcept { return claimed_.load(std::memory_order_acquire); }
    private:
        std::atomic_bool claimed_{false};
    };
}
