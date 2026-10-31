#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace PrismaUI::GPU::LateRerender {

struct Usage {
    std::size_t operations = 0;
    std::size_t textureBytes = 0;
    std::size_t nonTextureBytes = 0;
    std::size_t bytes = 0;
};

struct Caps {
    std::size_t operations = 0;
    std::size_t textureBytes = 0;
    std::size_t nonTextureBytes = 0;
    std::size_t bytes = 0;
};

inline constexpr std::size_t kWindowRenders = 64;
inline constexpr std::uint32_t kMissLimit = 3;
inline constexpr std::uint64_t kBackoffMs = 1000;
inline constexpr std::uint64_t kLogIntervalMs = 10000;

[[nodiscard]] inline constexpr std::size_t Saturating(std::size_t after, std::size_t before) noexcept {
    return after > before ? after - before : 0;
}

[[nodiscard]] inline constexpr Usage Delta(const Usage& after, const Usage& before) noexcept {
    return {Saturating(after.operations, before.operations), Saturating(after.textureBytes, before.textureBytes),
            Saturating(after.nonTextureBytes, before.nonTextureBytes), Saturating(after.bytes, before.bytes)};
}

[[nodiscard]] inline constexpr Usage Largest(const Usage& a, const Usage& b) noexcept {
    return {std::max(a.operations, b.operations), std::max(a.textureBytes, b.textureBytes),
            std::max(a.nonTextureBytes, b.nonTextureBytes), std::max(a.bytes, b.bytes)};
}

class DeltaWindow {
public:
    void Record(const Usage& delta) noexcept {
        ring_[next_] = delta;
        next_ = (next_ + 1) % kWindowRenders;
        if (count_ < kWindowRenders) ++count_;
    }

    [[nodiscard]] Usage LargestRecorded() const noexcept {
        Usage largest;
        for (std::size_t i = 0; i < count_; ++i) largest = Largest(largest, ring_[i]);
        return largest;
    }

private:
    std::array<Usage, kWindowRenders> ring_{};
    std::size_t next_ = 0;
    std::size_t count_ = 0;
};

struct Backoff {
    std::uint32_t misses = 0;
    std::uint64_t untilMs = 0;

    void Record(bool hit, std::uint64_t nowMs) noexcept {
        if (hit) {
            misses = 0;
            return;
        }
        if (++misses < kMissLimit) return;
        misses = 0;
        untilMs = nowMs + kBackoffMs;
    }
};

struct Inputs {
    Usage pending;
    Usage largest;
    bool structuralChange = false;
    bool spentThisPresent = false;
    std::uint64_t nowMs = 0;
    std::uint64_t backoffUntilMs = 0;
};

[[nodiscard]] inline constexpr bool WithinHeadroom(std::size_t pending, std::size_t largest, std::size_t cap) noexcept {
    const auto half = cap / 2;
    return largest <= half && pending <= half - largest;
}

[[nodiscard]] inline constexpr bool Allow(const Inputs& in, const Caps& caps) noexcept {
    if (in.structuralChange || in.spentThisPresent || in.nowMs < in.backoffUntilMs) return false;
    if (in.pending.operations > caps.operations / 8 || in.pending.textureBytes > caps.textureBytes / 16 ||
        in.pending.nonTextureBytes > caps.nonTextureBytes / 16 || in.pending.bytes > caps.bytes / 16)
        return false;
    return WithinHeadroom(in.pending.operations, in.largest.operations, caps.operations) &&
           WithinHeadroom(in.pending.textureBytes, in.largest.textureBytes, caps.textureBytes) &&
           WithinHeadroom(in.pending.nonTextureBytes, in.largest.nonTextureBytes, caps.nonTextureBytes) &&
           WithinHeadroom(in.pending.bytes, in.largest.bytes, caps.bytes);
}

[[nodiscard]] inline std::uint64_t SteadyNowMs() noexcept {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

}
