#pragma once

#include <string>
#include <string_view>

namespace PrismaUI::InvokeResult {

inline std::string Error(std::string_view message) {
    std::string result{"{\"error\":\""};
    result.reserve(result.size() + message.size() + 3);
    for (const unsigned char ch : message) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (ch < 0x20) {
                static constexpr char hex[] = "0123456789abcdef";
                result += "\\u00";
                result += hex[ch >> 4];
                result += hex[ch & 0x0f];
            } else {
                result += static_cast<char>(ch);
            }
        }
    }
    result += "\"}";
    return result;
}

}
