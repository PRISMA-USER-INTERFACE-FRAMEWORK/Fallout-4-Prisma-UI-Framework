#pragma once

#include <map>
#include <string>
#include <unordered_map>

namespace PrismaUI::Translations {

    std::string BuildL10NScript(const std::unordered_map<std::string, std::string>& translations);
    std::string EscapeJavaScript(const std::string& value);
    std::string BuildV4L10NScript(const std::map<std::string, std::string>& translations,
                                  const std::string& locale);

}
