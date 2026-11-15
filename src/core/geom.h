#pragma once
// Minimal vector math for pure-logic code and combat queries (no raylib dependency).
#include <cmath>

namespace lk::geom {

struct V3 {
    float x = 0, y = 0, z = 0;
};
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float Len(V3 a) { return std::sqrt(Dot(a, a)); }

// Squared distance between segments p1-q1 and p2-q2 (Ericson, Real-Time Collision Detection 5.1.9).
float SegmentSegmentDistSq(V3 p1, V3 q1, V3 p2, V3 q2);

struct Capsule {
    V3 a, b;
    float r = 0.1f;
};

bool CapsuleOverlap(const Capsule& c1, const Capsule& c2);
bool SphereCapsuleOverlap(V3 center, float r, const Capsule& c);

// Is direction `to` (from the defender) within `halfAngleDeg` of `facing` on the ground plane?
bool WithinArc(V3 facing, V3 to, float halfAngleDeg);

}  // namespace lk::geom
