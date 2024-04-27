#version 450

#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 uv;

layout(location = 0) uniform mat4 worldToScreen;
layout(location = 1) uniform mat4 screenToWorld;
layout(location = 2) uniform vec3 eye;
layout(location = 3) uniform float fov;
layout(location = 4) uniform vec2 screenDims;
layout(location = 5) uniform sampler2DArray octahedralProbeAtlasTexture;
layout(location = 6) uniform int debugRenderProbeLevel;
layout(location = 7) uniform float mergeTexelGatherOffset;
layout(location = 8) uniform float mergeTexelGatherRatio;

#include "../radiance-cascades/shared.h"

layout(std430, binding = 0) uniform RadianceCascadeConfigUBO {
  RadianceCascadesConfig config;
};

#include "octahedral.glsl"
#include "probes.glsl"
#include "shared.glsl"
#include <engine/gpu/morton.h>
#include <hotcart/types.h>

#define Sample(offset) ReadProbeLinear(probeGridPos + offset, sampleNormal, 0)

MapResult
ReadProbeLinear(vec3 probeGridPos, vec3 normal, int level) {
  uint probeIndex = MortonEncode(uvec3(probeGridPos));
  uint atlasProbeDiameter = config.atlasProbeDiameter * uint(pow(2, level));
  ivec2 probeOffset = ivec2(MortonDecode2D(probeIndex) *
                            OCTAPROBE_PADDED_DIAMETER(atlasProbeDiameter));

  vec2 probeUV = OctahedralEncode(normal);
  vec2 src = vec2(probeOffset) + probeUV * atlasProbeDiameter + 0.5;

  MapResult c00 = UnpackMapResult(
    texelFetch(octahedralProbeAtlasTexture, ivec3(src + vec2(0, 0), level), 0));
  MapResult c10 = UnpackMapResult(
    texelFetch(octahedralProbeAtlasTexture, ivec3(src + vec2(1, 0), level), 0));
  MapResult c01 = UnpackMapResult(
    texelFetch(octahedralProbeAtlasTexture, ivec3(src + vec2(0, 1), level), 0));
  MapResult c11 = UnpackMapResult(
    texelFetch(octahedralProbeAtlasTexture, ivec3(src + vec2(1, 1), level), 0));

  return Lerp2D(c00, c10, c01, c11, fract(src));
}

MapResult
SampleProbesWorldSpace(vec3 pos, vec3 surfaceNormal, vec3 sampleNormal) {
  vec3 probeGridPos = WorldToProbeGrid(pos, 0);

  vec3 hi = vec3(config.gridDiameter);
  MapResult c000 = Sample(vec3(0, 0, 0));
  MapResult c100 = Sample(vec3(1, 0, 0));
  MapResult c010 = Sample(vec3(0, 1, 0));
  MapResult c110 = Sample(vec3(1, 1, 0));
  MapResult c001 = Sample(vec3(0, 0, 1));
  MapResult c101 = Sample(vec3(1, 0, 1));
  MapResult c011 = Sample(vec3(0, 1, 1));
  MapResult c111 = Sample(vec3(1, 1, 1));
  // return c000;
  // return (c000 + c100 + c010 + c110 + c001 + c101 + c011 + c111) * 0.125;
  vec3 t = fract(probeGridPos);
  // t = vec3(0.5);
  return Lerp3D(c000, c100, c010, c110, c001, c101, c011, c111, t);
}

MapResult
MapProbes(vec3 pos, vec3 rayDir) {
  MapResult result;
  result.d = 10000000.0;
  result.level = -1;
  int level = debugRenderProbeLevel;
  // if (level > -1) {
  for (int level = 0; level <= debugRenderProbeLevel; level++) {
    float d = ProbesMapDistance(pos, level);
    if (d < result.d) {
      result.d = d;
      result.level = level;
    }
  }
  result.emission = vec3(0.0);
  result.throughput = vec3(1.0);
  return result;
}

vec3
CalcNormalProbes(vec3 p, int level) {
  const float h = 0.0001; // replace by an appropriate value
  const vec2 k = vec2(1, -1);
  return normalize(k.xyy * ProbesMapDistance(p + k.xyy * h, level) +
                   k.yyx * ProbesMapDistance(p + k.yyx * h, level) +
                   k.yxy * ProbesMapDistance(p + k.yxy * h, level) +
                   k.xxx * ProbesMapDistance(p + k.xxx * h, level));
}

void
main() {
  outColor = vec4(uv, 0.0, 1.0);
  outColor = vec4(0.1, 0.1, 0.1, 1.0);
  // return;
  vec3 rayDir = ComputeRayDirection(uv, screenToWorld);
  const int maxSteps = 128;
  int steps = maxSteps;
  float t = 0.00;

  while (steps-- > 0) {
    vec3 pos = eye + rayDir * t;
    MapResult result = map(pos);
    // result.d = 10000.0;

    float sceneDistance = result.d;
    MapResult probeResult;
    probeResult.d = 1000000.0;
    // DEBUG: Render probes
    if (true) { //debugRenderProbeLevel > -1) {
      probeResult = MapProbes(pos, rayDir);
      result = ProcessMaterial(result, probeResult, min(result.d, probeResult.d));
    }

    // TODO: make this multiplier a slider
    float eps = ComputeConeRadius(t, fov, screenDims.y) * 2.0;
    if (result.d <= eps) {

      if (probeResult.d == result.d) {
        vec3 probeNormal = CalcNormalProbes(pos, result.level);
        outColor = vec4(probeNormal * 0.5 + 0.5, 1.0);

        vec2 uv = OctahedralEncode(probeNormal);
        outColor = vec4(uv, 1.0 - length(uv), 1.0);

        // DEBUG: sample the probe directly
        if (true) {
          vec3 probeGridPos = WorldToProbeGrid(pos, result.level);
          outColor = vec4(fract(probeGridPos), 1.0);

          uint probeIndex = MortonEncode(uvec3(probeGridPos));
          uint atlasProbeDiameter = config.atlasProbeDiameter *
                                    uint(pow(2, result.level));
          ivec2 probeOffset = ivec2(MortonDecode2D(probeIndex) *
                                    OCTAPROBE_PADDED_DIAMETER(atlasProbeDiameter));

          vec2 probeUV = OctahedralEncode(probeNormal);
          vec2 src = vec2(probeOffset) + probeUV * atlasProbeDiameter + 0.5;

          ivec2 srcTexel = ivec2(src);
          MapResult c00 = UnpackMapResult(
            texelFetch(octahedralProbeAtlasTexture,
                       ivec3(srcTexel + ivec2(0, 0), result.level),
                       0));
          MapResult c10 = UnpackMapResult(
            texelFetch(octahedralProbeAtlasTexture,
                       ivec3(srcTexel + ivec2(1, 0), result.level),
                       0));
          MapResult c01 = UnpackMapResult(
            texelFetch(octahedralProbeAtlasTexture,
                       ivec3(srcTexel + ivec2(0, 1), result.level),
                       0));
          MapResult c11 = UnpackMapResult(
            texelFetch(octahedralProbeAtlasTexture,
                       ivec3(srcTexel + vec2(1, 1), result.level),
                       0));


          MapResult r = Lerp2D(c00, c10, c01, c11, fract(src));
          outColor = vec4(r.emission, 1.0);
        }
        return;
      } else {
        vec3 normal = CalcNormal(pos);
        // MapResult probeValue = SampleProbesWorldSpace(pos, normal, reflect(rayDir, normal));
        MapResult probeValue = SampleProbesWorldSpace(pos, normal, normal);
        outColor = vec4(probeValue.emission.rgb, 1.0);
        // outColor = vec4(probeValue.color.rgb, 1.0);
        // outColor = vec4(result.color.rgb * probeValue.emission.rgb, 1.0);
        outColor = vec4(probeValue.emission.rgb, 1.0);
        // outColor = vec4(result.color.rgb, 1.0);
        // outColor = vec4(normal * 0.5 + 0.5, 1.0);
        return;
      }
    }
    t += result.d;
  }
  outColor = vec4(0.1, 0.1, 0.1, 1.0);
}