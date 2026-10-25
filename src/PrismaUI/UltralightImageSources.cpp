#include "UltralightImageSources.h"

namespace PrismaUI::WebRuntimeUltralight {

    std::string UltralightImageSources::Reserve(std::uint64_t key) {
        return tokens_.Reserve(key);
    }

    bool UltralightImageSources::IsReserved(std::uint64_t key) const {
        return tokens_.IsReserved(key);
    }

    bool UltralightImageSources::IsRegisteredPath(std::string_view logicalPath, std::string& identifierOut) const {
        return tokens_.Resolve(logicalPath, identifierOut);
    }

    void UltralightImageSources::Publish(std::uint64_t key, std::uint32_t textureId, std::uint32_t width,
                                         std::uint32_t height) {
        const auto identifier = tokens_.Identifier(key);
        if (identifier.empty()) return;
        if (const auto found = published_.find(key); found != published_.end()) {
            const auto& current = found->second;
            if (current.textureId == textureId && current.width == width && current.height == height) {
                current.source->Invalidate();
                return;
            }
        }
        Remove(key);
        auto source = ultralight::ImageSource::CreateFromTexture(width, height, textureId,
                                                                 ultralight::Rect{0.0f, 0.0f, 1.0f, 1.0f});
        ultralight::ImageSourceProvider::instance().AddImageSource(ultralight::String(identifier.c_str()), source);
        published_.emplace(key, Published{identifier, source, textureId, width, height});
    }

    void UltralightImageSources::Remove(std::uint64_t key) {
        const auto found = published_.find(key);
        if (found == published_.end()) return;
        ultralight::ImageSourceProvider::instance().RemoveImageSource(
            ultralight::String(found->second.identifier.c_str()));
        published_.erase(found);
    }

    void UltralightImageSources::Release(std::uint64_t key) {
        Remove(key);
        tokens_.Release(key);
    }

    void UltralightImageSources::RemoveAll() {
        auto& provider = ultralight::ImageSourceProvider::instance();
        for (const auto& [_, entry] : published_) {
            provider.RemoveImageSource(ultralight::String(entry.identifier.c_str()));
        }
        published_.clear();
        tokens_.Clear();
    }

}
