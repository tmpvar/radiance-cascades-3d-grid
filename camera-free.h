#pragma once

#include <engine/linalg.h>
#include <hotcart/debug.h>

struct FreeCameraState {
  bool initialized;
  f32 aspect;
  f32 minDepth;
  f32 maxDepth;

  f32 sensitivity;
  f32 acceleration;
  f32 accelerationSprintMultiplier;

  f64 lastTime;
  v2 lastMouse;
  v2 currentMouse;
  v2 accMouse;

  quat rot;
  v3 pos;
  v3 velocity;
  f32 dampingCoefficient;
  v3 dir;
};

struct FreeCamera {
  FreeCameraState state;
  m4 viewMatrix;
};

static void
InitFreeCamera(FreeCamera *camera, const v3 &pos, const f64 currentTime) {
  camera->state.acceleration = 0.5f;
  camera->state.dampingCoefficient = 3.0f;
  camera->state.accelerationSprintMultiplier = 5.0f;

  camera->state.maxDepth = 2.0f;
  if (!camera->state.initialized) {
    camera->state.sensitivity = 0.1f;
    camera->state.initialized = true;
    camera->state.minDepth = 0.000001f;

    camera->state.lastMouse = v2(0.5f);
    camera->state.currentMouse = v2(0.5f);

    camera->state.pos = pos;
    camera->state.rot = quat(0.0f, 0.0f, 0.0f, 1.0f);
    camera->state.lastTime = currentTime;
  }
}

static void
TickFreeCamera(FreeCamera *camera, const v2 screenDims, const f64 currentTime) {
  const f64 deltaTime = currentTime - camera->state.lastTime;
  Assert(deltaTime >= 0.0, "invalid dt");
  camera->state.lastTime = currentTime;

  camera->state.pos += f32(deltaTime) * camera->state.velocity;

  camera->state.velocity = Lerp(camera->state.velocity,
                                v3(0.0f),
                                f32(deltaTime) * camera->state.dampingCoefficient);

  // Rotation
  v2 mouseDelta = (camera->state.currentMouse - camera->state.lastMouse) *
                  camera->state.sensitivity;

  if (camera->state.currentMouse != camera->state.lastMouse) {
    mouseDelta.x = -mouseDelta.x;
    mouseDelta.y = -mouseDelta.y;
    camera->state.accMouse += mouseDelta;

    if (camera->state.accMouse.y > 89.0f) {
      camera->state.accMouse.y = 89.0f;
    }

    if (camera->state.accMouse.y < -89.0f) {
      camera->state.accMouse.y = -89.0f;
    }

    const quat rot = camera->state.rot;
    const v3 up = v3(0.0f, 1.0f, 0.0f);
    const v3 right = v3(1.0f, 0.0f, 0.0f);
    const quat horizontal = AngleAxis(camera->state.accMouse.x * degreesToRadiansF32, up);
    const quat vertical = AngleAxis(camera->state.accMouse.y * degreesToRadiansF32,
                                    right);
    // const quat combined = horizontal * rot *  vertical;
    const quat combined = horizontal * vertical;
    camera->state.rot = Normalize(combined);
  }
  camera->state.dir = Normalize(Rotate(v3(0.0f, 0.0f, -1.0f), camera->state.rot));
  camera->state.lastMouse = camera->state.currentMouse;
  camera->viewMatrix = LookAt(camera->state.pos,
                              camera->state.pos + camera->state.dir,
                              v3(0.0f, 1.0f, 0.0f));
}

static void
MoveFreeCamera(FreeCamera *camera, v3 dir, bool sprint) {
  f32 acceleration = camera->state.acceleration;
  if (sprint) {
    acceleration *= camera->state.accelerationSprintMultiplier;
  }
  camera->state.velocity -= Normalize(Rotate(dir, camera->state.rot)) * acceleration;
}

static void
RotateFreeCamera(FreeCamera *camera, const v2 currentMouse, bool resetLastPos) {
  if (resetLastPos) {
    camera->state.lastMouse = currentMouse;
  }
  camera->state.currentMouse = currentMouse;
}

static void
RotateFreeCameraDelta(FreeCamera *camera, const v2 delta) {
  camera->state.currentMouse = camera->state.lastMouse + delta;
}