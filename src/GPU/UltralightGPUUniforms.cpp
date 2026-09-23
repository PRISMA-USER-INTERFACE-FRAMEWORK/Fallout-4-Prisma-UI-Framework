#include "UltralightGPUUniforms.h"

#include <algorithm>
#include <cstring>

namespace PrismaUI::GPU {

UltralightGPUUniformBlock BuildUltralightGPUUniformBlock(const ultralight::GPUState& state)
{
    UltralightGPUUniformBlock uniforms{};

    ultralight::Matrix transform;
    transform.Set(state.transform);

    ultralight::Matrix projection;
    projection.SetOrthographicProjection(static_cast<double>(state.viewport_width),
                                         static_cast<double>(state.viewport_height), false);
    projection.Transform(transform);
    const ultralight::Matrix4x4 projected = projection.GetMatrix4x4();

    uniforms.state[0] = 0.0F;
    uniforms.state[1] = static_cast<float>(state.viewport_width);
    uniforms.state[2] = static_cast<float>(state.viewport_height);
    uniforms.state[3] = 1.0F;
    std::memcpy(uniforms.transform, projected.data, sizeof(uniforms.transform));

    for (std::size_t i = 0; i < 8; ++i) {
        uniforms.scalar4[i / 4][i % 4] = state.uniform_scalar[i];
        uniforms.vectors[i][0] = state.uniform_vector[i].x;
        uniforms.vectors[i][1] = state.uniform_vector[i].y;
        uniforms.vectors[i][2] = state.uniform_vector[i].z;
        uniforms.vectors[i][3] = state.uniform_vector[i].w;
    }

    uniforms.clipSize = std::min<std::uint32_t>(state.clip_size, 8u);
    for (std::uint32_t i = 0; i < uniforms.clipSize; ++i) {
        std::memcpy(uniforms.clips[i], state.clip[i].data, sizeof(uniforms.clips[i]));
    }

    return uniforms;
}

}
