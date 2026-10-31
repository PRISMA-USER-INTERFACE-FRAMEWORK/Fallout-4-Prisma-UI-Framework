#pragma once

#include <atomic>
#include <mutex>

namespace PrismaUI::ModelPreview {

class ShutdownGate {
public:
    [[nodiscard]] std::unique_lock<std::mutex> EnterTick() {
        if (started_.load()) return {};
        std::unique_lock lock(mutex_, std::try_to_lock);
        if (lock.owns_lock() && started_.load()) lock.unlock();
        return lock;
    }

    [[nodiscard]] std::unique_lock<std::mutex> BeginShutdown() {
        started_.store(true);
        return std::unique_lock<std::mutex>(mutex_);
    }

private:
    std::mutex mutex_;
    std::atomic<bool> started_{ false };
};

inline ShutdownGate g_shutdownGate;

}
