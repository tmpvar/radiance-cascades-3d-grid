#ifndef OCTAHEDRAL_MAPPING_ENCODING
#define OCTAHEDRAL_MAPPING_ENCODING
//
// Octahedral mapping
//   refs:
//     + DDGI - https://www.gdcvault.com/play/1026182/
//     + https://gist.github.com/pyalot/cc7c3e5f144fb825d626
//     + https://twitter.com/Stubbesaurus/status/937994790553227264
//     + https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
//

vec2
msign(vec2 v) {
  return vec2((v.x >= 0.0) ? 1.0 : -1.0, (v.y >= 0.0) ? 1.0 : -1.0);
}

#define sectorize(value) step(0.0, (value)) * 2.0 - 1.0
#define sum(value) dot(clamp((value), 1.0, 1.0), (value))
vec2
OctahedralEncode(vec3 normal) {
  normal /= sum(abs(normal));
  if (normal.y > 0.0) {
    return normal.xz * 0.5 + 0.5;
  } else {
    vec2 suv = sectorize(normal.xz);
    vec2 uv = suv - suv * abs(normal.zx);
    return uv * 0.5 + 0.5;
  }
}

vec3
OctahedralDecode(vec2 uv) {
  uv = uv * 2.0 - 1.0;
  vec2 auv = abs(uv);
  vec2 suv = sectorize(uv);
  float l = sum(auv);

  if (l > 1.0) {
    uv = (1.0 - auv.ts) * suv;
  }

  return normalize(vec3(uv.s, 1.0 - l, uv.t));
}

#endif