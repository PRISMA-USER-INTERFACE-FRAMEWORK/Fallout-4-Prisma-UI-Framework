#include <cstdio>
#include <cstdlib>
#include <iterator>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "GPU/UltralightGPUUniforms.h"
#include "GPU/UltralightPipelineLayout.h"
#include "GPU/UltralightShaders.h"

namespace {

using Microsoft::WRL::ComPtr;
using namespace PrismaUI::GPU;

void Expect(HRESULT result, const char* step) {
    if (FAILED(result)) {
        std::fprintf(stderr, "gpu_pipeline_warp_tests: %s failed: 0x%08lX\n", step, static_cast<unsigned long>(result));
        std::abort();
    }
}

template <std::size_t N>
ComPtr<ID3DBlob> Compile(const char (&source)[N], const char* entry, const char* target) {
    ComPtr<ID3DBlob> code;
    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompile(source, N - 1, nullptr, nullptr, nullptr, entry, target,
                                      kUltralightShaderCompileFlags, 0, code.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(result)) {
        if (errors) std::fprintf(stderr, "%s\n", static_cast<const char*>(errors->GetBufferPointer()));
        Expect(result, target);
    }
    return code;
}

}

int main() {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    const D3D_FEATURE_LEVEL requested[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    Expect(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, requested, std::size(requested),
                             D3D11_SDK_VERSION, &device, nullptr, &context),
           "D3D11CreateDevice(WARP)");

    const auto fillPathVsBlob = Compile(kFillPathVS, "VS", "vs_4_0");
    const auto fillPathPsBlob = Compile(kFillPathPS, "PS", "ps_4_0");
    const auto fillVsBlob = Compile(kFillVS, "VS", "vs_4_0");
    const auto fillPsBlob = Compile(kFillPS, "PS", "ps_4_0");

    ComPtr<ID3D11VertexShader> fillPathVs;
    ComPtr<ID3D11PixelShader> fillPathPs;
    ComPtr<ID3D11VertexShader> fillVs;
    ComPtr<ID3D11PixelShader> fillPs;
    Expect(device->CreateVertexShader(fillPathVsBlob->GetBufferPointer(), fillPathVsBlob->GetBufferSize(), nullptr,
                                      &fillPathVs),
           "CreateVertexShader(FillPath)");
    Expect(device->CreatePixelShader(fillPathPsBlob->GetBufferPointer(), fillPathPsBlob->GetBufferSize(), nullptr,
                                     &fillPathPs),
           "CreatePixelShader(FillPath)");
    Expect(device->CreateVertexShader(fillVsBlob->GetBufferPointer(), fillVsBlob->GetBufferSize(), nullptr, &fillVs),
           "CreateVertexShader(Fill)");
    Expect(device->CreatePixelShader(fillPsBlob->GetBufferPointer(), fillPsBlob->GetBufferSize(), nullptr, &fillPs),
           "CreatePixelShader(Fill)");

    ComPtr<ID3D11InputLayout> fillPathLayout;
    Expect(device->CreateInputLayout(kFillPathLayout, static_cast<UINT>(std::size(kFillPathLayout)),
                                     fillPathVsBlob->GetBufferPointer(), fillPathVsBlob->GetBufferSize(),
                                     &fillPathLayout),
           "CreateInputLayout(FillPath)");
    ComPtr<ID3D11InputLayout> fillLayout;
    Expect(device->CreateInputLayout(kFillLayout, static_cast<UINT>(std::size(kFillLayout)),
                                     fillVsBlob->GetBufferPointer(), fillVsBlob->GetBufferSize(), &fillLayout),
           "CreateInputLayout(Fill)");

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(UltralightGPUUniformBlock);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ComPtr<ID3D11Buffer> constantBuffer;
    Expect(device->CreateBuffer(&cbDesc, nullptr, &constantBuffer), "CreateBuffer(uniforms)");

    std::printf("gpu_pipeline_warp_tests passed\n");
    return 0;
}
