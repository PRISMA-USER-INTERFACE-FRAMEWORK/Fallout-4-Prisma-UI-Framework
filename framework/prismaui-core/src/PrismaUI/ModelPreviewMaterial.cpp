#include "ModelPreviewInternal.h"

#include <cstring>
#include <string>
#include <vector>

namespace PrismaUI::ModelPreview {

std::string JoinNifPathsDisplay(const std::vector<std::string>& paths) {
    std::string s;
    for (size_t i = 0; i < paths.size(); ++i) { if (i) s += ", "; s += paths[i]; }
    return s;
}

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

bool LoadMaterialTextures(const std::string& matPath, MaterialTextures& out) {
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

}
