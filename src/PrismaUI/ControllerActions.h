#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace PrismaUI::ControllerActions {

using ViewId = std::uint64_t;
using FocusEntryCallback = void (*)(void* userdata);

enum class State : std::uint8_t { kPressed, kReleased, kRepeat, kNone };

struct Binding {
    std::array<std::string, 3> payloads;
};

struct Decision {
    bool mappingActive = false;
    bool consumeGameEvent = false;
    State state = State::kNone;
    std::shared_ptr<const Binding> binding;

    [[nodiscard]] const std::string& Payload() const noexcept;
};

class Registry {
public:
    static constexpr std::size_t kMaxActionLength = 64;
    static constexpr float kRepeatDelaySecs = 0.35F;
    static constexpr float kRepeatIntervalSecs = 0.10F;

    bool Bind(ViewId view, std::uint32_t buttonCode, std::string_view canonicalButton,
              std::string_view action);
    bool Unbind(ViewId view, std::uint32_t buttonCode) noexcept;
    void Clear(ViewId view) noexcept;
    void ResetHeld(ViewId view) noexcept;
    void OnFocusAccepted(ViewId view) noexcept;
    [[nodiscard]] bool HasMappings(ViewId view) noexcept;
    [[nodiscard]] Decision Decide(ViewId view, std::uint32_t buttonCode,
                                  bool justPressed, bool released,
                                  float heldDownSecs = 0.0F) noexcept;

private:
    struct Slot {
        std::shared_ptr<const Binding> binding;
        bool pressed = false;
        float nextRepeatSecs = kRepeatDelaySecs;
    };
    using Slots = std::unordered_map<std::uint32_t, Slot>;

    std::mutex mutex_;
    std::unordered_map<ViewId, Slots> mappings_;
};

enum class BridgeState : std::uint8_t { kMissing, kPending, kReady, kFailed };

struct BridgeInstall {
    bool startInstall = false;
    std::uint64_t ticket = 0;
};

class BridgeLifecycle {
public:
    static constexpr std::uint8_t kMaxAttempts = 3;

    [[nodiscard]] BridgeInstall Ensure(ViewId view) {
        if (!view) return {};
        std::lock_guard lock(mutex_);
        std::uint8_t attempts = 1;
        if (const auto found = bridges_.find(view); found != bridges_.end()) {
            if (found->second.state == BridgeState::kPending || found->second.state == BridgeState::kReady ||
                found->second.attempts >= kMaxAttempts) {
                return {};
            }
            attempts = static_cast<std::uint8_t>(found->second.attempts + 1);
        }

        auto ticket = ++nextTicket_;
        if (!ticket) ticket = ++nextTicket_;
        bridges_[view] = Entry{BridgeState::kPending, ticket, attempts};
        return {true, ticket};
    }

    [[nodiscard]] bool CanRetry(ViewId view) const noexcept {
        std::lock_guard lock(mutex_);
        const auto found = bridges_.find(view);
        return found != bridges_.end() && found->second.state == BridgeState::kFailed &&
            found->second.attempts < kMaxAttempts;
    }

    [[nodiscard]] bool Complete(ViewId view, std::uint64_t ticket, bool success) noexcept {
        std::lock_guard lock(mutex_);
        const auto found = bridges_.find(view);
        if (found == bridges_.end() || found->second.state != BridgeState::kPending ||
            found->second.ticket != ticket) {
            return false;
        }
        found->second.state = success ? BridgeState::kReady : BridgeState::kFailed;
        return true;
    }

    void Invalidate(ViewId view) noexcept {
        std::lock_guard lock(mutex_);
        bridges_.erase(view);
    }

    void Clear(ViewId view) noexcept {
        Invalidate(view);
    }

    [[nodiscard]] BridgeState Get(ViewId view) const noexcept {
        std::lock_guard lock(mutex_);
        const auto found = bridges_.find(view);
        return found == bridges_.end() ? BridgeState::kMissing : found->second.state;
    }

private:
    struct Entry {
        BridgeState state = BridgeState::kMissing;
        std::uint64_t ticket = 0;
        std::uint8_t attempts = 0;
    };

    mutable std::mutex mutex_;
    std::unordered_map<ViewId, Entry> bridges_;
    std::uint64_t nextTicket_ = 0;
};

class FocusEntrySequence {
public:
    [[nodiscard]] bool ShouldRequest(bool matchesBinding, bool anyFocusedView,
                                     bool justPressed) const noexcept {
        return matchesBinding && !anyFocusedView && !owned_ && justPressed;
    }

    void OnDispatchResult(bool accepted) noexcept { owned_ = accepted; }

    [[nodiscard]] bool Owns(bool matchesBinding, bool anyFocusedView, bool released) noexcept {
        if (anyFocusedView) {
            owned_ = false;
            return false;
        }
        if (!matchesBinding || !owned_) return false;
        if (released) owned_ = false;
        return true;
    }

    void Reset() noexcept { owned_ = false; }
    [[nodiscard]] bool IsOwned() const noexcept { return owned_; }

private:
    bool owned_ = false;
};

struct HandleResult {
    bool mappingActive = false;
    bool consumeGameEvent = false;
};

bool Bind(ViewId view, std::uint32_t buttonCode, const char* canonicalButton,
          const char* action) noexcept;
bool Unbind(ViewId view, std::uint32_t buttonCode) noexcept;
bool BindFocusEntry(ViewId view, std::uint32_t buttonCode,
                    FocusEntryCallback callback, void* userdata) noexcept;
bool UnbindFocusEntry(ViewId view, std::uint32_t buttonCode) noexcept;
[[nodiscard]] BridgeState GetBridgeState(ViewId view) noexcept;
[[nodiscard]] bool HasMappings(ViewId view) noexcept;
void InvalidateBridge(ViewId view) noexcept;
void Clear(ViewId view) noexcept;
void OnFocusAccepted(ViewId view) noexcept;
void InstallBridge(ViewId view);
[[nodiscard]] HandleResult Handle(ViewId view, std::uint32_t buttonCode,
                                  bool justPressed, bool released,
                                  float heldDownSecs = 0.0F) noexcept;
[[nodiscard]] HandleResult HandleFocusEntry(std::uint32_t buttonCode,
                                            bool justPressed, bool released) noexcept;

}
