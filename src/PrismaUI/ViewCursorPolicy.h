#pragma once

#include <cstdint>
#include <unordered_map>

namespace PrismaUI::ViewCursorPolicy {

using ViewId = std::uint64_t;

enum class Policy : std::uint8_t {
    Default = 0,
    Hidden = 1,
};

struct State {
    ViewId focusedView = 0;
    std::unordered_map<ViewId, Policy> policies;
};

inline void SetPolicy(State& state, ViewId view, Policy policy)
{
    if (view == 0) return;
    if (policy == Policy::Default) {
        state.policies.erase(view);
    } else {
        state.policies[view] = policy;
    }
}

inline Policy GetPolicy(const State& state, ViewId view)
{
    if (view == 0) return Policy::Default;
    const auto it = state.policies.find(view);
    return it == state.policies.end() ? Policy::Default : it->second;
}

inline void OnFocusChanged(State& state, ViewId newFocusedView)
{
    if (state.focusedView != 0 && state.focusedView != newFocusedView) {
        state.policies.erase(state.focusedView);
    }
    state.focusedView = newFocusedView;
}

inline void OnViewDestroyed(State& state, ViewId view)
{
    if (view == 0) return;
    state.policies.erase(view);
    if (state.focusedView == view) state.focusedView = 0;
}

inline bool ShouldDrawCompositorCursor(const State& state, bool cursorOwned)
{
    if (!cursorOwned) return false;
    if (state.focusedView == 0) return true;
    return GetPolicy(state, state.focusedView) != Policy::Hidden;
}

}
