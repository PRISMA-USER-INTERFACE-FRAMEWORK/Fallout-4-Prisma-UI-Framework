#include "ModelPreviewInternal.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace PrismaUI::ModelPreview {

std::string MakeFullKey(ViewId v, const std::string& id) { return std::to_string(v) + "\x1f" + id; }

static bool JsonFind(const std::string& j, const char* key, size_t& pos) {
    std::string needle = std::string("\"") + key + "\"";
    size_t p = j.find(needle); if (p == std::string::npos) return false;
    p = j.find(':', p + needle.size()); if (p == std::string::npos) return false;
    ++p; while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == '\n' || j[p] == '\r')) ++p;
    if (p >= j.size()) return false; pos = p; return true;
}
std::string JsonStr(const std::string& j, const char* key) {
    size_t p; if (!JsonFind(j, key, p) || j[p] != '"') return "";
    size_t e = j.find('"', p + 1); if (e == std::string::npos) return "";
    return j.substr(p + 1, e - p - 1);
}
double JsonNum(const std::string& j, const char* key, double fb) {
    size_t p; if (!JsonFind(j, key, p)) return fb; return atof(j.c_str() + p);
}

bool JsonBool(const std::string& j, const char* key) {
    size_t p; return JsonFind(j, key, p) && j.compare(p, 4, "true") == 0;
}

std::vector<std::string> JsonStrArray(const std::string& j, const char* key) {
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

void EmitStatus(const Preview& pv, bool ok, const std::string& error, const char* phase) {
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
    j += "]";
    if (!pv.image.url.empty()) { j += ",\"imgsrc\":\""; JsonEscInto(j, pv.image.url); j += '"'; }
    j += "}";
    g_statusSink(pv.viewId, j.c_str());
}

std::unordered_map<std::string, std::string> ParseSwapMap(const std::string& s) {
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

}
