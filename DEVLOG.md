## Next
- DEBUG: add a toggle to sample from probes sampled along a DDA
        to effectively add lit fog to show the cascades - maybe
        pre-cook this into a small voxel grid?
- use a grid and deinterleave directions
- add another scene or subscene for testing reflections
  + see: https://discord.com/channels/318590007881236480/1142751596858593372/1202525936138854400
- allow non-cubic cascade diameters
  + this changes how the probe atlas packing should work. My
    knee jerk for how this should work is you do the two smallest
    axes multiplied to get the height and then you use the largest axis as the width.
  + confirm that rectangular packing will not degenerate when
    using non-cubic cascades
  + convert everything over to use the rectangular packing??
- store ray data in octahedral maps
  + https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/


## Pending
- debug: add computed probe view
- debug: merged probe view should manually pentalinear interpolate so we can
         see the difference between the merged and the expected.

## 2024-02-08
- debug: add a debug view for a probe and its upper probes

## 2024-02-07
- make sure rays start where they are supposed to
## 2024-02-06
- DEBUG: add scale slider
- DEBUG: add a toggle for rendering probes directly
- DEBUG: add a slider for controlling cascade level 0's interval length
## 2024-02-04
- when merging, fetch values from all 8 upper probes and trilinear interpolate the
  result into a single vec4
## 2024-02-03
- make scale work
- merge cascade levels down while stitching the octahedrons
- build upper cascade levels
- use a basic packing where each cascade gets a c0 sized section
- DEBUG: render the cascade chain with imgui
         maybe use https://twitter.com/SebAaltonen/status/1327188239451611139
## 2024-02-02
- DEBUG: sample probes from primary ray hit position as reflection
- fix the interpolation seams
  + for border pixels and associated pixels to copy see
    https://handmade.network/p/75/monter/blog/p/7288-engine_work__global_illumination_with_irradiance_probes
## 2024-02-01
- add linear interpolation (between pixels) for sampling probes WITH SEAMS
- add border around every probe
- convert rgba32ui texture to rgba32f
## 2024-01-31
- atlas texture for octahedral probes
- octahedral mapping functions
- cast a ray from every probe texel center
- draw probe values on the debug probes as reflections
## 2024-01-30
- visualize the probe grid
- add a way to map materials onto objects
- add a room with a single emitter scene