#include "engine/editor.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace lk {

void FreeCamera::Reset(Vector3 pos, float yaw, float pitch) {
    pos_ = pos;
    yaw_ = yaw;
    pitch_ = pitch;
    Update(0.0f);
}

void FreeCamera::Update(float dt) {
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 d = GetMouseDelta();
        yaw_ -= d.x * 0.004f;
        pitch_ = std::clamp(pitch_ - d.y * 0.004f, -1.5f, 1.5f);
    }
    Vector3 fwd{std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_), std::cos(yaw_) * std::cos(pitch_)};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, Vector3{0, 1, 0}));
    float speed = (IsKeyDown(KEY_LEFT_SHIFT) ? 24.0f : 8.0f) * dt;
    if (IsKeyDown(KEY_W)) pos_ = Vector3Add(pos_, Vector3Scale(fwd, speed));
    if (IsKeyDown(KEY_S)) pos_ = Vector3Subtract(pos_, Vector3Scale(fwd, speed));
    if (IsKeyDown(KEY_D)) pos_ = Vector3Add(pos_, Vector3Scale(right, speed));
    if (IsKeyDown(KEY_A)) pos_ = Vector3Subtract(pos_, Vector3Scale(right, speed));
    if (IsKeyDown(KEY_E)) pos_.y += speed;
    if (IsKeyDown(KEY_Q)) pos_.y -= speed;
    cam_.position = pos_;
    cam_.target = Vector3Add(pos_, fwd);
    cam_.up = {0, 1, 0};
    cam_.fovy = 60.0f;
    cam_.projection = CAMERA_PERSPECTIVE;
}

bool MouseOnPlane(const Camera3D& cam, float height, Vector3& hit) {
    Ray ray = GetScreenToWorldRay(GetMousePosition(), cam);
    if (std::fabs(ray.direction.y) < 1e-5f) return false;
    float t = (height - ray.position.y) / ray.direction.y;
    if (t <= 0.0f) return false;
    hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    return true;
}

}  // namespace lk
