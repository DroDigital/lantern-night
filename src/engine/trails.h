#pragma once
// Ribbon trails: a strip that follows two points (e.g. sword base and tip), rebuilt each frame
// from recent samples and drawn additively with a fade along its age.
#include <deque>

#include "raylib.h"

namespace lk {

class Trail {
public:
    void SetLifetime(float s) { life_ = s; }
    void SetColor(Color c, float intensity) { color_ = c; intensity_ = intensity; }
    void Emit(bool on) { emitting_ = on; }
    void Push(Vector3 base, Vector3 tip, float now);
    void Clear() { samples_.clear(); }
    bool Empty() const { return samples_.size() < 2; }
    void Draw(float now, Shader shader, int intensityLoc) const;

private:
    struct S {
        Vector3 a, b;
        float t;
    };
    std::deque<S> samples_;
    float life_ = 0.14f;
    Color color_{255, 220, 170, 255};
    float intensity_ = 1.5f;
    bool emitting_ = false;
};

}  // namespace lk
