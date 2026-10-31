#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace PrismaUI::WebRuntime {
using ViewId = std::uint64_t;

namespace Test {
struct ViewState {
    bool valid = true;
    bool hidden = false;
    int order = 0;
    std::uint32_t role = 2u;
};
ViewState& View(ViewId view);
bool& AnyFocus();
void Reset();
}

inline bool IsValid(ViewId view) { return Test::View(view).valid; }
inline bool IsHidden(ViewId view) { return Test::View(view).hidden; }
inline int GetOrder(ViewId view) { return Test::View(view).order; }
inline std::uint32_t GetViewRole(ViewId view) { return Test::View(view).role; }
inline bool HasFocus(ViewId) { return false; }
inline bool HasAnyActiveFocus() { return Test::AnyFocus(); }
inline void Invoke(ViewId, const std::string&, std::function<void(std::string)> onResult) { onResult("true"); }
inline void InvokeDeferred(ViewId, const std::string&, std::function<void(std::string)>) {}
inline void InteropCall(ViewId, const std::string&, const std::string&) {}
}
