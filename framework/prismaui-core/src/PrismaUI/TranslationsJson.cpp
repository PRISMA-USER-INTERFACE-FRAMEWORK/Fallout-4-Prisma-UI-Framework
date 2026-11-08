#include "TranslationsJson.h"
#include "TranslationsUtf8.h"

#include <string_view>
#include <utility>

namespace PrismaUI::Translations {
namespace {

    constexpr size_t kMaximumJsonBytes = 1u << 20;
    constexpr size_t kMaximumNestingDepth = 32;
    constexpr size_t kMaximumMessages = 10000;
    constexpr size_t kMaximumKeyBytes = 1024;
    constexpr size_t kMaximumValueBytes = 64u << 10;

    bool IsValidUtf8(std::string_view value) {
        for (size_t i = 0; i < value.size();) {
            const unsigned char lead = static_cast<unsigned char>(value[i++]);
            if (lead <= 0x7F) continue;

            size_t continuationCount = 0;
            unsigned char secondMin = 0x80;
            unsigned char secondMax = 0xBF;
            if (lead >= 0xC2 && lead <= 0xDF) {
                continuationCount = 1;
            } else if (lead == 0xE0) {
                continuationCount = 2;
                secondMin = 0xA0;
            } else if (lead >= 0xE1 && lead <= 0xEC) {
                continuationCount = 2;
            } else if (lead == 0xED) {
                continuationCount = 2;
                secondMax = 0x9F;
            } else if (lead >= 0xEE && lead <= 0xEF) {
                continuationCount = 2;
            } else if (lead == 0xF0) {
                continuationCount = 3;
                secondMin = 0x90;
            } else if (lead >= 0xF1 && lead <= 0xF3) {
                continuationCount = 3;
            } else if (lead == 0xF4) {
                continuationCount = 3;
                secondMax = 0x8F;
            } else {
                return false;
            }

            if (i + continuationCount > value.size()) return false;
            const unsigned char second = static_cast<unsigned char>(value[i++]);
            if (second < secondMin || second > secondMax) return false;
            for (size_t j = 1; j < continuationCount; ++j) {
                const unsigned char continuation = static_cast<unsigned char>(value[i++]);
                if (continuation < 0x80 || continuation > 0xBF) return false;
            }
        }
        return true;
    }

    class Parser {
    public:
        explicit Parser(std::string_view input) : input_(input) {}

        bool Parse(std::map<std::string, std::string>& result) {
            if (input_.size() > kMaximumJsonBytes) return false;
            SkipWhitespace();
            if (!ParseObject({}, result, 0)) return false;
            SkipWhitespace();
            return position_ == input_.size();
        }

    private:
        void SkipWhitespace() {
            while (position_ < input_.size() && (input_[position_] == ' ' || input_[position_] == '\t' ||
                                                  input_[position_] == '\r' || input_[position_] == '\n')) {
                ++position_;
            }
        }

        bool ParseHex4(uint32_t& value) {
            if (position_ + 4 > input_.size()) return false;
            value = 0;
            for (size_t i = 0; i < 4; ++i) {
                const char c = input_[position_++];
                value <<= 4;
                if (c >= '0' && c <= '9') value |= static_cast<uint32_t>(c - '0');
                else if (c >= 'a' && c <= 'f') value |= static_cast<uint32_t>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') value |= static_cast<uint32_t>(c - 'A' + 10);
                else return false;
            }
            return true;
        }

        bool ParseString(std::string& result) {
            if (position_ >= input_.size() || input_[position_++] != '"') return false;
            result.clear();
            while (position_ < input_.size()) {
                const unsigned char c = static_cast<unsigned char>(input_[position_++]);
                if (c == '"') return true;
                if (c < 0x20) return false;
                if (c != '\\') {
                    result.push_back(static_cast<char>(c));
                    continue;
                }
                if (position_ >= input_.size()) return false;
                const char escape = input_[position_++];
                switch (escape) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case 'u': {
                    uint32_t codePoint = 0;
                    if (!ParseHex4(codePoint)) return false;
                    if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                        if (position_ + 6 > input_.size() || input_[position_] != '\\' ||
                            input_[position_ + 1] != 'u') return false;
                        position_ += 2;
                        uint32_t low = 0;
                        if (!ParseHex4(low) || low < 0xDC00 || low > 0xDFFF) return false;
                        codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
                        return false;
                    }
                    AppendUtf8(result, codePoint);
                    break;
                }
                default: return false;
                }
            }
            return false;
        }

        bool ParseObject(const std::string& prefix, std::map<std::string, std::string>& result,
                         size_t depth) {
            if (depth > kMaximumNestingDepth) return false;
            SkipWhitespace();
            if (position_ >= input_.size() || input_[position_++] != '{') return false;
            SkipWhitespace();
            if (position_ < input_.size() && input_[position_] == '}') {
                ++position_;
                return true;
            }
            while (position_ < input_.size()) {
                std::string key;
                if (!ParseString(key)) return false;
                if (key.size() > kMaximumKeyBytes || !IsValidUtf8(key)) return false;
                const std::string path = prefix.empty() ? key : prefix + "." + key;
                if (path.size() > kMaximumKeyBytes) return false;
                SkipWhitespace();
                if (position_ >= input_.size() || input_[position_++] != ':') return false;
                SkipWhitespace();
                if (position_ < input_.size() && input_[position_] == '{') {
                    if (!ParseObject(path, result, depth + 1)) return false;
                } else {
                    std::string value;
                    if (!ParseString(value) || value.size() > kMaximumValueBytes || !IsValidUtf8(value) ||
                        result.size() >= kMaximumMessages || !result.emplace(path, std::move(value)).second) {
                        return false;
                    }
                }
                SkipWhitespace();
                if (position_ >= input_.size()) return false;
                if (input_[position_] == '}') {
                    ++position_;
                    return true;
                }
                if (input_[position_++] != ',') return false;
                SkipWhitespace();
            }
            return false;
        }

        std::string_view input_;
        size_t position_ = 0;
    };

}

bool ParseJsonLocale(const std::string& json, std::map<std::string, std::string>& result) {
    result.clear();
    if (!Parser(json).Parse(result)) {
        result.clear();
        return false;
    }
    return true;
}

void MergeJsonLocale(const std::map<std::string, std::string>& locale,
                     std::map<std::string, std::string>& result) {
    for (const auto& [key, value] : locale) result[key] = value;
}

}
