#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <REL/Offset.h>
#include <REL/Relocation.h>

namespace PrismaUI::Engine::VRSceneDepth {

    inline constexpr std::uintptr_t kCommitGraphicsStateRva =
        0x1D9B5D0;
    inline constexpr std::uintptr_t kCaptureCallsiteRva =
        0x1D8E84A;

    inline constexpr std::array<std::uint8_t, 5>
        kOriginalCall{
            0xE8, 0x81, 0xCD, 0x00, 0x00
        };
    inline constexpr std::array<std::uint8_t, 19>
        kCommitPrologue{
            0x88, 0x54, 0x24, 0x10,
            0x88, 0x4C, 0x24, 0x08,
            0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57,
            0x48, 0x83, 0xEC, 0x50
        };
    inline constexpr std::array<std::uint8_t, 13>
        kCallsiteBoundary{
            0x33, 0xD2, 0x33, 0xC9,
            0xE8, 0x81, 0xCD, 0x00, 0x00,
            0x80, 0x7B, 0x2A, 0x00
        };
    inline constexpr std::size_t kCaptureCallsitePrefixSize = 4;

    inline std::uintptr_t CommitGraphicsStateAddress()
    {
        return REL::Relocation<std::uintptr_t>{ REL::Offset(kCommitGraphicsStateRva) }.address();
    }

    inline std::uintptr_t CaptureCallsiteAddress()
    {
        return REL::Relocation<std::uintptr_t>{ REL::Offset(kCaptureCallsiteRva) }.address();
    }
}
