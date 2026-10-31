#pragma once

#include "ModelPreview.h"
#include "ModelPreviewReads.h"

#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

namespace PrismaUI::ModelPreview {

inline std::string JoinNifPathsKey(const std::vector<std::string>& paths) {
    std::string k;
    for (auto& p : paths) { k += LowerStr(p); k += '|'; }
    return k;
}

struct Xf {
    float r[3][3] = { {1,0,0},{0,1,0},{0,0,1} };
    float t[3] = { 0,0,0 };
    float s = 1.0f;
    Xf operator*(const Xf& c) const {
        Xf o;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                float sum = 0; for (int k = 0; k < 3; ++k) sum += r[i][k] * c.r[k][j];
                o.r[i][j] = sum;
            }
        for (int i = 0; i < 3; ++i) {
            float v = 0; for (int k = 0; k < 3; ++k) v += r[i][k] * c.t[k];
            o.t[i] = t[i] + s * v;
        }
        o.s = s * c.s;
        return o;
    }
    void apply(const float in[3], float out[3]) const {
        for (int i = 0; i < 3; ++i) {
            float v = 0; for (int k = 0; k < 3; ++k) v += r[i][k] * in[k];
            out[i] = t[i] + s * v;
        }
    }
};

struct CpuShape {
    std::string name;
    Xf world;
    uint64_t desc = 0;
    uint32_t numTris = 0, numVerts = 0, stride = 0;
    std::vector<uint8_t> vertexData, indexData;
    float mn[3] = {}, mx[3] = {};
    std::string diffuse, normal;
    std::string material;
    float emissive[3] = { 0,0,0 };
    float emissiveMult = 0.0f;
    bool effect = false;

    bool  alphaBlend = false;
    bool  alphaAdditive = false;
    bool  alphaTest = false;
    float alphaThreshold = 0.0f;
};

struct BoundSphere { DirectX::XMFLOAT3 c = { 0,0,0 }; float r = 0; };
struct ExtBox {
    float mn[3] = { FLT_MAX, FLT_MAX, FLT_MAX }, mx[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool any = false;
    void extend(const float p[3]) { for (int k = 0; k < 3; ++k) { mn[k] = p[k] < mn[k] ? p[k] : mn[k];
        mx[k] = p[k] > mx[k] ? p[k] : mx[k]; } any = true; }
};

enum class BlendClass { Opaque, Alpha, Additive };

struct GpuItem {
    Microsoft::WRL::ComPtr<ID3D11Buffer> vb, ib;
    std::string name;
    std::string diffusePath;
    RE::NiPointer<RE::NiTexture> engineTex;

    bool texSettled = false;
    bool srvReady = false;
    ID3D11ShaderResourceView* srvFormatCachedFor = nullptr;
    bool srvNeedsSrgbDecode = false;
    UINT stride = 0, indexCount = 0;
    uint64_t desc = 0;
    int vsIndex = 0;
    bool floatPos = false, effect = false;
    BlendClass blend = BlendClass::Opaque;
    float alphaTestThreshold = -1.0f;
    DirectX::XMFLOAT4 emissive = { 0,0,0,0 };
    DirectX::XMFLOAT4X4 world;
};
struct PreviewModel {
    std::vector<GpuItem> items;
    BoundSphere bound, fxBound;
    ExtBox box;
    int drawn = 0, fx = 0;
    bool hasInvMarker = false;
    float invRot[3] = {};
    std::size_t gpuBytes = 0;
};

struct PxRect { float x = 0, y = 0, w = 0, h = 0; };

struct Request {
    ViewId viewId = 0;
    std::string id, formType;

    std::vector<std::string> nifPaths;
    std::string matSwap;
    PxRect rect;

    PxRect clip;
    float zoom = 1, panX = 0, panY = 0, roll = 0, pitch = 0, brightness = 1;
    float spin = -10000.0f, yaw = -10000.0f;
    bool flip = false;
    bool imageSource = false;
};

enum : int { kKindGeneric = 0, kKindElongate = 1, kKindShield = 2, kKindArmor = 3, kKindAmmo = 4,
             kKindClothing = 5, kKindStatic = 6 };

struct CpuModel {
    std::string key; std::vector<std::string> nifPaths; std::string matSwap;
    std::vector<CpuShape> shapes;
    int totalShapes = 0, totalDropped = 0, totalSkinnedSkipped = 0;
    bool hasInvMarker = false; float invRot[3] = {};
};

struct LoadResult {
    std::string fullKey; uint64_t loadGen = 0; std::string key;
    std::shared_ptr<PreviewModel> model; int kind = 0; bool ok = false;
    std::shared_ptr<CpuModel> cpu;
};

bool WorkerThreadsEnabled() noexcept;
void SetLoadDevice(ID3D11Device* dev);
void SetDeviceSingleThreaded(bool singleThreaded);

std::shared_ptr<CpuModel> LoadCpuModel(const std::string& key, const std::vector<std::string>& nifPaths,
                                       const std::string& matSwap);
void MarkLoadMissing(const std::string& key);
std::shared_ptr<PreviewModel> FinishModel(ID3D11Device* dev, CpuModel&& cpu);
std::shared_ptr<PreviewModel> CacheLoadedModel(std::string key, std::shared_ptr<PreviewModel> pm);

bool PostJob(const std::string& fullKey, uint64_t loadGen, const Request& req);
void CancelQueuedJobsForView(ViewId viewId);
void CancelQueuedJob(ViewId viewId, const std::string& fullKey);
std::deque<LoadResult> TakeLoadResults();
void FinishLoadResult(ID3D11Device* dev, LoadResult& res);
void LoadCacheUsage(std::size_t& bytes, std::size_t& entries);
void LoadQueueUsage(int& queuedJobs, int& workerCount);
void ShutdownLoadWorkers();
void ResetLoadState();

}
