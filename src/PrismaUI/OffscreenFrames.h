#pragma once

#include <wrl/client.h>

#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace PrismaUI::Web {
    struct IWebBackend;
}

namespace PrismaUI::OffscreenFrames {
    using ViewId = std::uint64_t;
    using Backend = PrismaUI::Web::IWebBackend*;

    void Track(ViewId view);

    void Forget(ViewId view);

    bool IsTracked(ViewId view);

    void* PeekSRV(ViewId view);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> AcquireSRV(ViewId view);

    void Update(Backend backend, std::uint64_t presentGeneration, ID3D11Device* device, ID3D11DeviceContext* context);
}
