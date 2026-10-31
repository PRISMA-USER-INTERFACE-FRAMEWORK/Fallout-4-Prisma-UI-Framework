#include <cassert>
#include <map>
#include <string>

#include "PrismaUI/TranslationsJson.h"

int main() {
    std::map<std::string, std::string> translations;
    assert(PrismaUI::Translations::ParseJsonLocale(
        "{\"title\":\"Ready\",\"fastTravel\":{\"errors\":{\"COMBAT\":\"Blocked\"}},\"escaped\":\"a\\\"b\",\"unicode\":\"\\u00E9\"}",
        translations));
    assert(translations.at("title") == "Ready");
    assert(translations.at("fastTravel.errors.COMBAT") == "Blocked");
    assert(translations.at("escaped") == "a\"b");
    assert(translations.at("unicode") == "\xC3\xA9");
    assert(PrismaUI::Translations::ParseJsonLocale("{\"controls\":\"\\t\\u0000\"}", translations));
    assert(translations.at("controls") == std::string("\t\0", 2));
    assert(PrismaUI::Translations::ParseJsonLocale("{\"emoji\":\"\\uD83D\\uDE00\"}", translations));
    assert(translations.at("emoji") == "\xF0\x9F\x98\x80");

    assert(!PrismaUI::Translations::ParseJsonLocale("{\"broken\":}", translations));
    assert(translations.empty());
    assert(!PrismaUI::Translations::ParseJsonLocale("{\"items\":[]}", translations));
    assert(!PrismaUI::Translations::ParseJsonLocale("{\"a\":\"one\",\"a\":\"two\"}", translations));
    const std::string exactFile = "{\"value\":\"x\"}" + std::string((1u << 20) - 13, ' ');
    assert(exactFile.size() == (1u << 20));
    assert(PrismaUI::Translations::ParseJsonLocale(exactFile, translations));
    assert(translations.at("value") == "x");
    const std::string oversized((1u << 20) + 1, ' ');
    assert(!PrismaUI::Translations::ParseJsonLocale(oversized, translations));
    const std::string exactKey = "{\"" + std::string(1024, 'k') + "\":\"x\"}";
    assert(PrismaUI::Translations::ParseJsonLocale(exactKey, translations));
    assert(translations.size() == 1);
    assert(!PrismaUI::Translations::ParseJsonLocale("{\"bad\":\"\xC3\x28\"}", translations));
    assert(!PrismaUI::Translations::ParseJsonLocale("{\"" + std::string(1025, 'k') + "\":\"x\"}", translations));
    const std::string exactValue = "{\"value\":\"" + std::string(64u << 10, 'x') + "\"}";
    assert(PrismaUI::Translations::ParseJsonLocale(exactValue, translations));
    assert(translations.at("value").size() == (64u << 10));
    assert(!PrismaUI::Translations::ParseJsonLocale("{\"value\":\"" + std::string(65537, 'x') + "\"}", translations));
    std::string exactDeep;
    for (int i = 0; i < 33; ++i) exactDeep += "{\"nested\":";
    exactDeep += "\"value\"";
    for (int i = 0; i < 33; ++i) exactDeep += '}';
    assert(PrismaUI::Translations::ParseJsonLocale(exactDeep, translations));
    assert(translations.size() == 1);
    std::string deep;
    for (int i = 0; i < 34; ++i) deep += "{\"nested\":";
    deep += "\"value\"";
    for (int i = 0; i < 34; ++i) deep += '}';
    assert(!PrismaUI::Translations::ParseJsonLocale(deep, translations));
    const std::string exactPath = "{\"" + std::string(512, 'a') + "\":{" +
                                  "\"" + std::string(511, 'b') + "\":\"x\"}}";
    assert(PrismaUI::Translations::ParseJsonLocale(exactPath, translations));
    assert(translations.at(std::string(512, 'a') + "." + std::string(511, 'b')) == "x");
    const std::string oversizedPath = "{\"" + std::string(512, 'a') + "\":{" +
                                      "\"" + std::string(512, 'b') + "\":\"x\"}}";
    assert(!PrismaUI::Translations::ParseJsonLocale(oversizedPath, translations));
    std::string many = "{";
    for (int i = 0; i < 10001; ++i) {
        if (i != 0) many += ',';
        many += "\"k" + std::to_string(i) + "\":\"v\"";
    }
    many += '}';
    assert(!PrismaUI::Translations::ParseJsonLocale(many, translations));
    std::string exactMessages = "{";
    for (int i = 0; i < 10000; ++i) {
        if (i != 0) exactMessages += ',';
        exactMessages += "\"k" + std::to_string(i) + "\":\"v\"";
    }
    exactMessages += '}';
    assert(PrismaUI::Translations::ParseJsonLocale(exactMessages, translations));
    assert(translations.size() == 10000);
    std::map<std::string, std::string> english{{"menu.play", "Play"}, {"menu.quit", "Quit"}};
    std::map<std::string, std::string> selected{{"menu.play", "Jugar"}};
    PrismaUI::Translations::MergeJsonLocale(english, translations);
    PrismaUI::Translations::MergeJsonLocale(selected, translations);
    assert(translations.at("menu.play") == "Jugar");
    assert(translations.at("menu.quit") == "Quit");
    assert(PrismaUI::Translations::ParseJsonLocale("{}", translations));
    assert(translations.empty());
    return 0;
}
