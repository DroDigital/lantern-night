#include "game/camera_rig.h"

#include <algorithm>
#include <cmath>

#include "engine/physics.h"

namespace lk {

void CameraTuning::Load(const json& j) {
    distance = j.value("distance", distance);
    minDistance = j.value("min_distance", minDistance);
    hideKnightBelow = j.value("hide_knight_below", hideKnightBelow);
    pivotHeight = j.value("pivot_height", pivotHeight);
    pitchDefault = j.value("pitch_default_deg", pitchDefault);
    pitchMin = j.value("pitch_min_deg", pitchMin);
    pitchMax = j.value("pitch_max_deg", pitchMax);
    followSharpness = j.value("follow_sharpness", followSharpness);
    collisionRadius = j.value("collision_radius", collisionRadius);
    collisionOut = j.value("collision_sharpness_out", collisionOut);
    fov = j.value("fov_deg", fov);
    lockonPitch = j.value("lockon_pitch_deg", lockonPitch);
    lockonTurn = j.value("lockon_turn_sharpness", lockonTurn);
    lockonShoulder = j.value("lockon_shoulder", lockonShoulder);
    lockonDistance = j.value("lockon_distance", lockonDistance);
    shakeDecay = j.value("shake_decay", shakeDecay);
    shakeFrequency = j.value("shake_frequency", shakeFrequency);
}

static float Damp(float sharpness, float dt) { return 1.0f - std::exp(-sharpness * dt); }

static float WrapAngle(float a) {
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}

void CameraRig::Reset(Vector3 target, float yawRad) {
    pivot_ = Vector3Add(target, Vector3{0, tuning.pivotHeight, 0});
    yaw_ = yawRad;
    pitch_ = tuning.pitchDefault * DEG2RAD;
    curDist_ = tuning.distance;
    cam_.fovy = tuning.fov;
    cam_.projection = CAMERA_PERSPECTIVE;
    cam_.up = {0, 1, 0};
    Update(0.0f, target, {0, 0}, nullptr, nullptr);
}

Vector3 CameraRig::Forward() const { return Vector3{-std::sin(yaw_), 0, -std::cos(yaw_)}; }
Vector3 CameraRig::Right() const { return Vector3{std::cos(yaw_), 0, -std::sin(yaw_)}; }

void CameraRig::Update(float dt, Vector3 followPos, Vector2 look, const Vector3* lockTarget, const Physics* physics, float lockHeight) {
    float big = std::max(0.0f, lockHeight - 1.4f);
    Vector3 desiredPivot = Vector3Add(followPos, Vector3{0, tuning.pivotHeight, 0});
    pivot_ = dt > 0 ? Vector3Lerp(pivot_, desiredPivot, Damp(tuning.followSharpness, dt)) : desiredPivot;

    if (lockTarget) {
        // Face the target: yaw toward it, pitch so both stay in frame.
        Vector3 to = Vector3Subtract(*lockTarget, pivot_);
        float wantYaw = std::atan2(-to.x, -to.z);
        float dy = WrapAngle(wantYaw - yaw_);
        yaw_ = WrapAngle(yaw_ + dy * Damp(tuning.lockonTurn, dt));
        float flat = std::sqrt(to.x * to.x + to.z * to.z);
        float wantPitch = (tuning.lockonPitch + big * 4.0f) * DEG2RAD + std::atan2(-to.y, std::max(flat, 1.0f)) * 0.5f;
        pitch_ += (wantPitch - pitch_) * Damp(tuning.lockonTurn * 0.5f, dt);
    } else {
        yaw_ = WrapAngle(yaw_ - look.x);
        pitch_ += look.y;
    }
    pitch_ = std::clamp(pitch_, tuning.pitchMin * DEG2RAD, tuning.pitchMax * DEG2RAD);

    Vector3 back{std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_), std::cos(yaw_) * std::cos(pitch_)};
    // Locked on: slide over the knight's shoulder so the target is not hidden behind them.
    float wantShoulder = lockTarget ? tuning.lockonShoulder : 0.0f;
    shoulder_ += (wantShoulder - shoulder_) * (dt > 0 ? Damp(6.0f, dt) : 1.0f);
    Vector3 side = Vector3Scale(Right(), shoulder_);
    Vector3 basePivot = pivot_;
    pivot_ = Vector3Add(pivot_, side);
    float dist = lockTarget ? tuning.lockonDistance + big * 1.6f : tuning.distance;
    float want = dist;
    if (physics) {
        Vector3 far = Vector3Add(pivot_, Vector3Scale(back, dist));
        RayHit h = physics->SphereCast(basePivot, far, tuning.collisionRadius);
        if (h.hit) want = std::max(tuning.minDistance, dist * h.fraction - 0.05f);
    }
    // Pull in to a wall at once (never render from inside it); ease back out.
    if (dt <= 0 || want < curDist_) curDist_ = want;
    else curDist_ += (want - curDist_) * Damp(tuning.collisionOut, dt);

    // Squeezed against a wall: tilt up and look over the knight instead of hugging their back.
    float squeeze = std::clamp((dist - curDist_) / std::max(dist, 0.01f), 0.0f, 1.0f);
    squeeze_ += (squeeze - squeeze_) * (dt > 0 ? Damp(5.0f, dt) : 1.0f);
    if (squeeze_ > 0.01f) {
        float p2 = std::min(pitch_ + squeeze_ * 40.0f * DEG2RAD, 75.0f * DEG2RAD);
        Vector3 back2{std::sin(yaw_) * std::cos(p2), std::sin(p2), std::cos(yaw_) * std::cos(p2)};
        float d2 = dist;
        if (physics) {
            RayHit h2 = physics->SphereCast(basePivot, Vector3Add(pivot_, Vector3Scale(back2, dist)), tuning.collisionRadius);
            if (h2.hit) d2 = std::max(tuning.minDistance, dist * h2.fraction - 0.05f);
        }
        if (d2 > curDist_) {
            back = back2;
            curDist_ += (d2 - curDist_) * (dt > 0 ? Damp(tuning.collisionOut * 2.0f, dt) : 1.0f);
        }
    }
    Vector3 pos = Vector3Add(pivot_, Vector3Scale(back, curDist_));
    Vector3 target = pivot_;
    if (lockTarget) target = Vector3Lerp(pivot_, *lockTarget, big > 0 ? 0.35f : 0.25f);

    shakeTime_ += dt;
    shake_ = std::max(0.0f, shake_ - tuning.shakeDecay * dt * shake_ - dt * 0.2f);
    if (shakeEnabled_ && shake_ > 0.001f) {
        float s = shake_ * shake_ * 0.12f;
        float t = shakeTime_ * tuning.shakeFrequency;
        Vector3 off{std::sin(t * 1.3f) * s, std::sin(t * 1.7f + 1.0f) * s, std::sin(t * 1.1f + 2.0f) * s * 0.5f};
        pos = Vector3Add(pos, off);
        target = Vector3Add(target, Vector3Scale(off, 0.5f));
    }
    pivot_ = basePivot;
    cam_.position = pos;
    cam_.target = target;
    cam_.fovy = tuning.fov;
}

}  // namespace lk
