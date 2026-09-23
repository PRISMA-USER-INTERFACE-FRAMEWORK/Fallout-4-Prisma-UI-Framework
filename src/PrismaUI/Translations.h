#pragma once

#include <string>
#include <map>
#include <unordered_map>

#include "TranslationsScript.h"

namespace PrismaUI::Translations {

    constexpr const char* FALLBACK_LANG = "en";

    std::string DetectGameLanguage();

    std::unordered_map<std::string, std::string> ParseTranslationFile(const std::string& pluginName,
                                                                       const std::string& lang);

    std::string BuildL10NScript(const std::unordered_map<std::string, std::string>& translations);

    std::map<std::string, std::string> ParseJsonTranslationFile(const std::string& pluginName,
                                                                 const std::string& lang);

}
