#include "PrismaUI/ControllerActions.h"

#include <algorithm>

namespace PrismaUI::ControllerActions {
namespace {
const std::string kEmpty;

bool ValidAction(std::string_view action) noexcept {
    if (action.empty() || action.size() > Registry::kMaxActionLength) return false;
    return std::all_of(action.begin(), action.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-' || c == '.' || c == ':';
    });
}

bool IsDirectional(std::uint32_t code) noexcept {
    return code == 0x10001u || code == 0x10002u || code == 0x10004u || code == 0x10008u;
}

std::string Payload(std::string_view action, std::string_view canonicalButton,
                    std::uint32_t code, std::string_view state) {
    return "{\"action\":\"" + std::string(action) + "\",\"button\":\"" +
           std::string(canonicalButton) + "\",\"buttonCode\":" + std::to_string(code) +
           ",\"state\":\"" + std::string(state) + "\"}";
}
}

const std::string& Decision::Payload() const noexcept {
    if (!binding || state == State::kNone) return kEmpty;
    return binding->payloads[static_cast<std::size_t>(state)];
}

bool Registry::Bind(ViewId view, std::uint32_t buttonCode,
                    std::string_view canonicalButton, std::string_view action) {
    if (!view || canonicalButton.empty() || !ValidAction(action)) return false;
    auto binding = std::make_shared<Binding>(Binding{
        {Payload(action, canonicalButton, buttonCode, "pressed"),
         Payload(action, canonicalButton, buttonCode, "released"),
         Payload(action, canonicalButton, buttonCode, "repeat")},
    });
    std::lock_guard lock(mutex_);
    mappings_[view][buttonCode] = {std::move(binding), false};
    return true;
}

bool Registry::Unbind(ViewId view, std::uint32_t buttonCode) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = mappings_.find(view);
    if (found == mappings_.end() || !found->second.erase(buttonCode)) return false;
    if (found->second.empty()) mappings_.erase(found);
    return true;
}

void Registry::Clear(ViewId view) noexcept {
    std::lock_guard lock(mutex_);
    mappings_.erase(view);
}

void Registry::ResetHeld(ViewId view) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = mappings_.find(view);
    if (found == mappings_.end()) return;
    for (auto& [code, slot] : found->second) slot.pressed = false;
}

void Registry::OnFocusAccepted(ViewId view) noexcept {
    ResetHeld(view);
}

bool Registry::HasMappings(ViewId view) noexcept {
    std::lock_guard lock(mutex_);
    return mappings_.contains(view);
}

Decision Registry::Decide(ViewId view, std::uint32_t buttonCode,
                          bool justPressed, bool released, float heldDownSecs) noexcept {
    std::lock_guard lock(mutex_);
    const auto found = mappings_.find(view);
    if (found == mappings_.end()) return {};
    const auto mapping = found->second.find(buttonCode);
    if (mapping == found->second.end()) return {};
    auto& slot = mapping->second;
    Decision result{true, true, State::kNone, slot.binding};
    if (released && slot.pressed) {
        slot.pressed = false;
        result.state = State::kReleased;
    } else if (justPressed && !released && !slot.pressed) {
        slot.pressed = true;
        slot.nextRepeatSecs = kRepeatDelaySecs;
        result.state = State::kPressed;
    } else if (slot.pressed && IsDirectional(buttonCode) &&
               heldDownSecs >= slot.nextRepeatSecs) {
        result.state = State::kRepeat;
        do {
            slot.nextRepeatSecs += kRepeatIntervalSecs;
        } while (slot.nextRepeatSecs <= heldDownSecs);
    }
    return result;
}

}
