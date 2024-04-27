#version 450

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 uv;

layout(location = 0) uniform vec2 screenDims;
layout(location = 1) uniform sampler2DArray octahedralProbeAtlasTexture;
layout(location = 2) uniform int level;

layout(location = 3) uniform vec2 corner;
layout(location = 4) uniform vec2 dims;

layout(location = 5) uniform mat4 screenToWorld;
layout(location = 6) uniform vec3 eye;
layout(location = 7) uniform ivec3 gridPos;
layout(location = 8) uniform uint computeMergedValue;
layout(location = 9) uniform float mergeTexelGatherOffset;
layout(location = 10) uniform float mergeTexelGatherRatio;

#include "../radiance-cascades/shared.h"

layout(std430, binding = 0) uniform RadianceCascadeConfigUBO {
  RadianceCascadesConfig config;
};

#include "probes.glsl"
#include "shared.glsl"

// sphere of size ra centered at point ce
vec2
sphIntersect(in vec3 ro, in vec3 rd, in vec3 ce, float ra) {
  vec3 oc = ro - ce;
  float b = dot(oc, rd);
  float c = dot(oc, oc) - ra * ra;
  float h = b * b - c;
  if (h < 0.0)
    return vec2(-1.0); // no intersection
  h = sqrt(h);
  return vec2(-b - h, -b + h);
}

vec4
ReadProbeLinearOffset(uvec3 probeGridPos, vec3 normal, int level, vec2 texelOffset) {
  uint probeIndex = MortonEncode(probeGridPos);
  uint atlasProbeDiameter = config.atlasProbeDiameter *
                            uint(pow(2, level));
  ivec2 probeOffset = ivec2(MortonDecode2D(probeIndex) *
                            OCTAPROBE_PADDED_DIAMETER(atlasProbeDiameter));

  vec2 probeUV = OctahedralEncode(normal);
  vec2 src = vec2(probeOffset) +
             clamp(texelOffset * vec2(level + 1) + probeUV * atlasProbeDiameter,
                   vec2(0),
                   vec2(atlasProbeDiameter)) +
             OCTAPROBE_PADDING;

  vec4 c00 = texelFetch(octahedralProbeAtlasTexture, ivec3(src + vec2(0, 0), level), 0);
  vec4 c10 = texelFetch(octahedralProbeAtlasTexture, ivec3(src + vec2(1, 0), level), 0);
  vec4 c01 = texelFetch(octahedralProbeAtlasTexture, ivec3(src + vec2(0, 1), level), 0);
  vec4 c11 = texelFetch(octahedralProbeAtlasTexture, ivec3(src + vec2(1, 1), level), 0);

  return Lerp2D(c00, c10, c01, c11, fract(src));
}

vec4
ReadProbeLinearGather(uvec3 probeGridPos,
                      vec3 normal,
                      int level,
                      float texelOffset,
                      float texelOffsetMergeRatio) {

  vec4 acc = vec4(0.0);
  for (float x = -1.0; x <= 1.0; x += 1.0) {
    for (float y = -1.0; y <= 1.0; y += 1.0) {
      if (x == 0.0 && y == 0.0) {
        continue;
      }
      vec2 offset = vec2(x, y) * texelOffset;
      acc += ReadProbeLinearOffset(probeGridPos, normal, level, offset);
    }
  }

  vec4 center = ReadProbeLinearOffset(probeGridPos, normal, level, vec2(0.0));
  vec4 outer = acc / 8.0;
  // return outer;
  return center * texelOffsetMergeRatio + outer * (1.0 - texelOffsetMergeRatio);
}

// #define ReadProbe ReadProbeNearest
#define ReadProbe ReadProbeLinear

vec4
ReadProbesSpatialLinearAngularLinearGather(vec3 pos,
                                           vec3 normal,
                                           int level,
                                           float texelOffset,
                                           float mergeRatio) {
  // pos += 0.5;
  vec3 uvw = fract(pos * 0.5 + 0.5);

  uvec3 p = uvec3(pos);
  vec3 lo = vec3(0.0);
  vec3 hi = vec3(config.gridDiameter << level);
  vec2 probeUV = OctahedralEncode(normal);

  return Lerp3D(ReadProbeLinearGather(ComputeUpperGridPos(pos, vec3(0, 0, 0), hi),
                                      normal,
                                      level,
                                      texelOffset,
                                      mergeRatio),
                ReadProbeLinearGather(ComputeUpperGridPos(pos, vec3(1, 0, 0), hi),
                                      normal,
                                      level,
                                      texelOffset,
                                      mergeRatio),
                ReadProbeLinearGather(ComputeUpperGridPos(pos, vec3(0, 1, 0), hi),
                                      normal,
                                      level,
                                      texelOffset,
                                      mergeRatio),
                ReadProbeLinearGather(ComputeUpperGridPos(pos, vec3(1, 1, 0), hi),
                                      normal,
                                      level,
                                      texelOffset,
                                      mergeRatio),
                ReadProbeLinearGather(ComputeUpperGridPos(pos, vec3(0, 0, 1), hi),
                                      normal,
                                      level,
                                      texelOffset,
                                      mergeRatio),
                ReadProbeLinearGather(ComputeUpperGridPos(pos, vec3(1, 0, 1), hi),
                                      normal,
                                      level,
                                      texelOffset,
                                      mergeRatio),
                ReadProbeLinearGather(ComputeUpperGridPos(pos, vec3(0, 1, 1), hi),
                                      normal,
                                      level,
                                      texelOffset,
                                      mergeRatio),
                ReadProbeLinearGather(ComputeUpperGridPos(pos, vec3(1, 1, 1), hi),
                                      normal,
                                      level,
                                      texelOffset,
                                      mergeRatio),
                uvw);
}


vec4
ReadProbeNearest(v3u32 probeGridPos, vec3 normal, int level) {
  uint probeIndex = MortonEncode(probeGridPos);
  uint atlasProbeDiameter = config.atlasProbeDiameter *
                            uint(pow(2, level));
  ivec2 probeOffset = ivec2(MortonDecode2D(probeIndex) *
                            OCTAPROBE_PADDED_DIAMETER(atlasProbeDiameter));

  vec2 probeUV = OctahedralEncode(normal);
  vec2 src = vec2(probeOffset) + probeUV * atlasProbeDiameter + OCTAPROBE_PADDING;

  return texelFetch(octahedralProbeAtlasTexture, ivec3(src, level), 0);
}


void
main() {
  outColor = vec4(uv, 0.0, 1.0);
  float d = max(abs(uv.x - 0.5), abs(uv.y - 0.5));
  if (d < 0.05 && d > 0.04) {
    outColor = vec4(1.0);
    return;
  }

  vec3 rayDir = ComputeRayDirection(uv, screenToWorld);
  vec2 hit = sphIntersect(eye, rayDir, vec3(0.0), 1.0);
  if (hit.x > -1.0) {
    vec3 pos = eye + rayDir * hit.x;
    vec3 normal = normalize(pos);
    if (computeMergedValue != 0) {
      vec4 lowerSample = ReadProbeLinearGather(gridPos,
                                               normal,
                                               level,
                                               mergeTexelGatherOffset,
                                               mergeTexelGatherRatio);
      float lowerTransparency = 1.0 - lowerSample.a;
      vec4
        upperSample = ReadProbesSpatialLinearAngularLinearGather(gridPos,
                                                                 normal,
                                                                 level + 1,
                                                                 mergeTexelGatherOffset,
                                                                 mergeTexelGatherRatio);
      outColor = vec4(lowerSample.rgb + lowerTransparency * upperSample.rgb,
                      upperSample.a + lowerSample.a);
    } else {
      outColor = ReadProbeNearest(gridPos, normal, level);
    }
  } else {
    discard;
  }
}