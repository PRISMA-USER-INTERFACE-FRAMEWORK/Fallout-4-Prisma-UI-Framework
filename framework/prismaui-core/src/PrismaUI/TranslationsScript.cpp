#include "TranslationsScript.h"

namespace {

    std::string EscapeJavaScriptV3(const std::string& value) {
        std::string result;
        result.reserve(value.size() + 8);
        for (unsigned char c : value) {
            if (c == '\\') {
                result += "\\\\";
            } else if (c == '"') {
                result += "\\\"";
            } else if (c == '\n') {
                result += "\\n";
            } else if (c == '\r') {
                result += "\\r";
            } else {
                result += static_cast<char>(c);
            }
        }
        return result;
    }

}

namespace PrismaUI::Translations {

    std::string BuildL10NScript(const std::unordered_map<std::string, std::string>& translations) {
        if (translations.empty()) {
            return "";
        }

        std::string script;
        script.reserve(translations.size() * 64);
        script += "window.L10N={";

        bool first = true;
        for (const auto& kv : translations) {
            if (!first) {
                script += ',';
            }
            first = false;
            script += '"';
            script += EscapeJavaScriptV3(kv.first);
            script += "\":\"";
            script += EscapeJavaScriptV3(kv.second);
            script += '"';
        }

        script += "};window.t=function(k){return window.L10N[k]!==undefined?window.L10N[k]:k;};";
        return script;
    }

    std::string EscapeJavaScript(const std::string& value) {
        std::string result;
        result.reserve(value.size() + 8);
        for (unsigned char c : value) {
            if (c == '\\') {
                result += "\\\\";
            } else if (c == '"') {
                result += "\\\"";
            } else if (c == '\n') {
                result += "\\n";
            } else if (c == '\r') {
                result += "\\r";
            } else if (c < 0x20) {
                static constexpr char hex[] = "0123456789abcdef";
                result += "\\u00";
                result.push_back(hex[c >> 4]);
                result.push_back(hex[c & 0x0F]);
            } else {
                result += static_cast<char>(c);
            }
        }
        return result;
    }

    std::string BuildV4L10NScript(const std::map<std::string, std::string>& translations,
                                  const std::string& locale) {
        if (translations.empty()) return "";

        std::string script = "window.PrismaL10N={locale:\"";
        script += EscapeJavaScript(locale);
        script += "\",messages:Object.create(null)};";
        for (const auto& [key, value] : translations) {
            script += "window.PrismaL10N.messages[\"";
            script += EscapeJavaScript(key);
            script += "\"] = \"";
            script += EscapeJavaScript(value);
            script += "\";";
        }
        script += "window.PrismaL10N.t=function(k,vars){var v=window.PrismaL10N.messages[k];"
                  "if(typeof v!==\"string\")return k;"
                  "return v.replace(/\\{\\{([^{}]+)\\}\\}/g,function(_,name){"
                  "return vars&&Object.prototype.hasOwnProperty.call(vars,name)?String(vars[name]):\"{{\"+name+\"}}\";"
                  "});};";
        return script;
    }

}
