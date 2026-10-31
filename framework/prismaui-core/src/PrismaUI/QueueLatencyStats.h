#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace PrismaUI::QueueLatency {

inline constexpr std::uint64_t kReportIntervalNs = 10'000'000'000ull;
inline constexpr std::size_t kTrackedJsCallNames = 8;
inline constexpr std::size_t kMaxJsCallNameBytes = 64;

struct Snapshot {
    std::uint64_t count = 0;
    std::uint64_t sumNs = 0;
    std::uint64_t maxNs = 0;

    double AverageMs() const noexcept
    {
        return count ? static_cast<double>(sumNs) / static_cast<double>(count) / 1e6 : 0.0;
    }

    double MaxMs() const noexcept { return static_cast<double>(maxNs) / 1e6; }
};

class Stats {
public:
    using Clock = std::chrono::steady_clock;

    void Record(Clock::time_point enqueuedAt, Clock::time_point now) noexcept
    {
        RecordValue(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now - enqueuedAt).count()));
    }

    void RecordValue(std::uint64_t value) noexcept
    {
        count_.fetch_add(1, std::memory_order_relaxed);
        sumNs_.fetch_add(value, std::memory_order_relaxed);
        std::uint64_t seen = maxNs_.load(std::memory_order_relaxed);
        while (value > seen && !maxNs_.compare_exchange_weak(seen, value, std::memory_order_relaxed)) {
        }
    }

    bool TakeDue(Clock::time_point now, Snapshot& out) noexcept
    {
        const auto nowNs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
        std::uint64_t due = dueNs_.load(std::memory_order_relaxed);
        if (nowNs < due || !dueNs_.compare_exchange_strong(due, nowNs + kReportIntervalNs, std::memory_order_relaxed)) {
            return false;
        }
        out.count = count_.exchange(0, std::memory_order_relaxed);
        out.sumNs = sumNs_.exchange(0, std::memory_order_relaxed);
        out.maxNs = maxNs_.exchange(0, std::memory_order_relaxed);
        return out.count != 0;
    }

private:
    std::atomic<std::uint64_t> count_{0};
    std::atomic<std::uint64_t> sumNs_{0};
    std::atomic<std::uint64_t> maxNs_{0};
    std::atomic<std::uint64_t> dueNs_{0};
};

class JsCallTable {
public:
    struct Entry {
        std::string name;
        Stats stats;
    };

    JsCallTable() { entries_[kTrackedJsCallNames].name = "(other)"; }

    Entry& Lookup(std::string_view name)
    {
        for (std::size_t i = 0; i < kTrackedJsCallNames; ++i) {
            if (entries_[i].name == name) return entries_[i];
            if (entries_[i].name.empty()) {
                if (name.size() > kMaxJsCallNameBytes) break;
                entries_[i].name = name;
                return entries_[i];
            }
        }
        return entries_[kTrackedJsCallNames];
    }

private:
    std::array<Entry, kTrackedJsCallNames + 1> entries_{};
};

inline std::string SafeName(std::string_view name)
{
    std::string out(name);
    for (char& c : out) {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) c = '?';
    }
    return out;
}

}
