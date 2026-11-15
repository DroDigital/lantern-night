#pragma once
// The lantern is the knight's life and light: range and intensity follow flame / max,
// with a subtle flicker, and a sputter below the low-flame threshold.
#include "engine/data_store.h"
#include "engine/renderer.h"

namespace lk {

struct LanternTuning {
    Vector3 color{1.0f, 0.56f, 0.22f};
    float intensityMin = 1.6f, intensityMax = 5.0f;
    float rangeMin = 4.0f, rangeMax = 12.0f;
    float flickerAmount = 0.07f, flickerSpeed = 9.0f;
    float sputterBelow = 0.25f, sputterAmount = 0.35f;
    std::string bone = "hips";
    Vector3 offset{0.55f, 0.15f, 0.25f};
    float haloSize = 0.9f, haloIntensity = 2.2f;
    float lowVignette = 0.85f, lowVignetteRadius = 0.75f;
    void Load(const json& j);
};

struct LanternState {
    float flicker = 1.0f;   // current multiplier (also used for halo)
    bool sputtering = false;
};

// fraction = flame / max flame (0..1). t = time in seconds.
PointLight ComputeLantern(const LanternTuning& t, float fraction, float time, Vector3 pos, LanternState* st = nullptr);
// Generic flickering fire light.
float Flicker(float time, float speed, float amount, float seed);

}  // namespace lk
