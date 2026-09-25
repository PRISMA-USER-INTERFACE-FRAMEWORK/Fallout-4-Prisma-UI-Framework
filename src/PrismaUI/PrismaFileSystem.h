#pragma once

#include <filesystem>
#include <string>

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/Ultralight.h>
#pragma warning(pop)

namespace PrismaUI::WebRuntimeUltralight {

    class PrismaFileSystem final : public ultralight::FileSystem {
    public:
        explicit PrismaFileSystem(std::filesystem::path root);

        bool FileExists(const ultralight::String& filePath) override;
        ultralight::String GetFileMimeType(const ultralight::String& filePath) override;
        ultralight::String GetFileCharset(const ultralight::String& filePath) override;
        ultralight::RefPtr<ultralight::Buffer> OpenFile(const ultralight::String& filePath) override;

    private:
        std::filesystem::path Resolve(const std::string& request) const;

        std::filesystem::path root_;
        std::filesystem::path resourcesRoot_;
        std::filesystem::path inspectorRoot_;
    };

}
