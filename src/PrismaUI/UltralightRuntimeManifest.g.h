// GENERATED from runtime/ultralight-1.4.0.manifest.json by scripts/gen-runtime-manifest-header.py -- do not edit.
// Release packaging pins all SDK bytes; AppCore.dll and WebCore.dll are intentionally
// runtime-replaceable shared libraries and are therefore omitted from this integrity array.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace PrismaUI::WebRuntimeUltralight {
    struct PinnedRuntimeFile {
        std::wstring_view relative;
        std::uint64_t size;
        std::string_view sha256;
    };

    inline constexpr std::array kPinnedRuntimeFiles{
    PinnedRuntimeFile{L"libs/Ultralight.dll", 557568ull, "9c319e18b2f69cf12e2bf992b076079a6e4ffae482634dff3f61343d2c3919a7"},
    PinnedRuntimeFile{L"libs/UltralightCore.dll", 2554880ull, "ac782df832b528e030a1e8ae25ce6c66abc52888a0fbeff6191e0d76c21edfff"},
    PinnedRuntimeFile{L"resources/cacert.pem", 219265ull, "91f37c55b354111137dd456aacb3a57592d5135478d81554f9c87ea37deaedb4"},
    PinnedRuntimeFile{L"resources/icudt67l.dat", 6463504ull, "454ea0ba9b8cf87a4a6f801e8a51eefae77e4ad6fc5183c3b7544b920bbbd591"},
    };
}
