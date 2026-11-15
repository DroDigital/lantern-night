#include "engine/particles.h"

#include <algorithm>
#include <cmath>

#include "engine/log.h"
#include "engine/paths.h"
#include "raymath.h"
#include "rlgl.h"

namespace lk {

float Particles::Rand() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return float(rng_ & 0xFFFFFF) / float(0xFFFFFF);
}

bool Particles::Init(int cap) {
    cap_ = cap;
    alive_.reserve(size_t(cap));
    inst_.resize(size_t(cap) * 8);
    std::string vs = paths::Asset("shaders/particle.vs"), fs = paths::Asset("shaders/particle.fs");
    shader_ = LoadShader(vs.c_str(), fs.c_str());
    if (shader_.id == 0 || shader_.id == rlGetShaderIdDefault()) {
        LK_ERROR("particles: shader failed");
        return false;
    }
    locViewProj_ = GetShaderLocation(shader_, "viewProj");
    locRight_ = GetShaderLocation(shader_, "camRight");
    locUp_ = GetShaderLocation(shader_, "camUp");
    int locCorner = GetShaderLocationAttrib(shader_, "corner");
    int locPos = GetShaderLocationAttrib(shader_, "instPosSize");
    int locCol = GetShaderLocationAttrib(shader_, "instColor");

    const float quad[12] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f};
    vao_ = rlLoadVertexArray();
    rlEnableVertexArray(vao_);
    quadVbo_ = rlLoadVertexBuffer(quad, sizeof quad, false);
    rlSetVertexAttribute(unsigned(locCorner), 2, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(unsigned(locCorner));
    instVbo_ = rlLoadVertexBuffer(inst_.data(), int(inst_.size() * sizeof(float)), true);
    rlSetVertexAttribute(unsigned(locPos), 4, RL_FLOAT, false, 8 * sizeof(float), 0);
    rlEnableVertexAttribute(unsigned(locPos));
    rlSetVertexAttributeDivisor(unsigned(locPos), 1);
    rlSetVertexAttribute(unsigned(locCol), 4, RL_FLOAT, false, 8 * sizeof(float), 4 * sizeof(float));
    rlEnableVertexAttribute(unsigned(locCol));
    rlSetVertexAttributeDivisor(unsigned(locCol), 1);
    rlDisableVertexArray();
    return true;
}

void Particles::Shutdown() {
    if (vao_) {
        rlUnloadVertexBuffer(quadVbo_);
        rlUnloadVertexBuffer(instVbo_);
        rlUnloadVertexArray(vao_);
        vao_ = 0;
    }
    if (shader_.id) UnloadShader(shader_);
    shader_ = Shader{};
}

void Particles::Emit(const ParticleDesc& d) {
    if (int(alive_.size()) >= cap_) return;  // global cap: drop new particles
    alive_.push_back({d, 0.0f});
}

void Particles::Burst(const EmitterBurst& b) {
    Vector3 dir = Vector3Normalize(b.dir);
    Vector3 t1 = Vector3Normalize(Vector3CrossProduct(dir, std::fabs(dir.y) < 0.9f ? Vector3{0, 1, 0} : Vector3{1, 0, 0}));
    Vector3 t2 = Vector3CrossProduct(dir, t1);
    float spread = b.spreadDeg * DEG2RAD;
    for (int i = 0; i < b.count; ++i) {
        float ang = Rand() * 2.0f * PI;
        float off = std::sqrt(Rand()) * spread;
        Vector3 d = Vector3Add(Vector3Scale(dir, std::cos(off)),
                               Vector3Scale(Vector3Add(Vector3Scale(t1, std::cos(ang)), Vector3Scale(t2, std::sin(ang))),
                                            std::sin(off)));
        ParticleDesc p;
        p.pos = Vector3Add(b.pos, Vector3{(Rand() - 0.5f) * 2 * b.posJitter, (Rand() - 0.5f) * 2 * b.posJitter,
                                          (Rand() - 0.5f) * 2 * b.posJitter});
        p.vel = Vector3Scale(d, b.speedMin + (b.speedMax - b.speedMin) * Rand());
        p.life = b.lifeMin + (b.lifeMax - b.lifeMin) * Rand();
        p.size0 = b.sizeMin + (b.sizeMax - b.sizeMin) * Rand();
        p.size1 = b.sizeEnd;
        p.color0 = b.color0;
        p.color1 = b.color1;
        p.gravity = b.gravity;
        p.drag = b.drag;
        Emit(p);
    }
}

void Particles::Update(float dt) {
    for (size_t i = 0; i < alive_.size();) {
        P& p = alive_[i];
        p.age += dt;
        if (p.age >= p.d.life) {
            alive_[i] = alive_.back();
            alive_.pop_back();
            continue;
        }
        p.d.vel = Vector3Add(p.d.vel, Vector3Scale(p.d.gravity, dt));
        p.d.vel = Vector3Scale(p.d.vel, std::max(0.0f, 1.0f - p.d.drag * dt));
        p.d.pos = Vector3Add(p.d.pos, Vector3Scale(p.d.vel, dt));
        ++i;
    }
}

void Particles::Draw(const Camera3D& cam) {
    if (alive_.empty() || !vao_) return;
    size_t n = alive_.size();
    for (size_t i = 0; i < n; ++i) {
        const P& p = alive_[i];
        float t = p.age / p.d.life;
        float* o = &inst_[i * 8];
        o[0] = p.d.pos.x;
        o[1] = p.d.pos.y;
        o[2] = p.d.pos.z;
        o[3] = p.d.size0 + (p.d.size1 - p.d.size0) * t;
        o[4] = p.d.color0.x + (p.d.color1.x - p.d.color0.x) * t;
        o[5] = p.d.color0.y + (p.d.color1.y - p.d.color0.y) * t;
        o[6] = p.d.color0.z + (p.d.color1.z - p.d.color0.z) * t;
        o[7] = p.d.color0.w + (p.d.color1.w - p.d.color0.w) * t;
    }
    rlDrawRenderBatchActive();
    Matrix view = rlGetMatrixModelview();
    Matrix proj = rlGetMatrixProjection();
    Matrix vp = MatrixMultiply(view, proj);
    Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, cam.up));
    Vector3 up = Vector3CrossProduct(right, fwd);
    rlEnableShader(shader_.id);
    rlSetUniformMatrix(locViewProj_, vp);
    rlSetUniform(locRight_, &right, RL_SHADER_UNIFORM_VEC3, 1);
    rlSetUniform(locUp_, &up, RL_SHADER_UNIFORM_VEC3, 1);
    rlSetBlendMode(RL_BLEND_ADDITIVE);
    rlDisableDepthMask();
    rlEnableVertexArray(vao_);
    rlUpdateVertexBuffer(instVbo_, inst_.data(), int(n * 8 * sizeof(float)), 0);
    rlDrawVertexArrayInstanced(0, 6, int(n));
    rlDisableVertexArray();
    rlEnableDepthMask();
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlDisableShader();
}

}  // namespace lk
