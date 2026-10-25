#include "Engine/EngineTexture.h"
#include "Engine/EngineTextureLoad.h"
#include "GameThreadDispatcher.h"
#include "ModelPreviewInternal.h"

#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace PrismaUI::ModelPreview {

static void MergeSphere(BoundSphere& acc, const XMFLOAT3& c, float r) {
    if (acc.r <= 0) { acc = { c, r }; return; }
    float dx = c.x - acc.c.x, dy = c.y - acc.c.y, dz = c.z - acc.c.z;
    float d = sqrtf(dx * dx + dy * dy + dz * dz);
    if (acc.r >= d + r) return;
    if (r >= d + acc.r) { acc = { c, r }; return; }
    float nr = (d + acc.r + r) * 0.5f, t = (nr - acc.r) / d;
    acc.c = { acc.c.x + dx * t, acc.c.y + dy * t, acc.c.z + dz * t }; acc.r = nr;
}

static void ToWorldMatrix(const Xf& xf, XMFLOAT4X4& out) {
    float s = xf.s;
    out = XMFLOAT4X4(
        s * xf.r[0][0], s * xf.r[1][0], s * xf.r[2][0], 0,
        s * xf.r[0][1], s * xf.r[1][1], s * xf.r[2][1], 0,
        s * xf.r[0][2], s * xf.r[1][2], s * xf.r[2][2], 0,
        xf.t[0], xf.t[1], xf.t[2], 1);
}

static constexpr std::size_t kMaxTexCacheEntries = 512;

static constexpr int    kMaxTexRequestsInFlight = 1;

static constexpr double kTexRequestWatchdogSec = 10.0;

static std::atomic<int>  g_texDispatchFailures{ 0 };
static constexpr int     kMaxTexDispatchFailures = 600;

void ServiceTextureWatchdog() {
    if (g_texFailClosed.load(std::memory_order_acquire)) return;
    const auto now = std::chrono::steady_clock::now();
    std::string stuck;
    {
        std::lock_guard<std::mutex> lock(g_texMutex);
        for (auto& [path, started] : g_texInFlight) {
            if (std::chrono::duration<double>(now - started).count() >= kTexRequestWatchdogSec) {
                stuck = path;
                break;
            }
        }
    }
    if (stuck.empty()) return;
    g_texFailClosed.store(true, std::memory_order_release);
    logger::critical("[ModelPreview] engine GetTexture has not returned for '{}' in {}s on the game "
                     "thread. Not issuing more texture requests; previews stay untextured. This is "
                     "the AE wait-for-completion loop (0x17c6570 on .240) failing to converge.",
                     stuck, kTexRequestWatchdogSec);
}

bool AcquireEngineTexture(const std::string& path, RE::NiPointer<RE::NiTexture>& out) {
    if (path.empty()) return true;

    if (REX::FModule::IsRuntimeOG()) {
        Engine::CallEngineGetTexture(path.c_str(), out);
        logger::info("[ModelPreview] engine texture '{}': niTexture={}", path,
                     static_cast<bool>(out));
        return true;
    }

    if (!REX::FModule::IsRuntimeAE() && !REX::FModule::IsRuntimeNG()) return true;

    {
        std::lock_guard<std::mutex> lock(g_texMutex);
        auto done = g_texDone.find(path);
        if (done != g_texDone.end()) {
            out = done->second;
            return true;
        }
        if (g_texFailClosed.load(std::memory_order_acquire)) return true;
        if (g_texInFlight.count(path)) return false;
        if ((int)g_texInFlight.size() >= kMaxTexRequestsInFlight) return false;
        g_texInFlight.emplace(path, std::chrono::steady_clock::now());
    }

    const bool queued = GameThreadDispatcher::Dispatch([path] {

        RE::NiPointer<RE::NiTexture> tex;
        Engine::CallEngineGetTexture(path.c_str(), tex);
        {
            std::lock_guard<std::mutex> lock(g_texMutex);
            g_texInFlight.erase(path);

            if (g_texShutdown.load(std::memory_order_acquire)) return;

            if (g_texDone.insert_or_assign(path, tex).second) {
                g_texOrder.push_back(path);
                while (g_texOrder.size() > kMaxTexCacheEntries) {
                    g_texDone.erase(g_texOrder.front());
                    g_texOrder.pop_front();
                }
            }
        }
        ++g_texResolved;
        logger::info("[ModelPreview] engine texture '{}' resolved on game thread: niTexture={}",
                     path, static_cast<bool>(tex));
    });

    if (!queued) {

        {
            std::lock_guard<std::mutex> lock(g_texMutex);
            g_texInFlight.erase(path);
        }

        if (++g_texDispatchFailures >= kMaxTexDispatchFailures &&
            !g_texFailClosed.exchange(true, std::memory_order_acq_rel)) {
            logger::error("[ModelPreview] GameThreadDispatcher refused {} consecutive texture "
                          "requests; previews stay untextured on this runtime.",
                          kMaxTexDispatchFailures);
        }
        return false;
    }
    g_texDispatchFailures.store(0, std::memory_order_relaxed);
    ++g_texRequested;
    return false;
}

ID3D11ShaderResourceView* ResolveSrv(void* rendererTexture) {
    if (!rendererTexture) return nullptr;

    ID3D11ShaderResourceView* srv = Engine::ResolveEngineTextureSRV(rendererTexture);

    static std::atomic<bool> s_reported{ false };
    if (!s_reported.exchange(true)) {
        if (srv)
            logger::info("[ModelPreview] SRV resolved at BSGraphics::Texture+0x00 "
                         "(RE-confirmed 1.10.163: Renderer::SetTexture RVA 0xaec4b0 binds *(void**)tex; "
                         "Renderer::GetScaleformTextureType RVA 0x1d13960 calls ID3D11View::GetResource "
                         "through it) -- QueryInterface-validated against this binary");
        else
            logger::error("[ModelPreview] SRV NOT present at BSGraphics::Texture+0x00 -- the struct "
                          "layout this build assumes does not match this runtime. Every model preview "
                          "will render untextured. Do NOT widen this into an offset search; re-derive the "
                          "offset in Ghidra for this exe version.");
    }
    return srv;
}

static bool IsSrgbFormat(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}

bool SrvNeedsShaderSrgbDecode(ID3D11ShaderResourceView* srv) {
    if (!srv) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC d{};
    srv->GetDesc(&d);
    const bool needs = !IsSrgbFormat(d.Format);

    static std::atomic<bool> s_reported{ false };
    if (!s_reported.exchange(true))
        logger::info("[ModelPreview][srgb] first diffuse SRV format={} -> sampler decode={}, shader decode={}",
                     (int)d.Format, !needs, needs);
    return needs;
}

static std::shared_ptr<PreviewModel> BuildGpuModel(ID3D11Device* dev, std::vector<CpuShape>& shapes) {
    auto pm = std::make_shared<PreviewModel>();
    for (auto& s : shapes) {
        if (!DescHasFlag(s.desc, 0x1) || s.vertexData.empty() || s.indexData.empty()) continue;
        GpuItem it;
        D3D11_BUFFER_DESC bd = {};
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.ByteWidth = (UINT)s.vertexData.size();
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA init = { s.vertexData.data(), 0, 0 };
        if (FAILED(dev->CreateBuffer(&bd, &init, &it.vb))) continue;
        bd.ByteWidth = (UINT)s.indexData.size();
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        init.pSysMem = s.indexData.data();
        if (FAILED(dev->CreateBuffer(&bd, &init, &it.ib))) continue;

        const char* blendName = s.effect ? "additive"
                              : (s.alphaBlend ? (s.alphaAdditive ? "additive" : "alpha") : "opaque");
        logger::info("[ModelPreview]   shape '{}' tris={} diffuse='{}' normal='{}' mat='{}' blend={} alphaTest={} thr={:.2f} "
                     "emissive=({:.3f},{:.3f},{:.3f})x{:.2f} center=({:.1f},{:.1f},{:.1f})",
                     s.name, s.numTris, s.diffuse, s.normal, s.material, blendName, s.alphaTest, s.alphaThreshold,
                     s.emissive[0], s.emissive[1], s.emissive[2], s.emissiveMult,
                     (s.mn[0] + s.mx[0]) * 0.5f, (s.mn[1] + s.mx[1]) * 0.5f, (s.mn[2] + s.mx[2]) * 0.5f);
        it.name = s.name;
        it.diffusePath = s.diffuse;
        it.emissive = { s.emissive[0], s.emissive[1], s.emissive[2], s.emissiveMult };
        it.stride = s.stride;
        it.indexCount = s.numTris * 3;
        it.desc = s.desc;
        it.vsIndex = (DescHasFlag(s.desc, 0x2) ? 1 : 0) | (DescHasFlag(s.desc, 0x8) ? 2 : 0);
        it.floatPos = DescPosBytes(s.desc) >= 16;
        it.effect = s.effect;

        it.blend = s.effect ? BlendClass::Additive
                 : s.alphaBlend ? (s.alphaAdditive ? BlendClass::Additive : BlendClass::Alpha)
                 : BlendClass::Opaque;
        it.alphaTestThreshold = s.alphaTest ? s.alphaThreshold : -1.0f;
        ToWorldMatrix(s.world, it.world);

        float corners[8][3];
        ExtBox wbox;
        for (int cn = 0; cn < 8; ++cn) {
            float lp[3] = { (cn & 1) ? s.mx[0] : s.mn[0], (cn & 2) ? s.mx[1] : s.mn[1],
                            (cn & 4) ? s.mx[2] : s.mn[2] };
            s.world.apply(lp, corners[cn]); wbox.extend(corners[cn]);
        }
        float ex = wbox.mx[0] - wbox.mn[0], ey = wbox.mx[1] - wbox.mn[1], ez = wbox.mx[2] - wbox.mn[2];
        XMFLOAT3 sc = { (wbox.mn[0] + wbox.mx[0]) * 0.5f, (wbox.mn[1] + wbox.mx[1]) * 0.5f,
                        (wbox.mn[2] + wbox.mx[2]) * 0.5f };
        float sr = 0.5f * sqrtf(ex * ex + ey * ey + ez * ez);
        if (s.effect) { MergeSphere(pm->fxBound, sc, sr); pm->fx++; }
        else { MergeSphere(pm->bound, sc, sr); for (auto& cc : corners) pm->box.extend(cc); pm->drawn++; }
        pm->items.push_back(std::move(it));
    }
    return pm->items.empty() ? nullptr : pm;
}

std::shared_ptr<CpuModel> LoadCpuModel(const std::string& key, const std::vector<std::string>& nifPaths,
                                       const std::string& matSwap) {
    auto swaps = ParseSwapMap(matSwap);
    std::vector<CpuShape> allShapes;
    bool anyRead = false, hasInvMarker = false;
    float invRot[3] = {};
    int totalShapes = 0, totalDropped = 0, totalSkinnedSkipped = 0;

    std::vector<NifParse> pieces;
    std::vector<std::string> pieceNames;
    pieces.reserve(nifPaths.size());
    for (auto& nifPath : nifPaths) {
        std::vector<uint8_t> data;
        if (!ReadGameFile("meshes\\" + nifPath, data) && !ReadGameFile(nifPath, data)) {
            logger::warn("[ModelPreview] mesh not found '{}' (1 of {} composite piece(s))", nifPath, nifPaths.size());
            continue;
        }
        anyRead = true;
        NifParse np; ParseStats st;
        if (!ParseNif(data, np, st, swaps)) continue;
        totalShapes += st.shapes; totalDropped += st.dropped; totalSkinnedSkipped += st.skinnedSkipped;

        if (!hasInvMarker && np.hasInvMarker) { hasInvMarker = true; memcpy(invRot, np.invRot, sizeof(invRot)); }
        pieces.push_back(std::move(np));
        pieceNames.push_back(nifPath);
    }

    std::vector<Xf> pieceXf(pieces.size());
    std::vector<bool> placed(pieces.size(), false);
    if (!pieces.empty()) placed[0] = true;

    std::map<std::string, Xf> sockets;
    auto offerSockets = [&](size_t i) {
        const Xf& base = pieceXf[i];
        for (const auto& cp : pieces[i].cpParents) {
            Xf rootWorld;
            if (auto it = pieces[i].nodeWorld.find(cp.root); it != pieces[i].nodeWorld.end())
                rootWorld = it->second;
            sockets.emplace(SocketKey(cp.name), base * rootWorld * cp.xf);
        }
    };
    if (!pieces.empty()) offerSockets(0);

    int attached = 0, anchored = 0, unplaced = 0;
    for (bool progress = true; progress; ) {
        progress = false;
        for (size_t i = 1; i < pieces.size(); ++i) {
            if (placed[i]) continue;
            for (const auto& target : pieces[i].cpChildren) {
                auto it = sockets.find(SocketKey(target));
                if (it == sockets.end()) continue;
                pieceXf[i] = it->second;
                placed[i] = true;
                offerSockets(i);
                ++attached;
                progress = true;
                break;
            }
        }

        if (!progress) {
            for (size_t i = 1; i < pieces.size(); ++i) {
                if (placed[i] || pieces[i].cpParents.empty()) continue;
                placed[i] = true;
                offerSockets(i);
                ++anchored;
                progress = true;
                break;
            }
        }
    }

    for (size_t i = 0; i < pieces.size(); ++i) {
        if (i > 0 && !placed[i]) {
            ++unplaced;
            logger::debug("[ModelPreview] piece '{}' has no matching connect point -- left at origin",
                          pieceNames[i]);
        }
        const bool identity = (i == 0) || !placed[i];
        for (auto& cs : pieces[i].shapes) {
            if (!identity) cs.world = pieceXf[i] * cs.world;
            allShapes.push_back(std::move(cs));
        }
    }
    if (pieces.size() > 1)
        logger::info("[ModelPreview] assembled {} piece(s): {} attached at connect points, {} anchored as hosts, "
                     "{} left at origin", pieces.size(), attached, anchored, unplaced);
    if (!anyRead) {
        logger::warn("[ModelPreview] none of {} composite piece(s) could be read (first: '{}')",
                     nifPaths.size(), nifPaths.front());
        MarkLoadMissing(key);
        return nullptr;
    }
    auto cpu = std::make_shared<CpuModel>();
    cpu->key = key; cpu->nifPaths = nifPaths; cpu->matSwap = matSwap; cpu->shapes = std::move(allShapes);
    cpu->totalShapes = totalShapes; cpu->totalDropped = totalDropped; cpu->totalSkinnedSkipped = totalSkinnedSkipped;
    cpu->hasInvMarker = hasInvMarker; memcpy(cpu->invRot, invRot, sizeof(invRot));
    return cpu;
}

std::shared_ptr<PreviewModel> FinishModel(ID3D11Device* dev, CpuModel&& cpu) {
    std::shared_ptr<PreviewModel> pm = BuildGpuModel(dev, cpu.shapes);
    if (pm && cpu.hasInvMarker) { pm->hasInvMarker = true; memcpy(pm->invRot, cpu.invRot, sizeof(pm->invRot)); }
    logger::info("[ModelPreview] load '{}' ({} piece(s)) shapes={} dropped={} skin={} ok={} invMarker={} "
                 "extent=({:.1f},{:.1f},{:.1f}) matSwap='{}'",
                 JoinNifPathsDisplay(cpu.nifPaths), cpu.nifPaths.size(), cpu.totalShapes, cpu.totalDropped,
                 cpu.totalSkinnedSkipped, pm != nullptr, pm ? pm->hasInvMarker : false,
                 pm && pm->box.any ? pm->box.mx[0] - pm->box.mn[0] : 0.0f,
                 pm && pm->box.any ? pm->box.mx[1] - pm->box.mn[1] : 0.0f,
                 pm && pm->box.any ? pm->box.mx[2] - pm->box.mn[2] : 0.0f, cpu.matSwap);
    if (!pm) return pm;
    pm->gpuBytes = 0;
    for (const auto& it : pm->items) {
        D3D11_BUFFER_DESC bd{};
        if (it.vb) { it.vb->GetDesc(&bd); pm->gpuBytes += bd.ByteWidth; }
        if (it.ib) { it.ib->GetDesc(&bd); pm->gpuBytes += bd.ByteWidth; }
    }
    return CacheLoadedModel(std::move(cpu.key), pm);
}

}
