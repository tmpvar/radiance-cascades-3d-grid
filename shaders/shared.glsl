#include "lerp.glsl"
#include <engine/gpu/colormap-turbo.glsl>
#include <hotcart/types.h>

struct MapResult {
  vec3 color;
  vec3 emission;
  vec3 throughput;
  float d;
  int level;
};

vec4
PackMapResult(MapResult mapResult) {
  vec4 packed;
  packed.x = uintBitsToFloat(packUnorm4x8(vec4(mapResult.color, 0.0)));
  packed.y = uintBitsToFloat(packHalf2x16(mapResult.emission.xy));
  packed.z = uintBitsToFloat(
    packHalf2x16(vec2(mapResult.emission.z, mapResult.throughput.x)));
  packed.w = uintBitsToFloat(packHalf2x16(mapResult.throughput.yz));
  return packed;
}

MapResult
UnpackMapResult(vec4 packed) {
  MapResult mapResult;

  uvec4 bits = uvec4(
    floatBitsToUint(packed.x),
    floatBitsToUint(packed.y),
    floatBitsToUint(packed.z),
    floatBitsToUint(packed.w)
  );

  mapResult.color = unpackUnorm4x8(bits.x).xyz;
  mapResult.emission.xy = unpackHalf2x16(bits.y);
  vec2 tmp = unpackHalf2x16(bits.z);
  mapResult.emission.z = tmp.x;
  mapResult.throughput.x = tmp.y;
  mapResult.throughput.yz = unpackHalf2x16(bits.w);
  return mapResult;
}

MapResult
Lerp3D(MapResult c000,
       MapResult c100,
       MapResult c010,
       MapResult c110,

       MapResult c001,
       MapResult c101,
       MapResult c011,
       MapResult c111,

       vec3 t) {

  MapResult r;
  r.color = Lerp3D(c000.color,
                   c100.color,
                   c010.color,
                   c110.color,
                   c001.color,
                   c101.color,
                   c011.color,
                   c111.color,
                   t);

  r.emission = Lerp3D(c000.emission,
                      c100.emission,
                      c010.emission,
                      c110.emission,
                      c001.emission,
                      c101.emission,
                      c011.emission,
                      c111.emission,
                      t);

  r.throughput = Lerp3D(c000.throughput,
                        c100.throughput,
                        c010.throughput,
                        c110.throughput,
                        c001.throughput,
                        c101.throughput,
                        c011.throughput,
                        c111.throughput,
                        t);
  return r;
}

MapResult
Lerp2D(MapResult c00,
       MapResult c10,
       MapResult c01,
       MapResult c11,

       vec2 t) {

  MapResult r;
  r.color = Lerp2D(c00.color, c10.color, c01.color, c11.color, t);
  r.emission = Lerp2D(c00.emission, c10.emission, c01.emission, c11.emission, t);
  r.throughput = Lerp2D(c00.throughput,
                        c10.throughput,
                        c01.throughput,
                        c11.throughput,
                        t);
  return r;
}

MapResult
Box(vec3 p, vec3 b, vec3 color) {
  MapResult result;
  result.color = color;
  vec3 q = abs(p) - b;
  result.d = length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
  result.emission = vec3(0.0);
  result.throughput = vec3(1.0);
  return result;
}

float
sdSphere(vec3 p, float s) {
  return length(p) - s;
}

MapResult
Sphere(vec3 p, float s, vec3 color) {
  MapResult result;
  result.color = color;
  result.d = length(p) - s;
  result.emission = vec3(0.0);
  result.throughput = vec3(1.0);
  return result;
}

MapResult
EmissiveSphere(vec3 p, float s, vec3 color, vec3 emission) {
  MapResult result;
  result.color = color;
  result.d = length(p) - s;
  result.emission = emission;
  result.throughput = vec3(1.0);
  return result;
}

MapResult
ProcessMaterial(MapResult a, MapResult b, float d) {
  MapResult result;

  if (abs(a.d) < abs(b.d)) {
    result.color = a.color;
    result.emission = a.emission;
    result.throughput = a.throughput;
    result.level = a.level;
  } else {
    result.color = b.color;
    result.emission = b.emission;
    result.throughput = b.throughput;
    result.level = b.level;
  }
  result.d = d;
  return result;
}

MapResult
Union(MapResult a, MapResult b) {
  return ProcessMaterial(a, b, min(a.d, b.d));
}

MapResult
Cut(MapResult a, MapResult b) {
  return ProcessMaterial(a, b, max(a.d, -b.d));
}

MapResult
map(vec3 p) {
  MapResult result;
  result.d = 1000000000.0;
  result.throughput = vec3(1.0);
  int level = 0;
#if 1
  float wallThickness = 0.2;
  float emitterRadius = 1.4;
  vec3 roomSize = vec3(12.0, 12.0, 12.0);
  result = Box(p, roomSize, vec3(1.0, 0.0, 0.0));
  result = Cut(result, Box(p, roomSize - wallThickness, vec3(0.0, 1.0, 0.0)));
  result = Cut(result, Box(p - vec3(0.0, 0.0, roomSize.z),
                             vec3(roomSize.x * 1.5,
                                  roomSize.y * 1.5,
                                  wallThickness * 1.5), vec3(0.0, 0.0, 1.0)));
  result = Union(result, EmissiveSphere(p, emitterRadius, vec3(1.0), vec3(100.0)));

#else
  result = Union(Box(p - vec3(0.0, -3.0, 0.0), vec3(3.5, 0.25, 3.5), vec3(1.0, 1.0, 1.0)),
                 Box(p + vec3(0.0, 1.25, -2.5),
                     vec3(0.5, 2, 0.5),
                     vec3(0.78, 0.094, 0.333)));
  // result = Union(Box(p - vec3(0.0, -2.0, 0.0), vec3(.5, 1.0, .25), vec3(0.0, 0.0, 1.0)),
  //                Union(Box(p - vec3(0.0, -3.0, 0.0),
  //                          vec3(3.5, 0.25, 3.5),
  //                          vec3(1.0, 1.0, 1.0)),
  //                      Box(p + vec3(-2.5, 1.25, -2.5),
  //                          vec3(0.5, 2, 0.5),
  //                          vec3(0.78, 0.094, 0.333))));
#endif
  return result;
}

float
ComputeConeRadius(float t, float fov, float screenHeight) {
  return t * tan(fov * 0.5) / (screenHeight * 0.5);
}

vec3
CalcNormal(vec3 p) {
  const float h = 0.0001; // replace by an appropriate value
  const vec2 k = vec2(1, -1);
  return normalize(k.xyy * map(p + k.xyy * h).d + k.yyx * map(p + k.yyx * h).d +
                   k.yxy * map(p + k.yxy * h).d + k.xxx * map(p + k.xxx * h).d);
}

vec3
CalcSphereNormal(vec3 p, float r) {
  const float h = 0.0001; // replace by an appropriate value
  const vec2 k = vec2(1, -1);
  return normalize(
    k.xyy * sdSphere(p + k.xyy * h, r) + k.yyx * sdSphere(p + k.yyx * h, r) +
    k.yxy * sdSphere(p + k.yxy * h, r) + k.xxx * sdSphere(p + k.xxx * h, r));
}

vec3
ComputeRayDirection(vec2 uv, mat4 inv) {
  uv = uv * 2.0 - 1.0;
  vec4 farPlane = inv * vec4(uv.x, uv.y, 0.01, 1.0);
  farPlane /= farPlane.w;
  vec4 nearPlane = inv * vec4(uv.x, uv.y, 0.2, 1.0);
  nearPlane /= nearPlane.w;
  vec3 dir = normalize(vec3(farPlane) - vec3(nearPlane));
  // dir.y = -dir.y;
  return dir;
}

float
MaxComponent(vec2 p) {
  return max(p.x, p.y);
}