#pragma once

#include "ModelPreviewImageSourcePolicy.h"
#include "ModelPreviewLoad.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <d3d11.h>
#include <DirectXMath.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace PrismaUI::ModelPreview {

struct Config {
    bool loaded = false;
    bool enabled = true;
    int rtSize = 512;
    float spinDegPerSec = 45.0f;
    bool workerThreads = true;

    int maxLivePreviews = 192;
    std::string soloShape;
};

inline Config g_cfg;

struct Cur {
    const uint8_t* p = nullptr;
    size_t n = 0, off = 0;
    bool ok = true;
    bool need(size_t k) { if (off + k > n) { ok = false; return false; } return true; }
    uint8_t  u8()  { if (!need(1)) return 0; return p[off++]; }
    uint16_t u16() { if (!need(2)) return 0; uint16_t v; memcpy(&v, p + off, 2); off += 2; return v; }
    uint32_t u32() { if (!need(4)) return 0; uint32_t v; memcpy(&v, p + off, 4); off += 4; return v; }
    uint64_t u64() { if (!need(8)) return 0; uint64_t v; memcpy(&v, p + off, 8); off += 8; return v; }
    float    f32() { if (!need(4)) return 0; float v; memcpy(&v, p + off, 4); off += 4; return v; }
    void skip(size_t k) { if (need(k)) off += k; }
    bool bytes(void* dst, size_t k) { if (!need(k)) return false; memcpy(dst, p + off, k); off += k; return true; }

    std::string sized() {
        uint32_t len = u32();
        if (len == 0 || len > 4096 || !need(len)) return std::string();
        std::string s(reinterpret_cast<const char*>(p + off), len);
        off += len;
        return s;
    }
};

struct MaterialTextures {
    std::string diffuse;
    std::string normal;
    bool ok = false;
};

struct ParseStats { int shapes = 0, dropped = 0, skinnedSkipped = 0; };

struct ConnectPoint {
    std::string root;
    std::string name;
    Xf          xf;
};

struct NifParse {
    std::vector<CpuShape> shapes;
    bool hasInvMarker = false;
    float invRot[3] = {};

    std::vector<ConnectPoint>    cpParents;
    std::vector<std::string>     cpChildren;
    std::map<std::string, Xf>    nodeWorld;
};

struct Preview {
    ViewId viewId = 0;
    std::string id;
    std::vector<std::string> nifPaths;
    PxRect rect;
    PxRect clip;
    float zoom = 1, panX = 0, panY = 0, roll = 0, pitch = 0, brightness = 1;
    float spin = -10000.0f, yaw = 0;
    bool flip = false;
    float lastAngleDeg = 0;
    std::chrono::steady_clock::time_point spinStart;
    bool spinStarted = false;
    std::string modelKey, wantKey;
    uint64_t loadGen = 0;
    uint64_t continuityKey = 0;
    bool imageSource = false;
    ImageSourcePolicy::State image;
    std::shared_ptr<PreviewModel> model;
    XMFLOAT3 center = { 0,0,0 };
    float radius = 1;
    XMFLOAT4X4 orient = {};
    int kind = 0;
    bool dirty = true;

    uint64_t lastRenderFrame = 0;

    uint64_t touch = 0;

    bool drawn = false;
    bool statusTexSent = false;
    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
};

struct RtTarget {
    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
};

struct CBData { XMFLOAT4X4 wvp, world; XMFLOAT4 light0, light1, params, emissive, colorSpace; };
static_assert(sizeof(CBData) % 16 == 0);

struct ScopedStateBackup {
    ID3D11DeviceContext* ctx;
    ID3D11InputLayout* layout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11Buffer* vb = nullptr; UINT vbStride = 0, vbOffset = 0;
    ID3D11Buffer* ib = nullptr; DXGI_FORMAT ibFormat = DXGI_FORMAT_UNKNOWN; UINT ibOffset = 0;
    ID3D11VertexShader* vs = nullptr; ID3D11PixelShader* ps = nullptr;
    ID3D11Buffer* vsCb = nullptr; ID3D11Buffer* psCb = nullptr;
    ID3D11ShaderResourceView* psSrv = nullptr; ID3D11SamplerState* psSamp = nullptr;
    ID3D11RasterizerState* rs = nullptr;
    UINT numVP = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_VIEWPORT vps[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    ID3D11BlendState* blend = nullptr; FLOAT blendFactor[4] = {}; UINT sampleMask = 0;
    ID3D11DepthStencilState* dss = nullptr; UINT stencilRef = 0;
    ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView* dsv = nullptr;
    ID3D11GeometryShader* gs = nullptr;
    ID3D11HullShader* hs = nullptr;
    ID3D11DomainShader* ds = nullptr;
    explicit ScopedStateBackup(ID3D11DeviceContext* c) : ctx(c) {
        ctx->IAGetInputLayout(&layout); ctx->IAGetPrimitiveTopology(&topology);
        ctx->IAGetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);
        ctx->IAGetIndexBuffer(&ib, &ibFormat, &ibOffset);
        ctx->VSGetShader(&vs, nullptr, nullptr); ctx->PSGetShader(&ps, nullptr, nullptr);
        ctx->VSGetConstantBuffers(0, 1, &vsCb); ctx->PSGetConstantBuffers(0, 1, &psCb);
        ctx->PSGetShaderResources(0, 1, &psSrv); ctx->PSGetSamplers(0, 1, &psSamp);
        ctx->RSGetState(&rs); ctx->RSGetViewports(&numVP, vps);
        ctx->OMGetBlendState(&blend, blendFactor, &sampleMask);
        ctx->OMGetDepthStencilState(&dss, &stencilRef);
        ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, &dsv);
        ctx->GSGetShader(&gs, nullptr, nullptr);
        ctx->HSGetShader(&hs, nullptr, nullptr);
        ctx->DSGetShader(&ds, nullptr, nullptr);

        ctx->GSSetShader(nullptr, nullptr, 0);
        ctx->HSSetShader(nullptr, nullptr, 0);
        ctx->DSSetShader(nullptr, nullptr, 0);
    }
    ~ScopedStateBackup() {
        ctx->GSSetShader(gs, nullptr, 0);
        ctx->HSSetShader(hs, nullptr, 0);
        ctx->DSSetShader(ds, nullptr, 0);
        ctx->IASetInputLayout(layout); ctx->IASetPrimitiveTopology(topology);
        ctx->IASetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);
        ctx->IASetIndexBuffer(ib, ibFormat, ibOffset);
        ctx->VSSetShader(vs, nullptr, 0); ctx->PSSetShader(ps, nullptr, 0);
        ctx->VSSetConstantBuffers(0, 1, &vsCb); ctx->PSSetConstantBuffers(0, 1, &psCb);
        ctx->PSSetShaderResources(0, 1, &psSrv); ctx->PSSetSamplers(0, 1, &psSamp);
        ctx->RSSetState(rs); if (numVP) ctx->RSSetViewports(numVP, vps);
        ctx->OMSetBlendState(blend, blendFactor, sampleMask);
        ctx->OMSetDepthStencilState(dss, stencilRef);
        ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, dsv);
        auto rel = [](IUnknown* p) { if (p) p->Release(); };
        rel(layout); rel(vb); rel(ib); rel(vs); rel(ps); rel(vsCb); rel(psCb);
        rel(psSrv); rel(psSamp); rel(rs); rel(blend); rel(dss); rel(dsv);
        rel(gs); rel(hs); rel(ds);
        for (auto* r : rtvs) rel(r);
    }
};

inline std::mutex g_texMutex;
inline std::unordered_map<std::string, RE::NiPointer<RE::NiTexture>> g_texDone;
inline std::deque<std::string> g_texOrder;
inline std::unordered_map<std::string, std::chrono::steady_clock::time_point> g_texInFlight;
inline std::atomic<bool>     g_texFailClosed{ false };
inline std::atomic<bool>     g_texShutdown{ false };
inline std::atomic<uint64_t> g_texRequested{ 0 }, g_texResolved{ 0 };

inline std::map<std::string, Preview> g_previews;
inline std::vector<uint64_t> g_retractedImageKeys;
inline std::vector<uint64_t> g_releasedImageKeys;

inline ID3D11Device* g_dev = nullptr;
inline bool g_d3dReady = false;
inline bool g_srgbRtSupported = false;
inline ComPtr<ID3D11Texture2D> g_depthTex;
inline ComPtr<ID3D11DepthStencilView> g_dsv;
inline ComPtr<ID3D11Buffer> g_cb;
inline ComPtr<ID3D11SamplerState> g_sampler;
inline ComPtr<ID3D11RasterizerState> g_rasterizer;
inline ComPtr<ID3D11DepthStencilState> g_depthState;
inline ComPtr<ID3D11DepthStencilState> g_depthStateNoWrite;
inline ComPtr<ID3D11BlendState> g_blendState;
inline ComPtr<ID3D11BlendState> g_blendAlpha;
inline ComPtr<ID3D11BlendState> g_blendAdditive;
inline ComPtr<ID3D11ShaderResourceView> g_graySrv;
inline ComPtr<ID3D11PixelShader> g_ps;
inline ComPtr<ID3D11VertexShader> g_vs[4];
inline ComPtr<ID3DBlob> g_vsBlob[4];
inline std::map<uint64_t, ComPtr<ID3D11InputLayout>> g_layouts;

inline std::vector<RtTarget> g_rtFreeList;
inline size_t g_rtLive = 0;

inline uint64_t g_previewFrame = 0;

inline uint64_t g_overlayCacheFrame = ~uint64_t{0};
inline std::map<ViewId, std::vector<Overlay>> g_overlayCache;

inline uint64_t g_evictions = 0;

inline Microsoft::WRL::ComPtr<IDXGIAdapter3> g_vramAdapter;

std::string JoinNifPathsDisplay(const std::vector<std::string>& paths);
bool LoadMaterialTextures(const std::string& matPath, MaterialTextures& out);
uint32_t DescAttrOffset(uint64_t d, int a);
bool DescHasFlag(uint64_t d, uint32_t f);
uint32_t DescPosBytes(uint64_t d);
std::string SocketKey(const std::string& s);
bool ParseNif(const std::vector<uint8_t>& data, NifParse& out, ParseStats& st,
              const std::unordered_map<std::string, std::string>& swapMap = {});
void ServiceTextureWatchdog();
bool AcquireEngineTexture(const std::string& path, RE::NiPointer<RE::NiTexture>& out);
ID3D11ShaderResourceView* ResolveSrv(void* rendererTexture);
bool SrvNeedsShaderSrgbDecode(ID3D11ShaderResourceView* srv);
std::string MakeFullKey(ViewId v, const std::string& id);
std::string JsonStr(const std::string& j, const char* key);
double JsonNum(const std::string& j, const char* key, double fb);
bool JsonBool(const std::string& j, const char* key);
std::vector<std::string> JsonStrArray(const std::string& j, const char* key);
void EmitStatus(const Preview& pv, bool ok, const std::string& error, const char* phase);
std::unordered_map<std::string, std::string> ParseSwapMap(const std::string& s);
bool EnsureRenderResources(ID3D11Device* dev);
ID3D11InputLayout* GetOrCreateLayout(uint64_t desc, int vsIndex, bool floatPos);
void RebuildOverlayCache();
bool EnsurePreviewTarget(Preview& pv);
void ReleaseTarget(Preview& pv);
float EffSpin(float s);
bool ComputePose(Preview& pv, std::shared_ptr<PreviewModel> pm, const std::string& key, int kind);
bool RenderPreviewInner(ID3D11DeviceContext* ctx, Preview& pv);
void LogMemoryTelemetry();

}
