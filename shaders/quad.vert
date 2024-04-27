#version 450

const vec2 verts[6] = vec2[6](vec2(0.0, 0.0),
                              vec2(1.0, 1.0),
                              vec2(0.0, 1.0),
                              vec2(0.0, 0.0),
                              vec2(1.0, 0.0),
                              vec2(1.0, 1.0));

layout(location = 0) out vec2 uv;

layout(location = 0) uniform vec2 screenDims;
layout(location = 1) uniform sampler2DArray octahedralProbeAtlasTexture;
layout(location = 2) uniform int level;

layout(location = 3) uniform vec2 corner;
layout(location = 4) uniform vec2 dims;


void
main() {
  uv = verts[gl_VertexID];
  float aspect = screenDims.x / screenDims.y;

  gl_Position = vec4((corner + uv * dims * vec2(1.0, aspect)) * 2.0 - 1.0, 0, 1.0);
}