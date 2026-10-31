#include "TranslationsText.h"
#include "TranslationsUtf8.h"

#include <cstdint>
#include <cstddef>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace PrismaUI::Translations {
namespace {

    std::string Utf16LeToUtf8(std::string_view raw) {
#ifdef _WIN32
        std::wstring wide;
        wide.reserve((raw.size() - 2) / sizeof(wchar_t));
        for (size_t i = 2; i + 1 < raw.size(); i += 2) {
            wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(raw[i]) |
                                                (static_cast<uint16_t>(static_cast<unsigned char>(raw[i + 1])) << 8)));
        }
        if (wide.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0,
                                             nullptr, nullptr);
        if (size <= 0) return {};
        std::string out(static_cast<size_t>(size), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), size, nullptr,
                                nullptr) <= 0) {
            return {};
        }
        return out;
#else
        std::string out;
        out.reserve(raw.size());
        for (size_t i = 2; i + 1 < raw.size(); i += 2) {
            uint32_t codePoint = static_cast<unsigned char>(raw[i]) |
                                  (static_cast<uint32_t>(static_cast<unsigned char>(raw[i + 1])) << 8);
            if (codePoint >= 0xD800 && codePoint <= 0xDBFF && i + 3 < raw.size()) {
                const uint32_t low = static_cast<unsigned char>(raw[i + 2]) |
                                     (static_cast<uint32_t>(static_cast<unsigned char>(raw[i + 3])) << 8);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    i += 2;
                }
            }
            AppendUtf8(out, codePoint);
        }
        return out;
#endif
    }

    void ParseLines(std::string_view text, std::unordered_map<std::string, std::string>& result) {
        size_t start = 0;
        while (start <= text.size()) {
            const size_t end = text.find('\n', start);
            std::string line(text.substr(start, end == std::string_view::npos ? text.size() - start : end - start));
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty() && line[0] == '$') {
                const size_t tab = line.find('\t');
                if (tab != std::string::npos) result[line.substr(0, tab)] = line.substr(tab + 1);
            }
            if (end == std::string_view::npos) break;
            start = end + 1;
        }
    }

}

void ParseBethesdaTranslation(std::string_view raw, std::unordered_map<std::string, std::string>& result) {
    result.clear();
    if (raw.size() < 2) return;
    if (static_cast<unsigned char>(raw[0]) == 0xFF && static_cast<unsigned char>(raw[1]) == 0xFE) {
        ParseLines(Utf16LeToUtf8(raw), result);
        return;
    }
    ParseLines(raw, result);
}

}
