#pragma once

#include "IWebBackend.h"

namespace PrismaUI::WebRuntime::InputRouting {

    [[nodiscard]] constexpr Web::ViewId DestinationForFocusedView(Web::ViewId view) noexcept { return view; }

    [[nodiscard]] constexpr bool HasDestination(Web::ViewId view) noexcept { return view != 0; }

    inline void DispatchKey(Web::IWebBackend& backend, Web::ViewId view, const Web::KeyInput& input) {
        if (HasDestination(view)) backend.SendKey(view, input);
    }

    inline void DispatchMouse(Web::IWebBackend& backend, Web::ViewId view, const Web::MouseInput& input) {
        if (HasDestination(view)) backend.SendMouse(view, input);
    }

    inline void DispatchScroll(Web::IWebBackend& backend, Web::ViewId view, const Web::ScrollInput& input) {
        if (HasDestination(view)) backend.SendScroll(view, input);
    }

}
