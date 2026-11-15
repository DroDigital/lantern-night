#pragma once
// CPU particles: pooled, with a global cap, drawn as instanced camera-facing billboards
// (additive, HDR colours so they feed bloom).
#include <vector>

#include "raylib.h"

namespace lk {

struct ParticleDesc {
    Vector3 pos{};
    Vector3 vel{};
    float life = 1.0f;
    float size0 = 0.1f, size1 = 0.0f;
    Vector4 color0{1, 0.6f, 0.2f, 1}, color1{1, 0.2f, 0.05f, 0};   // linear rgb * intensity, alpha
    Vector3 gravity{0, 0, 0};
    float drag = 0.0f;
};

struct EmitterBurst {
    int count = 10;
    Vector3 pos{};
    Vector3 dir{0, 1, 0};
    float spreadDeg = 45.0f;
    float speedMin = 1.0f, speedMax = 3.0f;
    float lifeMin = 0.3f, lifeMax = 0.6f;
    float sizeMin = 0.03f, sizeMax = 0.06f;
    float sizeEnd = 0.0f;
    Vector4 color0{3, 1.6f, 0.6f, 1}, color1{1, 0.3f, 0.05f, 0};
    Vector3 gravity{0, -6, 0};
    float drag = 1.0f;
    float posJitter = 0.05f;
};

class Particles {
public:
    bool Init(int cap);
    void Shutdown();
    void Emit(const ParticleDesc& d);
    void Burst(const EmitterBurst& b);
    void Update(float dt);
    // Call inside BeginMode3D (after opaque geometry).
    void Draw(const Camera3D& cam);
    int Alive() const { return int(alive_.size()); }
    void Clear() { alive_.clear(); }

private:
    struct P {
        ParticleDesc d;
        float age = 0;
    };
    float Rand();
    std::vector<P> alive_;
    std::vector<float> inst_;
    int cap_ = 2000;
    Shader shader_{};
    int locViewProj_ = -1, locRight_ = -1, locUp_ = -1;
    unsigned vao_ = 0, quadVbo_ = 0, instVbo_ = 0;
    unsigned rng_ = 0x9E3779B9u;
};

}  // namespace lk
