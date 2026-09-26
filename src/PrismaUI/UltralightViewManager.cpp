#include "UltralightViewManager.h"

namespace PrismaUI::WebRuntimeUltralight {

    PrismaUI::Web::ViewId UltralightViewManager::NextId() noexcept {
        auto id = nextId_.fetch_add(1, std::memory_order_relaxed);
        if (!id) id = nextId_.fetch_add(1, std::memory_order_relaxed);
        return id;
    }

    PrismaUI::Web::ViewId UltralightViewManager::Reserve(
        std::string relativePath, std::string owner, std::function<bool()> canAccept) {
        std::lock_guard lock(mutex_);
        if (canAccept && !canAccept()) return 0;
        const auto id = NextId();
        states_[id] = ViewReservationState::Reserved;
        health_[id] = PrismaUI::Web::ViewHealth::Creating;
        metadata_[id] = {std::move(relativePath), std::move(owner)};
        return id;
    }

    bool UltralightViewManager::BeginCreation(PrismaUI::Web::ViewId id) {
        std::lock_guard lock(mutex_);
        const auto it = states_.find(id);
        if (it == states_.end() || it->second != ViewReservationState::Reserved) return false;
        it->second = ViewReservationState::Creating;
        return true;
    }

    bool UltralightViewManager::IsDestroyRequested(PrismaUI::Web::ViewId id) const {
        std::lock_guard lock(mutex_);
        const auto it = states_.find(id);
        return it == states_.end() || it->second == ViewReservationState::DestroyRequested;
    }

    bool UltralightViewManager::RequestDestroy(PrismaUI::Web::ViewId id) {
        std::lock_guard lock(mutex_);
        const auto it = states_.find(id);
        if (it == states_.end() || it->second == ViewReservationState::DestroyRequested) return false;
        it->second = ViewReservationState::DestroyRequested;
        return true;
    }

    ViewDispatchState UltralightViewManager::DispatchState(PrismaUI::Web::ViewId id) const {
        std::lock_guard lock(mutex_);
        const auto it = states_.find(id);
        if (it == states_.end()) return ViewDispatchState::Unknown;
        if (it->second == ViewReservationState::DestroyRequested ||
            it->second == ViewReservationState::Failed)
            return ViewDispatchState::Rejected;
        if (it->second == ViewReservationState::Reserved || it->second == ViewReservationState::Creating)
            return ViewDispatchState::Queued;
        return ViewDispatchState::Ready;
    }

    bool UltralightViewManager::FailCreation(
        PrismaUI::Web::ViewId id, PrismaUI::Web::DomReadyCallback onReady,
        std::string detail, PrismaUI::Web::ViewErrorCallback report) {
        {
            std::lock_guard lock(mutex_);
            const auto it = states_.find(id);
            if (it == states_.end() || it->second == ViewReservationState::DestroyRequested) return false;
            it->second = ViewReservationState::Failed;
            health_[id] = PrismaUI::Web::ViewHealth::LoadFailed;
            metadata_.erase(id);
            states_.erase(id);
            health_.erase(id);
        }
        if (report) report(id, PrismaUI::Web::ViewHealth::LoadFailed,
                           "Ultralight view creation failed", detail);
        if (onReady) onReady(0);
        return true;
    }

    bool UltralightViewManager::PublishCreated(PrismaUI::Web::ViewId id) {
        std::lock_guard lock(mutex_);
        const auto it = states_.find(id);
        if (it == states_.end() || it->second == ViewReservationState::DestroyRequested) return false;
        it->second = ViewReservationState::Live;
        health_[id] = PrismaUI::Web::ViewHealth::Creating;
        return true;
    }

    void UltralightViewManager::ErasePublished(PrismaUI::Web::ViewId id) {
        std::lock_guard lock(mutex_);
        states_.erase(id);
        health_.erase(id);
        metadata_.erase(id);
    }

    void UltralightViewManager::SetHealth(
        ViewRecord& record, PrismaUI::Web::ViewHealth health) {
        record.health = health;
        std::lock_guard lock(mutex_);
        health_[record.id] = health;
    }

    bool UltralightViewManager::IsValid(PrismaUI::Web::ViewId id) const {
        std::lock_guard lock(mutex_);
        const auto state = states_.find(id);
        if (state == states_.end() || state->second == ViewReservationState::DestroyRequested ||
            state->second == ViewReservationState::Failed)
            return false;
        const auto health = health_.find(id);
        return health != health_.end() && health->second != PrismaUI::Web::ViewHealth::LoadFailed;
    }

    PrismaUI::Web::ViewHealth UltralightViewManager::GetHealth(PrismaUI::Web::ViewId id) const {
        std::lock_guard lock(mutex_);
        const auto it = health_.find(id);
        return it == health_.end() ? PrismaUI::Web::ViewHealth::LoadFailed : it->second;
    }

    void UltralightViewManager::Enumerate(
        const PrismaUI::Web::ViewEnumCallback& callback) const {
        if (!callback) return;
        std::vector<std::tuple<PrismaUI::Web::ViewId, std::string, std::string>> values;
        {
            std::lock_guard lock(mutex_);
            values.reserve(metadata_.size());
            for (const auto& [id, metadata] : metadata_)
                values.emplace_back(id, metadata.first, metadata.second);
        }
        for (const auto& [id, path, owner] : values) callback(id, path, owner);
    }

    ViewRecord* UltralightViewManager::Find(PrismaUI::Web::ViewId id) noexcept {
        const auto it = records_.find(id);
        return it == records_.end() ? nullptr : it->second.get();
    }

    const ViewRecord* UltralightViewManager::Find(PrismaUI::Web::ViewId id) const noexcept {
        const auto it = records_.find(id);
        return it == records_.end() ? nullptr : it->second.get();
    }

    void UltralightViewManager::Add(std::unique_ptr<ViewRecord> record) {
        if (record) records_.emplace(record->id, std::move(record));
    }

    void UltralightViewManager::EraseRecord(PrismaUI::Web::ViewId id) noexcept {
        records_.erase(id);
    }

    void UltralightViewManager::ClearRecords() noexcept {
        records_.clear();
    }

    void UltralightViewManager::ClearPublished() noexcept {
        std::lock_guard lock(mutex_);
        states_.clear();
        health_.clear();
        metadata_.clear();
    }

}
