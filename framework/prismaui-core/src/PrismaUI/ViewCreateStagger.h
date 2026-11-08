#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace PrismaUI::WebRuntimeUltralight {

    class ViewCreateStagger final {
    public:
        using Task = std::function<void()>;
        using ViewId = std::uint64_t;

        static constexpr std::int64_t kWindowMs = 16;
        static constexpr std::int64_t kBackoffPollMs = 16;

        struct Limits {
            std::size_t maxCreatesPerWindow = 1;
            std::int64_t minGapMs = 0;
        };

        enum class Step : std::uint8_t { Empty, Run, Wait };

        struct Next {
            Step step = Step::Empty;
            Task task;
            std::int64_t waitMs = 0;
        };

        void Configure(Limits limits) {
            std::lock_guard lock(mutex_);
            if (limits.maxCreatesPerWindow == 0) limits.maxCreatesPerWindow = 1;
            if (limits.minGapMs < 0) limits.minGapMs = 0;
            limits_ = limits;
        }

        bool PushCreate(ViewId view, Task& task) {
            std::lock_guard lock(mutex_);
            if (closed_) return false;
            if (!pending_.try_emplace(view, 0).second) return false;
            ++pending_[view];
            queue_.push_back(Entry{view, true, std::move(task)});
            return true;
        }

        bool PushOp(ViewId view, Task& task) {
            std::lock_guard lock(mutex_);
            if (closed_) return false;
            const auto it = pending_.find(view);
            if (it == pending_.end()) return false;
            ++it->second;
            queue_.push_back(Entry{view, false, std::move(task)});
            return true;
        }

        [[nodiscard]] bool IsStaged(ViewId view) const {
            std::lock_guard lock(mutex_);
            return pending_.contains(view);
        }

        [[nodiscard]] bool HasPending() const {
            std::lock_guard lock(mutex_);
            return !queue_.empty();
        }

        std::vector<Task> Cancel(ViewId view) {
            std::vector<Task> dropped;
            std::lock_guard lock(mutex_);
            pending_.erase(view);
            for (auto it = queue_.begin(); it != queue_.end();) {
                if (it->view == view) {
                    dropped.push_back(std::move(it->task));
                    it = queue_.erase(it);
                } else {
                    ++it;
                }
            }
            return dropped;
        }

        std::vector<Task> Close() {
            std::vector<Task> dropped;
            std::lock_guard lock(mutex_);
            closed_ = true;
            pending_.clear();
            for (auto& entry : queue_) dropped.push_back(std::move(entry.task));
            queue_.clear();
            return dropped;
        }

        Next TakeNext(std::int64_t nowMs, bool backoff) {
            Next out;
            std::lock_guard lock(mutex_);
            if (queue_.empty()) return out;
            if (queue_.front().create) {
                if (backoff) {
                    out.step = Step::Wait;
                    out.waitMs = kBackoffPollMs;
                    return out;
                }
                if (!windowOpen_ || nowMs - windowStartMs_ >= kWindowMs) {
                    windowOpen_ = true;
                    windowStartMs_ = nowMs;
                    createsInWindow_ = 0;
                }
                if (createsInWindow_ >= limits_.maxCreatesPerWindow) {
                    out.step = Step::Wait;
                    out.waitMs = windowStartMs_ + kWindowMs - nowMs;
                    return out;
                }
                if (haveLastCreate_ && nowMs - lastCreateMs_ < limits_.minGapMs) {
                    out.step = Step::Wait;
                    out.waitMs = lastCreateMs_ + limits_.minGapMs - nowMs;
                    return out;
                }
                ++createsInWindow_;
                lastCreateMs_ = nowMs;
                haveLastCreate_ = true;
            }
            out.step = Step::Run;
            out.task = std::move(queue_.front().task);
            const ViewId view = queue_.front().view;
            queue_.pop_front();
            const auto it = pending_.find(view);
            if (it != pending_.end() && --it->second == 0) pending_.erase(it);
            return out;
        }

    private:
        struct Entry {
            ViewId view = 0;
            bool create = false;
            Task task;
        };

        mutable std::mutex mutex_;
        std::deque<Entry> queue_;
        std::unordered_map<ViewId, std::size_t> pending_;
        Limits limits_;
        bool closed_ = false;
        bool windowOpen_ = false;
        std::int64_t windowStartMs_ = 0;
        std::size_t createsInWindow_ = 0;
        bool haveLastCreate_ = false;
        std::int64_t lastCreateMs_ = 0;
    };

}
