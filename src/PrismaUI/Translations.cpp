#include "Translations.h"

#include "TranslationsJson.h"
#include "TranslationsScript.h"
#include "TranslationsText.h"

#include <array>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>
#include <windows.h>

namespace PrismaUI::Translations {

    namespace {

        constexpr size_t kMaximumTranslationBytes = 1u << 20;

        bool DrainTranslationStream(RE::BSResourceNiBinaryStream& stream, std::string& result) {
            if (!stream) return false;

            result.clear();
            std::array<char, 8192> buffer{};
            for (;;) {
                const size_t bytes = stream.DoRead(buffer.data(), buffer.size());
                if (bytes == 0) return !result.empty();
                if (bytes > kMaximumTranslationBytes - result.size()) {
                    result.clear();
                    return false;
                }
                result.append(buffer.data(), bytes);
            }
        }

        bool ReadTranslationResource(const std::string& resourcePath, std::string& result) {
#ifdef PRISMAUI_FO4VR
            RE::BSResourceNiBinaryStream stream(resourcePath.c_str());
#else
            RE::BSResourceNiBinaryStream stream(resourcePath.c_str(), false, nullptr, true);
#endif
            return DrainTranslationStream(stream, result);
        }

    }

    std::string DetectGameLanguage() {
        logger::debug("Translations::DetectGameLanguage: scanning INI files");
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);

        std::string path(exePath);
        auto slashPos = path.rfind('\\');
        if (slashPos == std::string::npos) {
            return FALLBACK_LANG;
        }
        std::string gameDir = path.substr(0, slashPos);

        std::vector<std::string> candidates;

        char appData[MAX_PATH] = {};
        if (GetEnvironmentVariableA("USERPROFILE", appData, MAX_PATH)) {
            std::string myGames = std::string(appData) + "\\Documents\\My Games\\Fallout4\\";
            candidates.push_back(myGames + "Fallout4Custom.ini");
            candidates.push_back(myGames + "Fallout4Prefs.ini");
        }
        candidates.push_back(gameDir + "\\Fallout4.ini");

        for (const auto& iniPath : candidates) {
            char langBuf[64] = {};
            DWORD result = GetPrivateProfileStringA("General", "sLanguage", "", langBuf, sizeof(langBuf),
                                                    iniPath.c_str());
            if (result > 0) {
                std::string lang(langBuf);
                for (auto& c : lang) {
                    c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                }
                if (!lang.empty()) {
                    logger::info("Translations::DetectGameLanguage: '{}' from {}", lang, iniPath);
                    return lang;
                }
            } else {
                logger::debug("Translations::DetectGameLanguage: no sLanguage in {}", iniPath);
            }
        }

        logger::warn("Translations::DetectGameLanguage: no language found in any INI, falling back to '{}'",
                     FALLBACK_LANG);
        return FALLBACK_LANG;
    }

    std::unordered_map<std::string, std::string> ParseTranslationFile(const std::string& pluginName,
                                                                       const std::string& lang) {
        std::unordered_map<std::string, std::string> result;

        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string path(exePath);
        auto slashPos = path.rfind('\\');
        if (slashPos == std::string::npos) {
            logger::error("Translations::ParseTranslationFile: cannot determine game directory from '{}'", exePath);
            return result;
        }
        std::string dataDir = path.substr(0, slashPos) + "\\Data";
        std::string filePath = dataDir + "\\Interface\\Translations\\" + pluginName + "_" + lang + ".txt";
        logger::debug("Translations::ParseTranslationFile: trying '{}'", filePath);

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            if (lang != FALLBACK_LANG) {
                std::string fallbackPath =
                    dataDir + "\\Interface\\Translations\\" + pluginName + "_" + FALLBACK_LANG + ".txt";
                logger::warn("Translations::ParseTranslationFile: '{}' not found, trying fallback '{}'",
                             filePath, fallbackPath);
                file.open(fallbackPath, std::ios::binary);
                if (!file.is_open()) {
                    logger::warn("Translations::ParseTranslationFile: fallback '{}' not found either, no translations loaded",
                                 fallbackPath);
                    return result;
                }
                logger::info("Translations::ParseTranslationFile: using fallback '{}'", fallbackPath);
            } else {
                logger::warn("Translations::ParseTranslationFile: '{}' not found, no translations loaded", filePath);
                return result;
            }
        } else {
            logger::info("Translations::ParseTranslationFile: opened '{}'", filePath);
        }

        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        if (size < 2) {
            return result;
        }

        std::string raw(static_cast<size_t>(size), '\0');
        file.read(raw.data(), size);
        file.close();

        const bool utf16 = static_cast<unsigned char>(raw[0]) == 0xFF &&
                           static_cast<unsigned char>(raw[1]) == 0xFE;
        ParseBethesdaTranslation(raw, result);
        logger::info("Translations::ParseTranslationFile: loaded {} entries ({}) for '{}'",
                     result.size(), utf16 ? "UTF-16 LE" : "UTF-8", pluginName);
        return result;
    }

    std::map<std::string, std::string> ParseJsonTranslationFile(const std::string& pluginName,
                                                                 const std::string& lang) {
        std::map<std::string, std::string> result;

        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::string path(exePath);
        auto slashPos = path.rfind('\\');
        if (slashPos == std::string::npos) return result;
        std::string dataDir = path.substr(0, slashPos) + "\\Data";
        const auto loadLocale = [&](const std::string& locale, std::map<std::string, std::string>& loaded) {
            const std::string filePath =
                dataDir + "\\Interface\\Translations\\" + pluginName + "_" + locale + ".json";
            const std::string resourcePath =
                "Interface/Translations/" + pluginName + "_" + locale + ".json";
            std::string json;
            if (!ReadTranslationResource(resourcePath, json)) return false;
            const size_t bom = json.starts_with("\xEF\xBB\xBF") ? 3 : 0;
            if (!ParseJsonLocale(json.substr(bom), loaded)) {
                logger::warn("Translations::ParseJsonTranslationFile: malformed JSON '{}'", filePath);
                return false;
            }
            return true;
        };

        std::vector<std::string> locales{FALLBACK_LANG};
        if (lang != FALLBACK_LANG) {
            const size_t separator = lang.find_first_of("-_");
            if (separator != std::string::npos && separator > 0) {
                const std::string base = lang.substr(0, separator);
                if (base != FALLBACK_LANG) locales.push_back(base);
            }
            locales.push_back(lang);
        }
        for (const auto& locale : locales) {
            std::map<std::string, std::string> loaded;
            if (loadLocale(locale, loaded)) MergeJsonLocale(loaded, result);
        }
        return result;
    }

}
