#include "game/session.h"

#include <algorithm>
#include <cmath>

#include "engine/assets.h"
#include "engine/audio.h"
#include "engine/data_store.h"
#include "engine/log.h"
#include "engine/paths.h"
#include "engine/physics.h"

namespace lk {

static Vector3 V3(const json& j, Vector3 d) {
    if (!j.is_array() || j.size() < 3) return d;
    return Vector3{j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

static float LerpAngle(float a, float b, float t) {
    float d = std::fmod(b - a + 3.0f * PI, 2.0f * PI) - PI;
    return a + d * t;
}

Session::Session(Ctx& ctx) : ctx_(ctx) {}
Session::~Session() { lanternMesh_.Unload(); }

void Session::ReloadTunables() {
    const json& r = ctx_.data.Get("render.json");
    RenderSettings& s = ctx_.renderer.Settings();
    s.ambient = V3(r.value("ambient", json()), s.ambient);
    s.fogColor = V3(r.value("fog_color", json()), s.fogColor);
    s.fogDensity = r.value("fog_density", s.fogDensity);
    s.voidColor = V3(r.value("void_color", json()), s.voidColor);
    s.lightFalloff = r.value("light_falloff", s.lightFalloff);
    s.fogHeight = r.value("fog_height", s.fogHeight);
    s.fogHeightFalloff = r.value("fog_height_falloff", s.fogHeightFalloff);
    s.rimColor = V3(r.value("rim_color", json()), s.rimColor);
    s.rimStrength = r.value("rim_strength", s.rimStrength);
    s.characterMinLight = r.value("character_min_light", s.characterMinLight);
    s.bloomThreshold = r.value("bloom_threshold", s.bloomThreshold);
    s.bloomKnee = r.value("bloom_knee", s.bloomKnee);
    s.bloomStrength = r.value("bloom_strength", s.bloomStrength);
    s.exposure = r.value("exposure", s.exposure);
    s.vignette = r.value("vignette", s.vignette);
    s.vignetteRadius = r.value("vignette_radius", s.vignetteRadius);
    s.vignetteColor = V3(r.value("vignette_color", json()), s.vignetteColor);
    s.shadowTint = V3(r.value("shadow_tint", json()), s.shadowTint);
    s.saturation = r.value("saturation", s.saturation);
    s.tonemapper = r.value("tonemapper", std::string("aces")) == "agx" ? 1 : 0;
    drawDistance_ = r.value("draw_distance", drawDistance_);
    if (r.contains("lantern")) lantern_.Load(r["lantern"]);
    if (r.contains("blob_shadow")) {
        blobTune_.x = r["blob_shadow"].value("radius", 0.55f);
        blobTune_.y = r["blob_shadow"].value("strength", 0.65f);
    }
    cam_.tuning.Load(ctx_.data.Get("camera.json"));
}

bool Session::Load(const std::string& zoneFile) {
    ReloadTunables();
    ZoneData zone;
    std::string err;
    if (!ParseZone(ctx_.data.Get("levels/" + zoneFile), zone, err)) {
        LK_ERROR("zone %s: %s", zoneFile.c_str(), err.c_str());
        return false;
    }
    if (!world_.Build(zone, ctx_.assets, ctx_.physics)) return false;
    BuildLights();
    lanternMesh_.Init();

    const json& anims = ctx_.data.Get("anims/knight.json");
    if (!knight_.Init(ctx_.assets, anims)) return false;
    for (const char* n : {"1H_Sword_Offhand", "Badge_Shield", "Rectangle_Shield", "Spike_Shield", "2H_Sword"})
        knight_.HideMesh(n);
    knight_.Play("idle", 0.0f);

    const json& ps = ctx_.data.Get("player/stats.json");
    CharacterDesc cd;
    cd.radius = ps.value("capsule_radius", 0.3f);
    cd.height = ps.value("capsule_height", 1.2f);
    pos_ = prevPos_ = Vector3{zone.playerSpawn.x, zone.playerSpawn.y, zone.playerSpawn.z};
    yaw_ = prevYaw_ = zone.playerYaw * DEG2RAD;
    body_ = ctx_.physics.CreateCharacter(cd, pos_);
    cam_.Reset(pos_, yaw_ + PI);
    return true;
}

void Session::BuildLights() {
    zoneLights_.clear();
    const json& r = ctx_.data.Get("render.json");
    for (const auto& p : world_.Zone().lights) {
        ZoneLight zl;
        zl.id = p.id;
        zl.light.pos = Vector3{p.pos.x, p.pos.y, p.pos.z};
        std::string preset = p.extra.value("preset", "");
        json base = preset.empty() ? json::object() : r.value(preset, json::object());
        zl.baseColor = V3(p.extra.value("color", base.value("color", json())), Vector3{1, 0.6f, 0.3f});
        zl.baseIntensity = p.extra.value("intensity", base.value("intensity", 1.0f));
        zl.light.range = p.extra.value("range", base.value("range", 5.0f));
        zl.flicker = p.extra.value("flicker", base.value("flicker_amount", 0.0f));
        zl.seed = float(zoneLights_.size()) * 1.37f;
        zoneLights_.push_back(zl);
    }
}

Vector3 Session::PlayerPos() const { return pos_; }

void Session::GatherLights(std::vector<PointLight>& out, float t) {
    out.clear();
    float sc = world_.Zone().modelScale;
    Matrix world = MatrixMultiply(MatrixMultiply(MatrixScale(sc, sc, sc), MatrixRotateY(yaw_)),
                                  MatrixTranslate(pos_.x, pos_.y, pos_.z));
    Matrix hip = knight_.BoneWorld(lantern_.bone, world);
    // Lantern hangs off the hip: offset is in world units relative to the knight's facing.
    Vector3 hipPos{hip.m12, hip.m13, hip.m14};
    Vector3 off = Vector3Transform(lantern_.offset, MatrixRotateY(yaw_));
    Vector3 lanternPos = Vector3Add(hipPos, off);
    lanternPos_ = lanternPos;
    out.push_back(ComputeLantern(lantern_, flameFraction_, t, lanternPos, &lanternState_));
    for (auto& z : zoneLights_) {
        PointLight L = z.light;
        float k = z.baseIntensity * (z.flicker > 0 ? Flicker(t, 8.0f, z.flicker, z.seed) : 1.0f);
        L.color = Vector3Scale(z.baseColor, k);
        out.push_back(L);
    }
}

void Session::Step(const InputFrame& in, float dt) {
    time_ += dt;
    prevPos_ = pos_;
    prevYaw_ = yaw_;
    const json& ps = ctx_.data.Get("player/stats.json");
    Vector3 fwd = cam_.Forward(), right = cam_.Right();
    Vector3 wish = Vector3Add(Vector3Scale(fwd, in.move.y), Vector3Scale(right, in.move.x));
    float mag = std::min(1.0f, Vector3Length(wish));
    float speed = 0.0f;
    if (mag > 0.05f) {
        bool walk = mag < ps.value("walk_threshold", 0.55f);
        speed = walk ? ps.value("walk_speed", 1.6f) : ps.value("run_speed", 3.6f);
        float target = std::atan2(wish.x, wish.z);
        yaw_ = LerpAngle(yaw_, target, std::min(1.0f, dt * 12.0f));
        knight_.Play(walk ? "walk" : "run", 0.15f);
    } else {
        knight_.Play("idle", 0.2f);
    }
    Vector3 vel = mag > 0.05f ? Vector3Scale(Vector3Normalize(wish), speed) : Vector3{0, 0, 0};
    if (body_) {
        body_->Move(vel, dt, ps.value("gravity", -20.0f));
        pos_ = body_->Position();
    }
    knight_.Update(dt);
    cam_.Update(dt, pos_, in.look, nullptr, &ctx_.physics);
}

void Session::Render(float alpha, int screenW, int screenH) {
    Vector3 rp = Vector3Lerp(prevPos_, pos_, alpha);
    float ry = LerpAngle(prevYaw_, yaw_, alpha);
    std::vector<PointLight> lights;
    GatherLights(lights, time_);
    ctx_.renderer.SetLights(lights, true);

    // Low flame tightens the vignette.
    RenderSettings& s = ctx_.renderer.Settings();
    float baseV = ctx_.data.Get("render.json").value("vignette", 0.45f);
    float baseR = ctx_.data.Get("render.json").value("vignette_radius", 0.95f);
    float low = std::clamp(1.0f - flameFraction_ / std::max(lantern_.sputterBelow, 0.01f), 0.0f, 1.0f);
    s.vignette = baseV + (lantern_.lowVignette - baseV) * low;
    s.vignetteRadius = baseR + (lantern_.lowVignetteRadius - baseR) * low;

    Camera3D cam = cam_.Cam();
    ctx_.renderer.BeginScene(cam);
    world_.Draw(ctx_.renderer, cam.position, drawDistance_);
    float sc = world_.Zone().modelScale;
    Matrix world = MatrixMultiply(MatrixMultiply(MatrixScale(sc, sc, sc), MatrixRotateY(ry)), MatrixTranslate(rp.x, rp.y, rp.z));
    knight_.Draw(ctx_.renderer, world, DrawParams{});
    ctx_.renderer.DrawBlobShadow(rp, blobTune_.x, blobTune_.y);
    const PointLight& L = lights[0];
    lanternMesh_.Draw(ctx_.renderer, lanternPos_, ry, flameFraction_, lanternState_.flicker, Vector3{1.0f, 0.55f, 0.2f});
    ctx_.renderer.DrawHalo(L.pos, lantern_.haloSize * (0.8f + 0.2f * lanternState_.flicker),
                           Color{255, 170, 90, 255}, lantern_.haloIntensity * lanternState_.flicker * (0.3f + 0.7f * flameFraction_));
    ctx_.renderer.EndScene();
    ctx_.renderer.Present(screenW, screenH);
}

}  // namespace lk
