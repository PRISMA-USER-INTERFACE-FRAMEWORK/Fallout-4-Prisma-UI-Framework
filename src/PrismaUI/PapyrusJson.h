#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace PrismaUI::PapyrusJson {

inline bool IsSafeCallbackId(std::string_view id) {
    if (id.empty() || id.size() > 16) return false;
    for (const unsigned char c : id) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

inline std::string JsStringLiteral(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    out.push_back('"');
    for (unsigned char uc : s) {
        switch (uc) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\'': out += "\\'"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '<':  out += "\\u003c"; break; 
            default:
                if (uc < 0x20) {
                    static constexpr char hex[] = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[uc >> 4]);
                    out.push_back(hex[uc & 0x0f]);
                } else {
                    out.push_back(static_cast<char>(uc));
                }
                break;
        }
    }
    out.push_back('"');
    return out;
}

inline std::string PluginStem(std::string name) {
    auto slash = name.find_last_of("\\/");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    auto dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return name;
}

inline bool IsVanillaMaster(std::string_view esp) {
    std::string lower(esp);
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower == "fallout4.esm" || lower == "dlcrobot.esm" || lower == "dlcworkshop01.esm" ||
           lower == "dlccoast.esm" || lower == "dlcworkshop02.esm" || lower == "dlcworkshop03.esm" ||
           lower == "dlcnukaworld.esm";
}

inline bool EspAllowedForView(std::string_view owner, std::string_view esp, bool writing) {
    if (esp.empty()) return false;
    if (PluginStem(std::string(owner)) == PluginStem(std::string(esp))) return true;
    if (!writing && IsVanillaMaster(esp)) return true;
    return false;
}

struct Object {
    std::map<std::string, std::string> strings;
    std::map<std::string, double> numbers;
};

namespace detail {

inline void SkipWs(std::string_view j, std::size_t& i) {
    while (i < j.size() && std::isspace(static_cast<unsigned char>(j[i]))) ++i;
}

inline bool ParseString(std::string_view j, std::size_t& i, std::string& out) {
    if (i >= j.size() || j[i] != '"') return false;
    ++i;
    out.clear();
    while (i < j.size()) {
        const char c = j[i++];
        if (c == '"') return true;
        if (c == '\\') {
            if (i >= j.size()) return false;
            const char n = j[i++];
            switch (n) {
                case '"':
                case '\\':
                case '/': out.push_back(n); break;
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'u': {
                    if (i + 4 > j.size()) return false;
                    unsigned code = 0;
                    for (int k = 0; k < 4; ++k) {
                        const char h = j[i++];
                        code <<= 4;
                        if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
                        else return false;
                    }
                    if (code < 0x80) out.push_back(static_cast<char>(code));
                    else if (code < 0x800) {
                        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    } else {
                        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                    }
                    break;
                }
                default: return false;
            }
        } else if (static_cast<unsigned char>(c) < 0x20) {
            return false;
        } else {
            out.push_back(c);
        }
    }
    return false;
}

inline bool ParseNumber(std::string_view j, std::size_t& i, double& out) {
    const std::size_t start = i;
    if (i < j.size() && (j[i] == '-' || j[i] == '+')) ++i;
    bool digits = false;
    while (i < j.size() && std::isdigit(static_cast<unsigned char>(j[i]))) {
        digits = true;
        ++i;
    }
    if (i < j.size() && j[i] == '.') {
        ++i;
        while (i < j.size() && std::isdigit(static_cast<unsigned char>(j[i]))) {
            digits = true;
            ++i;
        }
    }
    if (!digits) return false;
    try {
        out = std::stod(std::string(j.substr(start, i - start)));
    } catch (...) {
        return false;
    }
    if (!std::isfinite(out)) return false;
    return true;
}

}

inline bool ParseFlatObject(std::string_view json, Object& out) {
    out = {};
    std::size_t i = 0;
    detail::SkipWs(json, i);
    if (i >= json.size() || json[i] != '{') return false;
    ++i;
    for (;;) {
        detail::SkipWs(json, i);
        if (i >= json.size()) return false;
        if (json[i] == '}') return true;
        std::string key;
        if (!detail::ParseString(json, i, key)) return false;
        detail::SkipWs(json, i);
        if (i >= json.size() || json[i] != ':') return false;
        ++i;
        detail::SkipWs(json, i);
        if (i >= json.size()) return false;
        if (json[i] == '"') {
            std::string value;
            if (!detail::ParseString(json, i, value)) return false;
            out.strings.emplace(std::move(key), std::move(value));
        } else if (json[i] == 't' && json.substr(i, 4) == "true") {
            i += 4;
            out.numbers.emplace(std::move(key), 1.0);
        } else if (json[i] == 'f' && json.substr(i, 5) == "false") {
            i += 5;
            out.numbers.emplace(std::move(key), 0.0);
        } else if (json[i] == 'n' && json.substr(i, 4) == "null") {
            i += 4;
        } else if (json[i] == '-' || json[i] == '+' || std::isdigit(static_cast<unsigned char>(json[i]))) {
            double n = 0;
            if (!detail::ParseNumber(json, i, n)) return false;
            out.numbers.emplace(std::move(key), n);
        } else if (json[i] == '{' || json[i] == '[') {
            const char open = json[i];
            const char close = open == '{' ? '}' : ']';
            int depth = 0;
            bool inStr = false;
            bool esc = false;
            for (; i < json.size(); ++i) {
                const char c = json[i];
                if (inStr) {
                    if (esc) esc = false;
                    else if (c == '\\') esc = true;
                    else if (c == '"') inStr = false;
                    continue;
                }
                if (c == '"') inStr = true;
                else if (c == open) ++depth;
                else if (c == close) {
                    --depth;
                    if (depth == 0) {
                        ++i;
                        break;
                    }
                }
            }
            if (depth != 0) return false;
        } else {
            return false;
        }
        detail::SkipWs(json, i);
        if (i < json.size() && json[i] == ',') {
            ++i;
            continue;
        }
        if (i < json.size() && json[i] == '}') return true;
        return false;
    }
}

inline std::string GetString(const Object& o, std::string_view key) {
    auto it = o.strings.find(std::string(key));
    return it == o.strings.end() ? std::string{} : it->second;
}

inline double GetNumber(const Object& o, std::string_view key) {
    auto it = o.numbers.find(std::string(key));
    return it == o.numbers.end() ? 0.0 : it->second;
}

}
