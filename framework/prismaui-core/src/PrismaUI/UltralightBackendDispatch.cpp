#include "UltralightBackend.h"

#include "FreezeDiagnostics.h"
#include "ViewLifecycleTelemetry.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    namespace {
        constexpr std::int64_t kStaggerBackoffCapMs = 60000;
        constexpr int kStagedRunsPerPass = 16;
        constexpr auto kStagedPassBudget = std::chrono::milliseconds(2);
    }

    bool UltralightBackend::Dispatch(UltralightDispatchQueue::Task function) {
        return dispatchQueue_.Dispatch(std::move(function));
    }

    void UltralightBackend::DispatchForView(PrismaUI::Web::ViewId id,
                                            UltralightDispatchQueue::Task function) {
        if (staggerEnabled_.load(std::memory_order_acquire) && StageOp(id, function)) return;
        if (!dispatchQueue_.IsOwnerThread()) {
            if (viewManager_.DispatchState(id) == ViewDispatchState::Rejected) return;
            (void)Dispatch(std::move(function));
            return;
        }
        const auto state = viewManager_.DispatchState(id);
        if (state == ViewDispatchState::Unknown || state == ViewDispatchState::Rejected) return;
        if (state == ViewDispatchState::Queued) {
            (void)dispatchQueue_.DispatchDeferred(std::move(function));
            return;
        }
        function();
    }

    bool UltralightBackend::DispatchDeferred(UltralightDispatchQueue::Task function) {
        return dispatchQueue_.DispatchDeferred(std::move(function));
    }

    bool UltralightBackend::DispatchDeferredForView(PrismaUI::Web::ViewId id,
                                                    UltralightDispatchQueue::Task function) {
        if (staggerEnabled_.load(std::memory_order_acquire) && StageOp(id, function)) return true;
        return DispatchDeferred(std::move(function));
    }

    bool UltralightBackend::StageCreate(PrismaUI::Web::ViewId id, UltralightDispatchQueue::Task& task) {
        if (!staging_.PushCreate(id, task)) return false;
        dispatchQueue_.NoteStagedWork();
        return true;
    }

    bool UltralightBackend::StageOp(PrismaUI::Web::ViewId id, UltralightDispatchQueue::Task& task) {
        if (!staging_.PushOp(id, task)) return false;
        dispatchQueue_.NoteStagedWork();
        return true;
    }

    std::int64_t UltralightBackend::DrainStagedCreates() {
        dispatchQueue_.ClearStagedWork();
        const bool backoff = FreezeDiagnostics::GameLoadBackoffActive(kStaggerBackoffCapMs);
        const auto start = std::chrono::steady_clock::now();
        for (int ran = 0; ran < kStagedRunsPerPass; ++ran) {
            auto next = staging_.TakeNext(ViewLifecycle::NowUs() / 1000, backoff);
            if (next.step == ViewCreateStagger::Step::Run) {
                try {
                    next.task();
                } catch (...) {
                }
                if (std::chrono::steady_clock::now() - start >= kStagedPassBudget) break;
                continue;
            }
            if (next.step == ViewCreateStagger::Step::Wait) return std::max<std::int64_t>(1, next.waitMs);
            return 0;
        }
        dispatchQueue_.NoteStagedWork();
        return 0;
    }

}
