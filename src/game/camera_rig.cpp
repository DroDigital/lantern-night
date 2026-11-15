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
    // Longest clear boom along a direction (sphere cast from the pivot).
    auto clearDist = [&](Vector3 dir) {
        if (!physics) return dist;
        RayHit h = physics->SphereCast(basePivot, Vector3Add(pivot_, Vector3Scale(dir, dist)), tuning.collisionRadius);
        return h.hit ? std::max(tuning.minDistance, dist * h.fraction - 0.05f) : dist;
    };
    float want = clearDist(back);
    // Squeezed against a wall: tilt up and look over the knight instead of hugging their back.
    float squeeze = std::clamp((dist - want) / std::max(dist, 0.01f), 0.0f, 1.0f);
    squeeze_ += (squeeze - squeeze_) * (dt > 0 ? Damp(5.0f, dt) : 1.0f);
    if (squeeze_ > 0.01f) {
        float p2 = std::min(pitch_ + squeeze_ * 40.0f * DEG2RAD, 75.0f * DEG2RAD);
        Vector3 back2{std::sin(yaw_) * std::cos(p2), std::sin(p2), std::cos(yaw_) * std::cos(p2)};
        float want2 = clearDist(back2);
        if (want2 > want) {
            back = back2;
            want = want2;
        }
    }
    // Pull in to a wall at once (never render from inside it); ease back out.
    if (dt <= 0 || want < curDist_) curDist_ = want;
    else curDist_ += (want - curDist_) * Damp(tuning.collisionOut, dt);
    Vector3 pos = Vector3Add(pivot_, Vector3Scale(back, curDist_));
    Vector3 target = pivot_;
    // Backed into a wall so the boom cannot clear it at any tilt: rise over the knight's head and
    // look down at them (the crypt has no ceilings), blending in so it does not pop.
    float overhead = 0.0f;
    if (physics && curDist_ < tuning.hideKnightBelow * 1.5f) {
        float up = dist * 0.8f;
        RayHit hu = physics->SphereCast(basePivot, Vector3Add(basePivot, Vector3{0, up, 0}), tuning.collisionRadius);
        if (hu.hit) up *= hu.fraction;
        overhead = std::clamp((up - 1.0f) / 1.5f, 0.0f, 1.0f);
        if (overhead > 0) overhead *= std::clamp(1.0f - (curDist_ - tuning.hideKnightBelow) / (tuning.hideKnightBelow * 0.5f), 0.0f, 1.0f);
        overheadPos_ = Vector3Add(basePivot, Vector3{std::sin(yaw_) * 0.12f, up, std::cos(yaw_) * 0.12f});
    }
    overhead_ += (overhead - overhead_) * (dt > 0 ? Damp(6.0f, dt) : 1.0f);
    if (overhead_ > 0.01f) pos = Vector3Lerp(pos, overheadPos_, overhead_);
    boom_ = Vector3Distance(pos, pivot_);
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
