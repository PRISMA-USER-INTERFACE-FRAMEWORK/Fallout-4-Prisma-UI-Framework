#include "SystemClipboard.h"

#include <Ultralight/Ultralight.h>

#include <windows.h>

#include <cwchar>

namespace PrismaUI::SystemClipboard {
    namespace {
        std::wstring ReadText(std::size_t maxChars) {
            if (!::OpenClipboard(nullptr)) return {};
            std::wstring out;
            if (HANDLE data = ::GetClipboardData(CF_UNICODETEXT)) {
                if (const auto* text = static_cast<const wchar_t*>(::GlobalLock(data))) {
                    const std::size_t capacity = ::GlobalSize(data) / sizeof(wchar_t);
                    out.assign(text, ::wcsnlen(text, capacity < maxChars ? capacity : maxChars));
                    ::GlobalUnlock(data);
                }
            }
            ::CloseClipboard();
            if (out.size() == maxChars && out.back() >= 0xD800 && out.back() <= 0xDBFF) out.pop_back();
            return out;
        }

        class Clipboard final : public ultralight::Clipboard {
        public:
            void Clear() override {}

            ultralight::String ReadPlainText() override {
                const std::wstring text = ReadText(kMaxReadChars);
                return ultralight::String(reinterpret_cast<const ultralight::Char16*>(text.data()), text.size());
            }

            void WritePlainText(const ultralight::String&) override {}
        };
    }

    std::string ReadTextUtf8(std::size_t maxChars) {
        const std::wstring text = ReadText(maxChars);
        if (text.empty()) return {};
        const int length = static_cast<int>(text.size());
        const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string out(static_cast<std::size_t>(size), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, text.data(), length, out.data(), size, nullptr, nullptr);
        return out;
    }

    ultralight::Clipboard& UltralightClipboard() {
        static Clipboard clipboard;
        return clipboard;
    }
}
