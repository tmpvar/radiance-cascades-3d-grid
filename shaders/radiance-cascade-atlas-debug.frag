#version 450

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 uv;

layout(location = 0) uniform vec2 screenDims;
layout(location = 1) uniform sampler2DArray octahedralProbeAtlasTexture;
layout(location = 2) uniform int level;

layout(location = 5) uniform float debugScale;
layout(location = 6) uniform vec2 debugOffset;

#include "../radiance-cascades/shared.h"

layout(std430, binding = 0) uniform RadianceCascadeConfigUBO {
  RadianceCascadesConfig config;
};

#include <engine/gpu/morton.h>
#include <hotcart/types.h>

#include "shared.glsl"

void
main() {
  outColor = vec4(uv, 0.0, 1.0);

  if (any(lessThan(uv, vec2(0.002))) || any(greaterThan(uv, vec2(0.999)))) {
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
    return;
  }

  vec2 scaledUV = uv / debugScale;
  if (any(greaterThanEqual(scaledUV, vec2(1.0)))) {
    outColor = vec4(0.0, 0.0, 0.0, 0.125);
  } else {
    vec3 size = textureSize(octahedralProbeAtlasTexture, 0);
    vec4 textureValue = texelFetch(octahedralProbeAtlasTexture,
                                   ivec3(floor(scaledUV * size.xy) + debugOffset, level),
                                   0);

    MapResult r = UnpackMapResult(textureValue);

    outColor = vec4(r.color, 1.0);
    outColor = vec4(r.emission, 1.0);
  }
}