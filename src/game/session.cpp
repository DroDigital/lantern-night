#include "game/session.h"

#include <algorithm>
#include <cmath>

#include "engine/assets.h"
#include "engine/audio.h"
#include "engine/data_store.h"
#include "engine/log.h"
#include "engine/paths.h"
#include "engine/physics.h"
#include "engine/ui.h"
#include "game/game_state.h"
#include "rlgl.h"

namespace lk {

static Vector3 V3(const json& j, Vector3 d) {
    if (!j.is_array() || j.size() < 3) return d;
    return Vector3{j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

static float Rand01(unsigned& s) {
    s = s * 1664525u + 1013904223u;
    return float((s >> 8) & 0xFFFF) / 65535.0f;
}

Session::Session(Ctx& ctx) : ctx_(ctx) {}

Session::~Session() {
    lanternMesh_.Unload();
    particles_.Shutdown();
    if (trailShader_.id) UnloadShader(trailShader_);
    enemies_.clear();
}

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
    feel_ = ctx_.data.Get("feel.json");
    player_.LoadTuning(ctx_.data.Get("player/stats.json"));
    for (auto& [k, d] : defs_) {
        std::string file = "enemies/" + k + ".json";
        const json& j = ctx_.data.Get(file);
        d->Load(k, j, ctx_.data.Get(j.value("anims", "")));
    }
}

const EnemyDef* Session::Def(const std::string& kind) {
    auto it = defs_.find(kind);
    if (it != defs_.end()) return it->second.get();
    std::string file = "enemies/" + kind + ".json";
    if (!ctx_.data.Has(file)) {
        LK_ERROR("unknown enemy kind '%s'", kind.c_str());
        return nullptr;
    }
    auto d = std::make_unique<EnemyDef>();
    const json& j = ctx_.data.Get(file);
    if (!d->Load(kind, j, ctx_.data.Get(j.value("anims", "")))) return nullptr;
    auto* ptr = d.get();
    defs_[kind] = std::move(d);
    return ptr;
}

bool Session::Load(const std::string& zoneFile) {
    ZoneData zone;
    std::string err;
    if (!ParseZone(ctx_.data.Get("levels/" + zoneFile), zone, err)) {
        LK_ERROR("zone %s: %s", zoneFile.c_str(), err.c_str());
        return false;
    }
    if (!world_.Build(zone, ctx_.assets, ctx_.physics)) return false;
    nav_.Build(zone);
    BuildLights();
    lanternMesh_.Init();
    feel_ = ctx_.data.Get("feel.json");
    particles_.Init(feel_.value("particle_cap", 3000));
    trailShader_ = LoadShader(nullptr, paths::Asset("shaders/trail.fs").c_str());
    trailIntensityLoc_ = GetShaderLocation(trailShader_, "intensity");

    Vector3 spawn{zone.playerSpawn.x, zone.playerSpawn.y, zone.playerSpawn.z};
    if (!player_.Init(ctx_, spawn, zone.playerYaw)) return false;
    ReloadTunables();
    SpawnEnemies();
    cam_.Reset(spawn, zone.playerYaw * DEG2RAD + PI);
    ctx_.audio.PlayMusic(zone.ambientMusic, 2.0f);
    return true;
}

void Session::SpawnEnemies() {
    enemies_.clear();
    int id = 1;
    for (const auto& p : world_.Zone().enemies) {
        const EnemyDef* d = Def(p.model);
        if (!d) continue;
        auto e = std::make_unique<Enemy>();
        if (!e->Init(ctx_, *d, Vector3{p.pos.x, p.pos.y, p.pos.z}, p.rot, id++, p.id)) continue;
        enemies_.push_back(std::move(e));
    }
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
        zl.halo = preset == "torch_light" || preset == "brazier_light";
        zl.seed = float(zoneLights_.size()) * 1.37f;
        zoneLights_.push_back(zl);
    }
}

void Session::SetFlameFraction(float f) {
    player_.flame = player_.flameMax * std::clamp(f, 0.0f, 1.0f);
    flameTrail_ = player_.FlameFrac();
}

Enemy* Session::LockTarget() const {
    for (const auto& e : enemies_)
        if (e->Id() == lockId_) return e.get();
    return nullptr;
}

void Session::GatherLights(std::vector<PointLight>& out, float t) {
    out.clear();
    out.push_back(ComputeLantern(lantern_, player_.FlameFrac(), t, player_.LanternPos(), &lanternState_));
    for (auto& z : zoneLights_) {
        PointLight L = z.light;
        float k = z.baseIntensity * (z.flicker > 0 ? Flicker(t, 8.0f, z.flicker, z.seed) : 1.0f);
        L.color = Vector3Scale(z.baseColor, k);
        out.push_back(L);
    }
    for (const auto& p : projectiles_) {
        if (!p.alive) continue;
        PointLight L;
        L.pos = p.pos;
        L.color = Vector3Scale(p.def->boltColor, 0.6f);
        L.range = 3.5f;
        out.push_back(L);
    }
}

void Session::Hitstop(float seconds) { hitstop_ = std::max(hitstop_, seconds); }

int Session::Advance(float realDt) {
    realTime_ += realDt;
    float scale = 1.0f;
    if (hitstop_ > 0) {
        hitstop_ -= realDt;
        scale = feel_.value("hitstop_time_scale", 0.0f);
    }
    flash_ = std::max(0.0f, flash_ - realDt * flashDecay_);
    const double step = 1.0 / 60.0;
    acc_ += double(realDt) * double(scale);
    int n = 0;
    while (acc_ >= step && n < 8) {
        Step(ctx_.input.ConsumeStep(), float(step));
        acc_ -= step;
        ++n;
    }
    if (n == 8) acc_ = 0;
    alpha_ = float(acc_ / step);
    return n;
}

void Session::UpdateLockOn(const InputFrame& in, float dt) {
    const PlayerTuning& t = player_.Tuning();
    Enemy* cur = LockTarget();
    Vector3 camPos = cam_.Cam().position;
    Vector3 camFwd = Vector3Normalize(Vector3Subtract(cam_.Cam().target, camPos));
    auto visible = [&](const Enemy& e) {
        return !ctx_.physics.RayCast(Vector3Add(player_.Pos(), Vector3{0, 1.0f, 0}), e.LockPoint()).hit;
    };
    auto candidates = [&]() {
        std::vector<Enemy*> v;
        for (auto& e : enemies_) {
            if (!e->Lockable()) continue;
            float d = Vector3Distance(e->Pos(), player_.Pos());
            if (d > t.lockRange) continue;
            Vector3 to = Vector3Normalize(Vector3Subtract(e->LockPoint(), camPos));
            float ang = std::acos(std::clamp(Vector3DotProduct(to, camFwd), -1.0f, 1.0f)) * RAD2DEG;
            if (ang > t.lockMaxAngle) continue;
            if (!visible(*e)) continue;
            v.push_back(e.get());
        }
        return v;
    };
    if (in.Pressed(Act::LockOn)) {
        if (cur) {
            lockId_ = -1;
            cur = nullptr;
        } else {
            Enemy* best = nullptr;
            float bestScore = 1e9f;
            for (Enemy* e : candidates()) {
                Vector3 to = Vector3Normalize(Vector3Subtract(e->LockPoint(), camPos));
                float ang = std::acos(std::clamp(Vector3DotProduct(to, camFwd), -1.0f, 1.0f));
                float score = ang * 4.0f + Vector3Distance(e->Pos(), player_.Pos()) * 0.15f;
                if (score < bestScore) {
                    bestScore = score;
                    best = e;
                }
            }
            if (best) {
                lockId_ = best->Id();
                lockLost_ = 0;
            } else {
                // No target: recentre the camera behind the knight.
                cam_.Reset(player_.Pos(), player_.Yaw() + PI);
            }
            cur = best;
        }
    }
    if (cur && (in.Pressed(Act::TargetLeft) || in.Pressed(Act::TargetRight))) {
        float dir = in.Pressed(Act::TargetLeft) ? -1.0f : 1.0f;
        Vector3 right = cam_.Right();
        float curX = Vector3DotProduct(Vector3Subtract(cur->LockPoint(), player_.Pos()), right);
        Enemy* best = nullptr;
        float bestD = 1e9f;
        for (Enemy* e : candidates()) {
            if (e == cur) continue;
            float x = Vector3DotProduct(Vector3Subtract(e->LockPoint(), player_.Pos()), right);
            float d = (x - curX) * dir;
            if (d > 0.05f && d < bestD) {
                bestD = d;
                best = e;
            }
        }
        if (best) lockId_ = best->Id();
        cur = LockTarget();
    }
    if (cur) {
        bool lost = !cur->Lockable() || Vector3Distance(cur->Pos(), player_.Pos()) > t.lockRange * 1.25f;
        if (!lost && !visible(*cur)) lockLost_ += dt;
        else lockLost_ = 0;
        if (lost || lockLost_ > t.lockLostTime) lockId_ = -1;
    }
}

void Session::ResolveCombat() {
    // Player weapon vs enemy hurtboxes.
    if (player_.AttackActive()) {
        Vector3 b, tip;
        player_.WeaponPoints(b, tip);
        std::vector<geom::Capsule> caps;
        player_.Swing().Capsules(b, tip, player_.WeaponRadius(), caps);
        for (auto& e : enemies_) {
            if (!e->Alive() || player_.Swing().hitIds.count(e->Id())) continue;
            geom::Capsule hb = e->Hurtbox();
            bool hit = false;
            for (const auto& c : caps)
                if (geom::CapsuleOverlap(c, hb)) hit = true;
            if (!hit) continue;
            player_.Swing().hitIds.insert(e->Id());
            PlayerStrike s;
            s.damage = player_.StrikeDamage();
            s.posture = player_.StrikePosture();
            s.heavy = player_.StrikeHeavy();
            s.from = player_.Pos();
            s.point = Vector3Lerp(tip, R(hb.b), 0.5f);
            StrikeResult r = e->TakeStrike(s, player_.CritMult(), fx_);
            barTimers_[e->Id()] = feel_.value("enemy_bar_time", 4.0f);
            if (r.blocked) continue;  // EnemyBlocked fx already queued
            fx_.push_back({r.critical ? FxType::HitCrit : FxType::HitFlesh, s.point, r.critical ? 1.5f : 1.0f, ""});
            if (r.killed) {
                ++kills_;
                if (lockId_ == e->Id()) lockId_ = -1;
            }
        }
    }
    // Enemy weapons vs the player.
    for (auto& e : enemies_) {
        if (!e->AttackActive() || e->Swing().hitIds.count(0)) continue;
        Vector3 b, tip;
        e->WeaponPoints(b, tip);
        std::vector<geom::Capsule> caps;
        e->Swing().Capsules(b, tip, e->Def().hitRadius * e->Scale(), caps);
        geom::Capsule hb = player_.Hurtbox();
        bool hit = false;
        for (const auto& c : caps)
            if (geom::CapsuleOverlap(c, hb)) hit = true;
        if (!hit) continue;
        e->Swing().hitIds.insert(0);
        IncomingHit h;
        h.damage = e->CurrentMove() ? e->CurrentMove()->damage : 10.0f;
        h.posture = e->CurrentMove() ? e->CurrentMove()->posturePlayer : 0.0f;
        h.from = e->Pos();
        IncomingResult r = player_.ReceiveHit(h, ctx_.state.godMode, fx_);
        if (r.parried) e->OnParried(fx_);
    }
}

void Session::UpdateProjectiles(float dt) {
    for (auto& s : spawns_) {
        Projectile p;
        p.pos = s.pos;
        p.vel = Vector3Scale(s.dir, s.def->boltSpeed);
        p.life = s.def->boltLife;
        p.owner = s.owner;
        p.def = s.def;
        projectiles_.push_back(p);
    }
    spawns_.clear();
    unsigned seed = unsigned(steps_ * 7919);
    for (auto& p : projectiles_) {
        if (!p.alive) continue;
        p.life -= dt;
        // Gentle homing toward the player's chest.
        Vector3 to = Vector3Normalize(Vector3Subtract(Vector3Add(player_.Pos(), Vector3{0, 0.7f, 0}), p.pos));
        Vector3 v = Vector3Normalize(p.vel);
        float maxTurn = p.def->boltTurn * DEG2RAD * dt;
        float ang = std::acos(std::clamp(Vector3DotProduct(v, to), -1.0f, 1.0f));
        if (ang > 1e-4f) v = Vector3Normalize(Vector3Lerp(v, to, std::min(1.0f, maxTurn / ang)));
        p.vel = Vector3Scale(v, p.def->boltSpeed);
        Vector3 next = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        RayHit wall = ctx_.physics.RayCast(p.pos, next);
        if (geom::SphereCapsuleOverlap(G(next), p.def->boltRadius, player_.Hurtbox())) {
            IncomingHit h;
            h.damage = p.def->boltDamage;
            h.from = p.pos;
            h.parryable = false;
            IncomingResult r = player_.ReceiveHit(h, ctx_.state.godMode, fx_);
            if (!r.ignored) {
                p.alive = false;
                fx_.push_back({FxType::Sound, next, 1.0f, "bolt_hit"});
                EmitterBurst b;
                b.pos = next;
                b.count = 20;
                b.color0 = Vector4{p.def->boltColor.x, p.def->boltColor.y, p.def->boltColor.z, 1};
                b.color1 = Vector4{0.2f, 0.3f, 1.0f, 0};
                b.spreadDeg = 180;
                particles_.Burst(b);
                continue;
            }
        }
        if (wall.hit || p.life <= 0) {
            p.alive = false;
            EmitterBurst b;
            b.pos = wall.hit ? wall.point : p.pos;
            b.count = 12;
            b.spreadDeg = 180;
            b.color0 = Vector4{p.def->boltColor.x, p.def->boltColor.y, p.def->boltColor.z, 1};
            b.color1 = Vector4{0.1f, 0.2f, 0.8f, 0};
            particles_.Burst(b);
            fx_.push_back({FxType::Sound, b.pos, 0.7f, "bolt_hit"});
            continue;
        }
        p.pos = next;
        if (Rand01(seed) < 0.7f) {
            ParticleDesc d;
            d.pos = p.pos;
            d.vel = Vector3{(Rand01(seed) - 0.5f) * 0.3f, (Rand01(seed) - 0.5f) * 0.3f, (Rand01(seed) - 0.5f) * 0.3f};
            d.life = 0.35f;
            d.size0 = p.def->boltRadius * 0.9f;
            d.size1 = 0.0f;
            d.color0 = Vector4{p.def->boltColor.x, p.def->boltColor.y, p.def->boltColor.z, 0.8f};
            d.color1 = Vector4{0.1f, 0.2f, 1.0f, 0.0f};
            particles_.Emit(d);
        }
    }
    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(), [](const Projectile& p) { return !p.alive; }),
                       projectiles_.end());
}

void Session::ProcessFx() {
    const json& hs = feel_.value("hitstop", json::object());
    const json& sh = feel_.value("shake", json::object());
    auto burst = [&](const char* preset, Vector3 pos, Vector3 dir, float scale) {
        const json& p = feel_.value(preset, json::object());
        EmitterBurst b;
        b.pos = pos;
        b.dir = dir;
        b.spreadDeg = 70;
        b.count = int(float(p.value("count", 12)) * scale);
        if (p.contains("speed")) {
            b.speedMin = p["speed"][0];
            b.speedMax = p["speed"][1];
        }
        if (p.contains("life")) {
            b.lifeMin = p["life"][0];
            b.lifeMax = p["life"][1];
        }
        if (p.contains("size")) {
            b.sizeMin = p["size"][0];
            b.sizeMax = p["size"][1];
        }
        Vector3 c = V3(p.value("color", json()), Vector3{4, 2, 0.8f});
        b.color0 = Vector4{c.x, c.y, c.z, 1};
        b.color1 = Vector4{c.x * 0.3f, c.y * 0.15f, c.z * 0.05f, 0};
        particles_.Burst(b);
    };
    auto& audio = ctx_.audio;
    for (const FxEvent& e : fx_) {
        switch (e.type) {
            case FxType::HitFlesh:
                Hitstop(hs.value("hit", 0.07f));
                cam_.AddShake(sh.value("hit", 0.22f));
                burst("hit_burst", e.pos, Vector3{0, 1, 0}, 1.0f);
                audio.PlayAt("hit_bone", e.pos);
                break;
            case FxType::HitCrit:
                Hitstop(hs.value("crit", 0.12f));
                cam_.AddShake(sh.value("crit", 0.5f));
                burst("hit_burst", e.pos, Vector3{0, 1, 0}, 2.5f);
                burst("sparks", e.pos, Vector3{0, 1, 0}, 1.0f);
                flash_ = 0.12f;
                flashColor_ = WHITE;
                flashDecay_ = 3.0f;
                audio.PlayAt("crit", e.pos);
                audio.PlayAt("hit_bone", e.pos);
                break;
            case FxType::Blocked:
            case FxType::EnemyBlocked:
                Hitstop(hs.value("block", 0.05f));
                cam_.AddShake(sh.value("block", 0.2f));
                burst("sparks", e.pos, Vector3Subtract(e.pos, player_.Pos()), 1.0f);
                audio.PlayAt("block", e.pos);
                break;
            case FxType::Parry: {
                Hitstop(hs.value("parry", 0.12f));
                cam_.AddShake(sh.value("parry", 0.35f));
                burst("parry_burst", e.pos, Vector3{0, 1, 0}, 1.0f);
                const json& pf = feel_.value("parry_flash", json::object());
                Vector3 c = V3(pf.value("color", json()), Vector3{190, 220, 255});
                flashColor_ = Color{(unsigned char)c.x, (unsigned char)c.y, (unsigned char)c.z, 255};
                flash_ = pf.value("strength", 0.55f);
                flashDecay_ = pf.value("decay", 4.0f);
                audio.PlayAt("parry", e.pos);
                break;
            }
            case FxType::PlayerHurt: {
                Hitstop(hs.value("hurt", 0.07f));
                cam_.AddShake(sh.value("hurt", 0.45f));
                burst("death_embers", player_.LanternPos(), Vector3{0, 1, 0}, 0.5f);
                const json& hf = feel_.value("hurt_flash", json::object());
                Vector3 c = V3(hf.value("color", json()), Vector3{255, 90, 40});
                flashColor_ = Color{(unsigned char)c.x, (unsigned char)c.y, (unsigned char)c.z, 255};
                flash_ = hf.value("strength", 0.18f);
                flashDecay_ = hf.value("decay", 5.0f);
                audio.PlayAt("player_hurt", e.pos);
                break;
            }
            case FxType::PostureBreak:
                Hitstop(hs.value("posture_break", 0.1f));
                burst("sparks", e.pos, Vector3{0, 1, 0}, 1.5f);
                audio.PlayAt("crit", e.pos, 0.7f);
                break;
            case FxType::Glint: {
                ParticleDesc d;
                d.pos = e.pos;
                d.life = 0.35f;
                d.size0 = 0.55f;
                d.size1 = 0.05f;
                d.color0 = Vector4{6.0f, 5.0f, 3.5f, 1.0f};
                d.color1 = Vector4{2.0f, 1.0f, 0.4f, 0.0f};
                particles_.Emit(d);
                audio.PlayAt("glint", e.pos);
                break;
            }
            case FxType::Swing: audio.PlayAt("swing", e.pos, 1.0f); break;
            case FxType::SwingHeavy: audio.PlayAt("swing_heavy", e.pos); break;
            case FxType::Footstep:
                audio.PlayAt("footstep", e.pos, e.strength);
                if (GetRandomValue(0, 3) == 0) audio.PlayAt("armor", e.pos, 0.6f);
                break;
            case FxType::Rattle: audio.PlayAt("rattle", e.pos, 0.6f); break;
            case FxType::Roll: audio.PlayAt("roll", e.pos); break;
            case FxType::EnemyDeath:
                burst("death_embers", e.pos, Vector3{0, 1, 0}, 1.0f);
                audio.PlayAt("skeleton_die", e.pos);
                embers_ += int(e.strength);
                emberFlash_ = 1.0f;
                break;
            case FxType::Death: audio.Play("death"); break;
            case FxType::Embers: burst("death_embers", e.pos, Vector3{0, 1, 0}, 0.3f * e.strength); break;
            case FxType::Sound: audio.PlayAt(e.name, e.pos, e.strength); break;
            case FxType::Shake: cam_.AddShake(e.strength); break;
        }
    }
    fx_.clear();
}

void Session::Step(const InputFrame& in, float dt) {
    ++steps_;
    simTime_ += dt;
#if defined(LK_DEBUG_TOOLS)
    if (IsKeyPressed(KEY_F1)) showDebug_ = !showDebug_;
    if (showDebug_ && IsKeyPressed(KEY_G)) ctx_.state.godMode = !ctx_.state.godMode;
#endif
    UpdateLockOn(in, dt);
    Enemy* lock = LockTarget();
    Vector3 lockPt = lock ? lock->LockPoint() : Vector3{};

    PlayerEnv pe;
    pe.camForward = cam_.Forward();
    pe.camRight = cam_.Right();
    pe.lockTarget = lock ? &lockPt : nullptr;
    pe.godMode = ctx_.state.godMode;
    pe.time = simTime_;
    // Riposte: a staggered enemy right in front of the knight.
    for (auto& e : enemies_)
        if (e->Staggered() && Vector3Distance(e->Pos(), player_.Pos()) < 1.8f &&
            geom::WithinArc(G(player_.Facing()), G(Vector3Subtract(e->Pos(), player_.Pos())), 50))
            pe.riposteReady = true;
    player_.Step(in, pe, dt, fx_);

    EnemyEnv ee;
    ee.playerPos = player_.Pos();
    ee.playerFacing = player_.Facing();
    ee.playerAlive = player_.Alive();
    ee.playerAttacking = player_.AttackActive();
    ee.physics = &ctx_.physics;
    ee.nav = &nav_;
    ee.tokens = &tokens_;
    ee.spawns = &spawns_;
    ee.time = simTime_;
    for (auto& e : enemies_)
        if (e->Alive()) ee.others.push_back(e->Pos());
    for (auto& e : enemies_) e->Step(ee, dt, fx_);

    ResolveCombat();
    UpdateProjectiles(dt);
    particles_.Update(dt);

    // Embers drifting from the lantern.
    emberAcc_ += dt * feel_.value("lantern_embers_per_sec", 5.0f) * (0.3f + 0.7f * player_.FlameFrac());
    unsigned seed = unsigned(steps_ * 2654435761u);
    while (emberAcc_ >= 1.0f && player_.Alive()) {
        emberAcc_ -= 1.0f;
        ParticleDesc d;
        d.pos = Vector3Add(player_.LanternPos(), Vector3{(Rand01(seed) - 0.5f) * 0.08f, 0.05f, (Rand01(seed) - 0.5f) * 0.08f});
        d.vel = Vector3{(Rand01(seed) - 0.5f) * 0.25f, 0.35f + Rand01(seed) * 0.35f, (Rand01(seed) - 0.5f) * 0.25f};
        d.life = 1.2f + Rand01(seed) * 1.0f;
        d.size0 = 0.022f + Rand01(seed) * 0.015f;
        d.size1 = 0.0f;
        d.color0 = Vector4{5.0f, 2.2f, 0.6f, 1.0f};
        d.color1 = Vector4{2.0f, 0.5f, 0.1f, 0.0f};
        d.gravity = Vector3{0, 0.15f, 0};
        d.drag = 0.6f;
        particles_.Emit(d);
    }

    ProcessFx();
    for (auto it = barTimers_.begin(); it != barTimers_.end();) {
        it->second -= dt;
        it = it->second <= 0 ? barTimers_.erase(it) : std::next(it);
    }
    emberFlash_ = std::max(0.0f, emberFlash_ - dt * 2.0f);
    float ff = player_.FlameFrac();
    flameTrail_ = ff >= flameTrail_ ? ff : std::max(ff, flameTrail_ - dt * 0.35f);

    // Lantern crackle below the low-flame threshold.
    float low = std::clamp(1.0f - ff / std::max(lantern_.sputterBelow, 0.01f), 0.0f, 1.0f);
    ctx_.audio.SetLoop("lantern_crackle", player_.Alive() ? low * 0.8f : 0.0f);

    lock = LockTarget();
    lockPt = lock ? lock->LockPoint() : Vector3{};
    cam_.Update(dt, player_.Pos(), in.look, lock ? &lockPt : nullptr, &ctx_.physics);
    ctx_.audio.SetListener(cam_.Cam().position, cam_.Forward(), cam_.Right());
}

void Session::Render(float alpha, int sw, int sh) {
    std::vector<PointLight> lights;
    GatherLights(lights, realTime_);
    ctx_.renderer.SetLights(lights, true);

    RenderSettings& s = ctx_.renderer.Settings();
    const json& rj = ctx_.data.Get("render.json");
    float baseV = rj.value("vignette", 0.45f), baseR = rj.value("vignette_radius", 0.95f);
    float low = std::clamp(1.0f - player_.FlameFrac() / std::max(lantern_.sputterBelow, 0.01f), 0.0f, 1.0f);
    s.vignette = baseV + (lantern_.lowVignette - baseV) * low;
    s.vignetteRadius = baseR + (lantern_.lowVignetteRadius - baseR) * low;

    Camera3D cam = cam_.Cam();
    Renderer& r = ctx_.renderer;
    r.BeginScene(cam);
    world_.Draw(r, cam.position, drawDistance_);
    Vector3 pp = player_.RenderPos(alpha);
    player_.Draw(r, alpha);
    r.DrawBlobShadow(pp, blobTune_.x, blobTune_.y);
    for (const auto& e : enemies_)
        if (Vector3Distance(e->Pos(), cam.position) < drawDistance_) e->Draw(r, alpha, realTime_);
    lanternMesh_.Draw(r, player_.LanternPos(), player_.Yaw(), player_.FlameFrac(), lanternState_.flicker,
                      Vector3{1.0f, 0.55f, 0.2f});
    // Additive pass: halos, shafts, trails, particles, bolts.
    r.DrawHalo(player_.LanternPos(), lantern_.haloSize * (0.8f + 0.2f * lanternState_.flicker), Color{255, 170, 90, 255},
               lantern_.haloIntensity * lanternState_.flicker * (0.3f + 0.7f * player_.FlameFrac()));
    for (const auto& z : zoneLights_)
        if (z.halo) r.DrawHalo(z.light.pos, 0.9f, Color{255, 150, 70, 255}, 1.4f * Flicker(realTime_, 8.0f, z.flicker, z.seed));
    for (const auto& p : projectiles_) r.DrawHalo(p.pos, p.def->boltRadius * 5.0f, Color{120, 170, 255, 255}, 3.0f);
    player_.DrawTrail(simTime_, trailShader_, trailIntensityLoc_);
    particles_.Draw(cam);
    r.EndScene();
    r.Present(sw, sh, flash_, flashColor_);
    RenderHud(sw, sh);
    if (showDebug_) RenderDebug(sw, sh);
}

void Session::RenderHud(int sw, int sh) {
    Ui& ui = ctx_.ui;
    HudState hs;
    hs.flame = player_.FlameFrac();
    hs.flameTrail = flameTrail_;
    hs.flameMax = player_.flameMax;
    hs.stamina = player_.StaminaFrac();
    hs.staminaMax = player_.stamina.max;
    hs.exhausted = player_.stamina.exhausted;
    hs.flasks = player_.flasks;
    hs.flaskMax = player_.flaskMax;
    hs.embers = embers_;
    hs.emberFlash = emberFlash_;
    hs.lowFlamePulse = std::clamp(1.0f - player_.FlameFrac() / std::max(lantern_.sputterBelow, 0.01f), 0.0f, 1.0f);
    DrawHud(ui, hs, realTime_);
    // Small bars over recently damaged enemies.
    Camera3D cam = cam_.Cam();
    for (auto& e : enemies_) {
        auto it = barTimers_.find(e->Id());
        if (it == barTimers_.end() || !e->Alive()) continue;
        Vector3 head = Vector3Add(e->Pos(), Vector3{0, e->Def().height * e->Scale() + 0.35f, 0});
        Vector3 toE = Vector3Subtract(head, cam.position);
        if (Vector3DotProduct(toE, Vector3Subtract(cam.target, cam.position)) <= 0) continue;
        Vector2 sp = GetWorldToScreen(head, cam);
        Vector2 vp{sp.x / ui.Scale(), sp.y / ui.Scale()};
        float a = std::clamp(it->second, 0.0f, 1.0f);
        DrawEnemyBar(ui, vp, e->Hp() / e->HpMax(), e->GetPosture().value / e->GetPosture().max, a);
    }
    // Lock-on reticle.
    if (Enemy* lock = LockTarget()) {
        Vector2 sp = GetWorldToScreen(lock->LockPoint(), cam);
        float s = 7.0f * ui.Scale();
        DrawCircleV(sp, s, Color{255, 240, 220, 230});
        DrawCircleLinesV(sp, s * 1.8f, Color{255, 200, 140, 160});
    }
    (void)sw;
    (void)sh;
}

void Session::RenderDebug(int sw, int sh) {
    Ui& ui = ctx_.ui;
    ui.Panel({ui.W() - 520, 40, 480, 330}, Color{255, 255, 255, 220});
    float x = ui.W() - 500, y = 58;
    auto line = [&](const std::string& s) {
        ui.Text(s, x, y, 24, Color{220, 230, 220, 255}, Align::Left, false);
        y += 30;
    };
    line(TextFormat("FPS %d  steps %d", GetFPS(), steps_));
    line(TextFormat("player %s  combo %d", PStateName(player_.State()), player_.ComboIndex()));
    line(TextFormat("flame %.0f/%.0f  stamina %.0f/%.0f%s", player_.flame, player_.flameMax, player_.stamina.value,
                    player_.stamina.max, player_.stamina.exhausted ? " EXH" : ""));
    if (Enemy* l = LockTarget())
        line(TextFormat("lock %s %s hp %.0f posture %.0f/%.0f", l->Def().name.c_str(), EStateName(l->State()), l->Hp(),
                        l->GetPosture().value, l->GetPosture().max));
    else line("lock -");
    line(TextFormat("enemies %zu  tokens %d  particles %d", enemies_.size(), tokens_.Count(), particles_.Alive()));
    line(TextFormat("draw calls %d", ctx_.renderer.DrawCalls()));
    line(TextFormat("god mode [G] %s", ctx_.state.godMode ? "ON" : "off"));
    Vector3 p = player_.Pos();
    line(TextFormat("pos %.1f %.1f %.1f", p.x, p.y, p.z));
    (void)sw;
    (void)sh;
}

}  // namespace lk
