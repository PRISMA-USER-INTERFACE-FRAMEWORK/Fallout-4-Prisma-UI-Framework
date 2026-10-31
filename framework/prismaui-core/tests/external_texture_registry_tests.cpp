#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <thread>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include "GPU/ExternalTextureRegistry.h"

namespace {

using Microsoft::WRL::ComPtr;
using PrismaUI::GPU::ExternalTextureRegistry;

constexpr std::uint32_t kThreads = 3;
constexpr std::uint32_t kIterations = 3000;

void Expect(HRESULT result, const char* step) {
    if (FAILED(result)) {
        std::fprintf(stderr, "external_texture_registry_tests: %s failed: 0x%08lX\n", step,
                     static_cast<unsigned long>(result));
        std::abort();
    }
}

void Check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "external_texture_registry_tests: %s\n", message);
        std::abort();
    }
}

ComPtr<ID3D11Device> CreateWarpDevice() {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL requested[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    Expect(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, requested, std::size(requested),
                             D3D11_SDK_VERSION, &device, nullptr, &context),
           "D3D11CreateDevice(WARP)");
    return device;
}

ComPtr<ID3D11ShaderResourceView> CreateSrv(ID3D11Device* device) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 64;
    desc.Height = 64;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> texture;
    Expect(device->CreateTexture2D(&desc, nullptr, &texture), "CreateTexture2D");
    ComPtr<ID3D11ShaderResourceView> srv;
    Expect(device->CreateShaderResourceView(texture.Get(), nullptr, &srv), "CreateShaderResourceView");
    return srv;
}

ULONG RefCount(ID3D11ShaderResourceView* srv) {
    srv->AddRef();
    return srv->Release();
}

}

int main() {
    const ComPtr<ID3D11Device> deviceA = CreateWarpDevice();
    const ComPtr<ID3D11Device> deviceB = CreateWarpDevice();
    const ComPtr<ID3D11ShaderResourceView> srvA = CreateSrv(deviceA.Get());
    const ComPtr<ID3D11ShaderResourceView> srvA2 = CreateSrv(deviceA.Get());
    const ComPtr<ID3D11ShaderResourceView> srvB = CreateSrv(deviceB.Get());
    const ULONG baselineA = RefCount(srvA.Get());
    const ULONG baselineA2 = RefCount(srvA2.Get());
    const ULONG baselineB = RefCount(srvB.Get());

    ExternalTextureRegistry registry;

    Check(!registry.Bind(0, 1, srvA.Get(), 64, 64, 1, deviceA.Get()), "id 0 is rejected");
    Check(!registry.Bind(1, 1, nullptr, 64, 64, 1, deviceA.Get()), "a null SRV is rejected");
    Check(!registry.Bind(1, 1, srvA.Get(), 64, 64, 1, nullptr), "a null device is rejected");
    Check(registry.Size() == 0, "invalid binds store nothing");
    Check(registry.Acquire(1).Get() == nullptr, "an unknown id acquires empty");
    Check(!registry.HasLiveView(99), "an unknown key is not live");

    Check(registry.Bind(1, 10, srvA.Get(), 64, 64, 1, deviceA.Get()), "a same-device Bind succeeds");
    Check(registry.HasLiveView(10), "a bound key is live");
    Check(registry.Acquire(1).Get() == srvA.Get(), "Acquire returns the bound SRV");
    Check(RefCount(srvA.Get()) == baselineA + 1, "Bind holds a strong reference");

    Check(!registry.Bind(2, 20, srvB.Get(), 64, 64, 1, deviceA.Get()), "a cross-device SRV is rejected");
    Check(!registry.HasLiveView(20), "a rejected cross-device key is not live");
    Check(!registry.Contains(2), "a rejected SRV is not stored");
    Check(RefCount(srvB.Get()) == baselineB, "a rejected SRV takes no reference");

    Check(registry.Bind(1, 11, srvA2.Get(), 64, 64, 2, deviceA.Get()), "a replacing Bind succeeds");
    Check(RefCount(srvA.Get()) == baselineA, "a replacing Bind releases the old SRV");
    Check(RefCount(srvA2.Get()) == baselineA2 + 1, "a replacing Bind holds the new SRV");
    Check(registry.Size() == 1, "a replacing Bind keeps one entry");

    registry.Unbind(1);
    Check(!registry.HasLiveView(11), "an unbound key is not live");
    Check(registry.Contains(1), "Unbind leaves a tombstone for the id");
    Check(registry.Acquire(1).Get() == nullptr, "Unbind leaves no view");
    Check(RefCount(srvA2.Get()) == baselineA2, "Unbind releases the SRV");

    Check(registry.Bind(1, 12, srvA.Get(), 64, 64, 3, deviceA.Get()), "Bind before ReleaseViews succeeds");
    registry.ReleaseViews();
    Check(registry.Contains(1), "ReleaseViews keeps the id");
    Check(registry.Acquire(1).Get() == nullptr, "ReleaseViews drops the view");
    Check(RefCount(srvA.Get()) == baselineA, "ReleaseViews releases the SRV");
    Check(!registry.HasLiveView(12), "ReleaseViews leaves no live key");

    Check(registry.Bind(1, 12, srvA.Get(), 64, 64, 3, deviceA.Get()), "Bind before Clear succeeds");
    Check(registry.Bind(2, 13, srvA2.Get(), 64, 64, 3, deviceA.Get()), "second Bind before Clear succeeds");
    registry.Clear();
    registry.Clear();
    Check(registry.Size() == 0, "Clear empties the registry");
    Check(!registry.Contains(1), "Clear removes the ids");
    Check(RefCount(srvA.Get()) == baselineA, "Clear releases the first SRV");
    Check(RefCount(srvA2.Get()) == baselineA2, "Clear releases the second SRV");

    std::vector<std::thread> threads;
    for (std::uint32_t t = 0; t < kThreads; ++t) {
        threads.emplace_back([&registry, &srvA, &srvB, &deviceA, t] {
            for (std::uint32_t i = 0; i < kIterations; ++i) {
                const std::uint32_t id = 1 + (i + t) % 3;
                registry.Bind(id, i, srvA.Get(), 64, 64, i, deviceA.Get());
                registry.Bind(id + 3, i, srvB.Get(), 64, 64, i, deviceA.Get());
                (void)registry.Acquire(id);
                if (i % 5 == 0) registry.Unbind(id);
                if (i % 1000 == 999) registry.Clear();
            }
        });
    }
    for (auto& thread : threads) thread.join();
    registry.Clear();
    Check(RefCount(srvA.Get()) == baselineA, "the threaded loop leaves no SRV reference behind");
    Check(RefCount(srvB.Get()) == baselineB, "the threaded loop leaves no rejected SRV reference behind");

    std::printf("external_texture_registry_tests passed\n");
    return 0;
}
