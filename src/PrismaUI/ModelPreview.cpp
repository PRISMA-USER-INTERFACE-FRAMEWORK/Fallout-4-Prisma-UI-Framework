#include "ModelPreview.h"

#include "Engine/EngineTexture.h"
#include "Engine/EngineTextureLoad.h"
#include "FreezeDiagnostics.h"
#include "GameThreadDispatcher.h"
#include "ModelPreviewReads.h"
#include "Utils/ModulePath.h"

#include <windows.h>
#include <psapi.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

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
static Config g_cfg;

static void LoadConfigOnce() {

    static std::once_flag s_once;
    std::call_once(s_once, [] {
    g_cfg.loaded = true;

    const std::wstring ini =
        (PrismaUI::Utils::ThisModuleDir() / L"PrismaUI_ModelPreview.ini").wstring();
    g_cfg.enabled = GetPrivateProfileIntW(L"General", L"bEnabled", 1, ini.c_str()) != 0;
    int sz = GetPrivateProfileIntW(L"General", L"iRTSize", 512, ini.c_str());
    g_cfg.rtSize = sz < 128 ? 128 : (sz > 2048 ? 2048 : sz);
    g_cfg.spinDegPerSec = (float)GetPrivateProfileIntW(L"General", L"iSpinDegPerSec", 45, ini.c_str());

    g_cfg.workerThreads = GetPrivateProfileIntW(L"General", L"bWorkerThreads", 1, ini.c_str()) != 0;
    int cap = GetPrivateProfileIntW(L"General", L"iMaxLivePreviews", 192, ini.c_str());
    g_cfg.maxLivePreviews = cap < 32 ? 32 : (cap > 4096 ? 4096 : cap);
    wchar_t solo[256]{};
    GetPrivateProfileStringW(L"Debug", L"sSoloShape", L"", solo, 256, ini.c_str());
    if (solo[0]) { int n = WideCharToMultiByte(CP_UTF8, 0, solo, -1, nullptr, 0, nullptr, nullptr);
                   std::string s(n > 0 ? n - 1 : 0, '\0');
                   if (n > 0) WideCharToMultiByte(CP_UTF8, 0, solo, -1, s.data(), n, nullptr, nullptr);
                   g_cfg.soloShape = s; }
    logger::warn("[ModelPreview] model parse/GPU = {}; engine file reads always marshalled to render thread",
                 g_cfg.workerThreads ? "WORKER POOL" : "RENDER THREAD (inline)");
    logger::info("[ModelPreview] enabled={} rtSize={} spin={}deg/s maxLivePreviews={} soloShape='{}'",
                 g_cfg.enabled, g_cfg.rtSize, g_cfg.spinDegPerSec, g_cfg.maxLivePreviews, g_cfg.soloShape);
    });
}

bool Enabled() {
    LoadConfigOnce();
    return g_cfg.enabled;
}

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

static std::string JoinNifPathsKey(const std::vector<std::string>& paths) {
    std::string k;
    for (auto& p : paths) { k += LowerStr(p); k += '|'; }
    return k;
}

static std::string JoinNifPathsDisplay(const std::vector<std::string>& paths) {
    std::string s;
    for (size_t i = 0; i < paths.size(); ++i) { if (i) s += ", "; s += paths[i]; }
    return s;
}

struct MaterialTextures {
    std::string diffuse;
    std::string normal;
    bool ok = false;
};

static std::string BgsmStr(Cur& c) {
    uint32_t len = c.u32();
    if (len == 0 || len > 1024 || !c.need(len)) return std::string();
    size_t take = (c.p[c.off + len - 1] == 0) ? len - 1 : len;
    std::string s(reinterpret_cast<const char*>(c.p + c.off), take);
    c.off += len;
    return s;
}

static void ParseMaterialHeader(Cur& c, uint32_t version) {
    c.u32();
    c.f32(); c.f32(); c.f32(); c.f32();
    c.f32();
    c.u8();
    c.u32(); c.u32();
    c.u8(); c.u8(); c.u8(); c.u8();
    c.u8(); c.u8(); c.u8(); c.u8();
    c.u8(); c.u8(); c.u8(); c.u8();
    c.f32();
    c.u8();
    c.f32();
    c.u8();
    if (version >= 6) c.u8();
}

static bool ParseMaterial(const std::vector<uint8_t>& data, MaterialTextures& out) {
    Cur c{ data.data(), data.size() };
    char sig[4]; if (!c.bytes(sig, 4)) return false;
    uint32_t version = c.u32();
    const bool bgsm = memcmp(sig, "BGSM", 4) == 0;
    const bool bgem = memcmp(sig, "BGEM", 4) == 0;
    if (!bgsm && !bgem) return false;
    ParseMaterialHeader(c, version);

    const int slots = bgsm ? 9 : 5;
    std::vector<std::string> tex;
    tex.reserve(slots);
    for (int i = 0; i < slots && c.ok; ++i) tex.push_back(BgsmStr(c));
    if (!c.ok) return false;
    if (bgsm) { out.diffuse = tex.size() > 0 ? tex[0] : ""; out.normal = tex.size() > 1 ? tex[1] : ""; }
    else      { out.diffuse = tex.size() > 0 ? tex[0] : ""; out.normal = tex.size() > 3 ? tex[3] : ""; }

    auto normTex = [](std::string& t) {
        if (t.empty()) return;
        for (auto& ch : t) if (ch == '/') ch = '\\';
        std::string low = LowerStr(t);

        if (size_t tp = low.rfind("textures\\"); tp != std::string::npos) t = t.substr(tp);
        else t = "textures\\" + t;
    };
    normTex(out.diffuse); normTex(out.normal);
    out.ok = true;
    return true;
}

static bool LoadMaterialTextures(const std::string& matPath, MaterialTextures& out) {
    if (matPath.empty()) return false;
    std::string p = matPath;
    for (auto& ch : p) if (ch == '/') ch = '\\';
    std::string lp = LowerStr(p);

    if (size_t mp = lp.rfind("materials\\"); mp != std::string::npos) p = p.substr(mp);
    else p = "materials\\" + p;
    std::vector<uint8_t> data;
    if (!ReadGameFile(p, data)) return false;
    return ParseMaterial(data, out);
}

static uint32_t DescStride(uint64_t d) { return (uint32_t)(d & 0xF) * 4; }
static uint32_t DescAttrOffset(uint64_t d, int a) { return (uint32_t)((d >> (4 * a + 2)) & 0x3C); }
static bool DescHasFlag(uint64_t d, uint32_t f) { return ((d >> 44) & f) != 0; }

static uint32_t DescPosBytes(uint64_t d) {
    uint32_t pos = DescStride(d);
    for (int a = 1; a <= 6; ++a) { uint32_t o = DescAttrOffset(d, a); if (o > 0 && o < pos) pos = o; }
    return pos;
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

struct ParseStats { int shapes = 0, dropped = 0, skinnedSkipped = 0; };

static bool DecodeLocalAabb(const std::vector<uint8_t>& v, uint64_t desc, uint32_t stride, uint32_t count,
                            float mnOut[3], float mxOut[3]) {
    if (!stride || !count || v.size() < (size_t)stride * count) return false;
    bool floatPos = DescPosBytes(desc) >= 16;
    float mn[3] = { FLT_MAX, FLT_MAX, FLT_MAX }, mx[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* p = v.data() + (size_t)i * stride;
        float c[3];
        if (floatPos) memcpy(c, p, 12);
        else { const uint16_t* h = reinterpret_cast<const uint16_t*>(p);
               for (int k = 0; k < 3; ++k) c[k] = PackedVector::XMConvertHalfToFloat(h[k]); }
        for (int k = 0; k < 3; ++k) {
            if (!std::isfinite(c[k]) || fabsf(c[k]) > 1e5f) return false;
            mn[k] = c[k] < mn[k] ? c[k] : mn[k];
            mx[k] = c[k] > mx[k] ? c[k] : mx[k];
        }
    }
    memcpy(mnOut, mn, 12); memcpy(mxOut, mx, 12);
    return true;
}

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

static std::string SocketKey(const std::string& s) {
    size_t i = 0;
    if (s.size() > 2 && (s[0] == 'P' || s[0] == 'p' || s[0] == 'C' || s[0] == 'c') && s[1] == '-')
        i = 2;
    std::string k;
    k.reserve(s.size() - i);
    for (; i < s.size(); ++i) k += (char)std::tolower((unsigned char)s[i]);
    return k;
}

static void QuatToXf(const float q[4], Xf& xf) {
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    const float n = w * w + x * x + y * y + z * z;
    if (n < 1e-8f) return;
    const float s = 2.0f / n;
    const float xs = x * s,  ys = y * s,  zs = z * s;
    const float wx = w * xs, wy = w * ys, wz = w * zs;
    const float xx = x * xs, xy = x * ys, xz = x * zs;
    const float yy = y * ys, yz = y * zs, zz = z * zs;
    xf.r[0][0] = 1 - (yy + zz); xf.r[0][1] = xy - wz;       xf.r[0][2] = xz + wy;
    xf.r[1][0] = xy + wz;       xf.r[1][1] = 1 - (xx + zz); xf.r[1][2] = yz - wx;
    xf.r[2][0] = xz - wy;       xf.r[2][1] = yz + wx;       xf.r[2][2] = 1 - (xx + yy);
}

static std::string NormMatKey(const std::string& in) {
    std::string o; o.reserve(in.size());
    for (char c : in) o += static_cast<char>(std::tolower(static_cast<unsigned char>(c == '\\' ? '/' : c)));
    if (o.rfind("materials/", 0) == 0) o.erase(0, 10);
    return o;
}

static bool ParseNif(const std::vector<uint8_t>& data, NifParse& out, ParseStats& st,
                     const std::unordered_map<std::string, std::string>& swapMap = {}) {
    Cur c{ data.data(), data.size() };
    { uint8_t ch; size_t g = 0; do { ch = c.u8(); } while (ch != '\n' && ++g < 256 && c.ok); }
    uint32_t ver = c.u32(); uint8_t endian = c.u8(); (void)endian;
    uint32_t userVer = c.u32(); uint32_t nblocks = c.u32(); uint32_t bsver = c.u32();
    (void)ver; (void)userVer;
    if (!c.ok || nblocks == 0 || nblocks > 100000) return false;
    const bool leProps = bsver <= 34;

    for (int i = 0; i < 3; ++i) { uint8_t len = c.u8(); c.skip(len); }
    if (bsver >= 130) { uint8_t len = c.u8(); c.skip(len); }

    uint16_t ntypes = c.u16();
    std::vector<std::string> types(ntypes);
    for (auto& t : types) { uint32_t len = c.u32(); if (len > 1000) return false; t.resize(len);
        if (len && !c.bytes(t.data(), len)) return false; }
    std::vector<uint16_t> tidx(nblocks);
    for (auto& x : tidx) x = c.u16();
    std::vector<uint32_t> sizes(nblocks);
    for (auto& x : sizes) x = c.u32();
    uint32_t nstr = c.u32(), maxlen = c.u32(); (void)maxlen;
    if (nstr > 100000) return false;
    std::vector<std::string> strings(nstr);
    for (auto& s : strings) { uint32_t len = c.u32(); if (len > 4096) return false; s.resize(len);
        if (len && !c.bytes(s.data(), len)) return false; }
    uint32_t ngroups = c.u32(); c.skip((size_t)ngroups * 4);
    if (!c.ok) return false;

    std::vector<size_t> blockOff(nblocks);
    { size_t o = c.off; for (uint32_t i = 0; i < nblocks; ++i) { blockOff[i] = o; o += sizes[i]; } }

    auto TypeOf = [&](int32_t bi) -> const std::string& {
        static const std::string none;
        if (bi < 0 || (uint32_t)bi >= nblocks) return none;
        uint16_t ti = tidx[bi];
        if (ti >= types.size()) return none;
        return types[ti];
    };
    auto NameOf = [&](uint32_t si) -> std::string { return si < strings.size() ? strings[si] : std::string(); };
    auto CursorAt = [&](int32_t bi) { return Cur{ data.data(), data.size(), blockOff[bi] }; };

    struct Node { Xf xf; uint32_t flags = 0; std::vector<int32_t> children; std::string name; };
    struct Shape { CpuShape cs; Xf local; uint32_t flags = 0; int32_t shaderRef = -1;
                   int32_t alphaRef = -1; bool valid = false; };
    std::map<int32_t, Node> nodes;
    std::map<int32_t, Shape> shapes;
    std::map<int32_t, std::vector<std::string>> texSets;
    struct Shader { int32_t texSet = -1; std::string material; bool effect = false;
                    std::string effBase, effNormal; float emissive[3] = { 0,0,0 }; float emissiveMult = 0.0f; };
    std::map<int32_t, Shader> shaders;
    struct AlphaInfo { uint16_t flags = 0; uint8_t threshold = 0; };
    std::map<int32_t, AlphaInfo> alphaProps;

    auto ReadObjectNet = [&](Cur& b, uint32_t& nameIdx) -> bool {
        nameIdx = b.u32(); uint32_t nx = b.u32(); if (nx > 1000) return false;
        b.skip((size_t)nx * 4); b.u32();
        return b.ok;
    };

    auto ReadAvBase = [&](Cur& b, Xf& xf, uint32_t& flags, uint32_t& nameIdx) -> bool {
        nameIdx = b.u32(); uint32_t nx = b.u32(); if (nx > 1000) return false;
        b.skip((size_t)nx * 4); b.u32();
        flags = b.u32();
        float t[3], r[9], s;
        if (!b.bytes(t, 12) || !b.bytes(r, 36)) return false; s = b.f32();
        for (int i = 0; i < 3; ++i) xf.t[i] = t[i];
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) xf.r[i][j] = r[i * 3 + j];
        xf.s = s;
        if (leProps) { uint32_t np = b.u32(); if (np > 1000) return false; b.skip((size_t)np * 4); }
        b.u32();
        return b.ok;
    };

    for (uint32_t i = 0; i < nblocks; ++i) {
        const std::string& t = TypeOf((int32_t)i);
        if (t.empty()) continue;
        const bool isNode = t.find("Node") != std::string::npos || t == "BSFadeNode" || t == "BSLeafAnimNode";

        if (isNode) {
            Cur b = CursorAt((int32_t)i);
            Node nr; uint32_t nameIdx = 0;
            if (ReadAvBase(b, nr.xf, nr.flags, nameIdx)) {
                nr.name = NameOf(nameIdx);
                uint32_t nChildren = b.u32();
                if (nChildren < 100000) {
                    nr.children.resize(nChildren);
                    bool okc = true;
                    for (auto& ch : nr.children) { ch = (int32_t)b.u32(); if (!b.ok) { okc = false; break; } }
                    if (okc) nodes.emplace((int32_t)i, std::move(nr));
                }
            }
        }

        else if (t == "BSConnectPoint::Parents") {
            Cur b = CursorAt((int32_t)i);
            b.u32();
            uint32_t n = b.u32();
            if (b.ok && n <= 256) {
                for (uint32_t k = 0; k < n && b.ok; ++k) {
                    ConnectPoint cp;
                    cp.root = b.sized();
                    cp.name = b.sized();
                    float q[4];
                    if (!b.bytes(q, 16)) break;
                    QuatToXf(q, cp.xf);
                    if (!b.bytes(cp.xf.t, 12)) break;
                    cp.xf.s = b.f32();
                    if (cp.xf.s <= 0.0f) cp.xf.s = 1.0f;
                    if (b.ok && !cp.name.empty()) out.cpParents.push_back(std::move(cp));
                }
            }
        }
        else if (t == "BSConnectPoint::Children") {
            Cur b = CursorAt((int32_t)i);
            b.u32();
            b.u8();
            uint32_t n = b.u32();
            if (b.ok && n <= 256) {
                for (uint32_t k = 0; k < n && b.ok; ++k) {
                    std::string target = b.sized();
                    if (b.ok && !target.empty()) out.cpChildren.push_back(std::move(target));
                }
            }
        }
        else if (t == "BSTriShape" || t == "BSSubIndexTriShape" || t == "BSMeshLODTriShape") {
            Cur b = CursorAt((int32_t)i);
            Shape ps; uint32_t nameIdx = 0;
            if (!ReadAvBase(b, ps.local, ps.flags, nameIdx)) { st.dropped++; continue; }
            ps.cs.name = NameOf(nameIdx);
            b.skip(12 + 4);
            if (bsver > 139) b.skip(24);
            uint32_t skinRef = b.u32();
            ps.shaderRef = (int32_t)b.u32();
            ps.alphaRef = (int32_t)b.u32();
            uint64_t desc = b.u64();
            uint32_t vs = DescStride(desc);
            uint32_t numTris = b.u32();
            uint16_t numVerts = b.u16();
            uint32_t dataSize = b.u32();
            if (skinRef != 0xFFFFFFFF && dataSize == 0) { st.skinnedSkipped++; continue; }
            if (vs >= 8 && vs <= 64 && numVerts &&
                dataSize == (uint64_t)numVerts * vs + (uint64_t)numTris * 6) {
                ps.cs.desc = desc; ps.cs.stride = vs; ps.cs.numTris = numTris; ps.cs.numVerts = numVerts;
                ps.cs.vertexData.resize((size_t)numVerts * vs);
                ps.cs.indexData.resize((size_t)numTris * 6);
                if (b.bytes(ps.cs.vertexData.data(), ps.cs.vertexData.size()) &&
                    (numTris == 0 || b.bytes(ps.cs.indexData.data(), ps.cs.indexData.size())) &&
                    DecodeLocalAabb(ps.cs.vertexData, desc, vs, numVerts, ps.cs.mn, ps.cs.mx)) {
                    ps.valid = numTris > 0;
                }
            }
            if (ps.valid) shapes.emplace((int32_t)i, std::move(ps)); else st.dropped++;
        }
        else if (t == "BSGeometry") {

            Cur b = CursorAt((int32_t)i);
            Shape ps; uint32_t nameIdx = 0;
            if (!ReadAvBase(b, ps.local, ps.flags, nameIdx)) { st.dropped++; continue; }
            ps.cs.name = NameOf(nameIdx);
            b.skip(16);
            if (bsver > 139) b.skip(24);
            b.u32();
            ps.shaderRef = (int32_t)b.u32();
            ps.alphaRef = (int32_t)b.u32();
            uint64_t desc = 0xBULL | (16ULL << 6) | (24ULL << 14) | (0xBULL << 44);
            bool gotMesh = false;
            for (int mi = 0; mi < 4 && b.ok; ++mi) {
                uint8_t present = b.u8();
                if (!present) continue;
                uint32_t triSize = b.u32(); (void)triSize;
                uint32_t numVerts = b.u32(); (void)numVerts;
                b.u32();
                std::string meshName;
                if ( false) {

                }
                meshName = b.sized();
                if (!b.ok || meshName.empty()) continue;
                std::vector<uint8_t> md;
                if (!ReadGameFile("geometry\\" + meshName, md) && !ReadGameFile(meshName, md)) continue;
                Cur m{ md.data(), md.size() };
                uint32_t mv = m.u32();
                if (!m.ok || mv > 2) continue;
                uint32_t nTriIndices = m.u32();
                if (!nTriIndices || nTriIndices > 3000000 || (nTriIndices % 3) != 0) continue;
                std::vector<uint16_t> indices((size_t)nTriIndices);
                for (auto& ix : indices) ix = m.u16();
                float scale = m.f32();
                uint32_t weightsPerVert = m.u32(); (void)weightsPerVert;
                uint32_t nVertices = m.u32();
                if (!m.ok || !nVertices || nVertices > 200000) continue;
                std::vector<float> pos((size_t)nVertices * 3);
                for (uint32_t v = 0; v < nVertices; ++v) {
                    for (int k = 0; k < 3; ++k) {
                        int16_t q = (int16_t)m.u16();
                        pos[(size_t)v * 3 + k] = (q < 0 ? (float)q / 32768.0f : (float)q / 32767.0f) * scale * 69.969f;
                    }
                }
                uint32_t nUv1 = m.u32();
                std::vector<uint16_t> uv((size_t)nVertices * 2, 0);
                for (uint32_t u = 0; u < nUv1; ++u) { uint16_t x = m.u16(), y = m.u16(); if (u < nVertices) { uv[(size_t)u * 2] = x; uv[(size_t)u * 2 + 1] = y; } }
                uint32_t nUv2 = m.u32(); m.skip((size_t)nUv2 * nVertices * 4);
                uint32_t nColors = m.u32(); m.skip((size_t)nColors * 4);
                uint32_t nNormals = m.u32();
                std::vector<uint32_t> normals(nVertices, 0xC0000000u);
                for (uint32_t n = 0; n < nNormals; ++n) { uint32_t q = m.u32(); if (n < nVertices) normals[n] = q; }
                uint32_t nTangents = m.u32(); m.skip((size_t)nTangents * 4);
                if (!m.ok) continue;
                ps.cs.desc = desc; ps.cs.stride = 32; ps.cs.numVerts = nVertices; ps.cs.numTris = nTriIndices / 3;
                ps.cs.vertexData.resize((size_t)nVertices * 32);
                for (uint32_t v = 0; v < nVertices; ++v) {
                    uint8_t* dst = ps.cs.vertexData.data() + (size_t)v * 32;
                    memcpy(dst, &pos[(size_t)v * 3], 12); float one = 1.0f; memcpy(dst + 12, &one, 4);
                    memcpy(dst + 16, &uv[(size_t)v * 2], 4);
                    uint32_t q = normals[v]; uint8_t nrm[4] = {
                        (uint8_t)(((q & 0x3FFu) * 255u) / 1023u),
                        (uint8_t)((((q >> 10) & 0x3FFu) * 255u) / 1023u),
                        (uint8_t)((((q >> 20) & 0x3FFu) * 255u) / 1023u), 255 };
                    memcpy(dst + 24, nrm, 4);
                }
                ps.cs.indexData.resize((size_t)nTriIndices * 2); memcpy(ps.cs.indexData.data(), indices.data(), ps.cs.indexData.size());
                if (DecodeLocalAabb(ps.cs.vertexData, desc, 32, nVertices, ps.cs.mn, ps.cs.mx)) { ps.valid = true; gotMesh = true; }
                if (gotMesh) break;
            }
            if (gotMesh) shapes.emplace((int32_t)i, std::move(ps)); else st.dropped++;
        }
        else if (t == "BSShaderTextureSet") {
            Cur b = CursorAt((int32_t)i);
            uint32_t n = b.u32();
            if (n <= 32) { std::vector<std::string> v; v.reserve(n); bool okc = true;
                for (uint32_t k = 0; k < n; ++k) { v.push_back(b.sized()); if (!b.ok) { okc = false; break; } }
                if (okc) texSets[(int32_t)i] = std::move(v); }
        }
        else if (t == "BSLightingShaderProperty") {
            Cur b = CursorAt((int32_t)i);
            b.u32();
            uint32_t nameIdx = 0;
            if (ReadObjectNet(b, nameIdx)) {
                b.u32(); b.u32();
                b.skip(8 + 8);
                Shader sh;
                sh.texSet = (int32_t)b.u32();
                sh.emissive[0] = b.f32(); sh.emissive[1] = b.f32(); sh.emissive[2] = b.f32();
                sh.emissiveMult = b.f32();

                if (b.ok) { sh.material = NameOf(nameIdx); shaders[(int32_t)i] = sh; }
            }
        }
        else if (t == "BSEffectShaderProperty") {
            Cur b = CursorAt((int32_t)i);
            uint32_t nameIdx = 0;
            if (ReadObjectNet(b, nameIdx)) {
                b.u32(); b.u32(); b.skip(8 + 8);
                Shader sh; sh.effect = true;

                sh.material = NameOf(nameIdx);
                sh.effBase = b.sized();
                b.u32();
                b.skip(4 * 4);
                b.skip(4 * 4 + 4 + 4);
                b.sized();
                b.sized();
                sh.effNormal = b.sized();
                if (b.ok) shaders[(int32_t)i] = sh;
            }
        }
        else if (t == "BSInvMarker") {
            Cur b = CursorAt((int32_t)i);
            uint32_t nameIdx = b.u32(); (void)nameIdx;
            uint16_t rx = b.u16(), ry = b.u16(), rz = b.u16();
            if (b.ok) { out.hasInvMarker = true; out.invRot[0] = rx / 1000.0f;
                        out.invRot[1] = ry / 1000.0f; out.invRot[2] = rz / 1000.0f; }
        }
        else if (t == "NiAlphaProperty") {

            Cur b = CursorAt((int32_t)i);
            uint32_t nameIdx = 0;
            if (ReadObjectNet(b, nameIdx)) {
                uint16_t flags = b.u16(); uint8_t threshold = b.u8();
                if (b.ok) alphaProps[(int32_t)i] = { flags, threshold };
            }
        }
    }

    for (auto& [idx, ps] : shapes) {
        auto shIt = shaders.find(ps.shaderRef);
        if (shIt == shaders.end()) continue;
        const Shader& sh = shIt->second;
        ps.cs.emissive[0] = sh.emissive[0]; ps.cs.emissive[1] = sh.emissive[1]; ps.cs.emissive[2] = sh.emissive[2];
        ps.cs.emissiveMult = sh.emissiveMult;

        std::string material = sh.material;
        if (!swapMap.empty() && !material.empty()) {
            if (auto sw = swapMap.find(NormMatKey(material)); sw != swapMap.end()) material = sw->second;
        }
        if (sh.effect) {
            ps.cs.effect = true;
            ps.cs.diffuse = sh.effBase; ps.cs.normal = sh.effNormal;
            ps.cs.material = material;

            if (ps.cs.diffuse.empty() && !material.empty()) {
                MaterialTextures mt;
                if (LoadMaterialTextures(material, mt) && mt.ok) {
                    if (ps.cs.diffuse.empty()) ps.cs.diffuse = mt.diffuse;
                    if (ps.cs.normal.empty())  ps.cs.normal = mt.normal;
                }
            }
        } else {
            ps.cs.material = material;

            bool fromMaterial = false;
            if (!material.empty()) {
                MaterialTextures mt;
                if (LoadMaterialTextures(material, mt) && mt.ok) {
                    ps.cs.diffuse = mt.diffuse; ps.cs.normal = mt.normal;
                    fromMaterial = true;
                } else {
                    logger::warn("[ModelPreview]   material '{}' load FAILED for shape '{}' (falling back to inline)",
                                 material, ps.cs.name);
                }
            }
            if (!fromMaterial) {
                auto ti = texSets.find(sh.texSet);
                if (ti != texSets.end()) {
                    if (ti->second.size() > 0) ps.cs.diffuse = ti->second[0];
                    if (ti->second.size() > 1) ps.cs.normal = ti->second[1];
                }
            }
        }
    }

    for (auto& [idx, ps] : shapes) {
        auto ai = alphaProps.find(ps.alphaRef);
        if (ai == alphaProps.end()) continue;
        const uint16_t f = ai->second.flags;
        ps.cs.alphaBlend = (f & 0x0001) != 0;
        const uint32_t dstMode = (uint32_t)((f >> 5) & 0xF);
        ps.cs.alphaAdditive = ps.cs.alphaBlend && (dstMode == 0 );
        ps.cs.alphaTest = (f & 0x0200) != 0;
        ps.cs.alphaThreshold = ai->second.threshold / 255.0f;
    }

    std::vector<bool> visited(nblocks, false);
    struct Frame { int32_t idx; Xf parent; int depth; };
    std::vector<Frame> stack;
    constexpr int kMaxDepth = 128;
    constexpr size_t kMaxVisited = 200000;
    size_t steps = 0;

    auto emitShape = [&](Shape& ps, const Xf& parent) {
        ps.cs.world = parent * ps.local;
        out.shapes.push_back(std::move(ps.cs));
        st.shapes++;
    };

    int32_t root = (nodes.count(0) || shapes.count(0)) ? 0 : -1;
    if (root >= 0) {
        stack.push_back({ root, Xf{}, 0 });
        while (!stack.empty() && steps++ < kMaxVisited) {
            Frame f = stack.back(); stack.pop_back();
            if (f.idx < 0 || (uint32_t)f.idx >= nblocks || visited[f.idx] || f.depth > kMaxDepth) continue;
            visited[f.idx] = true;
            if (auto ni = nodes.find(f.idx); ni != nodes.end()) {
                if (ni->second.flags & 1) continue;
                Xf world = f.parent * ni->second.xf;

                if (!ni->second.name.empty()) out.nodeWorld.emplace(ni->second.name, world);
                for (int32_t ch : ni->second.children) stack.push_back({ ch, world, f.depth + 1 });
            } else if (auto si = shapes.find(f.idx); si != shapes.end()) {
                if (si->second.flags & 1) continue;
                emitShape(si->second, f.parent);
            }
        }
    } else {

        for (auto& [idx, ps] : shapes) { if (!(ps.flags & 1)) emitShape(ps, Xf{}); }
    }
    return !out.shapes.empty();
}

struct BoundSphere { XMFLOAT3 c = { 0,0,0 }; float r = 0; };
struct ExtBox {
    float mn[3] = { FLT_MAX, FLT_MAX, FLT_MAX }, mx[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool any = false;
    void extend(const float p[3]) { for (int k = 0; k < 3; ++k) { mn[k] = p[k] < mn[k] ? p[k] : mn[k];
        mx[k] = p[k] > mx[k] ? p[k] : mx[k]; } any = true; }
};

static void MergeSphere(BoundSphere& acc, const XMFLOAT3& c, float r) {
    if (acc.r <= 0) { acc = { c, r }; return; }
    float dx = c.x - acc.c.x, dy = c.y - acc.c.y, dz = c.z - acc.c.z;
    float d = sqrtf(dx * dx + dy * dy + dz * dz);
    if (acc.r >= d + r) return;
    if (r >= d + acc.r) { acc = { c, r }; return; }
    float nr = (d + acc.r + r) * 0.5f, t = (nr - acc.r) / d;
    acc.c = { acc.c.x + dx * t, acc.c.y + dy * t, acc.c.z + dz * t }; acc.r = nr;
}

enum class BlendClass { Opaque, Alpha, Additive };

struct GpuItem {
    ComPtr<ID3D11Buffer> vb, ib;
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
    XMFLOAT4 emissive = { 0,0,0,0 };
    XMFLOAT4X4 world;
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

static void ToWorldMatrix(const Xf& xf, XMFLOAT4X4& out) {
    float s = xf.s;
    out = XMFLOAT4X4(
        s * xf.r[0][0], s * xf.r[1][0], s * xf.r[2][0], 0,
        s * xf.r[0][1], s * xf.r[1][1], s * xf.r[2][1], 0,
        s * xf.r[0][2], s * xf.r[1][2], s * xf.r[2][2], 0,
        xf.t[0], xf.t[1], xf.t[2], 1);
}

static std::mutex g_texMutex;
static std::unordered_map<std::string, RE::NiPointer<RE::NiTexture>> g_texDone;

static std::deque<std::string> g_texOrder;
static constexpr std::size_t kMaxTexCacheEntries = 512;
static std::unordered_map<std::string, std::chrono::steady_clock::time_point> g_texInFlight;
static std::atomic<bool>     g_texFailClosed{ false };
static std::atomic<bool>     g_texShutdown{ false };
static std::atomic<uint64_t> g_texRequested{ 0 }, g_texResolved{ 0 };

static constexpr int    kMaxTexRequestsInFlight = 1;

static constexpr double kTexRequestWatchdogSec = 10.0;

static std::atomic<int>  g_texDispatchFailures{ 0 };
static constexpr int     kMaxTexDispatchFailures = 600;

static void ServiceTextureWatchdog() {
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

static bool AcquireEngineTexture(const std::string& path, RE::NiPointer<RE::NiTexture>& out) {
    if (path.empty()) return true;

    if (REX::FModule::IsRuntimeOG()) {
        Engine::CallEngineGetTexture(path.c_str(), out);
        logger::info("[ModelPreview] engine texture '{}': niTexture={}", path,
                     static_cast<bool>(out));
        return true;
    }

    if (!REX::FModule::IsRuntimeAE()) return true;

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

static ID3D11ShaderResourceView* ResolveSrv(void* rendererTexture) {
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

static bool SrvNeedsShaderSrgbDecode(ID3D11ShaderResourceView* srv) {
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
};

enum : int { kKindGeneric = 0, kKindElongate = 1, kKindShield = 2, kKindArmor = 3, kKindAmmo = 4,
             kKindClothing = 5, kKindStatic = 6 };

static int KindFromFormType(const std::string& ft) {
    if (ft == "WEAP") return kKindElongate;
    if (ft == "AMMO") return kKindAmmo;
    if (ft == "ARMO") return kKindArmor;

    if (ft == "STAT" || ft == "FURN" || ft == "WORLD") return kKindStatic;
    return kKindGeneric;
}

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

static std::map<std::string, Preview> g_previews;
static uint64_t g_nextPreviewContinuity = 1;
static std::vector<uint64_t> g_removedOverlayKeys;
static ViewGateFn g_viewGate = nullptr;
static std::mutex g_reqMutex;
static std::map<std::string, Request> g_pendingShows;
static std::vector<std::string> g_pendingHides;

static std::string MakeFullKey(ViewId v, const std::string& id) { return std::to_string(v) + "\x1f" + id; }

struct LoadResult {
    std::string fullKey; uint64_t loadGen = 0; std::string key;
    std::shared_ptr<PreviewModel> model; int kind = 0; bool ok = false;
};
static std::mutex g_resultMutex;
static std::deque<LoadResult> g_results;

static bool JsonFind(const std::string& j, const char* key, size_t& pos) {
    std::string needle = std::string("\"") + key + "\"";
    size_t p = j.find(needle); if (p == std::string::npos) return false;
    p = j.find(':', p + needle.size()); if (p == std::string::npos) return false;
    ++p; while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == '\n' || j[p] == '\r')) ++p;
    if (p >= j.size()) return false; pos = p; return true;
}
static std::string JsonStr(const std::string& j, const char* key) {
    size_t p; if (!JsonFind(j, key, p) || j[p] != '"') return "";
    size_t e = j.find('"', p + 1); if (e == std::string::npos) return "";
    return j.substr(p + 1, e - p - 1);
}
static double JsonNum(const std::string& j, const char* key, double fb) {
    size_t p; if (!JsonFind(j, key, p)) return fb; return atof(j.c_str() + p);
}

static std::vector<std::string> JsonStrArray(const std::string& j, const char* key) {
    std::vector<std::string> out;
    size_t p; if (!JsonFind(j, key, p) || j[p] != '[') return out;
    size_t end = j.find(']', p + 1);
    if (end == std::string::npos) return out;
    size_t i = p + 1;
    while (i < end) {
        size_t q1 = j.find('"', i);
        if (q1 == std::string::npos || q1 >= end) break;
        size_t q2 = j.find('"', q1 + 1);
        if (q2 == std::string::npos || q2 > end) break;
        out.push_back(j.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return out;
}

static StatusSink g_statusSink = nullptr;
void SetStatusSink(StatusSink fn) { g_statusSink = fn; }

static void JsonEscInto(std::string& o, const std::string& s) {
    for (char c : s) {
        switch (c) {
            case '\\': o += "\\\\"; break;
            case '"':  o += "\\\""; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
                else o += c;
        }
    }
}
static const char* BlendName(BlendClass b) {
    return b == BlendClass::Additive ? "additive" : (b == BlendClass::Alpha ? "alpha" : "opaque");
}

static void EmitStatus(const Preview& pv, bool ok, const std::string& error, const char* phase) {
    if (!g_statusSink) return;
    std::string j = "{\"id\":\"";  JsonEscInto(j, pv.id);
    j += "\",\"nifPath\":\"";      JsonEscInto(j, JoinNifPathsDisplay(pv.nifPaths));
    j += "\",\"phase\":\"";        j += phase;
    j += "\",\"ok\":";             j += ok ? "true" : "false";
    j += ",\"error\":\"";          JsonEscInto(j, error);
    j += "\",\"shapes\":[";
    if (ok && pv.model) {
        bool first = true;
        char num[64];
        for (const auto& it : pv.model->items) {
            if (!first) j += ','; first = false;
            j += "{\"name\":\"";   JsonEscInto(j, it.name);
            snprintf(num, sizeof(num), "%u", it.indexCount / 3);
            j += "\",\"tris\":";   j += num;
            j += ",\"blend\":\"";  j += BlendName(it.blend);
            j += "\",\"alphaTest\":"; j += (it.alphaTestThreshold >= 0.0f) ? "true" : "false";
            snprintf(num, sizeof(num), "%.2f", it.alphaTestThreshold >= 0.0f ? it.alphaTestThreshold : 0.0f);
            j += ",\"thr\":";      j += num;
            j += ",\"diffuse\":\"";JsonEscInto(j, it.diffusePath);
            j += "\",\"textured\":"; j += it.srvReady ? "true" : "false";
            j += '}';
        }
    }
    j += "]}";
    g_statusSink(pv.viewId, j.c_str());
}

static std::mutex g_lruMutex;
static std::list<std::pair<std::string, std::shared_ptr<PreviewModel>>> g_lru;

static constexpr size_t LRU_MAX = 160;
static constexpr std::size_t kMeshBudgetBytes = 192ull * 1024 * 1024;
static std::size_t g_lruBytes = 0;
static std::unordered_set<std::string> g_missing;
static std::atomic<ID3D11Device*> g_devForWorker{ nullptr };

static std::atomic<bool> g_devSingleThreaded{ false };

static std::unordered_map<std::string, std::string> ParseSwapMap(const std::string& s) {
    std::unordered_map<std::string, std::string> m;
    for (size_t i = 0; i < s.size();) {
        size_t semi = s.find(';', i);
        std::string pair = s.substr(i, semi == std::string::npos ? std::string::npos : semi - i);
        if (size_t bar = pair.find('|'); bar != std::string::npos)
            m.emplace(pair.substr(0, bar), pair.substr(bar + 1));
        if (semi == std::string::npos) break;
        i = semi + 1;
    }
    return m;
}

static std::shared_ptr<PreviewModel> LoadModel(ID3D11Device* dev, const std::vector<std::string>& nifPaths,
                                                const std::string& matSwap) {
    if (nifPaths.empty()) return nullptr;
    std::string key = JoinNifPathsKey(nifPaths) + "\x1f" + LowerStr(matSwap);
    {
        std::lock_guard<std::mutex> lock(g_lruMutex);
        for (auto it = g_lru.begin(); it != g_lru.end(); ++it)
            if (it->first == key) { g_lru.splice(g_lru.begin(), g_lru, it); return g_lru.front().second; }
        if (g_missing.count(key)) return nullptr;
    }

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
        std::lock_guard<std::mutex> lock(g_lruMutex);
        g_missing.insert(std::move(key));
        return nullptr;
    }
    std::shared_ptr<PreviewModel> pm = BuildGpuModel(dev, allShapes);
    if (pm && hasInvMarker) { pm->hasInvMarker = true; memcpy(pm->invRot, invRot, sizeof(pm->invRot)); }
    logger::info("[ModelPreview] load '{}' ({} piece(s)) shapes={} dropped={} skin={} ok={} invMarker={} "
                 "extent=({:.1f},{:.1f},{:.1f}) matSwap='{}'",
                 JoinNifPathsDisplay(nifPaths), nifPaths.size(), totalShapes, totalDropped, totalSkinnedSkipped,
                 pm != nullptr, pm ? pm->hasInvMarker : false,
                 pm && pm->box.any ? pm->box.mx[0] - pm->box.mn[0] : 0.0f,
                 pm && pm->box.any ? pm->box.mx[1] - pm->box.mn[1] : 0.0f,
                 pm && pm->box.any ? pm->box.mx[2] - pm->box.mn[2] : 0.0f, matSwap);
    if (pm) {

        pm->gpuBytes = 0;
        for (const auto& it : pm->items) {
            D3D11_BUFFER_DESC bd{};
            if (it.vb) { it.vb->GetDesc(&bd); pm->gpuBytes += bd.ByteWidth; }
            if (it.ib) { it.ib->GetDesc(&bd); pm->gpuBytes += bd.ByteWidth; }
        }

        std::lock_guard<std::mutex> lock(g_lruMutex);

        for (auto& e : g_lru) if (e.first == key) return e.second;
        g_lruBytes += pm->gpuBytes;
        g_lru.emplace_front(std::move(key), pm);

        while ((g_lru.size() > LRU_MAX || g_lruBytes > kMeshBudgetBytes) && g_lru.size() > 1) {
            const auto& victim = g_lru.back();
            g_lruBytes -= (victim.second ? victim.second->gpuBytes : 0);
            g_lru.pop_back();
        }
    }
    return pm;
}

struct Job { std::string fullKey; uint64_t loadGen = 0; Request req; };
static std::mutex g_jobMutex;
static std::condition_variable g_jobCv;

static std::map<ViewId, std::deque<Job>> g_jobsByView;
static ViewId g_rrCursor = 0;

constexpr size_t kWorkerCount = 3;

static constexpr size_t kMaxJobsPerView = 256;
static std::vector<std::thread> g_workers;
static bool g_workerStarted = false;
static std::atomic<bool> g_workerExit{ false };

static bool AnyJobs() { for (auto& [v, q] : g_jobsByView) if (!q.empty()) return true; return false; }

static bool PopNextJob(Job& out) {
    if (g_jobsByView.empty()) return false;
    auto it = g_jobsByView.upper_bound(g_rrCursor);
    for (size_t n = 0; n <= g_jobsByView.size(); ++n) {
        if (it == g_jobsByView.end()) it = g_jobsByView.begin();
        if (!it->second.empty()) {
            g_rrCursor = it->first;
            out = std::move(it->second.front());
            it->second.pop_front();
            if (it->second.empty()) g_jobsByView.erase(it);
            return true;
        }
        ++it;
    }
    return false;
}

static void CancelQueuedJobsForView(ViewId viewId) {
    std::lock_guard<std::mutex> lock(g_jobMutex);
    g_jobsByView.erase(viewId);
}

static void CancelQueuedJob(ViewId viewId, const std::string& fullKey) {
    std::lock_guard<std::mutex> lock(g_jobMutex);
    auto it = g_jobsByView.find(viewId);
    if (it == g_jobsByView.end()) return;
    std::erase_if(it->second, [&](const Job& job) { return job.fullKey == fullKey; });
    if (it->second.empty()) g_jobsByView.erase(it);
}

static void WorkerMain() {
    SetWorkerThread(true);
    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(g_jobMutex);
            g_jobCv.wait(lock, [] {
                return g_workerExit.load() ||
                       (!GameLoadActive() && AnyJobs());
            });
            if (g_workerExit.load()) return;
            if (!PopNextJob(job)) continue;
        }
        LoadResult res; res.fullKey = job.fullKey; res.loadGen = job.loadGen;
        res.key = JoinNifPathsKey(job.req.nifPaths) + "\x1f" + LowerStr(job.req.matSwap);
        res.kind = KindFromFormType(job.req.formType);
        try {
            ID3D11Device* dev = g_devForWorker.load();
            if (dev && !job.req.nifPaths.empty()) { res.model = LoadModel(dev, job.req.nifPaths, job.req.matSwap);
                res.ok = res.model != nullptr; }
        } catch (...) {
            logger::error("[ModelPreview] exception during model load"); res.ok = false; res.model = nullptr;
        }
        { std::lock_guard<std::mutex> lock(g_resultMutex); g_results.push_back(std::move(res)); }
    }
}

static bool EnsureWorkersStarted(std::unique_lock<std::mutex>& lock) {
    if (g_workerStarted) return true;
    if (g_workerExit.load()) return false;
    try {
        g_workers.reserve(kWorkerCount);
        for (size_t i = 0; i < kWorkerCount; ++i) g_workers.emplace_back(WorkerMain);
        g_workerStarted = true;
        return true;
    } catch (const std::exception& e) {
        logger::error("[ModelPreview] worker pool startup failed: {}", e.what());
    } catch (...) {
        logger::error("[ModelPreview] worker pool startup failed with an unknown exception");
    }

    g_workerExit.store(true);
    lock.unlock();
    g_jobCv.notify_all();
    for (auto& worker : g_workers) if (worker.joinable()) worker.join();
    lock.lock();
    g_workers.clear();
    g_workerExit.store(false);
    return false;
}

static bool PostJob(const std::string& fullKey, uint64_t loadGen, const Request& req) {

    if (GameLoadActive()) return false;

    if (!g_cfg.workerThreads || g_devSingleThreaded.load()) {
        LoadResult res; res.fullKey = fullKey; res.loadGen = loadGen;
        res.key = JoinNifPathsKey(req.nifPaths) + "\x1f" + LowerStr(req.matSwap);
        res.kind = KindFromFormType(req.formType);
        try {
            ID3D11Device* dev = g_devForWorker.load();
            if (dev && !req.nifPaths.empty()) {
                res.model = LoadModel(dev, req.nifPaths, req.matSwap);
                res.ok = res.model != nullptr;
            }
        } catch (...) {
            logger::error("[ModelPreview] exception during inline (single-threaded device) model load");
            res.ok = false; res.model = nullptr;
        }
        { std::lock_guard<std::mutex> lock(g_resultMutex); g_results.push_back(std::move(res)); }
        return true;
    }

    std::unique_lock<std::mutex> lock(g_jobMutex);
    if (g_workerExit.load() || !EnsureWorkersStarted(lock)) return false;

    auto& queue = g_jobsByView[req.viewId];
    for (auto& queued : queue) {
        if (queued.fullKey == fullKey) {
            queued = Job{ fullKey, loadGen, req };
            lock.unlock();
            g_jobCv.notify_one();
            return true;
        }
    }
    if (queue.size() >= kMaxJobsPerView) return false;
    queue.push_back(Job{ fullKey, loadGen, req });
    lock.unlock();
    g_jobCv.notify_one();
    return true;
}

struct CBData { XMFLOAT4X4 wvp, world; XMFLOAT4 light0, light1, params, emissive, colorSpace; };
static_assert(sizeof(CBData) % 16 == 0);

static const char* SHADER_SRC = R"hlsl(
cbuffer CB : register(b0) { float4x4 gWVP; float4x4 gWorld; float4 gLight0; float4 gLight1; float4 gParams; float4 gEmissive; float4 gColorSpace; };
Texture2D gDiffuse : register(t0); SamplerState gSamp : register(s0);
struct VSIn { float4 pos : POSITION;
#ifdef HAS_UV
    float2 uv : TEXCOORD0;
#endif
#ifdef HAS_NORMAL
    float4 nrm : NORMAL;
#endif
};
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; float3 nrm : NORMAL0; };
VSOut VSMain(VSIn i) {
    VSOut o; o.pos = mul(float4(i.pos.xyz, 1.0), gWVP);
#ifdef HAS_UV
    o.uv = i.uv;
#else
    o.uv = float2(0.5, 0.5);
#endif
#ifdef HAS_NORMAL
    o.nrm = normalize(mul(i.nrm.xyz * 2.0 - 1.0, (float3x3)gWorld));
#else
    o.nrm = float3(0.0, -1.0, 0.3);
#endif
    return o;
}
float3 SrgbToLinear(float3 c) {
    c = saturate(c);
    float3 lo = c / 12.92;
    float3 hi = pow(abs(c + 0.055) / 1.055, 2.4);
    return lerp(lo, hi, step(0.04045, c));
}
float3 LinearToSrgb(float3 c) {
    c = saturate(c);
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow(abs(c), 1.0 / 2.4) - 0.055;
    return lerp(lo, hi, step(0.0031308, c));
}
// gColorSpace.x = decode diffuse to linear here; gColorSpace.y = encode to sRGB here. Both are 0 when
// the sampler / RTV already do it in hardware. Lighting below assumes LINEAR input.
float4 PSMain(VSOut i) : SV_Target {
    float4 tex = gDiffuse.Sample(gSamp, i.uv);
    if (gColorSpace.x > 0.5) tex.rgb = SrgbToLinear(tex.rgb);

    if (gParams.x >= 0.0) clip(tex.a - gParams.x);
    if (gParams.z > 0.5) {
        float glow = dot(tex.rgb, float3(0.299,0.587,0.114));
        float3 fx = tex.rgb;
        if (gColorSpace.y > 0.5) fx = LinearToSrgb(fx);
        return float4(fx, glow * tex.a);
    }
    float3 n = normalize(i.nrm);
    float bright = gParams.w;
    float lit = (0.48 + saturate(dot(n, -gLight0.xyz)) * gLight0.w + saturate(dot(n, -gLight1.xyz)) * gLight1.w) * bright;
    float3 viewDir = float3(0.0, -0.82, 0.57);
    float rim = pow(1.0 - saturate(dot(n, viewDir)), 2.5) * 0.45 * bright;

    float outA = (gParams.y > 0.5) ? tex.a : 1.0;

    float3 emit = gEmissive.rgb * gEmissive.a * 2.0;
    float3 col = tex.rgb * lit + rim * float3(0.9,0.95,1.0) + emit;
    if (gColorSpace.y > 0.5) col = LinearToSrgb(col);
    return float4(col, outA);
}
)hlsl";

static ID3D11Device* g_dev = nullptr;
static bool g_d3dReady = false, g_d3dFailed = false;

static bool g_srgbRtSupported = false;
static ComPtr<ID3D11Texture2D> g_depthTex;
static ComPtr<ID3D11DepthStencilView> g_dsv;
static ComPtr<ID3D11Buffer> g_cb;
static ComPtr<ID3D11SamplerState> g_sampler;
static ComPtr<ID3D11RasterizerState> g_rasterizer;
static ComPtr<ID3D11DepthStencilState> g_depthState;
static ComPtr<ID3D11DepthStencilState> g_depthStateNoWrite;
static ComPtr<ID3D11BlendState> g_blendState;
static ComPtr<ID3D11BlendState> g_blendAlpha;
static ComPtr<ID3D11BlendState> g_blendAdditive;
static ComPtr<ID3D11ShaderResourceView> g_graySrv;
static ComPtr<ID3D11PixelShader> g_ps;
static ComPtr<ID3D11VertexShader> g_vs[4];
static ComPtr<ID3DBlob> g_vsBlob[4];
static std::map<uint64_t, ComPtr<ID3D11InputLayout>> g_layouts;

static bool CompileShaders() {
    UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL2;
    for (int i = 0; i < 4; ++i) {
        D3D_SHADER_MACRO macros[3] = {}; int n = 0;
        if (i & 1) macros[n++] = { "HAS_UV", "1" };
        if (i & 2) macros[n++] = { "HAS_NORMAL", "1" };
        ComPtr<ID3DBlob> err;
        if (FAILED(D3DCompile(SHADER_SRC, strlen(SHADER_SRC), "ModelPreview", macros, nullptr, "VSMain",
                              "vs_5_0", flags, 0, &g_vsBlob[i], &err))) {
            logger::error("[ModelPreview] VS compile {} failed: {}", i,
                          err ? (const char*)err->GetBufferPointer() : "?");
            return false;
        }
        if (FAILED(g_dev->CreateVertexShader(g_vsBlob[i]->GetBufferPointer(), g_vsBlob[i]->GetBufferSize(),
                                             nullptr, &g_vs[i]))) return false;
    }
    ComPtr<ID3DBlob> psBlob, err;
    if (FAILED(D3DCompile(SHADER_SRC, strlen(SHADER_SRC), "ModelPreview", nullptr, nullptr, "PSMain",
                          "ps_5_0", flags, 0, &psBlob, &err))) {
        logger::error("[ModelPreview] PS compile failed: {}", err ? (const char*)err->GetBufferPointer() : "?");
        return false;
    }
    return SUCCEEDED(g_dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_ps));
}

static bool EnsureRenderResources(ID3D11Device* dev) {
    if (g_d3dReady) return true;
    if (g_d3dFailed) return false;
    g_dev = dev;
    auto fail = [&](const char* what) { logger::error("[ModelPreview] D3D init failed at {}", what);
        g_d3dFailed = true; return false; };

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = (UINT)g_cfg.rtSize; td.MipLevels = 1; td.ArraySize = 1;
    td.SampleDesc = { 1, 0 }; td.Usage = D3D11_USAGE_DEFAULT;
    td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &g_depthTex))) return fail("depth");
    if (FAILED(dev->CreateDepthStencilView(g_depthTex.Get(), nullptr, &g_dsv))) return fail("dsv");

    D3D11_BUFFER_DESC bd = {}; bd.ByteWidth = sizeof(CBData); bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&bd, nullptr, &g_cb))) return fail("cb");

    D3D11_SAMPLER_DESC sd = {}; sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP; sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(dev->CreateSamplerState(&sd, &g_sampler))) return fail("sampler");

    D3D11_RASTERIZER_DESC rd = {}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(dev->CreateRasterizerState(&rd, &g_rasterizer))) return fail("raster");

    D3D11_DEPTH_STENCIL_DESC dsd = {}; dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; dsd.DepthFunc = D3D11_COMPARISON_LESS;
    if (FAILED(dev->CreateDepthStencilState(&dsd, &g_depthState))) return fail("depth state");

    D3D11_DEPTH_STENCIL_DESC dsn = dsd; dsn.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    if (FAILED(dev->CreateDepthStencilState(&dsn, &g_depthStateNoWrite))) return fail("depth state (no-write)");

    D3D11_BLEND_DESC bld = {}; bld.RenderTarget[0].BlendEnable = FALSE;
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(dev->CreateBlendState(&bld, &g_blendState))) return fail("blend");

    D3D11_BLEND_DESC abd = {}; auto& art = abd.RenderTarget[0];
    art.BlendEnable = TRUE;
    art.SrcBlend = D3D11_BLEND_SRC_ALPHA;  art.DestBlend = D3D11_BLEND_INV_SRC_ALPHA; art.BlendOp = D3D11_BLEND_OP_ADD;
    art.SrcBlendAlpha = D3D11_BLEND_ONE;   art.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA; art.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    art.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(dev->CreateBlendState(&abd, &g_blendAlpha))) return fail("blend (alpha)");

    art.DestBlend = D3D11_BLEND_ONE; art.DestBlendAlpha = D3D11_BLEND_ONE;
    if (FAILED(dev->CreateBlendState(&abd, &g_blendAdditive))) return fail("blend (additive)");

    uint32_t gray = 0xFF808080;
    D3D11_TEXTURE2D_DESC gd = {}; gd.Width = gd.Height = 1; gd.MipLevels = 1; gd.ArraySize = 1;
    gd.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; gd.SampleDesc = { 1, 0 }; gd.Usage = D3D11_USAGE_IMMUTABLE;
    gd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA gi = { &gray, 4, 0 };
    ComPtr<ID3D11Texture2D> grayTex;
    if (FAILED(dev->CreateTexture2D(&gd, &gi, &grayTex))) return fail("gray tex");
    if (FAILED(dev->CreateShaderResourceView(grayTex.Get(), nullptr, &g_graySrv))) return fail("gray srv");

    {
        UINT sup = 0;
        g_srgbRtSupported =
            SUCCEEDED(dev->CheckFormatSupport(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, &sup)) &&
            (sup & D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0;
        logger::info("[ModelPreview][srgb] preview RT encode = {} (B8G8R8A8_UNORM_SRGB render-target "
                     "support={}). Lighting runs in LINEAR; the RT is handed to the compositor as plain "
                     "_UNORM so its sRGB-encoded bytes pass through unconverted.",
                     g_srgbRtSupported ? "HARDWARE (_SRGB RTV)" : "SHADER (manual LinearToSrgb)",
                     g_srgbRtSupported);
    }

    if (!CompileShaders()) { g_d3dFailed = true; return false; }

    {
        static bool s_envLogged = false;
        if (!s_envLogged) {
            s_envLogged = true;
            const char* rt = REX::FModule::IsRuntimeOG() ? "OG" : "NOT-OG (NG/AE)";
            std::string adapter = "unknown";
            Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDev;
            Microsoft::WRL::ComPtr<IDXGIAdapter> adap;
            if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&dxgiDev))) && dxgiDev &&
                SUCCEEDED(dxgiDev->GetAdapter(&adap)) && adap) {
                DXGI_ADAPTER_DESC ad{};
                if (SUCCEEDED(adap->GetDesc(&ad))) {
                    int n = WideCharToMultiByte(CP_UTF8, 0, ad.Description, -1, nullptr, 0, nullptr, nullptr);
                    if (n > 0) { adapter.assign((size_t)n - 1, '\0');
                                 WideCharToMultiByte(CP_UTF8, 0, ad.Description, -1, adapter.data(), n,
                                                     nullptr, nullptr); }
                    adapter += " (" + std::to_string(ad.DedicatedVideoMemory / (1024ull * 1024ull)) + " MB VRAM)";
                }
            }
            logger::warn("[ModelPreview] ENV: runtime={} featureLevel=0x{:04X} gpu={}",
                         rt, static_cast<unsigned>(dev->GetFeatureLevel()), adapter);
        }
    }
    const UINT devFlags = dev->GetCreationFlags();
    g_devSingleThreaded = (devFlags & D3D11_CREATE_DEVICE_SINGLETHREADED) != 0;
    if (g_devSingleThreaded) {
        logger::error("[ModelPreview] device has D3D11_CREATE_DEVICE_SINGLETHREADED (flags=0x{:08X}) -- "
                      "worker-thread GPU resource creation is UNSAFE on this device; serializing model "
                      "loads onto the render thread instead.", devFlags);
    } else {

        logger::info("[ModelPreview] device creation flags=0x{:08X}, SINGLETHREADED bit CLEAR as "
                     "reported through the loaded d3d11 wrapper (ENB proxies it) -- not independently "
                     "verified", devFlags);
    }

    logger::warn("[ModelPreview] EFFECTIVE model-load path = {} (bWorkerThreads={}, devSingleThreaded={})",
                 (!g_cfg.workerThreads || g_devSingleThreaded.load()) ? "RENDER THREAD (inline)"
                                                                      : "WORKER POOL",
                 g_cfg.workerThreads ? 1 : 0, g_devSingleThreaded.load() ? 1 : 0);

    g_d3dReady = true;
    logger::info("[ModelPreview] D3D resources ready ({}x{})", g_cfg.rtSize, g_cfg.rtSize);
    return true;
}

static ID3D11InputLayout* GetOrCreateLayout(uint64_t desc, int vsIndex, bool floatPos) {
    auto it = g_layouts.find(desc);
    if (it != g_layouts.end()) return it->second.Get();
    std::vector<D3D11_INPUT_ELEMENT_DESC> e;
    e.push_back({ "POSITION", 0, floatPos ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R16G16B16A16_FLOAT,
                  0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 });
    if (vsIndex & 1) e.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R16G16_FLOAT, 0, DescAttrOffset(desc, 1),
                                   D3D11_INPUT_PER_VERTEX_DATA, 0 });
    if (vsIndex & 2) e.push_back({ "NORMAL", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, DescAttrOffset(desc, 3),
                                   D3D11_INPUT_PER_VERTEX_DATA, 0 });
    ComPtr<ID3D11InputLayout> layout;
    if (FAILED(g_dev->CreateInputLayout(e.data(), (UINT)e.size(), g_vsBlob[vsIndex]->GetBufferPointer(),
                                        g_vsBlob[vsIndex]->GetBufferSize(), &layout))) {

        logger::error("[ModelPreview] CreateInputLayout FAILED for vertex desc 0x{:X} (vs{}) -- every mesh "
                      "with this layout will fall back to the flat icon for the rest of the session",
                      desc, vsIndex);
        g_layouts[desc] = nullptr; return nullptr;
    }
    g_layouts[desc] = layout; return layout.Get();
}

struct RtTarget {
    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
};
static std::vector<RtTarget> g_rtFreeList;
static size_t g_rtLive = 0;
static constexpr size_t kRtPoolCap = 64;

static constexpr int kMaxStaticRendersPerFrame = 8;

static constexpr int kMaxRendersPerFrame = 8;

static uint64_t g_previewFrame = 0;
static std::vector<Preview*> g_staticScratch;
static std::vector<Preview*> g_spinScratch;

static uint64_t g_overlayCacheFrame = ~uint64_t{0};
static std::map<ViewId, std::vector<Overlay>> g_overlayCache;

static void RebuildOverlayCache() {
    if (g_overlayCacheFrame == g_previewFrame) return;
    g_overlayCache.clear();
    g_overlayCacheFrame = g_previewFrame;
    if (!g_d3dReady) return;

    for (auto& [key, pv] : g_previews) {
        if (pv.viewId == 0 || !pv.model || !pv.srv || !pv.drawn) continue;
        Overlay o;
        o.srv = pv.srv.Get();
        o.contentGeneration = pv.lastRenderFrame + 1;
        o.continuityKey = pv.continuityKey;
        o.sourceWidth = static_cast<uint32_t>(g_cfg.rtSize);
        o.sourceHeight = static_cast<uint32_t>(g_cfg.rtSize);
        o.destLeft = (long)pv.rect.x; o.destTop = (long)pv.rect.y;
        o.destRight = (long)(pv.rect.x + pv.rect.w); o.destBottom = (long)(pv.rect.y + pv.rect.h);
        if (pv.clip.w > 0 && pv.clip.h > 0) {
            o.hasClip = true;
            o.clipLeft = (long)pv.clip.x; o.clipTop = (long)pv.clip.y;
            o.clipRight = (long)(pv.clip.x + pv.clip.w); o.clipBottom = (long)(pv.clip.y + pv.clip.h);
        }
        g_overlayCache[pv.viewId].push_back(o);
    }
}

static uint64_t g_touchClock = 0;
static uint64_t g_evictions = 0;

static uint64_t g_loadGenClock = 0;

static bool EnsurePreviewTarget(Preview& pv) {

    if (pv.tex && pv.rtv && pv.srv) return true;
    pv.tex.Reset(); pv.rtv.Reset(); pv.srv.Reset();

    if (!g_rtFreeList.empty()) {

        RtTarget t = std::move(g_rtFreeList.back());
        g_rtFreeList.pop_back();
        pv.tex = std::move(t.tex); pv.rtv = std::move(t.rtv); pv.srv = std::move(t.srv);
        ++g_rtLive; pv.dirty = true; pv.drawn = false; return true;
    }

    D3D11_TEXTURE2D_DESC td = {}; td.Width = td.Height = (UINT)g_cfg.rtSize; td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = g_srgbRtSupported ? DXGI_FORMAT_B8G8R8A8_TYPELESS : DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc = { 1, 0 };
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(g_dev->CreateTexture2D(&td, nullptr, &tex))) {
        logger::error("[ModelPreview] CreateTexture2D FAILED for a {}x{} preview RT ({} live) -- likely VRAM "
                      "exhaustion; preview will be blank", g_cfg.rtSize, g_cfg.rtSize, g_rtLive);
        return false;
    }

    D3D11_RENDER_TARGET_VIEW_DESC rtvd = {};
    rtvd.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;

    if (FAILED(g_dev->CreateRenderTargetView(tex.Get(), g_srgbRtSupported ? &rtvd : nullptr, &rtv))) {
        logger::error("[ModelPreview] CreateRenderTargetView FAILED for a preview RT -- preview will be blank");
        return false;
    }
    if (FAILED(g_dev->CreateShaderResourceView(tex.Get(), g_srgbRtSupported ? &srvd : nullptr, &srv))) {
        logger::error("[ModelPreview] CreateShaderResourceView FAILED for a preview RT -- preview will be blank");
        return false;
    }

    pv.tex = std::move(tex); pv.rtv = std::move(rtv); pv.srv = std::move(srv);
    if (++g_rtLive > kRtPoolCap)
        logger::warn("[ModelPreview] {} concurrent preview RTs (past soft cap {})", g_rtLive, kRtPoolCap);
    pv.dirty = true; pv.drawn = false; return true;
}

static void ReleaseTarget(Preview& pv) {

    const bool complete = pv.tex && pv.rtv && pv.srv;
    if (complete) {
        if (g_rtFreeList.size() < kRtPoolCap) {
            RtTarget t; t.tex = std::move(pv.tex); t.rtv = std::move(pv.rtv); t.srv = std::move(pv.srv);
            g_rtFreeList.push_back(std::move(t));
        }
        if (g_rtLive) --g_rtLive;
    }
    pv.tex.Reset(); pv.rtv.Reset(); pv.srv.Reset();
}

static float EffSpin(float s) { return s > -9999.0f ? s : g_cfg.spinDegPerSec; }

static bool ComputePose(Preview& pv, std::shared_ptr<PreviewModel> pm, const std::string& key, int kind) {
    if (!pm || pm->items.empty()) return false;
    pv.model = pm; pv.modelKey = key; pv.kind = kind;
    XMStoreFloat4x4(&pv.orient, XMMatrixIdentity());
    BoundSphere bound = pm->bound;
    if (bound.r <= 0.001f && pm->fxBound.r > 0.001f) bound = pm->fxBound;
    pv.center = bound.c; pv.radius = bound.r > 0.001f ? bound.r : 1.0f;

    float ex = 0, ey = 0, ez = 0; bool haveExt = pm->box.any;
    if (haveExt) {
        ex = pm->box.mx[0] - pm->box.mn[0]; ey = pm->box.mx[1] - pm->box.mn[1]; ez = pm->box.mx[2] - pm->box.mn[2];
        XMFLOAT3 mid = { (pm->box.mn[0] + pm->box.mx[0]) * 0.5f, (pm->box.mn[1] + pm->box.mx[1]) * 0.5f,
                         (pm->box.mn[2] + pm->box.mx[2]) * 0.5f };
        float extR = 0.5f * sqrtf(ex * ex + ey * ey + ez * ez);

        pv.center = mid;
        if (extR > 0.001f && (pv.radius < 0.25f * extR || pv.radius > 4.0f * extR)) pv.radius = extR;
    }

    if (pm->hasInvMarker) {
        XMMATRIX o = XMMatrixRotationZ(-pm->invRot[2]) * XMMatrixRotationY(-pm->invRot[1]) *
                     XMMatrixRotationX(-pm->invRot[0]);
        if (kind == kKindAmmo) o = o * XMMatrixRotationX(XM_PI);
        XMStoreFloat4x4(&pv.orient, o);
    } else if (kind == kKindElongate && haveExt) {

        const float e[3] = { ex, ey, ez };
        int L = (e[0] >= e[1] && e[0] >= e[2]) ? 0 : (e[1] >= e[2] ? 1 : 2);
        int S = (e[0] <= e[1] && e[0] <= e[2]) ? 0 : (e[1] <= e[2] ? 1 : 2);
        if (S == L) S = (L + 1) % 3;
        const int M = 3 - L - S;
        XMFLOAT4X4 m{};
        m.m[L][0] = 1.0f; m.m[S][1] = 1.0f; m.m[M][2] = 1.0f; m.m[3][3] = 1.0f;
        XMMATRIX o = XMLoadFloat4x4(&m);

        if (XMVectorGetX(XMMatrixDeterminant(o)) < 0.0f) {
            m.m[S][1] = -1.0f;
            o = XMLoadFloat4x4(&m);
        }
        XMStoreFloat4x4(&pv.orient, o);
    } else if ((kind == kKindAmmo || kind == kKindGeneric) && haveExt) {

        float mn = ex < ey ? (ex < ez ? ex : ez) : (ey < ez ? ey : ez);
        float mx = ex > ey ? (ex > ez ? ex : ez) : (ey > ez ? ey : ez);
        float mid = ex + ey + ez - mn - mx;
        bool xLargest = ex >= ey && ex >= ez, yLargest = ey >= ex && ey >= ez;
        bool xSmallest = ex <= ey && ex <= ez, zSmallest = ez <= ex && ez <= ey;
        XMMATRIX o = XMMatrixIdentity(); bool changed = false;
        if (mid > 0.001f && mx > 2.0f * mid) {
            if (xLargest)      { o = XMMatrixRotationY(-XM_PIDIV2); changed = true; }
            else if (yLargest) { o = XMMatrixRotationX(XM_PIDIV2);  changed = true; }

        } else if (mn > 0.001f && mid > 1.3f * mn) {
            if (zSmallest)      { o = XMMatrixRotationX(-XM_PIDIV2); changed = true; }
            else if (xSmallest) { o = XMMatrixRotationZ(XM_PIDIV2);  changed = true; }

        }
        if (kind == kKindAmmo) o = o * XMMatrixRotationX(XM_PI);
        if (changed || kind == kKindAmmo) XMStoreFloat4x4(&pv.orient, o);
    } else if ((kind == kKindArmor || kind == kKindClothing) && haveExt) {
        float planar = ex > ey ? ex : ey;
        if (planar > 1.35f * ez) XMStoreFloat4x4(&pv.orient, XMMatrixRotationX(-XM_PIDIV2));
    }
    pv.dirty = true;
    return true;
}

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

static bool RenderPreviewInner(ID3D11DeviceContext* ctx, Preview& pv) {
    if (!pv.model || !pv.rtv) return false;
    if (!pv.spinStarted) { pv.spinStart = std::chrono::steady_clock::now(); pv.spinStarted = true; }
    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - pv.spinStart).count();
    float spinRate = EffSpin(pv.spin);
    float angle = XMConvertToRadians(pv.yaw) + (float)(elapsed * spinRate * (XM_PI / 180.0));
    pv.lastAngleDeg = angle * (180.0f / XM_PI);

    const float clear[4] = { 0, 0, 0, 0 };
    ctx->ClearRenderTargetView(pv.rtv.Get(), clear);
    ctx->ClearDepthStencilView(g_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    ID3D11RenderTargetView* rtv = pv.rtv.Get();
    ctx->OMSetRenderTargets(1, &rtv, g_dsv.Get());
    D3D11_VIEWPORT vp = { 0, 0, (float)g_cfg.rtSize, (float)g_cfg.rtSize, 0, 1 };
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(g_rasterizer.Get());
    const float bf[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(g_blendState.Get(), bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(g_depthState.Get(), 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11SamplerState* samp = g_sampler.Get(); ctx->PSSetSamplers(0, 1, &samp);
    ID3D11Buffer* cb = g_cb.Get(); ctx->VSSetConstantBuffers(0, 1, &cb); ctx->PSSetConstantBuffers(0, 1, &cb);
    ctx->PSSetShader(g_ps.Get(), nullptr, 0);

    float R = pv.radius;
    float d = R / tanf(XMConvertToRadians(15.0f)) * 1.12f / pv.zoom;
    float elev = XMConvertToRadians(35.0f);
    XMVECTOR panOfs = XMVectorSet(pv.panX * R, 0, pv.panY * R, 0);
    XMVECTOR eye = XMVectorAdd(XMVectorSet(0, -d * cosf(elev), d * sinf(elev), 1), panOfs);
    XMVECTOR at = XMVectorAdd(XMVectorSet(0, 0, 0, 1), panOfs);
    XMMATRIX view = XMMatrixLookAtRH(eye, at, XMVectorSet(0, 0, 1, 0));
    XMMATRIX proj = XMMatrixPerspectiveFovRH(XMConvertToRadians(30.0f), 1.0f, 0.05f * R, d + 4.0f * R);
    XMMATRIX recenter = XMMatrixTranslation(-pv.center.x, -pv.center.y, -pv.center.z);
    XMMATRIX orient = XMLoadFloat4x4(&pv.orient);
    if (pv.flip) orient = orient * XMMatrixRotationX(XM_PI);
    if (pv.roll != 0.0f) orient = orient * XMMatrixRotationY(XMConvertToRadians(pv.roll));
    XMMATRIX spin = XMMatrixRotationZ(angle);
    if (pv.pitch != 0.0f) spin = spin * XMMatrixRotationX(XMConvertToRadians(pv.pitch));
    XMVECTOR l0 = XMVector3Normalize(XMVectorSet(0.45f, 0.80f, -0.50f, 0));
    XMVECTOR l1 = XMVector3Normalize(XMVectorSet(-0.60f, 0.40f, 0.25f, 0));

    bool texPending = false;

    auto drawOne = [&](GpuItem& item) {

        if (!g_cfg.soloShape.empty() && item.name.find(g_cfg.soloShape) == std::string::npos) return;
        ID3D11InputLayout* layout = GetOrCreateLayout(item.desc, item.vsIndex, item.floatPos);
        if (!layout) return;

        if (!item.diffusePath.empty() && !item.texSettled)
            item.texSettled = AcquireEngineTexture(item.diffusePath, item.engineTex);
        ID3D11ShaderResourceView* srv = nullptr;
        if (item.engineTex) srv = ResolveSrv(item.engineTex->rendererTexture);
        item.srvReady = (srv != nullptr);
        if (!srv) {

            if (!item.texSettled || item.engineTex) texPending = true;
            if (item.effect) return;
            srv = g_graySrv.Get();
        }
        if (item.srvFormatCachedFor != srv) {
            item.srvFormatCachedFor = srv;
            item.srvNeedsSrgbDecode = SrvNeedsShaderSrgbDecode(srv);
        }
        XMMATRIX world = XMLoadFloat4x4(&item.world) * recenter * orient * spin;
        XMMATRIX wvp = world * view * proj;
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (FAILED(ctx->Map(g_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
        auto* dptr = (CBData*)mapped.pData;
        XMStoreFloat4x4(&dptr->wvp, XMMatrixTranspose(wvp));
        XMStoreFloat4x4(&dptr->world, XMMatrixTranspose(world));
        XMStoreFloat4(&dptr->light0, XMVectorSetW(l0, 1.1f));
        XMStoreFloat4(&dptr->light1, XMVectorSetW(l1, 0.5f));

        dptr->params = { item.alphaTestThreshold, item.blend == BlendClass::Alpha ? 1.0f : 0.0f,
                         item.effect ? 1.0f : 0.0f, pv.brightness };
        dptr->emissive = item.emissive;
        dptr->colorSpace = { item.srvNeedsSrgbDecode ? 1.0f : 0.0f,
                             g_srgbRtSupported ? 0.0f : 1.0f, 0.0f, 0.0f };
        ctx->Unmap(g_cb.Get(), 0);
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->IASetInputLayout(layout);
        ctx->VSSetShader(g_vs[item.vsIndex].Get(), nullptr, 0);
        UINT offset = 0; ID3D11Buffer* vb = item.vb.Get();
        ctx->IASetVertexBuffers(0, 1, &vb, &item.stride, &offset);
        ctx->IASetIndexBuffer(item.ib.Get(), DXGI_FORMAT_R16_UINT, 0);
        ctx->DrawIndexed(item.indexCount, 0, 0);
    };

    ctx->OMSetBlendState(g_blendState.Get(), bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(g_depthState.Get(), 0);
    for (auto& item : pv.model->items) if (item.blend == BlendClass::Opaque) drawOne(item);

    ctx->OMSetBlendState(g_blendAlpha.Get(), bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(g_depthStateNoWrite.Get(), 0);
    for (auto& item : pv.model->items) if (item.blend == BlendClass::Alpha) drawOne(item);

    ctx->OMSetBlendState(g_blendAdditive.Get(), bf, 0xFFFFFFFF);
    for (auto& item : pv.model->items) if (item.blend == BlendClass::Additive) drawOne(item);

    pv.drawn = true;
    return texPending;
}

static void ErasePreview(std::map<std::string, Preview>::iterator& it) {
    if (it->second.continuityKey != 0) g_removedOverlayKeys.push_back(it->second.continuityKey);
    ReleaseTarget(it->second);
    it = g_previews.erase(it);
}

static void EvictToStub(Preview& pv) {
    ReleaseTarget(pv);
    pv.model.reset();
    pv.drawn = false;
    pv.statusTexSent = false;
}

void Show(ViewId viewId, const std::string& json) {
    if (!Enabled()) return;
    Request r;
    r.viewId = viewId;
    r.id = JsonStr(json, "id");

    r.nifPaths = JsonStrArray(json, "nifPaths");
    if (r.nifPaths.empty()) {
        std::string single = JsonStr(json, "nifPath");
        if (!single.empty()) r.nifPaths.push_back(std::move(single));
    }
    r.formType = JsonStr(json, "formType");
    r.matSwap = JsonStr(json, "matSwap");
    r.rect = { (float)JsonNum(json, "x", 0), (float)JsonNum(json, "y", 0),
               (float)JsonNum(json, "w", 0), (float)JsonNum(json, "h", 0) };
    r.clip = { (float)JsonNum(json, "clipX", 0), (float)JsonNum(json, "clipY", 0),
               (float)JsonNum(json, "clipW", 0), (float)JsonNum(json, "clipH", 0) };
    r.zoom = (float)JsonNum(json, "zoom", 1.0);
    r.panX = (float)JsonNum(json, "panX", 0.0);
    r.panY = (float)JsonNum(json, "panY", 0.0);
    r.roll = (float)JsonNum(json, "roll", 0.0);
    r.pitch = (float)JsonNum(json, "pitch", 0.0);
    r.brightness = (float)JsonNum(json, "brightness", 1.0);
    r.spin = (float)JsonNum(json, "spin", -10000.0);
    r.yaw = (float)JsonNum(json, "yaw", -10000.0);
    r.flip = JsonNum(json, "flip", 0.0) != 0.0;
    if (r.nifPaths.empty()) return;
    std::lock_guard<std::mutex> lock(g_reqMutex);
    const std::string key = MakeFullKey(viewId, r.id);

    std::erase(g_pendingHides, key);
    g_pendingShows[key] = std::move(r);
}

void Hide(ViewId viewId, const std::string& json) {
    std::string id = JsonStr(json, "id");
    if (id.empty()) {
        {
            std::lock_guard<std::mutex> lock(g_reqMutex);
            const std::string prefix = std::to_string(viewId) + "\x1f";
            std::erase_if(g_pendingShows, [&](const auto& kv) { return kv.first.rfind(prefix, 0) == 0; });
            g_pendingHides.push_back(prefix + "*");
        }
        CancelQueuedJobsForView(viewId);
    } else {
        const std::string key = MakeFullKey(viewId, id);
        {
            std::lock_guard<std::mutex> lock(g_reqMutex);
            g_pendingShows.erase(key);
            g_pendingHides.push_back(key);
        }
        CancelQueuedJob(viewId, key);
    }
}

void OnViewDestroyed(ViewId viewId) {
    {
        std::lock_guard<std::mutex> lock(g_reqMutex);

        const std::string prefix = std::to_string(viewId) + "\x1f";
        std::erase_if(g_pendingShows, [&](const auto& kv) { return kv.first.rfind(prefix, 0) == 0; });
        g_pendingHides.push_back(prefix + "*");
    }
    CancelQueuedJobsForView(viewId);
}

void GetOverlays(ViewId viewId, std::vector<Overlay>& out) {
    if (!g_d3dReady || viewId == 0) return;
    RebuildOverlayCache();
    auto it = g_overlayCache.find(viewId);
    if (it == g_overlayCache.end()) return;
    out.insert(out.end(), it->second.begin(), it->second.end());
}

void GetActiveViews(std::vector<ViewId>& out) {
    out.clear();
    if (!g_d3dReady) return;
    RebuildOverlayCache();
    out.reserve(g_overlayCache.size());
    for (const auto& [viewId, overlays] : g_overlayCache) {
        if (!overlays.empty()) out.push_back(viewId);
    }
}

void DrainRemovedOverlayKeys(std::vector<uint64_t>& out) {
    out.insert(out.end(), g_removedOverlayKeys.begin(), g_removedOverlayKeys.end());
    g_removedOverlayKeys.clear();
}

bool HasPendingWork() {
    if (!g_previews.empty()) return true;
    std::lock_guard<std::mutex> lock(g_reqMutex);
    return !g_pendingShows.empty() || !g_pendingHides.empty();
}

void SetViewGate(ViewGateFn fn) { g_viewGate = fn; }

void SetGameLoadActive(bool active) noexcept {
    SetReadsGameLoadActive(active);
    if (!active) g_jobCv.notify_all();
}

static void TickCoreImpl(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
    g_devForWorker.store(dev);

    static std::vector<std::string> hides;
    static std::vector<std::pair<std::string, Request>> shows;
    hides.clear();
    shows.clear();
    {
        std::lock_guard<std::mutex> lock(g_reqMutex);
        hides.swap(g_pendingHides);
        shows.reserve(g_pendingShows.size());
        for (auto& kv : g_pendingShows) shows.emplace_back(kv.first, std::move(kv.second));
        g_pendingShows.clear();
    }
    for (auto& h : hides) {
        bool wholeView = h.size() >= 2 && h.back() == '*' && h[h.size() - 2] == '\x1f';
        if (wholeView) {
            std::string prefix = h.substr(0, h.size() - 1);
            for (auto it = g_previews.begin(); it != g_previews.end();) {
                if (it->first.rfind(prefix, 0) == 0) ErasePreview(it); else ++it;
            }
        } else {
            auto it = g_previews.find(h);
            if (it != g_previews.end()) ErasePreview(it);
        }
    }

    if (!EnsureRenderResources(dev)) return;

    static std::vector<std::pair<ViewId, bool>> viewActive;
    viewActive.clear();
    auto viewIsActive = [&](ViewId v) -> bool {
        if (!g_viewGate) return true;
        for (const auto& [id, active] : viewActive)
            if (id == v) return active;
        const bool a = g_viewGate(v);
        viewActive.emplace_back(v, a);
        return a;
    };

    auto evictOneVictim = [&](const std::string& keepKey) -> bool {
        auto victim = g_previews.end();
        bool victimInactive = false;
        uint64_t victimTouch = 0;
        for (auto it = g_previews.begin(); it != g_previews.end(); ++it) {
            if (it->first == keepKey) continue;

            const Preview& cand = it->second;
            if (!(cand.tex && cand.rtv && cand.srv)) continue;
            const bool inactive = !viewIsActive(it->second.viewId);
            const uint64_t t = it->second.touch;
            bool better;
            if (victim == g_previews.end())      better = true;
            else if (inactive != victimInactive) better = inactive;
            else                                 better = t < victimTouch;
            if (better) { victim = it; victimInactive = inactive; victimTouch = t; }
        }
        if (victim == g_previews.end()) return false;
        EvictToStub(victim->second);
        ++g_evictions;
        return true;
    };

    auto ensureTargetCapped = [&](Preview& pv, const std::string& keepKey) -> bool {

        const bool hasCompleteTarget = pv.tex && pv.rtv && pv.srv;
        if (!hasCompleteTarget)
            while (g_rtLive >= (size_t)g_cfg.maxLivePreviews)
                if (!evictOneVictim(keepKey)) break;
        return EnsurePreviewTarget(pv);
    };

    static std::vector<std::pair<std::string, Request>> requeue;
    requeue.clear();

    for (auto& [fullKey, r] : shows) {
        Preview& pv = g_previews[fullKey];
        if (pv.continuityKey == 0) pv.continuityKey = g_nextPreviewContinuity++;
        pv.viewId = r.viewId; pv.id = r.id; pv.rect = r.rect; pv.clip = r.clip;
        bool contentChanged = pv.model && (pv.zoom != r.zoom || pv.panX != r.panX || pv.panY != r.panY ||
            pv.roll != r.roll || pv.pitch != r.pitch || pv.brightness != r.brightness ||
            pv.flip != r.flip || pv.spin != r.spin);
        pv.zoom = r.zoom; pv.panX = r.panX; pv.panY = r.panY; pv.roll = r.roll; pv.pitch = r.pitch;
        pv.brightness = r.brightness; pv.flip = r.flip;

        std::string itemKey = JoinNifPathsKey(r.nifPaths) + "\x1f" + LowerStr(r.matSwap);
        const bool sameItem = pv.modelKey == itemKey && pv.model;
        const bool yawProvided = r.yaw > -9999.0f;
        const bool wasSpinning = EffSpin(pv.spin) != 0.0f, willSpin = EffSpin(r.spin) != 0.0f;
        float oldYaw = pv.yaw;
        if (yawProvided) pv.yaw = r.yaw;
        else if (sameItem && wasSpinning && !willSpin) pv.yaw = pv.lastAngleDeg;
        else if (sameItem && !wasSpinning && willSpin) { pv.yaw = pv.lastAngleDeg;
            pv.spinStart = std::chrono::steady_clock::now(); pv.spinStarted = true; }
        else if (!sameItem) pv.yaw = 0.0f;
        pv.spin = r.spin;
        if (pv.model && (contentChanged || pv.yaw != oldYaw)) pv.dirty = true;

        if (!sameItem && pv.wantKey != itemKey) {
            if (GameLoadActive()) {
                requeue.emplace_back(fullKey, r);
                pv.touch = ++g_touchClock;
                continue;
            }
            pv.nifPaths = r.nifPaths; pv.wantKey = itemKey; pv.loadGen = ++g_loadGenClock;
            pv.statusTexSent = false;
            ensureTargetCapped(pv, fullKey);
            Request jobReq = r;
            if (!PostJob(fullKey, pv.loadGen, jobReq)) {

                pv.wantKey.clear();
                requeue.emplace_back(fullKey, r);
            }
        }
        pv.touch = ++g_touchClock;
    }

    if (!requeue.empty()) {
        std::lock_guard<std::mutex> lock(g_reqMutex);

        auto cancelledByHide = [&](const std::string& key) {
            for (const auto& h : g_pendingHides) {
                if (h == key) return true;
                if (h.size() >= 2 && h.back() == '*' && h[h.size() - 2] == '\x1f' &&
                    key.rfind(h.substr(0, h.size() - 1), 0) == 0) return true;
            }
            return false;
        };

        for (auto& [k, req] : requeue)
            if (!cancelledByHide(k)) g_pendingShows.emplace(std::move(k), std::move(req));
    }

    requeue.clear();
    shows.clear();

    static std::deque<LoadResult> results;
    results.clear();
    { std::lock_guard<std::mutex> lock(g_resultMutex); results.swap(g_results); }
    for (auto& res : results) {
        auto it = g_previews.find(res.fullKey);
        if (it == g_previews.end()) continue;
        Preview& pv = it->second;

        if (res.loadGen != pv.loadGen) continue;
        pv.wantKey.clear();
        if (res.ok && res.model && ensureTargetCapped(pv, res.fullKey) && ComputePose(pv, res.model, res.key, res.kind)) {
            pv.dirty = true;
            pv.statusTexSent = false;

            logger::info("[ModelPreview] preview ready: view={} nif='{}' rect=({},{},{},{}) items={}",
                         pv.viewId, JoinNifPathsDisplay(pv.nifPaths), (int)pv.rect.x, (int)pv.rect.y, (int)pv.rect.w,
                         (int)pv.rect.h, res.model->items.size());

            EmitStatus(pv, true, "", "loaded");
        } else { pv.model = nullptr; pv.modelKey.clear();
            logger::warn("[ModelPreview] no drawable preview for '{}'", res.key);
            EmitStatus(pv, false, "mesh not found, unreadable, or no drawable shapes", "error"); }
    }

    results.clear();

    ++g_previewFrame;

    bool any = false;
    for (auto& [k, pv] : g_previews)
        if (pv.model && pv.rtv && (pv.dirty || EffSpin(pv.spin) != 0.0f) && viewIsActive(pv.viewId)) {
            any = true; break;
        }
    if (any) {
        ScopedStateBackup backup(ctx);

        int totalBudget  = kMaxRendersPerFrame;
        int staticBudget = kMaxStaticRendersPerFrame;

        auto renderOne = [&](Preview& pv) {
            pv.dirty = RenderPreviewInner(ctx, pv);
            pv.lastRenderFrame = g_previewFrame;
            if (pv.model && !pv.dirty && !pv.statusTexSent) {
                EmitStatus(pv, true, "", "textured");
                pv.statusTexSent = true;
            }
        };

        g_staticScratch.clear();
        for (auto& [k, pv] : g_previews) {
            if (totalBudget <= 0) break;
            if (!pv.model || !pv.rtv) continue;
            if (EffSpin(pv.spin) != 0.0f) continue;
            if (!pv.dirty) continue;
            if (!viewIsActive(pv.viewId)) continue;
            g_staticScratch.push_back(&pv);
        }
        std::stable_sort(g_staticScratch.begin(), g_staticScratch.end(),
                         [](const Preview* a, const Preview* b) { return a->lastRenderFrame < b->lastRenderFrame; });
        for (Preview* pvp : g_staticScratch) {
            if (totalBudget <= 0 || staticBudget <= 0) break;
            staticBudget -= 1;
            totalBudget -= 1;
            renderOne(*pvp);
        }

        if (totalBudget > 0) {
            g_spinScratch.clear();
            for (auto& [k, pv] : g_previews) {
                if (!pv.model || !pv.rtv) continue;
                if (EffSpin(pv.spin) == 0.0f) continue;
                if (!viewIsActive(pv.viewId)) continue;
                g_spinScratch.push_back(&pv);
            }
            std::sort(g_spinScratch.begin(), g_spinScratch.end(),
                      [](const Preview* a, const Preview* b) { return a->lastRenderFrame < b->lastRenderFrame; });
            for (Preview* pvp : g_spinScratch) {
                if (totalBudget <= 0) break;
                --totalBudget;
                renderOne(*pvp);
            }
        }
    }
}

static Microsoft::WRL::ComPtr<IDXGIAdapter3> g_vramAdapter;

static IDXGIAdapter3* VramAdapter() {
    if (g_vramAdapter) return g_vramAdapter.Get();
    if (!g_dev) return nullptr;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDev;
    if (FAILED(g_dev->QueryInterface(IID_PPV_ARGS(&dxgiDev)))) return nullptr;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDev->GetAdapter(&adapter))) return nullptr;
    if (FAILED(adapter.As(&g_vramAdapter))) return nullptr;
    return g_vramAdapter.Get();
}

static std::atomic<bool>        g_sampleRequested{ false };
static std::atomic<const char*> g_sampleReason{ nullptr };
static std::atomic<bool>        g_burstPending{ false };
static std::atomic<long long>   g_lastViewCreateMs{ 0 };

constexpr long long kBurstQuietMs = 750;

static long long SteadyNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

void RequestMemorySample(const char* a_reason) {
    g_sampleReason.store(a_reason ? a_reason : "unspecified");
    g_sampleRequested.store(true);
}

void NoteViewCreated() {
    g_lastViewCreateMs.store(SteadyNowMs());
    g_burstPending.store(true);
    RequestMemorySample("view-created");
}

static void LogMemoryTelemetry() {
    using clock = std::chrono::steady_clock;
    static clock::time_point s_last{};
    const auto now = clock::now();

    const char* trigger = "timer";
    bool forced = false;
    if (g_burstPending.load() && SteadyNowMs() - g_lastViewCreateMs.load() >= kBurstQuietMs) {
        g_burstPending.store(false);
        forced = true;
        trigger = "view-burst-complete";
    } else if (g_sampleRequested.exchange(false)) {
        forced = true;
        const char* r = g_sampleReason.load();
        trigger = r ? r : "requested";
    }

    if (!forced && s_last != clock::time_point{} && now - s_last < std::chrono::seconds(30)) return;
    s_last = now;

    DXGI_QUERY_VIDEO_MEMORY_INFO vmi{};
    bool gotVram = false;
    if (auto* adapter = VramAdapter())
        gotVram = SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &vmi));

    constexpr std::uint64_t kMB = 1024ull * 1024ull;
    const std::uint64_t vramUsedMB   = gotVram ? vmi.CurrentUsage / kMB : 0;
    const std::uint64_t vramBudgetMB = gotVram ? vmi.Budget / kMB : 0;
    const std::uint64_t vramResMB    = gotVram ? vmi.CurrentReservation / kMB : 0;
    const std::int64_t  vramHeadMB   = static_cast<std::int64_t>(vramBudgetMB) -
                                       static_cast<std::int64_t>(vramUsedMB);

    const std::string vramField =
        gotVram
            ? std::string("vramLocal used=") + std::to_string(vramUsedMB) +
              " MB budget=" + std::to_string(vramBudgetMB) +
              " MB reserved=" + std::to_string(vramResMB) +
              " MB headroom=" + std::to_string(vramHeadMB) +
              " MB" + (vramHeadMB < 0 ? " OVER-BUDGET" : "")
            : std::string("vramLocal unavailable");

    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    const bool gotProc = GetProcessMemoryInfo(GetCurrentProcess(),
                                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                                              sizeof(pmc)) != FALSE;

    std::size_t modelBytes = 0, modelEntries = 0;
    {
        std::lock_guard<std::mutex> lock(g_lruMutex);
        modelBytes   = g_lruBytes;
        modelEntries = g_lru.size();
    }

    std::size_t texInFlight = 0, texCached = 0;
    {
        std::lock_guard<std::mutex> lock(g_texMutex);
        texInFlight = g_texInFlight.size();
        texCached   = g_texDone.size();
    }

    logger::info("[ModelPreview][mem] trigger={} | process private={} MB working={} MB "
                 "| modelCache={} MB ({} entries) "

                 "| previewEntries={} liveRT={}/{} pooledRT={} evicted={} (rt {}x{}) "
                 "| textures: engine-owned, requested={} resolved={} inFlight={} cached={}{} "
                 "| {}",
                 trigger,
                 gotProc ? pmc.PrivateUsage / (1024 * 1024) : 0,
                 gotProc ? pmc.WorkingSetSize / (1024 * 1024) : 0,
                 modelBytes / (1024 * 1024), modelEntries,
                 g_previews.size(), g_rtLive, g_cfg.maxLivePreviews, g_rtFreeList.size(), g_evictions,
                 g_cfg.rtSize, g_cfg.rtSize,
                 g_texRequested.load(), g_texResolved.load(), texInFlight, texCached,
                 g_texFailClosed.load() ? " FAIL-CLOSED" : "",
                 vramField);
}

void TickCore(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
    if (!Enabled() || !dev || !ctx) return;
    MarkTickCoreSeen();
    int queuedJobs = 0;
    int workerCount = 0;
    {
        std::lock_guard<std::mutex> lock(g_jobMutex);
        for (const auto& [view, jobs] : g_jobsByView) queuedJobs += static_cast<int>(jobs.size());
        workerCount = g_workerStarted ? static_cast<int>(kWorkerCount) : 0;
    }
    FreezeDiagnostics::SetModelPreviewState(queuedJobs, ReadQueueDepth(), WorkersWaitingRead(), workerCount);
    FreezeDiagnostics::ScopedStage tickStage(FreezeDiagnostics::Lane::ModelPreviewServicing,
                                             FreezeDiagnostics::Stage::ModelPreviewTick);
    try {
        RequestEngineReadService();
        ServiceTextureWatchdog();
        TickCoreImpl(dev, ctx);
        RequestEngineReadService();
        LogMemoryTelemetry();
    } catch (const std::exception& e) {
        logger::error("[ModelPreview] TickCore exception: {}", e.what());
    } catch (...) {
        logger::error("[ModelPreview] TickCore unknown exception");
    }
}

void Shutdown() {
    static std::atomic<bool> done{false};
    if (done.exchange(true)) return;

    BeginReadShutdown();
    ClearMissingReadPaths();
    ServiceEngineReadsOnGameThread();
    {
        std::lock_guard<std::mutex> lock(g_jobMutex);
        g_workerExit.store(true);
        g_jobsByView.clear();
    }
    g_jobCv.notify_all();
    for (auto& worker : g_workers) if (worker.joinable()) worker.join();
    {
        std::lock_guard<std::mutex> lock(g_jobMutex);
        g_workers.clear();
        g_workerStarted = false;
    }
    for (auto it = g_previews.begin(); it != g_previews.end();) ErasePreview(it);
    g_overlayCache.clear();
    g_overlayCacheFrame = ~uint64_t{0};
    g_rtFreeList.clear();
    g_rtLive = 0;
    { std::lock_guard<std::mutex> lock(g_lruMutex); g_lru.clear(); g_missing.clear(); g_lruBytes = 0; }

    g_texShutdown.store(true, std::memory_order_release);
    { std::lock_guard<std::mutex> lock(g_texMutex); g_texDone.clear(); g_texOrder.clear(); }
    { std::lock_guard<std::mutex> lock(g_resultMutex); g_results.clear(); }
    g_layouts.clear();
    for (auto& vs : g_vs) vs.Reset();
    for (auto& b : g_vsBlob) b.Reset();
    g_ps.Reset(); g_graySrv.Reset(); g_blendState.Reset(); g_depthState.Reset();
    g_blendAlpha.Reset(); g_blendAdditive.Reset(); g_depthStateNoWrite.Reset();
    g_rasterizer.Reset(); g_sampler.Reset(); g_cb.Reset(); g_dsv.Reset(); g_depthTex.Reset();
    g_d3dReady = false; g_dev = nullptr; g_devForWorker.store(nullptr);
    g_vramAdapter.Reset();
    logger::info("[ModelPreview] shutdown complete");
}

namespace {
struct WorkerExitBackstop {
    ~WorkerExitBackstop() {
        g_workerExit.store(true);
        BeginReadShutdown();
        g_jobCv.notify_all();
        for (auto& worker : g_workers) {
            if (worker.joinable()) worker.join();
        }
    }
};
WorkerExitBackstop g_workerExitBackstop;
}

}
