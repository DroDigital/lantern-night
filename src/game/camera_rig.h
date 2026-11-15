#pragma once
// Third-person orbit camera: smooth follow, sphere-cast collision against level colliders
// (pulls in so it never clips through walls), lock-on framing and light camera shake.
#include "engine/data_store.h"
#include "raylib.h"
#include "raymath.h"

namespace lk {

class Physics;

struct CameraTuning {
    float distance = 4.5f, minDistance = 0.6f, pivotHeight = 1.15f;
    float pitchDefault = 22.0f, pitchMin = -35.0f, pitchMax = 65.0f;
    float followSharpness = 12.0f, collisionRadius = 0.25f, collisionIn = 30.0f, collisionOut = 4.0f;
    float fov = 55.0f, lockonPitch = 18.0f, lockonTurn = 8.0f, lockonShoulder = 0.55f, lockonDistance = 4.8f;
    float shakeDecay = 7.0f, shakeFrequency = 32.0f;
    void Load(const json& j);
};

class CameraRig {
public:
    void Reset(Vector3 target, float yawRad);
    // look: input delta (radians). lockTarget: world point to keep in frame (nullptr if none).
    // lockHeight: height of the locked target (big targets pull the camera back and up).
    void Update(float dt, Vector3 followPos, Vector2 look, const Vector3* lockTarget, const Physics* physics, float lockHeight = 1.2f);
    void AddShake(float amount) { shake_ = std::fmin(1.0f, shake_ + amount); }
    void SetShakeEnabled(bool on) { shakeEnabled_ = on; }

    Camera3D Cam() const { return cam_; }
    float Yaw() const { return yaw_; }
    // Camera-relative basis on the ground plane.
    Vector3 Forward() const;
    Vector3 Right() const;

    CameraTuning tuning;

private:
    Camera3D cam_{};
    Vector3 pivot_{};
    float yaw_ = 0.0f;          // radians, 0 looks toward -Z
    float pitch_ = 0.35f;       // radians, positive looks down
    float curDist_ = 4.5f;
    float shake_ = 0.0f, shakeTime_ = 0.0f;
    float shoulder_ = 0.0f;
    float squeeze_ = 0.0f;
    bool shakeEnabled_ = true;
};

}  // namespace lk
