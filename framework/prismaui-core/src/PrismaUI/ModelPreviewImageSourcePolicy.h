#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace PrismaUI::ModelPreview::ImageSourcePolicy {

struct State {
    std::string url;
    bool published = false;
    uint64_t publishedGeneration = 0;
    bool live = false;
};

template <class Backend, class Srv>
bool Reconcile(State& state, Backend& backend, uint64_t key, bool active, Srv srv, uint32_t width, uint32_t height,
               uint64_t generation) {
    if (!active) return false;
    bool reservedNow = false;
    if (state.url.empty()) {
        state.url = backend.ReserveExternalImage(key);
        reservedNow = !state.url.empty();
    }
    if (state.url.empty() || (state.published && state.publishedGeneration == generation)) return reservedNow;
    backend.PublishExternalImage(key, std::move(srv), width, height, generation);
    state.published = true;
    state.publishedGeneration = generation;
    return reservedNow;
}

inline bool Retire(State& state) noexcept {
    const bool wasPublished = state.published;
    state.published = false;
    state.live = false;
    return wasPublished;
}

inline bool ExcludesHoleOverlay(const State& state) noexcept { return !state.url.empty() && state.published && state.live; }

}
