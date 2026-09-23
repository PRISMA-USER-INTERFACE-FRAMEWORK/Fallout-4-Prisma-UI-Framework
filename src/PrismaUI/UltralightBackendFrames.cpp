#include "UltralightBackend.h"

#include "GPU/FalloutGPUDriver.h"
#include "GPU/GenerationDrainState.h"

#include <cstring>
#include <map>
#include <utility>

namespace PrismaUI::WebRuntimeUltralight {

    void UltralightBackend::PublishFrames() {
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

    void UltralightBackend::ApplyClearColor(ViewRecord& record) {
        if (!record.view) return;
        auto* surface = record.view->surface();
        auto* bitmapSurface = surface ? dynamic_cast<ultralight::BitmapSurface*>(surface) : nullptr;
        if (!bitmapSurface) return;
        auto bitmap = bitmapSurface->bitmap();
        if (!bitmap) return;
        auto pixels = bitmap->LockPixelsSafe();
        if (!pixels || !pixels.data()) return;
        const uint8_t alpha = static_cast<uint8_t>(record.clearColor >> 24);
        const uint8_t red = static_cast<uint8_t>((static_cast<uint32_t>(record.clearColor >> 16) * alpha + 127) / 255);
        const uint8_t green = static_cast<uint8_t>((static_cast<uint32_t>(record.clearColor >> 8) * alpha + 127) / 255);
        const uint8_t blue = static_cast<uint8_t>((static_cast<uint32_t>(record.clearColor) * alpha + 127) / 255);
        for (uint32_t y = 0; y < bitmap->height(); ++y) {
            auto* row = static_cast<uint8_t*>(pixels.data()) + static_cast<std::size_t>(y) * bitmap->row_bytes();
            for (uint32_t x = 0; x < bitmap->width(); ++x) {
                row[x * 4 + 0] = blue;
                row[x * 4 + 1] = green;
                row[x * 4 + 2] = red;
                row[x * 4 + 3] = alpha;
            }
        }
        surface->set_dirty_bounds(
            ultralight::IntRect{0, 0, static_cast<int>(bitmap->width()), static_cast<int>(bitmap->height())});
    }

}
