#ifndef RADIANCE_CASCADES_SHARED
#define RADIANCE_CASCADES_SHARED

#include <hotcart/types.h>

struct RadianceCascadesConfig {
  // cascade level 0 config
  u32 gridDiameter;
  f32 rayLength;
  f32 scale;

  // octahedral atlas config
  u32 atlasProbeDiameter;
  u32 atlasGridDiameter;

  u32 debugFlags;
  u32 baseDiameter;
  i32 maxLevel;

  u32 branchingFactor;
  u32 pad0;
  u32 pad1;
  u32 pad2;
};

#define OCTAPROBE_DEBUG_RENDER_PROBES (1<<0)

#define OCTAPROBE_PADDING 1
#define OCTAPROBE_PADDED_DIAMETER(v) ((v) + OCTAPROBE_PADDING * 2)

#define OCTAPROBE_GROWTH_FACTOR (1)

#endif