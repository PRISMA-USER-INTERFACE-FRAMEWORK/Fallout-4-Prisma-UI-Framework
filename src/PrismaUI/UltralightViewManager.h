#pragma once

#include "IWebBackend.h"
#include "UltralightViewCallbacks.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace PrismaUI::WebRuntimeUltralight {

    struct ViewRecord {
        struct PendingCall {
            std::string name;
            std::string argumentJson;
        };

        PrismaUI::Web::ViewId id = 0;
        bool isInspector = false;
        PrismaUI::Web::ViewId inspectedView = 0;
        PrismaUI::Web::ViewId inspectorId = 0;
        PrismaUI::Web::ViewSource source;
        std::string owner;
        ultralight::RefPtr<ultralight::View> view;
        std::unique_ptr<UltralightViewCallbacks> callbacks;
        PrismaUI::Web::DomReadyCallback domReady;
        PrismaUI::Web::ViewHealth health = PrismaUI::Web::ViewHealth::Creating;
        bool domReadyFired = false;
        bool replayingPendingCalls = false;
        bool trustedDocumentReady = false;
        std::uint64_t documentGeneration = 0;
        bool navigationBlocked = false;
        std::string blockedNavigationUrl;
        bool viewportSized = true;
        uint32_t width = 1280;
        uint32_t height = 720;
        uint32_t clearColor = 0;
        PrismaUI::Web::ConsoleCallback console;
        std::map<std::string, PrismaUI::Web::ListenerCallback> listeners;
        std::deque<PendingCall> pendingCalls;
        std::size_t pendingCallBytes = 0;
        std::string localizationScript;
    };

    enum class ViewReservationState : uint8_t {
        Reserved,
        Creating,
        Live,
        DestroyRequested,
        Failed,
    };

    enum class ViewDispatchState : uint8_t {
        Unknown,
        Rejected,
        Queued,
        Ready,
    };

    class UltralightViewManager final {
    public:
        PrismaUI::Web::ViewId NextId() noexcept;
        PrismaUI::Web::ViewId Reserve(std::string relativePath, std::string owner,
                                      std::function<bool()> canAccept);
        bool BeginCreation(PrismaUI::Web::ViewId id);
        bool IsDestroyRequested(PrismaUI::Web::ViewId id) const;
        bool RequestDestroy(PrismaUI::Web::ViewId id);
        [[nodiscard]] ViewDispatchState DispatchState(PrismaUI::Web::ViewId id) const;
        bool FailCreation(PrismaUI::Web::ViewId id, PrismaUI::Web::DomReadyCallback onReady,
                          std::string detail, PrismaUI::Web::ViewErrorCallback report);
        bool PublishCreated(PrismaUI::Web::ViewId id);
        void ErasePublished(PrismaUI::Web::ViewId id);
        void SetHealth(ViewRecord& record, PrismaUI::Web::ViewHealth health);
        [[nodiscard]] bool IsValid(PrismaUI::Web::ViewId id) const;
        [[nodiscard]] PrismaUI::Web::ViewHealth GetHealth(PrismaUI::Web::ViewId id) const;
        void Enumerate(const PrismaUI::Web::ViewEnumCallback& callback) const;

        [[nodiscard]] ViewRecord* Find(PrismaUI::Web::ViewId id) noexcept;
        [[nodiscard]] const ViewRecord* Find(PrismaUI::Web::ViewId id) const noexcept;
        void Add(std::unique_ptr<ViewRecord> record);
        void EraseRecord(PrismaUI::Web::ViewId id) noexcept;
        void ClearRecords() noexcept;
        void ClearPublished() noexcept;

        template <class Function>
        void ForEach(Function&& function) {
            for (auto& [id, record] : records_) function(id, *record);
        }

    private:
        mutable std::mutex mutex_;
        std::atomic<PrismaUI::Web::ViewId> nextId_{1};
        std::map<PrismaUI::Web::ViewId, std::unique_ptr<ViewRecord>> records_;
        std::map<PrismaUI::Web::ViewId, ViewReservationState> states_;
        std::map<PrismaUI::Web::ViewId, PrismaUI::Web::ViewHealth> health_;
        std::map<PrismaUI::Web::ViewId, std::pair<std::string, std::string>> metadata_;
    };

}
