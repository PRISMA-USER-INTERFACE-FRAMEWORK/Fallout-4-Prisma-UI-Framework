#include "UltralightBackend.h"

#include "GPU/FalloutGPUDriver.h"
#include "GPU/GenerationDrainState.h"

#include <cassert>
#include <cstring>
#include <map>
#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    void UltralightBackend::PublishFrames() {
        assert(dispatchQueue_.IsOwnerThread());
        std::map<PrismaUI::Web::ViewId, std::pair<uint32_t, uint32_t>> publishedDims;
        {
            std::lock_guard lock(stateMutex_);
            for (const auto& [id, frame] : published_) publishedDims[id] = {frame.width, frame.height};
        }
        std::map<PrismaUI::Web::ViewId, PublishedFrame> next;
        viewManager_.ForEach([&](PrismaUI::Web::ViewId id, ViewRecord& record) {
            if (!record.view) return;
            if (accelerated_) {
                const auto target = record.view->render_target();
                if (target.is_empty || target.width == 0 || target.height == 0 || target.texture_id == 0 ||
                    target.texture_width == 0 || target.texture_height == 0 || !gpuDriver_)
                    return;
                PublishedFrame frame;
                frame.width = target.width;
                frame.height = target.height;
                frame.textureId = target.texture_id;
                frame.textureWidth = target.texture_width;
                frame.textureHeight = target.texture_height;
                frame.uvLeft = target.uv_coords.left;
                frame.uvTop = target.uv_coords.top;
                frame.uvRight = target.uv_coords.right;
                frame.uvBottom = target.uv_coords.bottom;
                frame.deviceEpoch = deviceEpoch_;
                next.emplace(id, std::move(frame));
                return;
            }
            auto* surface = record.view->surface();
            if (!surface || !surface->width() || !surface->height()) return;
            auto bitmapSurface = dynamic_cast<ultralight::BitmapSurface*>(surface);
            if (!bitmapSurface) return;
            auto bitmap = bitmapSurface->bitmap();
            if (!bitmap) return;
            const auto dims = publishedDims.find(id);
            if (surface->dirty_bounds().IsEmpty() && dims != publishedDims.end() &&
                dims->second.first == bitmap->width() && dims->second.second == bitmap->height()) {
                PublishedFrame frame;
                frame.carryForward = true;
                next.emplace(id, std::move(frame));
                return;
            }
            auto pixels = bitmap->LockPixelsSafe();
            if (!pixels || !pixels.data()) return;
            PublishedFrame frame;
            frame.width = bitmap->width();
            frame.height = bitmap->height();
            frame.textureWidth = bitmap->width();
            frame.textureHeight = bitmap->height();
            frame.rowBytes = bitmap->row_bytes();
            frame.format = DXGI_FORMAT_B8G8R8A8_UNORM;
            frame.pixels.resize(frame.rowBytes * frame.height);
            std::memcpy(frame.pixels.data(), pixels.data(), frame.pixels.size());
            next.emplace(id, std::move(frame));
            surface->ClearDirtyBounds();
        });
        if (next.empty()) {
            if (!accelerated_) return;
            std::lock_guard lock(stateMutex_);
            const uint64_t generation = gpuDriver_->PendingGenerationId();
            if (!GPU::GenerationDrain::HasPublishableGeneration(generation)) return;
            published_.clear();
            completedGeneration_ = generation;
            stateCv_.notify_all();
            return;
        }
        {
            std::lock_guard lock(stateMutex_);
            const uint64_t generation = accelerated_ ? gpuDriver_->PendingGenerationId() : nextGeneration_++;
            if (!GPU::GenerationDrain::HasPublishableGeneration(generation)) return;
            for (auto it = next.begin(); it != next.end();) {
                auto& frame = it->second;
                frame.generation = generation;
                if (accelerated_) frame.deviceEpoch = deviceEpoch_;
                const auto old = published_.find(it->first);
                if (frame.carryForward) {
                    if (old == published_.end()) {
                        it = next.erase(it);
                        continue;
                    }
                    auto carried = std::move(old->second);
                    carried.generation = generation;
                    carried.carryForward = false;
                    published_[it->first] = std::move(carried);
                    ++it;
                    continue;
                }
                frame.contentGeneration = generation;
                if (!accelerated_ && old != published_.end()) {
                    frame.texture = std::move(old->second.texture);
                    frame.srv = std::move(old->second.srv);
                }
                published_[it->first] = std::move(frame);
                ++it;
            }
            if (next.empty()) return;
            for (auto it = published_.begin(); it != published_.end();) {
                if (!next.contains(it->first))
                    it = published_.erase(it);
                else
                    ++it;
            }
            completedGeneration_ = generation;
        }
        stateCv_.notify_all();
    }

}
