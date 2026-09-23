#include "UltralightBackend.h"

#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    void UltralightBackend::Dispatch(UltralightDispatchQueue::Task function) {
        dispatchQueue_.Dispatch(std::move(function));
    }

    void UltralightBackend::DispatchForView(PrismaUI::Web::ViewId id,
                                            UltralightDispatchQueue::Task function) {
        if (!dispatchQueue_.IsOwnerThread()) {
            if (viewManager_.DispatchState(id) == ViewDispatchState::Rejected) return;
            Dispatch(std::move(function));
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

}
