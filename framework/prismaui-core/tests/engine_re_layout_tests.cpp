// source: RE-Work/subsystems/prisma-matrix-re/g1-interface3d-localmap and g2-texture-material findings.jsonl
#include <RE/Fallout.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using Renderer = RE::Interface3D::Renderer;
using RendererList = RE::BSTArray<Renderer*>;

static_assert(offsetof(Renderer, omsize) == 0x68);
static_assert(offsetof(Renderer, customRenderTarget) == 0x1C0);
static_assert(offsetof(Renderer, name) == 0x208);
static_assert(offsetof(RE::BSLightingShaderMaterialBase, diffuseTexture) == 0x48);
static_assert(offsetof(RE::BSShaderProperty, alpha) == 0x28);
static_assert(offsetof(RE::BSShaderProperty, lastRenderPassState) == 0x2C);
static_assert(offsetof(RE::BSShaderProperty, flags) == 0x30);
static_assert(sizeof(RE::BSShaderProperty::flags) == 8);
static_assert(offsetof(RE::BSShaderProperty, material) == 0x58);

int main() {
    Renderer* elements[8]{};
    elements[2] = reinterpret_cast<Renderer*>(0x3000);
    void* const data = elements;
    const std::uint32_t capacity = 8;
    const std::uint32_t size = 3;

    alignas(RendererList) unsigned char storage[sizeof(RendererList)]{};
    static_assert(sizeof(storage) >= 0x14);
    std::memcpy(storage + 0x00, &data, sizeof data);
    std::memcpy(storage + 0x08, &capacity, sizeof capacity);
    std::memcpy(storage + 0x10, &size, sizeof size);
    const auto& list = *std::launder(reinterpret_cast<const RendererList*>(storage));

    if (list.data() != elements || list.capacity() != 8 || list.size() != 3 || list[2] != elements[2]) {
        std::puts("FAIL: BSTArray<Renderer*> does not read {data +0x00, u32 capacity +0x08, u32 size +0x10}");
        return 1;
    }
    std::puts("engine_re_layout_tests ok");
    return 0;
}
