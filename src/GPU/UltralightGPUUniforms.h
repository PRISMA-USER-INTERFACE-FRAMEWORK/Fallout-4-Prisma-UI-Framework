#pragma once

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/platform/GPUDriver.h>
#pragma warning(pop)

#include <cstddef>
#include <cstdint>

namespace PrismaUI::GPU {

struct alignas(16) UltralightGPUUniformBlock {
    float state[4]{};
    float transform[16]{};
    float scalar4[2][4]{};
    float vectors[8][4]{};
    std::uint32_t clipSize = 0;
    std::uint32_t clipPadding[3]{};
    float clips[8][16]{};
};

static_assert(sizeof(ultralight::Vertex_2f_4ub_2f) == 20,
              "Ultralight 1.4 path vertex ABI changed");
static_assert(sizeof(ultralight::Vertex_2f_4ub_2f_2f_28f) == 140,
              "Ultralight 1.4 fill vertex ABI changed");
static_assert(sizeof(ultralight::IndexType) == 4,
              "Ultralight 1.4 index ABI changed");
static_assert(sizeof(UltralightGPUUniformBlock) == 768,
              "Prisma Ultralight constant-buffer layout changed");
static_assert(alignof(UltralightGPUUniformBlock) == 16,
              "Prisma Ultralight constant buffer must remain 16-byte aligned");

static_assert(offsetof(UltralightGPUUniformBlock, state) == 0, "state must be at register 0");
static_assert(offsetof(UltralightGPUUniformBlock, transform) == 16, "transform must follow state");
static_assert(offsetof(UltralightGPUUniformBlock, scalar4) == 80, "scalar4 must follow transform");
static_assert(offsetof(UltralightGPUUniformBlock, vectors) == 112, "vectors must follow scalar4");
static_assert(offsetof(UltralightGPUUniformBlock, clipSize) == 240, "clipSize must follow vectors");
static_assert(offsetof(UltralightGPUUniformBlock, clips) == 256, "clips must be 16-byte aligned");

[[nodiscard]] UltralightGPUUniformBlock BuildUltralightGPUUniformBlock(
    const ultralight::GPUState& state);

}
