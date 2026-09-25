#include "TextureOverlay.h"

#include <windows.h>
#include <d3d11.h>

#include <atomic>
#include <cmath>
#include <mutex>
#include <unordered_map>

#include "../Engine/EngineLocalMapTexture.h"
#include "../Engine/EngineTexture.h"

namespace PrismaUI::TextureOverlay {
namespace {
    struct Request {
        long l = 0, t = 0, r = 0, b = 0;
        bool active = false;
        bool lastAvail = false;
        bool localMap = true;
        std::uint64_t instanceKey = 0;
    };
    std::unordered_map<ViewId, Request> g_reqs;
    std::vector<std::uint64_t>          g_removedKeys;
    std::uint64_t                       g_nextViewKey = (std::uint64_t(1) << 63) | 1;
    std::mutex                          g_mutex;
    std::atomic<StatusSink>             g_sink{ nullptr };

    std::string JsonStr(const std::string& j, const char* key) {
        std::string pat = "\"" + std::string(key) + "\"";
        auto k = j.find(pat);
        if (k == std::string::npos) return {};
        auto c = j.find(':', k + pat.size());
        if (c == std::string::npos) return {};
        auto q1 = j.find('"', c + 1);
        if (q1 == std::string::npos) return {};
        auto q2 = j.find('"', q1 + 1);
        if (q2 == std::string::npos) return {};
        return j.substr(q1 + 1, q2 - q1 - 1);
    }
    double JsonNum(const std::string& j, const char* key, double fb) {
        std::string pat = "\"" + std::string(key) + "\"";
        auto k = j.find(pat);
        if (k == std::string::npos) return fb;
        auto c = j.find(':', k + pat.size());
        if (c == std::string::npos) return fb;
        try { return std::stod(j.substr(c + 1)); } catch (...) { return fb; }
    }
    std::string NumStr(float v) { return std::to_string(std::llround(v)); }

    ID3D11ShaderResourceView* AcquireLocalMapSrv() {

        RE::BSScaleformExternalTexture* tex = Engine::GetLocalMapScaleformTexture();
        RE::NiTexture* ni = tex ? tex->gamebryoTexture.get() : nullptr;
        ID3D11ShaderResourceView* srv = ni ? Engine::ResolveEngineTextureSRV(ni->rendererTexture) : nullptr;
        static int lastState = -1;
        const int state = (tex ? 4 : 0) | (ni ? 2 : 0) | (srv ? 1 : 0);
        if (state != lastState) {
            lastState = state;
            logger::info("[TextureOverlay] localmap chain: LocalMapTexture={} NiTexture={} SRV={}",
                         tex != nullptr, ni != nullptr, srv != nullptr);
        }
        return srv;
    }

    void SehGetExtents(RE::NiPoint3& origin, RE::NiPoint3& vCorner, RE::NiPoint3& uCorner) {
        __try { Engine::GetLocalMapExtents(origin, vCorner, uCorner); } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void MaybeReportStatus(ViewId v, bool avail) {
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            auto it = g_reqs.find(v);
            if (it == g_reqs.end() || it->second.lastAvail == avail) return;
            it->second.lastAvail = avail;
        }
        StatusSink sink = g_sink.load(std::memory_order_acquire);
        if (!sink) return;
        std::string js = "{\"source\":\"localmap\",\"available\":" + std::string(avail ? "true" : "false");
        if (avail) {
            RE::NiPoint3 origin{}, vCorner{}, uCorner{};
            SehGetExtents(origin, vCorner, uCorner);
            js += ",\"origin\":[" + NumStr(origin.x) + "," + NumStr(origin.y) + "]";
            js += ",\"uCorner\":[" + NumStr(uCorner.x) + "," + NumStr(uCorner.y) + "]";
            js += ",\"vCorner\":[" + NumStr(vCorner.x) + "," + NumStr(vCorner.y) + "]";
        }
        js += "}";
        sink(v, js.c_str());
    }
}

void Show(ViewId viewId, const std::string& json) {
    Request r;
    std::string source = JsonStr(json, "source");
    if (source.empty()) source = "localmap";
    r.localMap = source == "localmap";
    r.l = static_cast<long>(JsonNum(json, "x", 0));
    r.t = static_cast<long>(JsonNum(json, "y", 0));
    r.r = r.l + static_cast<long>(JsonNum(json, "w", 0));
    r.b = r.t + static_cast<long>(JsonNum(json, "h", 0));
    r.active = true;
    logger::info("[TextureOverlay] Show view={} source='{}' rect=({},{} {}x{})", viewId, source,
                 r.l, r.t, r.r - r.l, r.b - r.t);
    std::lock_guard<std::mutex> lk(g_mutex);
    r.lastAvail = false;
    const auto prev = g_reqs.find(viewId);
    r.instanceKey = (prev != g_reqs.end() && prev->second.instanceKey != 0) ? prev->second.instanceKey
                                                                            : g_nextViewKey++;
    g_reqs[viewId] = r;
}

void Hide(ViewId viewId, const std::string&) {
    std::lock_guard<std::mutex> lk(g_mutex);
    const auto it = g_reqs.find(viewId);
    if (it != g_reqs.end() && it->second.instanceKey != 0) g_removedKeys.push_back(it->second.instanceKey);
    g_reqs.erase(viewId);
}

void GetActiveViews(std::vector<ViewId>& out) {
    out.clear();
    std::lock_guard<std::mutex> lk(g_mutex);
    for (const auto& [v, req] : g_reqs) if (req.active && req.localMap) out.push_back(v);
}

bool HasPendingWork() {
    std::lock_guard<std::mutex> lk(g_mutex);
    for (const auto& [v, req] : g_reqs) if (req.active && req.localMap) return true;
    return false;
}

void GetOverlays(ViewId viewId, std::vector<ModelPreview::Overlay>& out) {
    Request req;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_reqs.find(viewId);
        if (it == g_reqs.end() || !it->second.active) return;
        req = it->second;
    }
    if (!req.localMap) return;

    ID3D11ShaderResourceView* srv = AcquireLocalMapSrv();
    MaybeReportStatus(viewId, srv != nullptr);
    if (!srv) return;

    ModelPreview::Overlay o;
    o.srv = srv;
    o.continuityKey = req.instanceKey;
    o.destLeft = req.l; o.destTop = req.t; o.destRight = req.r; o.destBottom = req.b;
    out.push_back(o);
}

void DrainRemovedOverlayKeys(std::vector<uint64_t>& out) {
    std::lock_guard<std::mutex> lk(g_mutex);
    out.insert(out.end(), g_removedKeys.begin(), g_removedKeys.end());
    g_removedKeys.clear();
}

void SetStatusSink(StatusSink fn) { g_sink.store(fn, std::memory_order_release); }

void OnViewDestroyed(ViewId viewId) {
    std::lock_guard<std::mutex> lk(g_mutex);
    const auto it = g_reqs.find(viewId);
    if (it != g_reqs.end() && it->second.instanceKey != 0) g_removedKeys.push_back(it->second.instanceKey);
    g_reqs.erase(viewId);
}
}