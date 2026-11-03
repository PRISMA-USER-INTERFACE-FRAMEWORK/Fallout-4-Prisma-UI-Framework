#pragma once

#include "IWebBackend.h"
#include "UltralightViewManager.h"

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/Ultralight.h>
#pragma warning(pop)

#include <functional>
#include <map>
#include <mutex>

namespace PrismaUI::WebRuntimeUltralight {

    class IUltralightInspectorHost {
    public:
        virtual ~IUltralightInspectorHost() = default;

        virtual ViewRecord* FindInspectorRecord(PrismaUI::Web::ViewId id) noexcept = 0;
        virtual uint32_t InspectorViewportWidth() const noexcept = 0;
        virtual uint32_t InspectorViewportHeight() const noexcept = 0;
        virtual void RequestInspectorFrame() noexcept = 0;
        virtual void DispatchInspector(PrismaUI::Web::ViewId id, std::function<void()> task) = 0;
        virtual void DispatchInspectorTask(std::function<void()> task) = 0;
        virtual bool CopyPublishedInspectorTarget(
            PrismaUI::Web::ViewId id, uint64_t generation,
            PrismaUI::Web::RenderTargetSnapshot& target) const = 0;
        virtual void NotifyInspectorState(PrismaUI::Web::ViewId owner, bool visible) = 0;
        virtual void ReleaseNativeGamepadForInspector() = 0;
        virtual void FocusOwnerView(PrismaUI::Web::ViewId owner) = 0;
    };

    class UltralightInspector final {
    public:
        explicit UltralightInspector(IUltralightInspectorHost& host);

        void Add(PrismaUI::Web::ViewId owner, PrismaUI::Web::ViewId id,
                 ultralight::RefPtr<ultralight::View> view,
                 uint32_t width, uint32_t height, int x, int y);
        void Clear() noexcept;
        void RemoveForOwner(PrismaUI::Web::ViewId owner);
        void OnInspectorRequestClose(PrismaUI::Web::ViewId inspectorId);
        void SetViewport(uint32_t width, uint32_t height);
        void SetVisibility(PrismaUI::Web::ViewId owner, bool visible);
        [[nodiscard]] bool IsVisible(PrismaUI::Web::ViewId owner) const;
        void SetBounds(PrismaUI::Web::ViewId owner, float x, float y,
                       uint32_t width, uint32_t height);
        void BeginMove(PrismaUI::Web::ViewId inspectorId, int localX, int localY);
        void BeginResize(PrismaUI::Web::ViewId inspectorId, int localX, int localY);
        PrismaUI::Web::ViewId UpdateGesture(int x, int y, int& localX, int& localY);
        PrismaUI::Web::ViewId GestureTargetAt(int x, int y, int& localX, int& localY) const;
        void EndGesture();
        void MoveBy(PrismaUI::Web::ViewId inspectorId, int dx, int dy);
        void ResizeBy(PrismaUI::Web::ViewId inspectorId, int dx, int dy);
        bool GetFrame(uint64_t generation, PrismaUI::Web::InspectorFrame& out) const;
        PrismaUI::Web::ViewId TargetAt(int x, int y, int& localX, int& localY) const;
        PrismaUI::Web::ViewId VisibleView() const;
        void InstallHost(ultralight::View& view, PrismaUI::Web::ViewId inspectorId);

    private:
        struct InspectorState {
            enum class Gesture { None, Move, Resize };

            PrismaUI::Web::ViewId id = 0;
            ultralight::RefPtr<ultralight::View> view;
            int x = 0;
            int y = 0;
            uint32_t width = 900;
            uint32_t height = 650;
            bool visible = false;
            Gesture gesture = Gesture::None;
            int lastCursorX = 0;
            int lastCursorY = 0;
        };

        void BeginGesture(PrismaUI::Web::ViewId inspectorId, int localX, int localY,
                          InspectorState::Gesture gesture);

        IUltralightInspectorHost& host_;
        mutable std::mutex mutex_;
        std::map<PrismaUI::Web::ViewId, InspectorState> inspectors_;
    };

}
