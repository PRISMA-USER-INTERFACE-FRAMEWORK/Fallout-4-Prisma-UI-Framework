#include "GPU/GPUDriverBase.h"

#include <cassert>
#include <cstddef>
#include <cstdint>

using PrismaUI::GPU::AcceleratedGenerationMailbox;
using PrismaUI::GPU::AcceleratedGenerationOperationType;

namespace {

constexpr std::size_t BgraBytes(std::uint32_t width, std::uint32_t height)
{
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
}

}

int main()
{
    static_assert(AcceleratedGenerationMailbox::kMaxNonTexturePayloadBytes == 64u * 1024u * 1024u);
    static_assert(AcceleratedGenerationMailbox::kMaxTexturePayloadBytes == 256u * 1024u * 1024u);
    static_assert(AcceleratedGenerationMailbox::kMaxPayloadBytes == 320u * 1024u * 1024u);

    constexpr auto bgra2k = BgraBytes(2048, 2048);
    constexpr auto bgra4k = BgraBytes(4096, 4096);
    constexpr auto bgra8k = BgraBytes(8192, 8192);
    constexpr auto bgra8193 = BgraBytes(8193, 8193);

    static_assert(bgra2k == 16u * 1024u * 1024u);
    static_assert(bgra4k == 64u * 1024u * 1024u);
    static_assert(bgra8k == 256u * 1024u * 1024u);
    static_assert(bgra8193 > AcceleratedGenerationMailbox::kMaxTexturePayloadBytes);

    assert(AcceleratedGenerationMailbox::PayloadShapeWithinBudget(
        AcceleratedGenerationOperationType::CreateTexture, 2048, 2048, bgra2k));
    assert(AcceleratedGenerationMailbox::PayloadShapeWithinBudget(
        AcceleratedGenerationOperationType::CreateTexture, 4096, 4096, bgra4k));
    assert(AcceleratedGenerationMailbox::PayloadShapeWithinBudget(
        AcceleratedGenerationOperationType::CreateTexture, 8192, 8192, bgra8k));
    assert(!AcceleratedGenerationMailbox::PayloadShapeWithinBudget(
        AcceleratedGenerationOperationType::CreateTexture, 8193, 8193, bgra8193));
    assert(!AcceleratedGenerationMailbox::PayloadShapeWithinBudget(
        AcceleratedGenerationOperationType::UpdateTexture,
        0,
        0,
        AcceleratedGenerationMailbox::kMaxTexturePayloadBytes + 1));

    assert(AcceleratedGenerationMailbox::PayloadShapeWithinBudget(
        AcceleratedGenerationOperationType::CreateGeometry,
        0,
        0,
        AcceleratedGenerationMailbox::kMaxNonTexturePayloadBytes));
    assert(!AcceleratedGenerationMailbox::PayloadShapeWithinBudget(
        AcceleratedGenerationOperationType::CreateGeometry,
        0,
        0,
        AcceleratedGenerationMailbox::kMaxNonTexturePayloadBytes + 1));

    // Aggregate non-texture lane: exactly 64 MiB is allowed, one additional byte is rejected even
    // though the generation-wide 320 MiB total still has ample room.
    AcceleratedGenerationMailbox::PayloadBudgetSummary geometryLane;
    assert(AcceleratedGenerationMailbox::AccumulatePayload(
        geometryLane,
        AcceleratedGenerationOperationType::CreateGeometry,
        0,
        0,
        AcceleratedGenerationMailbox::kMaxNonTexturePayloadBytes));
    assert(!AcceleratedGenerationMailbox::AccumulatePayload(
        geometryLane, AcceleratedGenerationOperationType::UpdateGeometry, 0, 0, 1));

    // Same lane overflow spread across multiple individually-valid operations.
    AcceleratedGenerationMailbox::PayloadBudgetSummary splitGeometryLane;
    constexpr auto halfGeometry = AcceleratedGenerationMailbox::kMaxNonTexturePayloadBytes / 2;
    assert(AcceleratedGenerationMailbox::AccumulatePayload(
        splitGeometryLane, AcceleratedGenerationOperationType::CreateGeometry, 0, 0, halfGeometry));
    assert(AcceleratedGenerationMailbox::AccumulatePayload(
        splitGeometryLane, AcceleratedGenerationOperationType::UpdateGeometry, 0, 0, halfGeometry));
    assert(!AcceleratedGenerationMailbox::AccumulatePayload(
        splitGeometryLane, AcceleratedGenerationOperationType::UpdateGeometry, 0, 0, 1));

    // One full 8K BGRA upload plus the complete 64 MiB non-texture lane is exactly the advertised
    // 320 MiB generation ceiling and must be accepted.
    AcceleratedGenerationMailbox::PayloadBudgetSummary fullMixedGeneration;
    assert(AcceleratedGenerationMailbox::AccumulatePayload(
        fullMixedGeneration, AcceleratedGenerationOperationType::CreateTexture, 8192, 8192, bgra8k));
    assert(AcceleratedGenerationMailbox::AccumulatePayload(
        fullMixedGeneration,
        AcceleratedGenerationOperationType::CreateGeometry,
        0,
        0,
        AcceleratedGenerationMailbox::kMaxNonTexturePayloadBytes));
    assert(fullMixedGeneration.textureBytes == AcceleratedGenerationMailbox::kMaxTexturePayloadBytes);
    assert(fullMixedGeneration.nonTextureBytes == AcceleratedGenerationMailbox::kMaxNonTexturePayloadBytes);
    assert(fullMixedGeneration.bytes == AcceleratedGenerationMailbox::kMaxPayloadBytes);

    // Texture lane overflow remains rejected even when each operation is individually valid.
    AcceleratedGenerationMailbox::PayloadBudgetSummary textureLane;
    assert(AcceleratedGenerationMailbox::AccumulatePayload(
        textureLane,
        AcceleratedGenerationOperationType::CreateTexture,
        4096,
        4096,
        AcceleratedGenerationMailbox::kMaxTexturePayloadBytes));
    assert(!AcceleratedGenerationMailbox::AccumulatePayload(
        textureLane, AcceleratedGenerationOperationType::UpdateTexture, 1, 1, 1));

    // Order must not matter: non-texture first, then texture, reaches the same exact combined limit.
    AcceleratedGenerationMailbox::PayloadBudgetSummary reverseMixedGeneration;
    assert(AcceleratedGenerationMailbox::AccumulatePayload(
        reverseMixedGeneration,
        AcceleratedGenerationOperationType::CreateGeometry,
        0,
        0,
        AcceleratedGenerationMailbox::kMaxNonTexturePayloadBytes));
    assert(AcceleratedGenerationMailbox::AccumulatePayload(
        reverseMixedGeneration, AcceleratedGenerationOperationType::CreateTexture, 8192, 8192, bgra8k));
    assert(reverseMixedGeneration.bytes == AcceleratedGenerationMailbox::kMaxPayloadBytes);

    return 0;
}
