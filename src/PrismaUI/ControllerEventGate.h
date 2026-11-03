#pragma once

#include <array>
#include <cstdint>

namespace PrismaUI::ControllerInputPolicy {

enum class InputChain : std::uint8_t {
    kMenu = 1,
    kGameplay = 2,
    kConverter = 4,
};

class CrossChainEventGate {
public:
    static constexpr std::size_t kCapacity = 8;

    void Start(const void* event, std::uint32_t timeCode, std::uint32_t button, float value,
               float heldDownSecs, InputChain chain) noexcept {
        auto* entry = Find(event, timeCode, button, value, heldDownSecs);
        if (entry && (entry->chains & Mask(chain)) != 0) entry->chains = 0;
    }

    [[nodiscard]] bool ConsumeDuplicate(const void* event, std::uint32_t timeCode,
                                         std::uint32_t button, float value, float heldDownSecs,
                                         InputChain chain) noexcept {
        auto* entry = Find(event, timeCode, button, value, heldDownSecs);
        if (!entry || entry->chains == 0 || (entry->chains & Mask(chain)) != 0) return false;
        entry->chains |= Mask(chain);
        return true;
    }

    void Remember(const void* event, std::uint32_t timeCode, std::uint32_t button, float value,
                  float heldDownSecs, InputChain chain) noexcept {
        if (auto* entry = Find(event, timeCode, button, value, heldDownSecs)) {
            entry->chains |= Mask(chain);
            return;
        }
        entries_[next_] = Entry{event, timeCode, button, value, heldDownSecs, Mask(chain)};
        next_ = (next_ + 1) % entries_.size();
    }

private:
    struct Entry {
        const void* event = nullptr;
        std::uint32_t timeCode = 0;
        std::uint32_t button = 0;
        float value = 0.0F;
        float heldDownSecs = 0.0F;
        std::uint8_t chains = 0;
    };

    [[nodiscard]] static constexpr std::uint8_t Mask(InputChain chain) noexcept {
        return static_cast<std::uint8_t>(chain);
    }

    [[nodiscard]] Entry* Find(const void* event, std::uint32_t timeCode, std::uint32_t button,
                               float value, float heldDownSecs) noexcept {
        for (auto& entry : entries_) {
            if (entry.event == event && entry.timeCode == timeCode && entry.button == button &&
                entry.value == value && entry.heldDownSecs == heldDownSecs) {
                return &entry;
            }
        }
        return nullptr;
    }

    std::array<Entry, kCapacity> entries_{};
    std::size_t next_ = 0;
};

}
