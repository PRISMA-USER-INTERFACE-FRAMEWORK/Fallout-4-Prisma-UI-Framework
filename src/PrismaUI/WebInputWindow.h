#pragma once

#include <atomic>

namespace PrismaUI::WebInput::Detail {

    class WindowState {
    public:
        void Publish(void* handle) noexcept { handle_.store(handle, std::memory_order_release); }
        void Clear() noexcept { handle_.store(nullptr, std::memory_order_release); }
        void* Get() const noexcept { return handle_.load(std::memory_order_acquire); }
        bool Installed() const noexcept { return handle_.load(std::memory_order_acquire) != nullptr; }

    private:
        std::atomic<void*> handle_{nullptr};
    };

}
