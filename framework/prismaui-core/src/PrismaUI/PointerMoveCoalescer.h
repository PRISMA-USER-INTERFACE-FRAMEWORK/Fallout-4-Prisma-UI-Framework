#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

namespace PrismaUI::PointerMoveCoalescer {

    template <class Input>
    class Coalescer {
    public:
        struct Pending {
            Input latest{};
        };
        using Ticket = std::shared_ptr<Pending>;

        Ticket Submit(std::uint64_t view, const Input& move) {
            std::lock_guard lock(mutex_);
            auto& open = open_[view];
            if (open) {
                open->latest = move;
                return {};
            }
            open = std::make_shared<Pending>();
            open->latest = move;
            return open;
        }

        Input Take(std::uint64_t view, const Ticket& ticket) {
            std::lock_guard lock(mutex_);
            const Input latest = ticket->latest;
            CloseLocked(view, ticket);
            return latest;
        }

        void Abandon(std::uint64_t view, const Ticket& ticket) {
            std::lock_guard lock(mutex_);
            CloseLocked(view, ticket);
        }

        void Barrier(std::uint64_t view) {
            std::lock_guard lock(mutex_);
            open_.erase(view);
        }

    private:
        void CloseLocked(std::uint64_t view, const Ticket& ticket) {
            const auto it = open_.find(view);
            if (it != open_.end() && it->second == ticket) open_.erase(it);
        }

        std::mutex mutex_;
        std::map<std::uint64_t, Ticket> open_;
    };

}
