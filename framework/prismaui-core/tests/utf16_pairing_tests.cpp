#include "PrismaUI/Utf16Pairing.h"

#include <cstdio>

using PrismaUI::Utf16Pairing::Pending;

static int g_failures = 0;

static void expect(bool condition, const char* what)
{
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) ++g_failures;
}

int main()
{
    constexpr char16_t kHigh = u'\xD83D';
    constexpr char16_t kLow = u'\xDE00';
    constexpr char32_t kEmoji = U'\U0001F600';

    {
        Pending pending;
        expect(pending.Feed(u'A') == U'A', "BMP char is forwarded as its own code point");
    }
    {
        Pending pending;
        expect(!pending.Feed(kHigh).has_value(), "high surrogate is held");
        expect(pending.Feed(kLow) == kEmoji, "valid pair emits one supplementary code point");
        expect(pending.Feed(u'B') == U'B', "BMP char after a completed pair is forwarded");
    }
    {
        Pending pending;
        pending.Feed(kHigh);
        expect(pending.Feed(u'A') == U'A', "lone high followed by BMP char drops the high and forwards the BMP");
        expect(!pending.Feed(kLow).has_value(), "high is cleared by the BMP char");
    }
    {
        Pending pending;
        expect(!pending.Feed(kLow).has_value(), "lone low surrogate is dropped");
        expect(pending.Feed(u'C') == U'C', "BMP char after a lone low surrogate is forwarded");
    }
    {
        Pending pending;
        pending.Feed(kHigh);
        expect(!pending.Feed(kHigh).has_value(), "second high surrogate replaces the pending high");
        expect(pending.Feed(kLow) == kEmoji, "high, high, low emits the pair of the second high");
    }
    {
        Pending pending;
        pending.Feed(kHigh);
        pending.Reset();
        expect(!pending.Feed(kLow).has_value(), "reset mid-pair drops the low surrogate");
    }
    {
        const auto pair = PrismaUI::Utf16Pairing::ToSurrogatePair(kEmoji);
        expect(pair[0] == kHigh && pair[1] == kLow, "U+1F600 encodes to D83D DE00");
        for (const char32_t codePoint : {U'\U00010000', U'\U00020BB7', U'\U0010FFFF'}) {
            Pending pending;
            const auto units = PrismaUI::Utf16Pairing::ToSurrogatePair(codePoint);
            pending.Feed(units[0]);
            expect(pending.Feed(units[1]) == codePoint, "encoded pair feeds back to the same code point");
        }
    }

    if (g_failures == 0) {
        std::printf("\nAll UTF-16 pairing assertions passed.\n");
        return 0;
    }
    std::printf("\n%d UTF-16 pairing assertion(s) failed.\n", g_failures);
    return 1;
}
