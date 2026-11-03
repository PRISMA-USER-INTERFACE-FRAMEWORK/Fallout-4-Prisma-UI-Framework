#pragma once

#include <cstdint>
#include <map>
#include <mutex>

#include <d3d11.h>
#include <wrl/client.h>

#include "PrismaUI/BorrowedSrvPolicy.h"

namespace PrismaUI::GPU {

    struct ExternalTexture {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint64_t generation = 0;
        std::uint64_t key = 0;
    };

    class ExternalTextureRegistry final {
    public:
        ExternalTextureRegistry() = default;
        ExternalTextureRegistry(const ExternalTextureRegistry&) = delete;
        ExternalTextureRegistry& operator=(const ExternalTextureRegistry&) = delete;

        bool Bind(std::uint32_t textureId, std::uint64_t key, ID3D11ShaderResourceView* srv, std::uint32_t width,
                  std::uint32_t height, std::uint64_t generation, ID3D11Device* canonicalDevice) {
            if (textureId == 0 || !srv || !canonicalDevice) return false;
            Microsoft::WRL::ComPtr<ID3D11Device> device;
            srv->GetDevice(device.GetAddressOf());
            const auto decision = PrismaUI::BorrowedSrvPolicy::Decide(PrismaUI::BorrowedSrvPolicy::Owner::PrismaOwned,
                                                                      device.Get() == canonicalDevice);
            if (decision == PrismaUI::BorrowedSrvPolicy::Decision::Reject) return false;
            std::lock_guard lock{mutex_};
            textures_[textureId] = ExternalTexture{srv, width, height, generation, key};
            return true;
        }

        [[nodiscard]] Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Acquire(std::uint32_t textureId) const {
            std::lock_guard lock{mutex_};
            const auto found = textures_.find(textureId);
            if (found == textures_.end()) return {};
            return found->second.srv;
        }

        [[nodiscard]] bool Contains(std::uint32_t textureId) const {
            std::lock_guard lock{mutex_};
            return textures_.contains(textureId);
        }

        [[nodiscard]] bool HasLiveView(std::uint64_t key) const {
            std::lock_guard lock{mutex_};
            for (const auto& [_, texture] : textures_)
                if (texture.key == key && texture.srv) return true;
            return false;
        }

        void Unbind(std::uint32_t textureId) {
            std::lock_guard lock{mutex_};
            if (const auto found = textures_.find(textureId); found != textures_.end()) found->second = {};
        }

        void ReleaseViews() {
            std::lock_guard lock{mutex_};
            for (auto& [_, texture] : textures_) texture.srv.Reset();
        }

        void Clear() {
            std::lock_guard lock{mutex_};
            textures_.clear();
        }

        [[nodiscard]] std::size_t Size() const {
            std::lock_guard lock{mutex_};
            return textures_.size();
        }

    private:
        mutable std::mutex mutex_;
        std::map<std::uint32_t, ExternalTexture> textures_;
    };

}
