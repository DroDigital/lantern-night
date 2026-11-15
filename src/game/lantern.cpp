#include "game/lantern.h"

#include <algorithm>
#include <cmath>

namespace lk {

static Vector3 ReadV3(const json& j, const char* k, Vector3 d) {
    if (!j.contains(k) || !j[k].is_array() || j[k].size() < 3) return d;
    return Vector3{j[k][0].get<float>(), j[k][1].get<float>(), j[k][2].get<float>()};
}

void LanternTuning::Load(const json& j) {
    color = ReadV3(j, "color", color);
    intensityMin = j.value("intensity_min", intensityMin);
    intensityMax = j.value("intensity_max", intensityMax);
    rangeMin = j.value("range_min", rangeMin);
    rangeMax = j.value("range_max", rangeMax);
    flickerAmount = j.value("flicker_amount", flickerAmount);
    flickerSpeed = j.value("flicker_speed", flickerSpeed);
    sputterBelow = j.value("sputter_below", sputterBelow);
    sputterAmount = j.value("sputter_amount", sputterAmount);
    bone = j.value("bone", bone);
    offset = ReadV3(j, "offset", offset);
    haloSize = j.value("halo_size", haloSize);
    haloIntensity = j.value("halo_intensity", haloIntensity);
    lowVignette = j.value("low_flame_vignette", lowVignette);
    lowVignetteRadius = j.value("low_flame_vignette_radius", lowVignetteRadius);
}

float Flicker(float time, float speed, float amount, float seed) {
    float n = std::sin(time * speed + seed) * 0.5f + std::sin(time * speed * 2.31f + seed * 1.7f) * 0.3f +
              std::sin(time * speed * 5.13f + seed * 3.1f) * 0.2f;
    return 1.0f + n * amount;
}

PointLight ComputeLantern(const LanternTuning& t, float fraction, float time, Vector3 pos, LanternState* st) {
    float f = std::clamp(fraction, 0.0f, 1.0f);
    float flick = Flicker(time, t.flickerSpeed, t.flickerAmount, 0.0f);
    bool sputter = f < t.sputterBelow;
    if (sputter) {
        // Irregular dips: the flame gutters.
        float s = std::sin(time * 23.0f) * std::sin(time * 7.3f + 1.3f) * std::sin(time * 3.1f);
        float dip = std::max(0.0f, s) * t.sputterAmount * (1.0f - f / std::max(t.sputterBelow, 0.001f) * 0.5f);
        flick *= 1.0f - dip;
    }
    PointLight L;
    L.pos = pos;
    float intensity = (t.intensityMin + (t.intensityMax - t.intensityMin) * f) * flick;
    L.color = Vector3{t.color.x * intensity, t.color.y * intensity, t.color.z * intensity};
    L.range = (t.rangeMin + (t.rangeMax - t.rangeMin) * f) * (0.9f + 0.1f * flick);
    if (st) {
        st->flicker = flick;
        st->sputtering = sputter;
    }
    return L;
}

}  // namespace lk
