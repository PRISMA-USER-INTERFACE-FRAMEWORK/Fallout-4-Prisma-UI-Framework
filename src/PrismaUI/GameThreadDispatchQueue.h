#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <utility>

namespace PrismaUI::GameThreadDispatchQueue {

inline constexpr std::size_t kMaxQueue = 1024;
inline constexpr std::size_t kMaxTasksPerWake = 64;

class Queue {
public:
    struct EnqueueResult {
        bool accepted = false;
        bool needsWake = false;
    };

    struct Task {
        uint64_t generation = 0;
        uint64_t view = 0;
        bool safety = false;
        std::function<void()> fn;
    };

    bool ready() const
    {
        std::lock_guard lock(mutex_);
        return ready_ && !failed_;
    }

    bool failed() const
    {
        std::lock_guard lock(mutex_);
        return failed_;
    }

    uint64_t generation() const
    {
        std::lock_guard lock(mutex_);
        return generation_;
    }

    bool isExecuting(uint64_t view) const
    {
        std::lock_guard lock(mutex_);
        return executing_ && executingView_ == view;
    }

    bool isCancelled(uint64_t view) const
    {
        std::lock_guard lock(mutex_);
        return view != 0 && cancelled_.contains(view);
    }

    bool draining() const
    {
        std::lock_guard lock(mutex_);
        return draining_;
    }

    void attach()
    {
        std::lock_guard lock(mutex_);
        if (failed_) return;
        ++generation_;
        ready_ = true;
        wakePending_ = false;
        draining_ = false;
        queue_.clear();
    }

    void detach()
    {
        std::lock_guard lock(mutex_);
        ++generation_;
        ready_ = false;
        wakePending_ = false;
        draining_ = false;
        queue_.clear();
        cv_.notify_all();
    }

    void failClosed()
    {
        std::lock_guard lock(mutex_);
        failClosedLocked();
        cv_.notify_all();
    }

    void noteWakeFailure() { failClosed(); }

    EnqueueResult tryEnqueue(std::function<void()> fn, uint64_t view = 0, bool safety = false)
    {
        if (!fn) return {};
        std::lock_guard lock(mutex_);
        if (failed_ || !ready_) return {};
        if (view != 0 && cancelled_.contains(view)) return {};
        if (queue_.size() >= kMaxQueue) {
            if (!safety || HasSafetyLocked()) return {};
        }
        const bool needsWake = !wakePending_ && !draining_;
        if (needsWake) wakePending_ = true;
        queue_.push_back(Task{generation_, view, safety, std::move(fn)});
        return {true, needsWake};
    }

    void dropView(uint64_t view)
    {
        if (view == 0) return;
        std::lock_guard lock(mutex_);
        cancelled_.insert(view);
        std::erase_if(queue_, [view](const Task& task) { return task.view == view; });
    }

    void beginDrain()
    {
        std::lock_guard lock(mutex_);
        draining_ = true;
        wakePending_ = false;
    }

    void endDrain()
    {
        std::lock_guard lock(mutex_);
        draining_ = false;
    }

    std::optional<Task> takeNext(const std::function<bool(uint64_t)>& viewLive)
    {
        std::lock_guard lock(mutex_);
        if (executing_) return std::nullopt;
        while (!queue_.empty()) {
            Task task = std::move(queue_.front());
            queue_.pop_front();
            if (!ready_ || failed_ || task.generation != generation_) continue;
            if (task.view != 0 && cancelled_.contains(task.view)) continue;
            if (task.view != 0 && viewLive && !viewLive(task.view)) continue;
            executing_ = true;
            executingView_ = task.view;
            return task;
        }
        return std::nullopt;
    }

    void finishExecuting()
    {
        std::lock_guard lock(mutex_);
        executing_ = false;
        executingView_ = 0;
        cv_.notify_all();
    }

    bool waitUntilNotExecutingFor(uint64_t view, std::chrono::milliseconds timeout) noexcept
    {
        if (view == 0) return true;
        try {
            std::unique_lock lock(mutex_);
            return cv_.wait_for(lock, timeout, [&] { return !executing_ || executingView_ != view; });
        } catch (...) {
            return false;
        }
    }

    void waitUntilNotExecuting(uint64_t view)
    {
        if (view == 0) return;
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [&] { return !executing_ || executingView_ != view; });
    }

    bool takeWakeRearm()
    {
        std::lock_guard lock(mutex_);
        if (draining_ || !ready_ || failed_ || queue_.empty() || wakePending_) return false;
        wakePending_ = true;
        return true;
    }

private:
    bool HasSafetyLocked() const
    {
        for (const auto& task : queue_) {
            if (task.safety) return true;
        }
        return false;
    }

    void failClosedLocked()
    {
        ++generation_;
        ready_ = false;
        failed_ = true;
        wakePending_ = false;
        draining_ = false;
        queue_.clear();
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::unordered_set<uint64_t> cancelled_;
    uint64_t generation_ = 0;
    uint64_t executingView_ = 0;
    bool ready_ = false;
    bool failed_ = false;
    bool wakePending_ = false;
    bool draining_ = false;
    bool executing_ = false;
    std::deque<Task> queue_;
};

}
