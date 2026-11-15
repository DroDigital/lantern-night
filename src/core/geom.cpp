#include "core/geom.h"

#include <algorithm>

namespace lk::geom {

float SegmentSegmentDistSq(V3 p1, V3 q1, V3 p2, V3 q2) {
    const float eps = 1e-8f;
    V3 d1 = q1 - p1, d2 = q2 - p2, r = p1 - p2;
    float a = Dot(d1, d1), e = Dot(d2, d2), f = Dot(d2, r);
    float s = 0, t = 0;
    if (a <= eps && e <= eps) return Dot(r, r);
    if (a <= eps) {
        t = std::clamp(f / e, 0.0f, 1.0f);
    } else {
        float c = Dot(d1, r);
        if (e <= eps) {
            s = std::clamp(-c / a, 0.0f, 1.0f);
        } else {
            float b = Dot(d1, d2);
            float denom = a * e - b * b;
            s = denom != 0.0f ? std::clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f) {
                t = 0.0f;
                s = std::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = std::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }
    V3 c1 = p1 + d1 * s, c2 = p2 + d2 * t;
    V3 d = c1 - c2;
    return Dot(d, d);
}

bool CapsuleOverlap(const Capsule& c1, const Capsule& c2) {
    float rr = c1.r + c2.r;
    return SegmentSegmentDistSq(c1.a, c1.b, c2.a, c2.b) <= rr * rr;
}

bool SphereCapsuleOverlap(V3 center, float r, const Capsule& c) {
    return CapsuleOverlap(Capsule{center, center, r}, c);
}

bool WithinArc(V3 facing, V3 to, float halfAngleDeg) {
    facing.y = 0;
    to.y = 0;
    float lf = Len(facing), lt = Len(to);
    if (lf < 1e-5f || lt < 1e-5f) return true;
    float cosA = Dot(facing, to) / (lf * lt);
    return cosA >= std::cos(halfAngleDeg * 3.14159265f / 180.0f);
}

}  // namespace lk::geom
