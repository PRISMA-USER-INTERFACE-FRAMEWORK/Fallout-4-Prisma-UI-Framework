#include "PCH.h"
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

    bool UltralightDispatchQueue::Dispatch(Task task) {
        if (IsOwnerThread()) {
            task();
            return true;
        }
        return Enqueue(std::move(task));
    }

    bool UltralightDispatchQueue::DispatchDeferred(Task task) {
        return Enqueue(std::move(task));
    }

    void UltralightDispatchQueue::DrainAbandoned() {
        std::queue<Entry> abandoned;
        {
            std::lock_guard lock(mutex_);
            abandoned.swap(tasks_);
        }
    }

    void UltralightDispatchQueue::WaitForWork() {
        std::unique_lock lock(mutex_);
        condition_.wait(lock, [this] {
            return stopping_.load(std::memory_order_acquire) || !tasks_.empty() ||
                   frameRequested_.load(std::memory_order_acquire);
        });
    }

    bool UltralightDispatchQueue::TryPop(Task& task) {
        Entry entry;
        {
            std::lock_guard lock(mutex_);
            if (tasks_.empty()) return false;
            entry = std::move(tasks_.front());
            tasks_.pop();
        }
        const auto now = std::chrono::steady_clock::now();
        ownerWait_.Record(entry.enqueuedAt, now);
        QueueLatency::Snapshot due;
        if (ownerWait_.TakeDue(now, due)) {
            logger::info("[Latency] ultralight-owner n={} avg={:.3f}ms max={:.3f}ms", due.count, due.AverageMs(),
                         due.MaxMs());
        }
        task = std::move(entry.task);
        return true;
    }

    std::size_t UltralightDispatchQueue::QueuedCount() const noexcept {
        std::lock_guard lock(mutex_);
        return tasks_.size();
    }

    void UltralightDispatchQueue::RecordPassDepth() {
        ownerDepth_.RecordValue(QueuedCount());
        const auto now = std::chrono::steady_clock::now();
        QueueLatency::Snapshot due;
        if (ownerDepth_.TakeDue(now, due)) {
            logger::info("[Latency] ultralight-owner-depth n={} avg={:.2f} max={}", due.count,
                         static_cast<double>(due.sumNs) / static_cast<double>(due.count), due.maxNs);
        }
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
        const auto enqueuedAt = std::chrono::steady_clock::now();
        {
            std::lock_guard lock(mutex_);
            if (stopping_.load(std::memory_order_acquire)) return false;
            tasks_.emplace(Entry{std::move(task), enqueuedAt});
        }
        condition_.notify_one();
        return true;
    }

}
