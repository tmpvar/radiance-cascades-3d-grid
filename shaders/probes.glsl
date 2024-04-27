
#include "lerp.glsl"
#include "octahedral.glsl"
#include <engine/gpu/morton.h>

vec3
WorldToProbeGrid(vec3 worldPos, int level) {
  vec3 gridDiameter = vec3(config.gridDiameter >> level);
  vec3 gridRadius = gridDiameter * 0.5f;
  vec3 cellDiameter = vec3(1 << level) * config.scale;
  vec3 cellRadius = cellDiameter * 0.5f;

  return (worldPos / cellDiameter) + gridRadius;
}

float
ProbesMapDistance(vec3 pos, int level) {
  const vec3 gridDiameter = vec3(float(config.gridDiameter >> level));
  const vec3 gridRadius = gridDiameter * 0.5;
  const vec3 cellDiameter = vec3(1 << level);
  const vec3 cellRadius = cellDiameter * 0.5;
  const vec3 spacing = cellDiameter * config.scale;
  const vec3 p = pos - cellRadius * config.scale;
  const vec3 q = p - spacing * clamp(round(p / spacing), -gridRadius, gridRadius - 1);
  const float probeRadius = 0.1 * float(1 << level);
  return length(q) - probeRadius * config.scale;
}

uvec3
ComputeUpperGridPos(vec3 pos, vec3 offset, vec3 upperGridDiameter) {
  return uvec3(clamp(floor(pos * 0.5f - 0.5f) + offset, v3(0.0f), upperGridDiameter));
}

uvec3
ComputeGridPos(vec3 pos, vec3 offset, vec3 gridDiameter) {
  return uvec3(clamp(floor(pos) + offset, v3(0.0f), gridDiameter));
}
