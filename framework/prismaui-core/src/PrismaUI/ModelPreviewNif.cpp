#include "ModelPreviewInternal.h"

#include <DirectXPackedVector.h>

#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace PrismaUI::ModelPreview {

static uint32_t DescStride(uint64_t d) { return (uint32_t)(d & 0xF) * 4; }
uint32_t DescAttrOffset(uint64_t d, int a) { return (uint32_t)((d >> (4 * a + 2)) & 0x3C); }
bool DescHasFlag(uint64_t d, uint32_t f) { return ((d >> 44) & f) != 0; }

uint32_t DescPosBytes(uint64_t d) {
    uint32_t pos = DescStride(d);
    for (int a = 1; a <= 6; ++a) { uint32_t o = DescAttrOffset(d, a); if (o > 0 && o < pos) pos = o; }
    return pos;
}

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

std::string SocketKey(const std::string& s) {
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

bool ParseNif(const std::vector<uint8_t>& data, NifParse& out, ParseStats& st,
                     const std::unordered_map<std::string, std::string>& swapMap) {
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

}
