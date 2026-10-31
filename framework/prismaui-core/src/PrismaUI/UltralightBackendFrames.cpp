#include "UltralightBackend.h"

#include "GPU/FalloutGPUDriver.h"
#include "GPU/GenerationDrainState.h"

#include <cassert>
#include <cstring>
#include <map>
#include <set>
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
        std::set<PrismaUI::Web::ViewId> liveViews;
        viewManager_.ForEach([&](PrismaUI::Web::ViewId id, ViewRecord& record) {
            if (!record.view) return;
            liveViews.insert(id);
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
            const auto* source = static_cast<const uint8_t*>(pixels.data());
            auto& shadow = cpuShadows_[id];
            const bool sameLayout = shadow.width == frame.width && shadow.height == frame.height &&
                                    shadow.rowBytes == frame.rowBytes && dims != publishedDims.end() &&
                                    dims->second.first == frame.width && dims->second.second == frame.height;
            if (sameLayout) {
                const auto bounds = surface->dirty_bounds();
                frame.dirty = PrismaUI::PartialUpload::Clamp(bounds.left, bounds.top, bounds.right, bounds.bottom,
                                                             frame.width, frame.height);
                if (frame.dirty.Empty()) {
                    PublishedFrame carried;
                    carried.carryForward = true;
                    next.emplace(id, std::move(carried));
                    surface->ClearDirtyBounds();
                    return;
                }
                PrismaUI::PartialUpload::CopyBox(shadow.pixels.data(), source, frame.rowBytes, frame.dirty);
                frame.partial = true;
            } else {
                shadow.width = frame.width;
                shadow.height = frame.height;
                shadow.rowBytes = frame.rowBytes;
                shadow.pixels.assign(source, source + static_cast<std::size_t>(frame.rowBytes) * frame.height);
                frame.pixels = shadow.pixels;
                frame.dirty = PrismaUI::PartialUpload::Full(frame.width, frame.height);
            }
            next.emplace(id, std::move(frame));
            surface->ClearDirtyBounds();
        });
        std::erase_if(cpuShadows_, [&](const auto& entry) { return !liveViews.contains(entry.first); });
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
                if (frame.partial) {
                    const auto& shadow = cpuShadows_[it->first];
                    if (old != published_.end() && old->second.width == frame.width &&
                        old->second.rowBytes == frame.rowBytes &&
                        PrismaUI::PartialUpload::Merge(old->second.pixels, old->second.pendingUpload, shadow.pixels,
                                                       frame.rowBytes, frame.height, frame.dirty)) {
                        auto& base = old->second;
                        base.generation = generation;
                        base.contentGeneration = generation;
                        ++it;
                        continue;
                    }
                    frame.pixels = shadow.pixels;
                    frame.dirty = PrismaUI::PartialUpload::Full(frame.width, frame.height);
                }
                frame.partial = false;
                frame.pendingUpload = frame.dirty;
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

    PrismaUI::GPU::LateRerender::Usage UltralightBackend::PendingUsage() const {
        if (!gpuDriver_) return {};
        const auto payload = gpuDriver_->PendingPayloadSummary();
        return {gpuDriver_->PendingCommandCount(), payload.textureBytes, payload.nonTextureBytes, payload.bytes};
    }

    PrismaUI::GPU::LateRerender::Usage UltralightBackend::BeginLateRenderWindow() {
        lateRerenderWanted_.store(false, std::memory_order_release);
        std::lock_guard lock(stateMutex_);
        structuralChange_ = false;
        return PendingUsage();
    }

    void UltralightBackend::RecordLateRenderUsage(const PrismaUI::GPU::LateRerender::Usage& before,
                                                  const PrismaUI::GPU::LateRerender::Usage& rendered) {
        PrismaUI::GPU::LateRerender::Usage largest;
        {
            std::lock_guard lock(stateMutex_);
            lateDeltas_.Record(PrismaUI::GPU::LateRerender::Delta(rendered, before));
            const auto now = PrismaUI::GPU::LateRerender::SteadyNowMs();
            if (now - lateLogMs_ < PrismaUI::GPU::LateRerender::kLogIntervalMs) return;
            lateLogMs_ = now;
            largest = lateDeltas_.LargestRecorded();
        }
        logger::info("[Latency] late-rerender largest ops={} texBytes={} nonTexBytes={} bytes={}",
                     largest.operations, largest.textureBytes, largest.nonTextureBytes, largest.bytes);
    }

}
