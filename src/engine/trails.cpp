#include "engine/trails.h"

#include "raymath.h"
#include "rlgl.h"

namespace lk {

void Trail::Push(Vector3 base, Vector3 tip, float now) {
    if (emitting_) samples_.push_front({base, tip, now});
    while (!samples_.empty() && now - samples_.back().t > life_) samples_.pop_back();
    while (samples_.size() > 64) samples_.pop_back();
}

void Trail::Draw(float now, Shader shader, int intensityLoc) const {
    if (samples_.size() < 2) return;
    rlDrawRenderBatchActive();
    BeginShaderMode(shader);
    SetShaderValue(shader, intensityLoc, &intensity_, SHADER_UNIFORM_FLOAT);
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlBegin(RL_TRIANGLES);
    for (size_t i = 0; i + 1 < samples_.size(); ++i) {
        const S& s0 = samples_[i];
        const S& s1 = samples_[i + 1];
        float u0 = (now - s0.t) / life_, u1 = (now - s1.t) / life_;
        rlColor4ub(color_.r, color_.g, color_.b, color_.a);
        rlTexCoord2f(u0, 0); rlVertex3f(s0.a.x, s0.a.y, s0.a.z);
        rlTexCoord2f(u0, 1); rlVertex3f(s0.b.x, s0.b.y, s0.b.z);
        rlTexCoord2f(u1, 1); rlVertex3f(s1.b.x, s1.b.y, s1.b.z);
        rlTexCoord2f(u0, 0); rlVertex3f(s0.a.x, s0.a.y, s0.a.z);
        rlTexCoord2f(u1, 1); rlVertex3f(s1.b.x, s1.b.y, s1.b.z);
        rlTexCoord2f(u1, 0); rlVertex3f(s1.a.x, s1.a.y, s1.a.z);
    }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    EndBlendMode();
    EndShaderMode();
}

}  // namespace lk
