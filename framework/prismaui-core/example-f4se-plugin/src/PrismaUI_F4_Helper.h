/*
 * PrismaUI_F4_Helper.h — optional utility helpers for PrismaUI_F4 plugin authors.
 *
 * This file is NOT part of the core API guarantee. It is a convenience layer.
 * Drop it into your project alongside PrismaUI_F4_API.h.
 *
 * This copy contains the JSON helpers only.
 * The full helper (including Papyrus event dispatch) ships with PrismaUI_F4
 * and requires the non-OG CommonLibF4.
 */
#pragma once
#include <string>
#include <string_view>

namespace PRISMA_UI_HELPER {

    // -------------------------------------------------------------------------
    // JSON convenience — extract simple values from the JSON string JS sends.
    // These are minimal helpers. Use nlohmann::json for anything complex.
    // -------------------------------------------------------------------------

    // Extract a string value for a key from a flat JSON object.
    // e.g. GetJsonString(R"({"key":"hello"})", "key") -> "hello"
    inline std::string GetJsonString(std::string_view json, std::string_view key)
    {
        std::string search = "\"";
        search += key;
        search += "\":\"";
        auto pos = json.find(search);
        if (pos == std::string_view::npos) return {};
        pos += search.size();
        auto end = json.find('"', pos);
        if (end == std::string_view::npos) return {};
        return std::string(json.substr(pos, end - pos));
    }

    // Extract a numeric value for a key from a flat JSON object.
    // e.g. GetJsonFloat(R"({"value":1.5})", "value") -> 1.5f
    inline float GetJsonFloat(std::string_view json, std::string_view key, float fallback = 0.0f)
    {
        std::string search = "\"";
        search += key;
        search += "\":";
        auto pos = json.find(search);
        if (pos == std::string_view::npos) return fallback;
        pos += search.size();
        try { return std::stof(std::string(json.substr(pos))); }
        catch (...) { return fallback; }
    }

    inline int GetJsonInt(std::string_view json, std::string_view key, int fallback = 0)
    {
        return static_cast<int>(GetJsonFloat(json, key, static_cast<float>(fallback)));
    }

    inline bool GetJsonBool(std::string_view json, std::string_view key, bool fallback = false)
    {
        std::string search = "\"";
        search += key;
        search += "\":";
        auto pos = json.find(search);
        if (pos == std::string_view::npos) return fallback;
        pos += search.size();
        auto sub = json.substr(pos);
        if (sub.starts_with("true"))  return true;
        if (sub.starts_with("false")) return false;
        return fallback;
    }
}
