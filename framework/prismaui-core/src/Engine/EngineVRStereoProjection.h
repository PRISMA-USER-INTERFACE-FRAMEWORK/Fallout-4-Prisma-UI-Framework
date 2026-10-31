#pragma once

#include <cstddef>
#include <cstdint>

#include <DirectXMath.h>
#include <REL/Offset.h>
#include <REL/Relocation.h>

namespace PrismaUI::Engine::VRStereoProjection {

    inline constexpr std::uintptr_t kRuntimeRootRva = 0x6235AC8;
    inline constexpr std::uintptr_t kFirstOriginOffset = 0x2590;
    inline constexpr std::uintptr_t kSecondOriginOffset = 0x25A0;
    inline constexpr std::uintptr_t kRecordsPointerOffset = 0x25D0;
    inline constexpr std::uintptr_t kRecordStride = 0x210;
    inline constexpr std::uintptr_t kCompositeOffset = 0xD0;
    inline constexpr std::uintptr_t kFirstCompositeOffset =
        kCompositeOffset;
    inline constexpr std::uintptr_t kSecondCompositeOffset =
        kRecordStride + kCompositeOffset;
    inline constexpr std::size_t kRecordsReadBytes =
        kSecondCompositeOffset +
        sizeof(DirectX::XMFLOAT4X4);

    struct RootFields
    {
        float firstOrigin[4]{};
        float secondOrigin[4]{};
        std::byte unused[0x20]{};
        std::uintptr_t records = 0;
    };

    static_assert(sizeof(RootFields) == 0x48);

    inline std::uintptr_t RuntimeRootAddress()
    {
        return REL::Relocation<std::uintptr_t>{ REL::Offset(kRuntimeRootRva) }.address();
    }
}
