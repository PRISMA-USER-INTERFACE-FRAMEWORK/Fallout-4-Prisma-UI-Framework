#pragma once

#include "VR/VRViewState.h"

#include <cstdint>

namespace PrismaUI::SpatialPresentation::Shared
{
    inline constexpr float maximumWorldMagnitude = 1.0e8f;
    inline constexpr float maximumPhysicalDimension = 1.0e6f;
    inline constexpr float minimumQuaternionNormSquared = 1.0e-8f;

    [[nodiscard]] std::uint32_t SupportedUpdateFlags() noexcept;
    [[nodiscard]] bool IsWorldMode(
        PRISMA_UI_VR_API::SpatialPresentationMode mode) noexcept;
}
