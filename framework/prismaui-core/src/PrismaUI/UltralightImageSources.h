#pragma once

#include "ImageSourceTokens.h"

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/Ultralight.h>
#pragma warning(pop)

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace PrismaUI::WebRuntimeUltralight {

    class UltralightImageSources final {
    public:
        [[nodiscard]] std::string Reserve(std::uint64_t key);
        [[nodiscard]] bool IsReserved(std::uint64_t key) const;
        [[nodiscard]] bool IsRegisteredPath(std::string_view logicalPath, std::string& identifierOut) const;
        void Publish(std::uint64_t key, std::uint32_t textureId, std::uint32_t width, std::uint32_t height);
        void Remove(std::uint64_t key);
        void Release(std::uint64_t key);
        void RemoveAll();

    private:
        struct Published {
            std::string identifier;
            ultralight::RefPtr<ultralight::ImageSource> source;
            std::uint32_t textureId = 0;
            std::uint32_t width = 0;
            std::uint32_t height = 0;
        };

        PrismaUI::ImageSourceTokens::TokenTable tokens_;
        std::map<std::uint64_t, Published> published_;
    };

}
