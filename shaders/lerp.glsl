#ifndef LERP_GLSL
#define LERP_GLSL

vec4
Lerp3D(vec4 c000,
       vec4 c100,
       vec4 c010,
       vec4 c110,

       vec4 c001,
       vec4 c101,
       vec4 c011,
       vec4 c111,

       vec3 t) {

  // front face
  vec4 c00 = c000 * (1.0 - t.x) + c100 * t.x;
  vec4 c01 = c010 * (1.0 - t.x) + c110 * t.x;

  // back face
  vec4 c10 = c001 * (1.0 - t.x) + c101 * t.x;
  vec4 c11 = c011 * (1.0 - t.x) + c111 * t.x;

  vec4 c0 = c00 * (1.0 - t.z) + c10 * t.z;
  vec4 c1 = c01 * (1.0 - t.z) + c11 * t.z;

  return c0 * (1.0 - t.y) + c1 * t.y;
}

vec3
Lerp3D(vec3 c000,
       vec3 c100,
       vec3 c010,
       vec3 c110,

       vec3 c001,
       vec3 c101,
       vec3 c011,
       vec3 c111,

       vec3 t) {

  // front face
  vec3 c00 = c000 * (1.0 - t.x) + c100 * t.x;
  vec3 c01 = c010 * (1.0 - t.x) + c110 * t.x;

  // back face
  vec3 c10 = c001 * (1.0 - t.x) + c101 * t.x;
  vec3 c11 = c011 * (1.0 - t.x) + c111 * t.x;

  vec3 c0 = c00 * (1.0 - t.z) + c10 * t.z;
  vec3 c1 = c01 * (1.0 - t.z) + c11 * t.z;

  return c0 * (1.0 - t.y) + c1 * t.y;
}

vec4
Lerp2D(vec4 c00, vec4 c10, vec4 c01, vec4 c11, vec2 t) {
  vec4 x0 = c00 * (1.0 - t.x) + c10 * t.x;
  vec4 x1 = c01 * (1.0 - t.x) + c11 * t.x;

  return x0 * (1.0 - t.y) + x1 * t.y;
}


vec3
Lerp2D(vec3 c00, vec3 c10, vec3 c01, vec3 c11, vec2 t) {
  vec3 x0 = c00 * (1.0 - t.x) + c10 * t.x;
  vec3 x1 = c01 * (1.0 - t.x) + c11 * t.x;

  return x0 * (1.0 - t.y) + x1 * t.y;
}

#endif