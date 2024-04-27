#pragma hotcart(include, "../../../apps/dust")

#include <engine/dust.h>
#include <hotcart/hotcart.h>

#include "camera-free.h"
#include "radiance-cascades/radiance-cascades.h"

struct State {
  Dust::Engine dust;

  Dust::Entity scene;
  MemoryArena heapArena;
  MemoryArena scratchArena;

  Dust::Camera cameraEntity;

  FreeCamera camera;

  RadianceCascades radianceCascades;

  constexpr static u32 HeapSize = Megabytes(32);
  u8 heap[HeapSize];

  constexpr static u32 ScratchSize = Megabytes(32);
  u8 scratch[ScratchSize];
};

CartSetup() {
  CartridgeContext *ctx = CartContext();
  State *state = CartState<State>();
  ArenaInitOnce(state->heapArena, state->heap, state->HeapSize);
  ArenaInitOnce(state->scratchArena, state->scratch, state->ScratchSize);

  Dust::EngineInit(state->dust, state->heapArena);

  InitFreeCamera(&state->camera,
                 v3(0.0, 0.0, 5),
                 f64(ctx->TimeNowMilliseconds()) / 1000.0);
  state->camera.state.acceleration = 0.5f;
  {
    Dust::CameraConfig config = Dust::DefaultCameraConfig();
    config.fovRadians = 90.0f * degreesToRadiansF32;
    config.pose.pos = v3(0.0f, 0.0f, 20.0f);
    config.pose.rot = AngleAxis(piF32 * 0.5f, v3(-1.0f, 0.0f, 0.0f));
    Dust::CreateCameraOnce("camera/main", config);
  }

  RadianceCascadesInit(state->radianceCascades);
}

static void
ProcessInput() {
  CartridgeContext *ctx = CartContext();
  State *state = CartState<State>();
  // Camera control
  if (!ImGui::GetIO().WantCaptureMouse) {
    bool sprinting = ButtonDown(ctx->inputs.keyboard, KeyLeftShift) ||
                     ButtonDown(ctx->inputs.keyboard, KeyRightShift);

    if (ButtonDown(ctx->inputs.keyboard, KeyW)) {
      MoveFreeCamera(&state->camera, v3(0.0f, 0.0f, 1.0f), sprinting);
    }

    if (ButtonDown(ctx->inputs.keyboard, KeyS)) {
      MoveFreeCamera(&state->camera, v3(0.0f, 0.0f, -1.0f), sprinting);
    }

    if (ButtonDown(ctx->inputs.keyboard, KeyA)) {
      MoveFreeCamera(&state->camera, v3(1.0f, 0.0f, 0.0f), sprinting);
    }

    if (ButtonDown(ctx->inputs.keyboard, KeyD)) {
      MoveFreeCamera(&state->camera, v3(-1.0f, 0.0f, 0.0f), sprinting);
    }

    if (ButtonDown(ctx->inputs.keyboard, KeySpace)) {
      MoveFreeCamera(&state->camera, v3(0.0f, -1.0f, 0.0f), sprinting);
    }
    if (ButtonDown(ctx->inputs.keyboard, KeyLeftControl)) {
      MoveFreeCamera(&state->camera, v3(0.0f, 1.0f, 0.0f), sprinting);
    }

    // if (ctx->IsCursorLocked()) {
    if (ButtonDown(ctx->inputs.mouse, MouseButtonLeft)) {
      state->camera.state.sensitivity = 0.15f;
      RotateFreeCameraDelta(&state->camera,
                            v2(ctx->inputs.mouse.rawDelta.x,
                               ctx->inputs.mouse.rawDelta.y));
    }
  }

  TickFreeCamera(&state->camera,
                 v2(f32(ctx->opengl.width), f32(ctx->opengl.height)),
                 f64(ctx->TimeNowMilliseconds()) / 1000.0);
}

CartLoop() {
  CartridgeContext *ctx = CartContext();
  State *state = CartState<State>();
  ArenaReset(state->scratchArena);

  ProcessInput();

  Dust::Entity cameraEntity = Dust::CameraGetActive();
  Dust::Pose pose = Dust::GetPose(cameraEntity);
  pose.pos = state->camera.state.pos;
  pose.rot = state->camera.state.rot;
  Dust::SetPose(cameraEntity, pose);

  Dust::FrameBegin(state->scratchArena);

  const Dust::Frame &frame = Dust::GetCurrentFrame();
  Dust::Info("pose pos(%f, %f, %f)", pose.pos.x, pose.pos.y, pose.pos.z);

  RadianceCascadesTick(state->radianceCascades, state->scratchArena);

  // Render Fractal
  {
    GLProgram *program = GLRasterProgram(state->scratchArena,
                                         "shaders/fractal.frag",
                                         "engine/renderer/shaders/debug-fullscreen.vert");

    if (program) {
      glUseProgram(program->handle);

      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_ALWAYS);

      glUniformMatrix4fv(0, 1, 0, (const GLfloat *)&frame.worldToScreen);
      glUniformMatrix4fv(1, 1, 0, (const GLfloat *)&frame.screenToWorld);
      glUniform3fv(2, 1, (const GLfloat *)&frame.eye);

      Dust::Camera *camera = (Dust::Camera *)Dust::GetExternalPtr(cameraEntity);
      glUniform1f(3, camera->config.fovRadians);
      v2 screenDims(ctx->opengl.width, ctx->opengl.width);
      glUniform2fv(4, 1, (const GLfloat *)&screenDims);

      glActiveTexture(GL_TEXTURE0);
      if (state->radianceCascades.debug.mergeTexelSampleOriginal) {
        glBindTexture(GL_TEXTURE_2D_ARRAY,
                      state->radianceCascades.octahedralProbeAtlasOriginal.handle);
      } else {
        glBindTexture(GL_TEXTURE_2D_ARRAY,
                      state->radianceCascades.octahedralProbeAtlas.handle);
      }
      glUniform1i(5, 0);
      glUniform1i(6, state->radianceCascades.debug.renderProbeLevel);

      glUniform1f(7, state->radianceCascades.debug.mergeTexelGatherOffset);
      glUniform1f(8, state->radianceCascades.debug.mergeTexelGatherRatio);

      // TODO: memory barrier for image reading
      glBindImageTexture(1,
                         state->radianceCascades.octahedralProbeAtlas.handle,
                         0,
                         0,
                         0,
                         GL_READ_ONLY,
                         GL_RGBA32F);

      Bind(state->radianceCascades.configUBO, 0, GL_UNIFORM_BUFFER);

      glDrawArrays(GL_TRIANGLES, 0, 3);
    }
  }

  RadianceCascadesDebugInfo(state->radianceCascades,
                            state->scratchArena,
                            frame.eye);

  // ImGui::ShowDemoWindow();
  // ImPlot::ShowDemoWindow();
  // Dust::DemoDebug();

  Dust::FrameEnd();

  // This is where you would apply post processing effects to the resolved
  // texture before it is painted onto the screen

  Dust::FramePresent();
}
