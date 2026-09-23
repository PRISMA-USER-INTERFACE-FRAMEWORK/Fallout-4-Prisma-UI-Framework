#include "UltralightDispatchQueue.h"

#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    void UltralightDispatchQueue::Start() noexcept {
        stopping_.store(false, std::memory_order_release);
        frameRequested_.store(false, std::memory_order_release);
    }

    void UltralightDispatchQueue::Stop() noexcept {
        stopping_.store(true, std::memory_order_release);
        condition_.notify_all();
    }

    bool UltralightDispatchQueue::IsStopping() const noexcept {
        return stopping_.load(std::memory_order_acquire);
    }

    bool UltralightDispatchQueue::IsOwnerThread() const noexcept {
        std::lock_guard lock(mutex_);
        return std::this_thread::get_id() == ownerId_;
    }

    void UltralightDispatchQueue::SetOwnerThread() noexcept {
        std::lock_guard lock(mutex_);
        ownerId_ = std::this_thread::get_id();
    }

    void UltralightDispatchQueue::Dispatch(Task task) {
        if (IsOwnerThread()) {
            task();
            return;
        }
        (void)Enqueue(std::move(task));
    }

    bool UltralightDispatchQueue::DispatchDeferred(Task task) {
        return Enqueue(std::move(task));
    }

    void UltralightDispatchQueue::WaitForWork() {
        std::unique_lock lock(mutex_);
        condition_.wait(lock, [this] {
            return stopping_.load(std::memory_order_acquire) || !tasks_.empty() ||
                   frameRequested_.load(std::memory_order_acquire);
        });
    }

    bool UltralightDispatchQueue::TryPop(Task& task) {
        std::lock_guard lock(mutex_);
        if (tasks_.empty()) return false;
        task = std::move(tasks_.front());
        tasks_.pop();
        return true;
    }

    bool UltralightDispatchQueue::FrameRequested() const noexcept {
        return frameRequested_.load(std::memory_order_acquire);
    }

    bool UltralightDispatchQueue::ConsumeFrameRequest() noexcept {
        return frameRequested_.exchange(false, std::memory_order_acq_rel);
    }

    void UltralightDispatchQueue::RequestFrame() noexcept {
        frameRequested_.store(true, std::memory_order_release);
    }

    void UltralightDispatchQueue::NotifyOne() noexcept {
        condition_.notify_one();
    }

    void UltralightDispatchQueue::NotifyAll() noexcept {
        condition_.notify_all();
    }

    bool UltralightDispatchQueue::Enqueue(Task task) {
        {
            std::lock_guard lock(mutex_);
            if (stopping_.load(std::memory_order_acquire)) return false;
            tasks_.emplace(std::move(task));
        }
        condition_.notify_one();
        return true;
    }

}
