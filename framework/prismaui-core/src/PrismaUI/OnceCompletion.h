#pragma once

#include <atomic>
#include <functional>
#include <tuple>
#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    template <typename... Args>
    class OnceCompletion final {
    public:
        using Callback = std::function<void(Args...)>;

        OnceCompletion(Callback callback, Args... abandoned)
            : callback_(std::move(callback)), abandoned_(std::move(abandoned)...) {}
        OnceCompletion(const OnceCompletion&) = delete;
        OnceCompletion& operator=(const OnceCompletion&) = delete;

        ~OnceCompletion() {
            if (completed_.exchange(true, std::memory_order_acq_rel)) return;
            try {
                std::apply(callback_, abandoned_);
            } catch (...) {
            }
        }

        void Complete(Args... args) {
            if (completed_.exchange(true, std::memory_order_acq_rel)) return;
            callback_(std::move(args)...);
        }

        [[nodiscard]] Callback Take() {
            if (completed_.exchange(true, std::memory_order_acq_rel)) return {};
            return std::move(callback_);
        }

    private:
        Callback callback_;
        std::tuple<Args...> abandoned_;
        std::atomic_bool completed_{false};
    };

}
