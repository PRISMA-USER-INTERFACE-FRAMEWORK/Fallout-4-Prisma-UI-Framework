#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace PrismaUI::ControllerInputPolicy {

struct ForwardedMenuKey {
    std::uint32_t key = 0;
    bool pressed = false;
};

class ForwardGuard {
public:
    static constexpr std::size_t kCapacity = 4;

    [[nodiscard]] bool Note(const ForwardedMenuKey& candidate) noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            if (entries_[i].key == candidate.key && entries_[i].pressed == candidate.pressed) return false;
        }
        if (count_ >= entries_.size()) return false;
        entries_[count_++] = candidate;
        return true;
    }

    [[nodiscard]] std::size_t Count() const noexcept { return count_; }

private:
    std::array<ForwardedMenuKey, kCapacity> entries_{};
    std::size_t count_ = 0;
};

}
