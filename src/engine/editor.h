#pragma once
// Generic editor helpers: a free-fly camera and mouse picking against horizontal planes.
#include "raylib.h"

namespace lk {

class FreeCamera {
public:
    void Reset(Vector3 pos, float yaw, float pitch);
    // Hold right mouse to look; WASD to move; Q/E down/up; Shift = fast.
    void Update(float dt);
    Camera3D Cam() const { return cam_; }
    Vector3 Position() const { return pos_; }

private:
    Camera3D cam_{};
    Vector3 pos_{};
    float yaw_ = 0, pitch_ = -0.6f;
};

// Intersect the mouse ray with the plane y = height. Returns false if parallel/behind.
bool MouseOnPlane(const Camera3D& cam, float height, Vector3& hit);

}  // namespace lk
