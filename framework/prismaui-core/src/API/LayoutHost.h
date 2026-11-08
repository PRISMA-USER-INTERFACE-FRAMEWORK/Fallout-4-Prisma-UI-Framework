#pragma once

#include "PrismaUI_F4_Modern_API.h"

#include <cstdint>
#include <string>

namespace PrismaUI::LayoutHost {
    namespace Flat = PRISMA_UI_FLAT_API;

    Flat::LayoutResult RegisterView(PrismaView view, const char* title, const Flat::LayoutRect* defaultRect, void* caller) noexcept;
    Flat::LayoutResult UnregisterView(PrismaView view, void* caller) noexcept;
    Flat::LayoutResult EnumerateViews(Flat::LayoutViewCallback callback, void* userdata, void* caller) noexcept;
    Flat::LayoutResult GetRect(PrismaView view, Flat::LayoutRect* out) noexcept;
    Flat::LayoutResult SetRect(PrismaView view, const Flat::LayoutRect* rect, void* caller) noexcept;
    Flat::LayoutResult SetUserVisible(PrismaView view, bool visible, void* caller) noexcept;
    Flat::LayoutResult ResetView(PrismaView view, void* caller) noexcept;
    Flat::LayoutResult Save() noexcept;
    Flat::LayoutResult Load() noexcept;
    Flat::LayoutResult SetEditMode(bool enabled) noexcept;
    bool IsEditMode() noexcept;
    std::uint32_t Subscribe(Flat::LayoutChangeCallback callback, void* userdata) noexcept;
    Flat::LayoutResult Unsubscribe(std::uint32_t token) noexcept;

    void OnViewCreated(std::uint64_t view, const std::string& path, const std::string& owner);
    void OnViewDestroyed(std::uint64_t view);
    void OnDockHidden();
    void RequestEditMode(bool enabled);
    void HandleEditorMessage(const std::string& message);
}
