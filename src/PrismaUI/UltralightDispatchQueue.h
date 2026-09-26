#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace PrismaUI::WebRuntimeUltralight {

    class UltralightDispatchQueue final {
    public:
        using Task = std::function<void()>;

        void Start() noexcept;
        void Stop() noexcept;
        [[nodiscard]] bool IsStopping() const noexcept;
        [[nodiscard]] bool IsOwnerThread() const noexcept;
        void SetOwnerThread() noexcept;

        void Dispatch(Task task);
        [[nodiscard]] bool DispatchDeferred(Task task);

        void WaitForWork();
        [[nodiscard]] bool TryPop(Task& task);
        [[nodiscard]] bool FrameRequested() const noexcept;
        [[nodiscard]] bool ConsumeFrameRequest() noexcept;
        void RequestFrame() noexcept;
        void NotifyOne() noexcept;
        void NotifyAll() noexcept;

    private:
        [[nodiscard]] bool Enqueue(Task task);

        mutable std::mutex mutex_;
        std::condition_variable condition_;
        std::queue<Task> tasks_;
        std::thread::id ownerId_;
        std::atomic_bool stopping_{false};
        std::atomic_bool frameRequested_{false};
    };

}
