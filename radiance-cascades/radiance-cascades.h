#pragma once

#include <engine/dust.h>
#include <engine/gpu/morton.h>

#include "shared.h"

struct RadianceCascades {
  RadianceCascadesConfig config;

  struct Debug {
    f32 atlasScale;
    i32 renderProbeLevel;

    bool renderAtlasContents;
    bool renderAtlasFollowCamera;

    bool debugMerge;
    v2 debugProbeMergeRotation;
    i32 debugProbeMergeLevel;
    v3u32 debugProbeMergeGridPos;

    float mergeTexelGatherOffset;
    float mergeTexelGatherRatio;
    bool mergeTexelSampleOriginal;

    bool dirty;
  };
  Debug debug;

  u32 cascade0ProbeCount;
  u32 probeAtlasByteSize;
  u32 totalLevels;

  Texture octahedralProbeAtlas;
  Texture octahedralProbeAtlasOriginal;
  SSBO configUBO;
};

inline RadianceCascadesConfig
RadianceCascadesDefaultConfig() {
  return {.gridDiameter = 64,
          .rayLength = 0.05,
          .scale = 0.25,
          .atlasProbeDiameter = 6,
          .maxLevel = -1,
          .branchingFactor = 1};
}

static void
RadianceCascadesInit(RadianceCascades &cascades,
                     RadianceCascadesConfig config = RadianceCascadesDefaultConfig()) {
  if (cascades.config.gridDiameter == 0) {
    cascades.config = config;

    cascades.debug.atlasScale = 1.0;
    cascades.debug.renderProbeLevel = -1;
    cascades.debug.mergeTexelGatherOffset = 1.0f;
    cascades.debug.mergeTexelGatherRatio = 0.75f;
    cascades.debug.mergeTexelSampleOriginal = false;
  }
  cascades.cascade0ProbeCount = Pow(config.gridDiameter, 3);

  u32 gridDiameter = NextPowerOfTwo(Sqrt(cascades.cascade0ProbeCount));
  cascades.config.baseDiameter = NextPowerOfTwo(
    gridDiameter * OCTAPROBE_PADDED_DIAMETER(config.atlasProbeDiameter));

  cascades.config.atlasGridDiameter = gridDiameter;
  cascades.debug.dirty = true;
  // Compute the total number of levels
  {
    cascades.totalLevels = 0;

    f32 level = 0.0;
    while (true) {
      u32 level = cascades.totalLevels;
      f32 spacing = Pow(2.0f, f32(level));
      f32 invSpacing = 1.0f / spacing;

      f32 probeCount = Pow(invSpacing, 3) * f32(cascades.cascade0ProbeCount);
      if (probeCount < 1.0) {
        break;
      }
      cascades.totalLevels++;
    }
  }

  printf("init texture width: %u\n", cascades.config.baseDiameter);
  printf("      total probes: %u\n", cascades.cascade0ProbeCount);
  printf("      total levels: %u\n", cascades.totalLevels);
  printf("     grid diameter: %u\n", cascades.config.atlasGridDiameter);
  printf("      total size: %.2fMB\n",
         f64(Pow(cascades.config.baseDiameter, 2) * 16 * cascades.totalLevels) /
           (1024.0 * 1024.0));
  if (GLTextureInit2DArray(cascades.octahedralProbeAtlas,
                           cascades.config.baseDiameter,
                           cascades.config.baseDiameter,
                           cascades.totalLevels,
                           GL_RGBA32F)) {
    glObjectLabel(GL_TEXTURE,
                  cascades.octahedralProbeAtlas.handle,
                  -1,
                  "RadianceCascades/OctahedralProbeAtlas");
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  if (GLTextureInit2DArray(cascades.octahedralProbeAtlasOriginal,
                           cascades.config.baseDiameter,
                           cascades.config.baseDiameter,
                           cascades.totalLevels,
                           GL_RGBA32F)) {
    glObjectLabel(GL_TEXTURE,
                  cascades.octahedralProbeAtlasOriginal.handle,
                  -1,
                  "RadianceCascades/OctahedralProbeAtlasOriginal");
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  SSBOInit(cascades.configUBO,
           sizeof(RadianceCascadesConfig),
           "RadianceCascades/Config",
           GL_DYNAMIC_STORAGE_BIT,
           GL_UNIFORM_BUFFER,
           (void *)&cascades.config);
}

static void
RadianceCascadesTick(RadianceCascades &cascades, const MemoryArena &scratchArena) {
  if (!cascades.debug.dirty) {
    return;
  }
  cascades.debug.dirty = false;

  glClearTexImage(cascades.octahedralProbeAtlas.handle, 0, GL_RGBA, GL_FLOAT, nullptr);

  // Build cascade levels
  {
    GLProgram *program = GLComputeProgram(scratchArena,
                                          "shaders/radiance-cascades-build.comp");
    if (program) {
      glUseProgram(program->handle);
      Bind(cascades.configUBO, 0, GL_UNIFORM_BUFFER);
      glBindImageTexture(1,
                         cascades.octahedralProbeAtlas.handle,
                         0,
                         0,
                         0,
                         GL_WRITE_ONLY,
                         GL_RGBA32F);
      ImGui::Text("RadianceCascadesTick/BuildCascadeLevels\n");

      const int maxLevel = cascades.config.maxLevel == -1 ? cascades.totalLevels - 2
                                                          : cascades.config.maxLevel;

      for (i32 level = maxLevel; level >= 0; level--) {
        u32 atlasProbeDiameter = cascades.config.atlasProbeDiameter * Pow(2u, level);
        u32 levelProbeCount = cascades.cascade0ProbeCount >> (level * 3);
        u32 levelRayCount = atlasProbeDiameter * atlasProbeDiameter;
        // ImGui::Text("  level %u\n", level);
        // ImGui::Text("    rays(%u) totalProbes(%u)\n", levelRayCount, levelProbeCount);

        glUniform1ui(0, level);

        u32 scalingFactor = 2;
        v2 rayRange = v2(level == 0 ? 0 : f32(1 << ((level - 1) * scalingFactor)),
                         f32(1 << (level * scalingFactor))) *
                      cascades.config.rayLength;

        ImGui::Text("level %u rayRange(%f, %f)\n", level, rayRange.x, rayRange.y);
        glUniform2f(1, rayRange.x, rayRange.y);

        GLDispatch(program, levelRayCount * levelProbeCount);
      }
    }
  }

  bool keepOriginalAtlasCopy = true;
  // Copy into debug original texture
  if (keepOriginalAtlasCopy) {
    glCopyImageSubData(cascades.octahedralProbeAtlas.handle,
                       GL_TEXTURE_2D_ARRAY,
                       0,
                       0,
                       0,
                       0,
                       cascades.octahedralProbeAtlasOriginal.handle,
                       GL_TEXTURE_2D_ARRAY,
                       0,
                       0,
                       0,
                       0,
                       cascades.octahedralProbeAtlas.width,
                       cascades.octahedralProbeAtlas.height,
                       cascades.octahedralProbeAtlas.depth);
  }

  // Merge and stitch
  if (1) {

    const int maxLevel = cascades.config.maxLevel == -1 ? cascades.totalLevels - 2
                                                        : cascades.config.maxLevel + 1;

    GLProgram *octahedronStitchingprogram = GLComputeProgram(
      scratchArena, "shaders/radiance-cascades-octaprobe-stitch.comp");

    // DEBUG: stitch the original atlas probes
    if (keepOriginalAtlasCopy && octahedronStitchingprogram) {
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
      glUseProgram(octahedronStitchingprogram->handle);
      Bind(cascades.configUBO, 0, GL_UNIFORM_BUFFER);
      glBindImageTexture(1,
                         cascades.octahedralProbeAtlasOriginal.handle,
                         0,
                         0,
                         0,
                         GL_READ_WRITE,
                         GL_RGBA32F);

      for (i32 level = maxLevel; level >= 0; level--) {
        glUniform1ui(0, level);
        const u32 levelProbeCount = cascades.cascade0ProbeCount >> (level * 3);
        glUniform1ui(1, levelProbeCount);
        ImGui::Text("stitch original: %i probes: %u", level, levelProbeCount);
        GLDispatch(octahedronStitchingprogram, levelProbeCount);
      }
    }
    if (octahedronStitchingprogram) {
      glUseProgram(octahedronStitchingprogram->handle);
      Bind(cascades.configUBO, 0, GL_UNIFORM_BUFFER);
      glBindImageTexture(1,
                         cascades.octahedralProbeAtlas.handle,
                         0,
                         0,
                         0,
                         GL_READ_WRITE,
                         GL_RGBA32F);
    }

    GLProgram
      *cascadeMergeProgram = GLComputeProgram(scratchArena,
                                              "shaders/radiance-cascades-merge.comp");

    if (cascadeMergeProgram) {
      glUseProgram(cascadeMergeProgram->handle);
      Bind(cascades.configUBO, 0, GL_UNIFORM_BUFFER);
      glBindImageTexture(1,
                         cascades.octahedralProbeAtlas.handle,
                         0,
                         0,
                         0,
                         GL_READ_WRITE,
                         GL_RGBA32F);
      glUniform1f(2, cascades.debug.mergeTexelGatherOffset);
      glUniform1f(3, cascades.debug.mergeTexelGatherRatio);
    }
    // for (i32 level = cascades.totalLevels - 2; level >= 0; level--) {
    for (i32 level = maxLevel + 1; level >= 0; level--) {
      ImGui::Text("grid diameter: %u at level: %u / %u",
                  cascades.config.atlasGridDiameter,
                  level,
                  cascades.config.gridDiameter >> (level + 1));

      u32 atlasProbeDiameter = cascades.config.atlasProbeDiameter * Pow(2u, level);

      // u32 atlasProbeDiameter = cascades.config.atlasProbeDiameter
      //                          << (level * cascades.config.branchingFactor);
      u32 levelProbeCount = cascades.cascade0ProbeCount >> (level * 3);
      u32 levelRayCount = atlasProbeDiameter * atlasProbeDiameter * levelProbeCount;

      // Merge down
      if (cascadeMergeProgram) {
        ImGui::Text("merge: %i rays: %i", level, levelRayCount);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                        GL_TEXTURE_FETCH_BARRIER_BIT);
        glUseProgram(cascadeMergeProgram->handle);
        glUniform1ui(0, level);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, cascades.octahedralProbeAtlas.handle);
        glUniform1i(1, 0);

        glUniform1ui(4, maxLevel);
        GLDispatch(cascadeMergeProgram, levelRayCount);
      }

      // Stich the Octahedron folds
      if (octahedronStitchingprogram) {
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                        GL_TEXTURE_FETCH_BARRIER_BIT);
        glUseProgram(octahedronStitchingprogram->handle);
        glUniform1ui(0, level);
        const u32 levelProbeCount = cascades.cascade0ProbeCount >> (level * 3);
        glUniform1ui(2, levelProbeCount);
        ImGui::Text("stitch: %i probes: %u", level, levelProbeCount);
        GLDispatch(octahedronStitchingprogram, levelProbeCount);
      }
    }
  }
}

static void
RadianceCascadesDebugInfo(RadianceCascades &cascades,
                          const MemoryArena &arena,
                          v3 cameraEye) {
  CartridgeContext *ctx = CartContext();

  bool configDirty = false;
  if (!ImGui::GetIO().WantCaptureKeyboard) {
    if (ButtonReleased(ctx->inputs.keyboard, Key1)) {
      cascades.config.maxLevel = 0;
      configDirty = true;
    }
    if (ButtonPressed(ctx->inputs.keyboard, Key2)) {
      cascades.config.maxLevel = 1;
      configDirty = true;
    }
    if (ButtonPressed(ctx->inputs.keyboard, Key3)) {
      cascades.config.maxLevel = 2;
      configDirty = true;
    }
    if (ButtonPressed(ctx->inputs.keyboard, Key4)) {
      cascades.config.maxLevel = 3;
      configDirty = true;
    }
    if (ButtonPressed(ctx->inputs.keyboard, Key0)) {
      cascades.config.maxLevel = -1;
      configDirty = true;
    }
  }

  // Render the debug ui
  {
    ImGui::Begin("Radiance Cascades Config");
    AccumulateOr(configDirty,
                 ImGui::DragFloat("c0 ray length",
                                  &cascades.config.rayLength,
                                  0.001f,
                                  0.01f,
                                  FLT_MAX));

    AccumulateOr(configDirty,
                 ImGui::DragFloat("probe grid scale",
                                  &cascades.config.scale,
                                  0.01f,
                                  0.01f,
                                  10.0f));

    AccumulateOr(configDirty,
                 ImGui::DragInt("max level",
                                &cascades.config.maxLevel,
                                0.05f,
                                -1,
                                cascades.totalLevels - 2));
    AccumulateOr(configDirty,
                 ImGui::DragInt("branching factor 2^i",
                                (int *)&cascades.config.branchingFactor,
                                0.05f,
                                1,
                                4));
    for (u32 i = 0; i < cascades.totalLevels; i++) {
      ImGui::Text("%u %u", i, cascades.config.atlasProbeDiameter * Pow(2u, i));
    }

    if (configDirty) {
      glNamedBufferSubData(cascades.configUBO.handle,
                           0,
                           sizeof(RadianceCascadesConfig),
                           &cascades.config);
      cascades.debug.dirty = true;
    }

    ImGui::Spacing();
    ImGui::Text("Debug - Render Atlas");
    ImGui::Indent();
    ImGui::Checkbox("enable##render-atlas-enable", &cascades.debug.renderAtlasContents);
    ImGui::Checkbox("follow camera##render-atlas-follow-camera",
                    &cascades.debug.renderAtlasFollowCamera);
    ImGui::DragFloat("scale##render-atlas-scale",
                     &cascades.debug.atlasScale,
                     0.01f,
                     0.01f,
                     100.0f);
    ImGui::Unindent();
    ImGui::Spacing();
    ImGui::Text("Debug - Render Probes");
    ImGui::DragInt("render probes @ level",
                   &cascades.debug.renderProbeLevel,
                   0.05f,
                   -1,
                   i32(cascades.totalLevels - 1));

    ImGui::DragInt3("gridPos",
                    (i32 *)&cascades.debug.debugProbeMergeGridPos,
                    0.1f,
                    0,
                    cascades.config.atlasGridDiameter);
    ImGui::Checkbox("debug merge", &cascades.debug.debugMerge);

    AccumulateOr(cascades.debug.dirty,
                 ImGui::DragFloat("gather radius",
                                  &cascades.debug.mergeTexelGatherOffset,
                                  0.01f,
                                  0.00f,
                                  10.0f));
    AccumulateOr(cascades.debug.dirty,
                 ImGui::DragFloat("gather ratio",
                                  &cascades.debug.mergeTexelGatherRatio,
                                  0.01f,
                                  0.0f,
                                  1.0f));
    ImGui::Checkbox("sample original", &cascades.debug.mergeTexelSampleOriginal);

    ImGui::End();
  }

  // DEBUG: render a probe, its 8 uppers and the merge result
  if (cascades.debug.debugMerge) {
    GLProgram *program = GLRasterProgram(arena,
                                         "shaders/debug-probe-merge.frag",
                                         "shaders/quad.vert");
    if (program) {
      glUseProgram(program->handle);

      v2 screenDims(ctx->opengl.width, ctx->opengl.height);
      glUniform2fv(0, 1, (const GLfloat *)&screenDims);

      f32 t = f64(CartContext()->TimeNowMilliseconds()) / 1000.0 * 2.0;
      f32 cameraDistance = 1.5;
      // v3 pos = v3(Cos(t), 0.0, Sin(t)) * cameraDistance;

      // arrow keys for rotation
      {
        f32 speed = 0.025f;
        const Keyboard &keyboard = CartContext()->inputs.keyboard;
        v2 &rot = cascades.debug.debugProbeMergeRotation;
        if (ButtonDown(keyboard, KeyLeft)) {
          rot.x -= speed;
        }
        if (ButtonDown(keyboard, KeyRight)) {
          rot.x += speed;
        }

        if (ButtonDown(keyboard, KeyUp)) {
          rot.y -= speed;
        }
        if (ButtonDown(keyboard, KeyDown)) {
          rot.y += speed;
        }

        if (ButtonPressed(keyboard, KeyPageUp)) {
          cascades.debug.debugProbeMergeLevel = Min(cascades.debug.debugProbeMergeLevel +
                                                      1,
                                                    int(cascades.totalLevels - 1));
        }

        if (ButtonPressed(keyboard, KeyPageDown)) {
          cascades.debug.debugProbeMergeLevel = Max(cascades.debug.debugProbeMergeLevel -
                                                      1,
                                                    0);
        }

        rot.y = Clamp(rot.y, -piF32 * 0.49f, piF32 * 0.49f);
      }

      quat rot = Normalize(
        AngleAxis(cascades.debug.debugProbeMergeRotation.x, v3(0.0f, 1.0f, 0.0f)) *
        AngleAxis(cascades.debug.debugProbeMergeRotation.y, v3(1.0f, 0.0f, 0.0f)));
      v3 pos = Rotate(v3(0.0, 0.0, cameraDistance), rot);

      m4 view = LookAt(pos, v3(0.0), v3(0.0, 1.0, 0.0));
      m4 proj = Perspective(90.0 * degreesToRadiansF32, 1.0f, 0.001f, 2.0f);
      m4 worldToScreen = proj * view;
      m4 screenToWorld = Invert(worldToScreen);

      int level = 0;
      f32 diameter = 0.125;
      f32 radius = diameter * 0.5f;

      ImDrawList *dl = ImGui::GetBackgroundDrawList();

      // draw the orignal probe
      {
        v2 labelPos = screenDims * diameter + v2(-radius * 0.25, radius) * screenDims;
        labelPos.y = screenDims.y - labelPos.y;
        dl->AddText(labelPos, 0xFFFFFFFF, "original");
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, cascades.octahedralProbeAtlasOriginal.handle);
        glUniform1i(1, 0);

        glUniform1i(2, cascades.debug.debugProbeMergeLevel);
        glUniform2f(3, 0.0, 0.0);
        glUniform2f(4, diameter, diameter);

        glUniformMatrix4fv(5, 1, 0, (const GLfloat *)&screenToWorld);
        glUniform3f(6, pos.x, pos.y, pos.z);
        glUniform3i(7,
                    cascades.debug.debugProbeMergeGridPos.x,
                    cascades.debug.debugProbeMergeGridPos.y,
                    cascades.debug.debugProbeMergeGridPos.z);

        // computedMergeValue
        glUniform1ui(8, 0);

        glUniform1f(9, cascades.debug.mergeTexelGatherOffset);
        glUniform1f(10, cascades.debug.mergeTexelGatherRatio);

        glDrawArrays(GL_TRIANGLES, 0, 6);
      }

      // draw the merged probe
      {
        v2 corner = v2(0.0f, diameter * 2.0f);
        v2 labelPos = screenDims * diameter + corner * screenDims +
                      v2(-radius * 0.25, radius) * screenDims;
        labelPos.y = screenDims.y - labelPos.y;
        dl->AddText(labelPos, 0xFFFFFFFF, "merged");

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, cascades.octahedralProbeAtlas.handle);
        glUniform1i(1, 0);

        glUniform1i(2, cascades.debug.debugProbeMergeLevel);
        glUniform2f(3, corner.x, corner.y);
        glUniform2f(4, diameter, diameter);

        glUniformMatrix4fv(5, 1, 0, (const GLfloat *)&screenToWorld);
        glUniform3f(6, pos.x, pos.y, pos.z);
        glUniform3i(7,
                    cascades.debug.debugProbeMergeGridPos.x,
                    cascades.debug.debugProbeMergeGridPos.y,
                    cascades.debug.debugProbeMergeGridPos.z);

        // computed
        glUniform1ui(8, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);
      }

      // draw the computed probe
      {
        v2 corner = v2(0.0f, diameter * 4.0f);
        v2 labelPos = screenDims * diameter + corner * screenDims +
                      v2(-radius * 0.25, radius) * screenDims;
        labelPos.y = screenDims.y - labelPos.y;
        dl->AddText(labelPos, 0xFFFFFFFF, "computed");

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, cascades.octahedralProbeAtlasOriginal.handle);
        glUniform1i(1, 0);

        glUniform1i(2, cascades.debug.debugProbeMergeLevel);
        glUniform2f(3, corner.x, corner.y);
        glUniform2f(4, diameter, diameter);

        glUniformMatrix4fv(5, 1, 0, (const GLfloat *)&screenToWorld);
        glUniform3f(6, pos.x, pos.y, pos.z);
        glUniform3i(7,
                    cascades.debug.debugProbeMergeGridPos.x,
                    cascades.debug.debugProbeMergeGridPos.y,
                    cascades.debug.debugProbeMergeGridPos.z);
        // computed
        glUniform1ui(8, 1);
        glDrawArrays(GL_TRIANGLES, 0, 6);
      }

      for (u32 upper = 0; upper < 8; upper++) {
        v3 gp = v3(cascades.debug.debugProbeMergeGridPos);

        u32 lowerLevel = cascades.debug.debugProbeMergeLevel;
        v3 lowerGridPos = v3(cascades.debug.debugProbeMergeGridPos) + 0.5;
        f32 upperGridDiameter = f32(cascades.config.gridDiameter >> (lowerLevel + 1));
        v3 upperOffset = v3(MortonDecode(upper));
        v3 upperGridPos = Clamp(Floor(lowerGridPos * 0.5f - 0.5f) + upperOffset,
                                v3(0.0f),
                                upperGridDiameter);
        v3 uvw = Fract(lowerGridPos * 0.5f + 0.5f);
        Dust::Info("upperGridPos: %.2f, %.2f, %.2f uvw: %.2f, %.2f, %.2f",
                   upperGridPos.x,
                   upperGridPos.y,
                   upperGridPos.z,
                   uvw.x,
                   uvw.y,
                   uvw.z);

        v2 corner = v2(MortonDecodeX(upper) + 2, MortonDecodeY(upper)) *
                    v2(diameter, diameter * 2.0);

        // Output a textual label for
        {
          v2 labelPos = screenDims * diameter + corner * screenDims +
                        v2(-radius * 0.25, radius) * screenDims;
          labelPos.y = screenDims.y - labelPos.y;
          char tmp[256];
          snprintf(tmp,
                   sizeof(tmp),
                   "(%i,%i,%i)",
                   i32(upperGridPos.x),
                   i32(upperGridPos.y),
                   i32(upperGridPos.z));
          dl->AddText(labelPos, 0xFFFFFFFF, tmp);
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, cascades.octahedralProbeAtlasOriginal.handle);
        glUniform1i(1, 0);

        glUniform1i(2, cascades.debug.debugProbeMergeLevel + 1);
        glUniform2f(3, corner.x, corner.y);
        glUniform2f(4, diameter, diameter);

        glUniformMatrix4fv(5, 1, 0, (const GLfloat *)&screenToWorld);
        glUniform3f(6, pos.x, pos.y, pos.z);
        glUniform3i(7, upperGridPos.x, upperGridPos.y, upperGridPos.z);

        // computed
        glUniform1ui(8, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);
      }
    }
  }

  // Render atlas contents
  if (cascades.debug.renderAtlasContents) {
    GLProgram *program = GLRasterProgram(arena,
                                         "shaders/radiance-cascade-atlas-debug.frag",
                                         "shaders/quad.vert");

    if (program) {
      m4 proj = Orthographic(0.0, 1.0, 1.0, 0.0, 1.0, 0.01f);

      glUseProgram(program->handle);

      glDisable(GL_DEPTH_TEST);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(false);

      v2 screenDims(ctx->opengl.width, ctx->opengl.height);
      glUniform2fv(0, 1, (const GLfloat *)&screenDims);

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, cascades.octahedralProbeAtlas.handle);
      glUniform1i(1, 0);

      Bind(cascades.configUBO, 0, GL_UNIFORM_BUFFER);

      glUniform1f(5, cascades.debug.atlasScale);

      ImGui::Text("eye(%.0f, %.0f, %.0f)", cameraEye.x, cameraEye.y, cameraEye.z);

      f32 stride = 1.0 / f32(cascades.totalLevels);
      f32 x = 0.0;
      for (u32 level = 0; level < cascades.totalLevels; level++) {
        glUniform1i(2, level);
        glUniform2f(3, x, 0.0);
        glUniform2f(4, stride, stride);

        v2 offset(0.0f);

        if (cascades.debug.renderAtlasFollowCamera) {
          v3 gridDiameter = v3(cascades.config.gridDiameter >> level);
          v3 gridRadius = gridDiameter * 0.5f;
          v3 cellDiameter = v3(1 << level) * cascades.config.scale;
          v3 cellRadius = cellDiameter * 0.5f;

          v3 probeGridPos = (cameraEye / cellDiameter) + gridRadius;
          u32 probeIndex = MortonEncode(
            v3u32(Clamp(probeGridPos, v3(0.0f), v3(cascades.config.atlasGridDiameter))));
          u32 atlasProbeDiameter = cascades.config.atlasProbeDiameter * Pow(2, level);
          offset = v2(MortonDecode2D(probeIndex) *
                      OCTAPROBE_PADDED_DIAMETER(atlasProbeDiameter));

          ImGui::Text("%u offset(%f, %f) grid(%.0f, %.0f, %.0f)",
                      level,
                      offset.x,
                      offset.y,
                      probeGridPos.x,
                      probeGridPos.y,
                      probeGridPos.z);
        }

        glUniform2f(6, offset.x, offset.y);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += stride;
      }

      glDisable(GL_BLEND);
      glDisable(GL_BLEND);
    }
  }

  // DEBUG: output the cascade sizes
  if (0) {
    ImGui::Begin("cascade atlas packing");
    u32 level = 0;

    ImDrawList *dl = ImGui::GetForegroundDrawList();

    ImVec2 wmin = ImGui::GetWindowContentRegionMin();
    wmin.x += ImGui::GetWindowPos().x;
    wmin.y += ImGui::GetWindowPos().y;

    v2 potOffset(300.0f, 50.0f);
    const v2 rectOffset(300.0f, 400.0f);

    i32 s = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &s);
    ImGui::Text("max texture size: %i", s);
    while (true) {
      f32 spacing = Pow(2.0f, f32(level));
      f32 invSpacing = 1.0f / spacing;

      f32 probeCount = Pow(invSpacing, 3) * f32(cascades.cascade0ProbeCount);
      if (probeCount < 1.0) {
        break;
      }

      ImGui::Text("Level %u ProbeGridDiameter(%u)\n",
                  level,
                  cascades.config.gridDiameter << level);

      f32 probeDiameter = spacing * f32(cascades.config.atlasProbeDiameter);
      f32 probeTexels = Pow(probeDiameter, 2);
      f32 pocCascadeDiameter = NextPowerOfTwo(probeDiameter * f32(Sqrt(probeCount)));

      // POT
      {
        dl->AddRect(v2(wmin.x + potOffset.x, wmin.y + potOffset.y),
                    v2(wmin.x + potOffset.x + pocCascadeDiameter,
                       wmin.y + pocCascadeDiameter + potOffset.y),
                    0xFFFFFFFF,
                    0.0,
                    0,
                    0.1);

        f32 actualDiamter = probeDiameter * f32(Sqrt(probeCount));

        dl->AddRect(v2(wmin.x + potOffset.x, wmin.y + potOffset.y),
                    v2(wmin.x + potOffset.x + actualDiamter,
                       wmin.y + actualDiamter + potOffset.y),
                    0xFF00FFFF,
                    0.0,
                    0,
                    0.1);

        potOffset.x += pocCascadeDiameter + 2;
      }

      // Rectangle
      {
        const v2 BaseDims(f32(cascades.config.gridDiameter *
                              cascades.config.atlasProbeDiameter),
                          Pow(f32(cascades.config.gridDiameter), 2.0f) *
                            cascades.config.atlasProbeDiameter);

        f32 height = Pow(cascades.config.gridDiameter >> level, 2);

        v2 lo(0.0, 0.0f);
        v2 dims(BaseDims.x, height * probeDiameter);

        if (level > 0) {
          lo.x = BaseDims.x + 2.0;
          lo.y = BaseDims.y - dims.y * 2.0 + (2 * (level - 1));
        }
        dl->AddRect(wmin + rectOffset + lo, wmin + rectOffset + lo + dims, 0xFFFFFFFF);
      }

      ImGui::Text("                2^2: %.0f", spacing);
      ImGui::Text("            1/(2^i): %.3f", invSpacing);
      ImGui::Text("        probe count: %.0f", probeCount);
      ImGui::Text("  sqrt(probe count): %.0f", Sqrt(probeCount));
      ImGui::Text("     probe diameter: %.0f", probeDiameter);
      ImGui::Text("       probe pixels: %.0f", probeTexels);
      ImGui::Text("   cascade diameter: %.0f", pocCascadeDiameter);
      ImGui::Text("               rays: %.0f", probeCount * probeTexels);

      ImGui::Text("");
      level++;
    }
    ImGui::End();
  }

  // DEBUG: render probe bounding quads
  if (0) {
    for (i32 level = 0; level < i32(cascades.totalLevels); level++) {

      const f32 gridDiameter = f32(cascades.config.gridDiameter >> level);
      const f32 gridRadius = gridDiameter * 0.5f;
      const f32 cellDiameter = cascades.config.scale * f32(1 << level);
      const f32 cellRadius = cellDiameter * 0.5f;

      const f32 circleRadius = f32(level + 1) * 3.0;
      ImDrawList *dl = ImGui::GetBackgroundDrawList();
      for (u32 gx = 0; gx < gridDiameter; gx++) {
        for (u32 gy = 0; gy < gridDiameter; gy++) {
          for (u32 gz = 0; gz < gridDiameter; gz++) {

            v3 p = (v3(gx, gy, gz) * cellDiameter) - gridRadius * cellDiameter +
                   cellRadius;

            v2 center = Dust::ProjectPoint(p);
            u32 index = MortonEncode(gx, gy, gz);

            v3i32 col = i32(index + 1) * v3i32(158, 2 * 156, 3 * 159);
            v3 color = v3(col.x % 255, col.y % 253, col.y % 127) / 255.0f;

            dl->AddCircleFilled(center, circleRadius, PackColor(v4(color, 1.0)));
            dl->AddCircle(center, circleRadius + 2, 0xFFFFFFFF);
          }
        }
      }
    }
  }

  // // DEBUG: output probe/ray counts per cascade
  // if (1) {
  //   u32 diameter = cascades.config.gridDiameter;
  //   u32 cascadeIndex = 0;
  //   ImGui::Text("probe counts");
  //   u32 totalRays = 0;
  //   u32 branchingFactor = 4;
  //   while (diameter > 0) {

  //     u32 raysPerFace = Pow(branchingFactor, cascadeIndex);
  //     u32 probeRayCount = raysPerFace * 6;

  //     u32 probeCount = Pow(diameter, 3);
  //     totalRays += probeRayCount * probeCount;
  //     ImGui::Text("  %u probes: %u probeRayCount: %u total: %u",
  //                 cascadeIndex,
  //                 probeCount,
  //                 probeRayCount,
  //                 probeRayCount * probeCount);

  //     cascadeIndex++;
  //     diameter = diameter >> 1;
  //   }
  //   ImGui::Text("total rays: %u", totalRays);
  // }

  // DEBUG: Draw a vertical line in the center of the screen
  if (false) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    float hw = float(ctx->opengl.width) * 0.5f;
    float h = float(ctx->opengl.height);
    dl->AddLine({hw, 0}, {hw, h}, 0xFFFFFFFF);
  }
}
