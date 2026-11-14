#pragma once
// The knight's hand lantern: a tiny procedural model (glass body that glows with the flame,
// dark metal cap, base and handle). KayKit has no hand lantern; see docs/DECISIONS.md.
#include "engine/renderer.h"

namespace lk {

class LanternMesh {
public:
    void Init();
    void Unload();
    // pos = centre of the glass; flame 0..1 drives the glow.
    void Draw(Renderer& r, Vector3 pos, float yawRad, float flame, float flicker, Vector3 glowColor, float scale = 1.0f) const;

private:
    Mesh glass_{}, cap_{}, base_{}, ring_{};
    Material mat_{};
    bool ready_ = false;
};

}  // namespace lk
