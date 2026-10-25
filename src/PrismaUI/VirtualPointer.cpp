#include "VirtualPointer.h"
#include "VirtualPointerClickState.h"
#include "VirtualPointerPolicy.h"
#include "ControllerGlyphs.h"
#include "InputOwnershipPolicy.h"
#include "WebInput.h"
#include "WebRuntime.h"
#include "PCH.h"
#include <Windows.h>
#include <chrono>

namespace PrismaUI::VirtualPointer {
    namespace {
        VirtualPointerClick::State g_click{};
        std::chrono::steady_clock::time_point g_lastSample{};

        struct Route {
            bool admitted = false;
            int x = 0;
            int y = 0;
        };

        VirtualPointerClick::Owner CurrentOwner() noexcept
        {
            const auto view = WebRuntime::GetFocusedView();
            if (!view) return {};
            return { view, WebRuntime::ControllerInputGeneration(view) };
        }

        Route EvaluateRoute() noexcept
        {
            Route route{};
            if (!WebInput::IsInstalled() || !WebInput::IsCursorOwned()) return route;
            const auto focused = WebRuntime::GetFocusedView();
            if (!focused || WebRuntime::IsOffscreen(focused)) return route;
            const int x = WebInput::GetLastCursorX();
            const int y = WebInput::GetLastCursorY();
            if (x < 0 || y < 0) return route;

            int routeX = x;
            int routeY = y;
            WebRuntime::MapClientPointToBrowser(routeX, routeY);
            route.admitted = InputOwnershipPolicy::ShouldRouteFocusedClick(
                InputOwnershipPolicy::CaptureOwner::kFocusedView,
                WebRuntime::ShouldRouteFocusedMouse(routeX, routeY));
            route.x = x;
            route.y = y;
            return route;
        }

        Route EvaluateOwnerRoute() noexcept
        {
            Route route{};
            if (!WebInput::IsInstalled() || !WebInput::IsCursorOwned()) return route;
            const int x = WebInput::GetTrackedCursorX();
            const int y = WebInput::GetTrackedCursorY();
            if (x < 0 || y < 0) return route;
            route.admitted = true;
            route.x = x;
            route.y = y;
            return route;
        }

        bool Dispatch(const Route& route, bool down) noexcept
        {
            if (!route.admitted) return false;
            WebRuntime::SendMouseClick(route.x, route.y, 0, !down, 1, 0, true);
            return true;
        }

        void ResetTiming() noexcept
        {
            g_lastSample = {};
        }
    }

    void OnStick(bool rightStick, float x, float y) noexcept
    {
        if (!rightStick) return;
        if (!WebRuntime::IsActive()) {
            CancelHeldClick();
            return;
        }

        const auto owner = CurrentOwner();
        if (!VirtualPointerClick::ValidOwner(owner) || WebRuntime::IsOffscreen(owner.view)) {
            CancelHeldClick();
            return;
        }

        int originX = 0, originY = 0, width = 0, height = 0;
        if (!WebInput::GetClientScreenRect(originX, originY, width, height)) {
            CancelHeldClick();
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        float seconds = 0.0F;
        if (g_click.hasSample) seconds = std::chrono::duration<float>(now - g_lastSample).count();
        g_lastSample = now;
        g_click.hasSample = true;

        POINT cursor{};
        if (!GetCursorPos(&cursor)) return;

        const int minimumX = originX;
        const int minimumY = originY;
        const int maximumX = originX + width - 1;
        const int maximumY = originY + height - 1;
        const int nextX = VirtualPointerPolicy::StepX(static_cast<int>(cursor.x), x, seconds, minimumX, maximumX);
        const int nextY = VirtualPointerPolicy::StepY(static_cast<int>(cursor.y), y, seconds, minimumY, maximumY);
        if (nextX == cursor.x && nextY == cursor.y) return;

        SetCursorPos(nextX, nextY);
        if (VirtualPointerClick::RecordMove(g_click))
            logger::info("[PrismaUI VirtualPointer] stick {}x{} cursor {}x{} -> {}x{} (dx {} dy {})", x, y,
                         static_cast<int>(cursor.x), static_cast<int>(cursor.y), nextX, nextY,
                         nextX - static_cast<int>(cursor.x), nextY - static_cast<int>(cursor.y));
    }

    bool OnButton(std::uint32_t buttonCode, bool pressed, bool released) noexcept
    {
        if (ControllerGlyphs::CanonicalFromCode(buttonCode) != "A") return false;
        const auto current = CurrentOwner();

        if (released) {
            if (!g_click.held) return false;
            const Route route = EvaluateOwnerRoute();
            const auto end = VirtualPointerClick::EndClick(g_click, current);
            if (end == VirtualPointerClick::End::kReleaseOwner && Dispatch(route, false))
                logger::info("[PrismaUI VirtualPointer] click released at {}x{}", route.x, route.y);
            else
                logger::info("[PrismaUI VirtualPointer] click discarded; its view is no longer the input target");
            return true;
        }

        if (!pressed) return g_click.held;
        const Route route = EvaluateRoute();
        if (VirtualPointerClick::BeginClick(g_click, route.admitted, current) != VirtualPointerClick::Begin::kAdmitted)
            return g_click.held;
        if (!Dispatch(route, true)) {
            ResetSession();
            return false;
        }
        logger::info("[PrismaUI VirtualPointer] click pressed at {}x{}", route.x, route.y);
        return true;
    }

    void CancelHeldClick() noexcept
    {
        if (g_click.held) {
            const Route route = EvaluateOwnerRoute();
            const bool ownsTarget = VirtualPointerClick::CancelClick(g_click, CurrentOwner());
            if (ownsTarget && Dispatch(route, false))
                logger::info("[PrismaUI VirtualPointer] click cancelled with its owner");
            else
                logger::info("[PrismaUI VirtualPointer] click cancelled; owner lost");
        }
        ResetSession();
    }

    void DiscardHeldClick(std::uint64_t view) noexcept
    {
        if (!VirtualPointerClick::DiscardIfOwnerView(g_click, view)) return;
        logger::info("[PrismaUI VirtualPointer] click discarded; its view {} is gone", view);
        ResetSession();
    }

    void ResetSession() noexcept
    {
        VirtualPointerClick::ResetSession(g_click);
        ResetTiming();
    }
}
