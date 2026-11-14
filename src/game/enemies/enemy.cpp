#include "game/enemies/enemy.h"

#include <algorithm>
#include <cmath>

#include "engine/assets.h"
#include "engine/log.h"
#include "engine/physics.h"
#include "engine/renderer.h"
#include "game/context.h"
#include "engine/nav.h"

namespace lk {

namespace {
Vector3 V3(const json& j, Vector3 d) {
    if (!j.is_array() || j.size() < 3) return d;
    return Vector3{j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}
float WrapAngle(float a) {
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}
float FlatDist(Vector3 a, Vector3 b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}
}  // namespace

const char* EStateName(EState s) {
    switch (s) {
        case EState::Idle: return "idle";
        case EState::Alert: return "alert";
        case EState::Approach: return "approach";
        case EState::Strafe: return "strafe";
        case EState::Attack: return "attack";
        case EState::Recover: return "recover";
        case EState::Hit: return "hit";
        case EState::Stagger: return "stagger";
        case EState::Dead: return "dead";
        case EState::Teleport: return "teleport";
        case EState::Dormant: return "dormant";
    }
    return "?";
}

bool EnemyDef::Load(const std::string& k, const json& j, const json& animMap) {
    key = k;
    raw = j;
    anims = animMap;
    name = j.value("name", k);
    model = j.value("model", "");
    brain = j.value("brain", "melee");
    hp = j.value("hp", hp);
    posture = j.value("posture", posture);
    postureDecay = j.value("posture_decay", postureDecay);
    postureDelay = j.value("posture_decay_delay", postureDelay);
    staggerTime = j.value("stagger_time", staggerTime);
    embers = j.value("embers", embers);
    scale = j.value("scale", scale);
    radius = j.value("radius", radius);
    height = j.value("height", height);
    turnSpeed = j.value("turn_speed_deg", turnSpeed);
    walkSpeed = j.value("walk_speed", walkSpeed);
    runSpeed = j.value("run_speed", runSpeed);
    circleSpeed = j.value("circle_speed", circleSpeed);
    aggroRange = j.value("aggro_range", aggroRange);
    loseRange = j.value("lose_range", loseRange);
    if (j.contains("strafe_range")) {
        strafeMin = j["strafe_range"][0].get<float>();
        strafeMax = j["strafe_range"][1].get<float>();
    }
    if (j.contains("attack_cooldown")) {
        cooldownMin = j["attack_cooldown"][0].get<float>();
        cooldownMax = j["attack_cooldown"][1].get<float>();
    }
    swingInterval = j.value("swing_interval", swingInterval);
    flinch = j.value("flinch", flinch);
    if (j.contains("shield")) {
        hasShield = true;
        shieldBlocksLight = j["shield"].value("blocks_light", true);
        shieldArc = j["shield"].value("front_angle_deg", shieldArc);
        shieldPostureMult = j["shield"].value("block_posture_mult", shieldPostureMult);
    }
    if (j.contains("weapon") && j["weapon"].is_object()) {
        const json& w = j["weapon"];
        weaponModel = w.value("model", "");
        weaponBone = w.value("bone", "handslot.r");
        hitBase = V3(w.value("base", json()), hitBase);
        hitTip = V3(w.value("tip", json()), hitTip);
        hitRadius = w.value("radius", hitRadius);
    }
    if (j.contains("offhand") && j["offhand"].is_object()) {
        offhandModel = j["offhand"].value("model", "");
        offhandBone = j["offhand"].value("bone", "handslot.l");
    }
    if (j.contains("eyes")) eyeColor = V3(j["eyes"].value("eye_color", json()), eyeColor);
    glintColor = V3(j.value("glint_color", json()), glintColor);
    if (j.contains("tint")) {
        auto t = j["tint"];
        tint = Color{(unsigned char)t[0].get<int>(), (unsigned char)t[1].get<int>(), (unsigned char)t[2].get<int>(), 255};
    }
    teleportRange = j.value("teleport_range", teleportRange);
    if (j.contains("teleport_distance")) {
        teleportMin = j["teleport_distance"][0].get<float>();
        teleportMax = j["teleport_distance"][1].get<float>();
    }
    teleportCooldown = j.value("teleport_cooldown", teleportCooldown);
    if (j.contains("bolt")) {
        const json& b = j["bolt"];
        boltSpeed = b.value("speed", boltSpeed);
        boltDamage = b.value("damage", boltDamage);
        boltRadius = b.value("radius", boltRadius);
        boltLife = b.value("life", boltLife);
        boltTurn = b.value("turn_rate_deg", boltTurn);
        boltColor = V3(b.value("color", json()), boltColor);
    }
    moves.clear();
    for (const auto& m : j.value("moves", json::array())) {
        EnemyMove mv;
        mv.name = m.value("name", "");
        mv.anim = m.value("anim", "");
        mv.combo = m.value("combo", "");
        mv.comboChance = m.value("combo_chance", 0.0f);
        if (m.contains("range")) {
            mv.rangeMin = m["range"][0].get<float>();
            mv.rangeMax = m["range"][1].get<float>();
        }
        mv.damage = m.value("damage", mv.damage);
        mv.posturePlayer = m.value("posture_player", 0.0f);
        if (m.contains("hold") && m["hold"].is_array()) {
            mv.holdMin = m["hold"][0].get<float>();
            mv.holdMax = m["hold"][1].get<float>();
        } else {
            mv.holdMin = mv.holdMax = m.value("hold", 0.4f);
        }
        mv.lunge = m.value("lunge", mv.lunge);
        mv.weight = m.value("weight", 1.0f);
        mv.projectile = m.value("projectile", false);
        mv.aoe = m.value("aoe", false);
        mv.aoeRadius = m.value("aoe_radius", 0.0f);
        mv.fx = m.value("fx", "");
        moves.push_back(mv);
    }
    return !model.empty();
}

bool AttackTokens::Request(int id) {
    if (holders_.count(id)) return true;
    if (int(holders_.size()) >= max) return false;
    holders_.insert(id);
    return true;
}

Enemy::Enemy() = default;
Enemy::~Enemy() = default;

float Enemy::Rand() {
    rng_ = rng_ * 1664525u + 1013904223u;
    return float((rng_ >> 8) & 0xFFFF) / 65535.0f;
}

bool Enemy::Init(Ctx& ctx, const EnemyDef& def, Vector3 pos, float yawDeg, int id, const std::string& spawnId) {
    ctx_ = &ctx;
    def_ = &def;
    id_ = id;
    spawnId_ = spawnId;
    rng_ = unsigned(id * 2654435761u) | 1u;
    spawnPos_ = pos;
    spawnYaw_ = yawDeg * DEG2RAD;
    if (!vis_.Init(ctx.assets, def.anims, def.model)) return false;
    // KayKit skeleton GLBs carry no weapons: attach the pack's item models to the hand slots.
    if (!def.weaponModel.empty()) vis_.Attach(ctx.assets, def.weaponModel, def.weaponBone, MatrixIdentity(), "weapon");
    if (!def.offhandModel.empty()) vis_.Attach(ctx.assets, def.offhandModel, def.offhandBone, MatrixIdentity(), "offhand");
    for (const auto& n : vis_.Asset()->meshNames)
        if (n.find("Eyes") != std::string::npos) vis_.SetEmissiveMesh(n);
    weaponBone_ = vis_.Bone(def.weaponBone.empty() ? "handslot.r" : def.weaponBone);
    CharacterDesc cd;
    cd.radius = def.radius * def.scale;
    cd.height = def.height * def.scale;
    body_ = ctx.physics.CreateCharacter(cd, pos);
    tint_ = def.tint;
    Respawn();
    return body_ != nullptr;
}

void Enemy::Respawn() {
    pos_ = prevPos_ = spawnPos_;
    yaw_ = prevYaw_ = spawnYaw_;
    vel_ = {0, 0, 0};
    hp_ = def_->hp;
    posture_ = Posture{};
    posture_.max = def_->posture;
    posture_.decayPerSec = def_->postureDecay;
    posture_.decayDelay = def_->postureDelay;
    posture_.staggerDuration = def_->staggerTime;
    aggro_ = false;
    move_ = nullptr;
    swing_.Stop();
    hitFlash_ = 0;
    lastHurt_ = -100;
    cooldown_ = 0.8f + Rand();
    swingTimer_ = 0;
    if (body_) {
        body_->SetEnabled(true);
        body_->SetPosition(pos_);
    }
    vis_.Anim().SetSpeedScale(1.0f);
    ChangeState(EState::Idle);
    vis_.Play("idle", 0.0f, true, Rand() * 0.5f);
}

void Enemy::SetDormant(bool d) {
    if (d) {
        ChangeState(EState::Dormant);
        vis_.Play("idle", 0.2f);
    } else if (state_ == EState::Dormant) {
        ChangeState(EState::Idle);
    }
}

void Enemy::SetPos(Vector3 p) {
    pos_ = prevPos_ = p;
    if (body_) body_->SetPosition(p);
}

void Enemy::ChangeState(EState s) {
    state_ = s;
    stateTime_ = 0;
}

Matrix Enemy::WorldMatrix(Vector3 p, float yaw) const {
    float s = 0.5f * def_->scale;
    return MatrixMultiply(MatrixMultiply(MatrixScale(s, s, s), MatrixRotateY(yaw)), MatrixTranslate(p.x, p.y, p.z));
}

Vector3 Enemy::Facing() const { return Vector3{std::sin(yaw_), 0, std::cos(yaw_)}; }

Vector3 Enemy::LockPoint() const { return Vector3Add(pos_, Vector3{0, def_->height * def_->scale * 0.72f, 0}); }

geom::Capsule Enemy::Hurtbox() const {
    float r = def_->radius * def_->scale * 1.3f;
    float h = def_->height * def_->scale;
    return geom::Capsule{G(Vector3Add(pos_, Vector3{0, r, 0})), G(Vector3Add(pos_, Vector3{0, std::max(r, h - r * 0.5f), 0})), r};
}

void Enemy::WeaponPoints(Vector3& base, Vector3& tip) const {
    Matrix bw = vis_.BoneWorld(weaponBone_, WorldMatrix(pos_, yaw_));
    base = Vector3Transform(def_->hitBase, bw);
    tip = Vector3Transform(def_->hitTip, bw);
}

void Enemy::FaceToward(Vector3 target, float dt, float speedMult) {
    Vector3 to = Vector3Subtract(target, pos_);
    if (to.x * to.x + to.z * to.z < 1e-4f) return;
    float want = std::atan2(to.x, to.z);
    float d = WrapAngle(want - yaw_);
    float step = def_->turnSpeed * DEG2RAD * speedMult * dt;
    yaw_ = WrapAngle(yaw_ + std::clamp(d, -step, step));
}

void Enemy::MoveToward(Vector3 target, float speed, const EnemyEnv& env, float dt) {
    Vector3 goal = target;
    if (env.nav) goal = env.nav->NextWaypoint(pos_, target);
    Vector3 to = Vector3Subtract(goal, pos_);
    to.y = 0;
    float len = Vector3Length(to);
    if (len < 0.05f) {
        vel_ = {0, 0, 0};
        return;
    }
    Vector3 dir = Vector3Scale(to, 1.0f / len);
    vel_ = Vector3Scale(dir, speed);
    FaceToward(Vector3Add(pos_, dir), dt);
    (void)dt;
}

const EnemyMove* Enemy::PickMove(float dist) const {
    float total = 0;
    for (const auto& m : def_->moves)
        if (m.weight > 0 && dist >= m.rangeMin && dist <= m.rangeMax) total += m.weight;
    if (total <= 0) return nullptr;
    float r = const_cast<Enemy*>(this)->Rand() * total;
    for (const auto& m : def_->moves) {
        if (m.weight <= 0 || dist < m.rangeMin || dist > m.rangeMax) continue;
        r -= m.weight;
        if (r <= 0) return &m;
    }
    return nullptr;
}

void Enemy::StartAttack(const EnemyMove& m, FxQueue& fx) {
    move_ = &m;
    ChangeState(EState::Attack);
    vis_.Play(m.anim, 0.12f, true);
    vis_.Anim().SetSpeedScale(1.0f);
    hold_ = m.holdMin + (m.holdMax - m.holdMin) * Rand();
    holding_ = false;
    holdDone_ = false;
    firedProjectile_ = false;
    lungeLeft_ = m.lunge;
    swing_.Stop();
    (void)fx;
}

void Enemy::OnParried(FxQueue& fx) {
    swing_.Stop();
    posture_.Add(posture_.max * 2.0f);  // force a stagger
    ChangeState(EState::Stagger);
    vis_.Anim().SetSpeedScale(1.0f);
    vis_.Play("stagger", 0.06f, true);
    fx.push_back({FxType::PostureBreak, LockPoint(), 1.0f, ""});
}

StrikeResult Enemy::TakeStrike(const PlayerStrike& s, float critMult, FxQueue& fx) {
    StrikeResult r;
    if (!Alive() || state_ == EState::Dormant) return r;
    r.landed = true;
    lastHurt_ = now_;
    aggro_ = true;
    Vector3 toAtk = Vector3Subtract(s.from, pos_);
    bool frontal = geom::WithinArc(G(Facing()), G(toAtk), def_->shieldArc * 0.5f);
    bool canBlock = def_->hasShield && def_->shieldBlocksLight && !s.heavy && frontal &&
                    (state_ == EState::Idle || state_ == EState::Strafe || state_ == EState::Approach ||
                     state_ == EState::Recover || state_ == EState::Hit || (state_ == EState::Attack && !swing_.active && !holdDone_));
    if (canBlock) {
        r.blocked = true;
        swing_.Stop();
        if (posture_.Add(s.posture * def_->shieldPostureMult)) {
            r.postureBroken = true;
            ChangeState(EState::Stagger);
            vis_.Play("stagger", 0.06f, true);
            fx.push_back({FxType::PostureBreak, LockPoint(), 1.0f, ""});
        } else {
            ChangeState(EState::Hit);
            vis_.Play("block_hit", 0.05f, true);
        }
        fx.push_back({FxType::EnemyBlocked, s.point, 1.0f, ""});
        return r;
    }
    float dmg = s.damage;
    if (Staggered()) {
        r.critical = true;
        dmg *= critMult;
        posture_.Reset();
    }
    hp_ -= dmg;
    r.damageDealt = dmg;
    hitFlash_ = 1.0f;
    if (hp_ <= 0) {
        hp_ = 0;
        r.killed = true;
        swing_.Stop();
        ChangeState(EState::Dead);
        vis_.Anim().SetSpeedScale(1.0f);
        vis_.Play("death", 0.08f, true);
        if (body_) body_->SetEnabled(false);
        fx.push_back({FxType::EnemyDeath, LockPoint(), float(def_->embers), def_->key});
        return r;
    }
    if (!r.critical && posture_.Add(s.posture)) {
        r.postureBroken = true;
        swing_.Stop();
        ChangeState(EState::Stagger);
        vis_.Anim().SetSpeedScale(1.0f);
        vis_.Play("stagger", 0.06f, true);
        fx.push_back({FxType::PostureBreak, LockPoint(), 1.0f, ""});
    } else if (r.critical) {
        ChangeState(EState::Hit);
        vis_.Play("hit", 0.05f, true);
    } else if (def_->flinch && state_ != EState::Attack && state_ != EState::Stagger) {
        ChangeState(EState::Hit);
        vis_.Play("hit", 0.05f, true);
    }
    return r;
}

void Enemy::Think(const EnemyEnv& env, float dt, FxQueue& fx) {
    const EnemyDef& d = *def_;
    float dist = FlatDist(pos_, env.playerPos);
    bool training = d.brain == "training";
    vel_ = {0, 0, 0};

    auto canSee = [&]() {
        if (!env.physics) return true;
        Vector3 eye = Vector3Add(pos_, Vector3{0, d.height * d.scale * 0.8f, 0});
        Vector3 tgt = Vector3Add(env.playerPos, Vector3{0, 0.8f, 0});
        return !env.physics->RayCast(eye, tgt).hit;
    };
    auto release = [&]() {
        if (env.tokens) env.tokens->Release(id_);
    };

    switch (state_) {
        case EState::Dormant:
            vis_.Play("idle", 0.2f);
            break;
        case EState::Idle: {
            if (training) {
                vis_.Play("idle", 0.2f);
                if (env.playerAlive && dist < d.aggroRange) {
                    FaceToward(env.playerPos, dt);
                    swingTimer_ += dt;
                    if (swingTimer_ >= d.swingInterval && !d.moves.empty()) {
                        swingTimer_ = 0;
                        StartAttack(d.moves[0], fx);
                    }
                }
                break;
            }
            // Return to the spawn point when not aggroed.
            if (FlatDist(pos_, spawnPos_) > 0.6f) {
                MoveToward(spawnPos_, d.walkSpeed, env, dt);
                vis_.Play("walk", 0.2f);
            } else {
                vis_.Play("idle", 0.25f);
                float d0 = WrapAngle(spawnYaw_ - yaw_);
                yaw_ = WrapAngle(yaw_ + std::clamp(d0, -dt * 2.0f, dt * 2.0f));
            }
            if (env.playerAlive && dist < d.aggroRange && std::fabs(env.playerPos.y - pos_.y) < 1.6f && canSee()) {
                aggro_ = true;
                ChangeState(EState::Alert);
                vis_.Play("alert", 0.1f, true);
                fx.push_back({FxType::Sound, LockPoint(), 1.0f, "skeleton_alert"});
            }
            break;
        }
        case EState::Alert:
            FaceToward(env.playerPos, dt);
            if (stateTime_ > 0.9f || vis_.Anim().Finished()) ChangeState(EState::Approach);
            break;
        case EState::Approach: {
            if (!env.playerAlive || dist > d.loseRange) {
                aggro_ = false;
                release();
                ChangeState(EState::Idle);
                break;
            }
            if (!canSee()) lostSight_ += dt;
            else lostSight_ = 0;
            if (lostSight_ > 6.0f) {
                aggro_ = false;
                ChangeState(EState::Idle);
                break;
            }
            if (dist <= d.strafeMax) {
                ChangeState(EState::Strafe);
                break;
            }
            MoveToward(env.playerPos, d.runSpeed, env, dt);
            vis_.Play("run", 0.18f);
            break;
        }
        case EState::Strafe: {
            if (!env.playerAlive || dist > d.loseRange) {
                aggro_ = false;
                release();
                ChangeState(EState::Idle);
                break;
            }
            if (dist > d.strafeMax + 1.0f) {
                ChangeState(EState::Approach);
                break;
            }
            FaceToward(env.playerPos, dt);
            // Mage: teleport away when the player closes in.
            if (d.brain == "mage" && dist < d.teleportRange && teleportCd_ <= 0) {
                ChangeState(EState::Teleport);
                vis_.Play("teleport", 0.1f, true);
                fx.push_back({FxType::Sound, LockPoint(), 1.0f, "teleport"});
                break;
            }
            strafeSwitch_ -= dt;
            if (strafeSwitch_ <= 0) {
                strafeDir_ = Rand() < 0.5f ? -1.0f : 1.0f;
                strafeSwitch_ = 1.5f + Rand() * 2.0f;
            }
            Vector3 to = Vector3Normalize(Vector3{env.playerPos.x - pos_.x, 0, env.playerPos.z - pos_.z});
            Vector3 side{-to.z * strafeDir_, 0, to.x * strafeDir_};
            float want = (d.strafeMin + d.strafeMax) * 0.5f;
            float radial = std::clamp((dist - want) * 0.8f, -1.0f, 1.0f);
            float sp = (d.brain == "rogue") ? d.circleSpeed : d.walkSpeed * 0.7f;
            vel_ = Vector3Add(Vector3Scale(side, sp), Vector3Scale(to, radial * d.walkSpeed));
            vis_.Play(strafeDir_ > 0 ? "strafe_left" : "strafe_right", 0.25f);
            cooldown_ -= dt;
            if (cooldown_ <= 0 && env.playerAlive) {
                const EnemyMove* m = PickMove(dist);
                if (!m && dist > 0.5f && d.brain != "mage") {
                    // Close the gap to melee range.
                    vel_ = Vector3Scale(to, d.walkSpeed);
                    vis_.Play("walk", 0.2f);
                } else if (m && (!env.tokens || env.tokens->Request(id_))) {
                    StartAttack(*m, fx);
                }
            }
            break;
        }
        case EState::Attack: {
            const ActionInfo* ai = vis_.Info(move_ ? move_->anim : "");
            float t = vis_.Anim().Time();
            float windup = ai && def_->anims["actions"].contains(move_->anim)
                               ? def_->anims["actions"][move_->anim].value("windup", ai->hit * 0.6f)
                               : 0.3f;
            bool beforeHit = ai && t < ai->hit;
            if (beforeHit) FaceToward(env.playerPos, dt, holding_ ? 1.2f : 0.7f);
            if (!holdDone_ && !holding_ && t >= windup) {
                holding_ = true;
                holdLeft_ = hold_;
                vis_.Anim().SetSpeedScale(0.04f);
                glint_ = 1.0f;
                Vector3 b, tp;
                WeaponPoints(b, tp);
                fx.push_back({FxType::Glint, tp, 1.0f, ""});
            }
            if (holding_) {
                holdLeft_ -= dt;
                if (holdLeft_ <= 0) {
                    holding_ = false;
                    holdDone_ = true;
                    vis_.Anim().SetSpeedScale(1.0f);
                }
            }
            // Lunge between the end of the hold and the hit frame.
            if (holdDone_ && beforeHit && lungeLeft_ > 0 && ai) {
                float remain = std::max(0.05f, ai->Real(ai->hit - t));
                float sp = lungeLeft_ / remain;
                Vector3 f = Facing();
                if (dist < 0.9f && move_->lunge < 1.5f) sp = 0;  // do not walk through the player
                vel_ = Vector3Scale(f, sp);
                lungeLeft_ -= sp * dt;
            }
            if (vis_.Anim().Finished()) {
                swing_.Stop();
                const EnemyMove* next = nullptr;
                if (move_ && !move_->combo.empty() && Rand() < move_->comboChance)
                    for (const auto& m : d.moves)
                        if (m.name == move_->combo && dist <= m.rangeMax + 0.5f) next = &m;
                if (next) {
                    StartAttack(*next, fx);
                } else {
                    release();
                    ChangeState(EState::Recover);
                    vis_.Play(training ? "idle" : "idle", 0.2f);
                    cooldown_ = d.cooldownMin + (d.cooldownMax - d.cooldownMin) * Rand();
                }
            }
            break;
        }
        case EState::Recover:
            FaceToward(env.playerPos, dt, 0.5f);
            if (stateTime_ > 0.55f) ChangeState(training ? EState::Idle : EState::Strafe);
            break;
        case EState::Hit:
            if (stateTime_ > 0.45f) {
                ChangeState(training ? EState::Idle : EState::Strafe);
                if (!training) cooldown_ = std::min(cooldown_, 0.3f + Rand() * 0.6f);  // punish back
            }
            break;
        case EState::Stagger:
            release();
            if (!posture_.Staggered() && stateTime_ > 0.3f) {
                ChangeState(EState::Recover);
                vis_.Play("idle", 0.25f);
            }
            break;
        case EState::Teleport: {
            if (stateTime_ > 0.55f) {
                // Blink to a clear spot away from the player.
                Vector3 away = Vector3Normalize(Vector3{pos_.x - env.playerPos.x, 0, pos_.z - env.playerPos.z});
                float best = -1;
                Vector3 bestPos = pos_;
                for (int k = 0; k < 10; ++k) {
                    float ang = (Rand() - 0.5f) * PI * 1.4f;
                    Vector3 dir{away.x * std::cos(ang) - away.z * std::sin(ang), 0, away.x * std::sin(ang) + away.z * std::cos(ang)};
                    float dd = d.teleportMin + (d.teleportMax - d.teleportMin) * Rand();
                    Vector3 cand = Vector3Add(pos_, Vector3Scale(dir, dd));
                    Vector3 chest = Vector3Add(pos_, Vector3{0, 0.8f, 0});
                    Vector3 candChest = Vector3Add(cand, Vector3{0, 0.8f, 0});
                    if (env.physics && env.physics->RayCast(chest, candChest).hit) continue;
                    if (env.nav && !env.nav->Walkable(cand)) continue;
                    float score = FlatDist(cand, env.playerPos);
                    if (score > best) {
                        best = score;
                        bestPos = cand;
                    }
                }
                if (best > 0) {
                    fx.push_back({FxType::Sound, LockPoint(), 1.0f, "teleport"});
                    SetPos(bestPos);
                }
                teleportCd_ = d.teleportCooldown;
                cooldown_ = 0.4f;
                ChangeState(EState::Strafe);
            }
            break;
        }
        case EState::Dead:
            break;
    }
}

void Enemy::Step(const EnemyEnv& env, float dt, FxQueue& fx) {
    prevPos_ = pos_;
    prevYaw_ = yaw_;
    now_ = env.time;
    stateTime_ += dt;
    posture_.Update(dt);
    hitFlash_ = std::max(0.0f, hitFlash_ - dt * 8.0f);
    glint_ = std::max(0.0f, glint_ - dt * 2.0f);
    teleportCd_ -= dt;
    if (state_ != EState::Dead) Think(env, dt, fx);

    vis_.Update(dt);
    for (const auto& e : vis_.Anim().Events()) {
        if (e.name == "rattle" || e.name == "footstep") fx.push_back({FxType::Rattle, pos_, 1.0f, ""});
        else if (e.name == "stomp") fx.push_back({FxType::Sound, pos_, 1.0f, "slam"});
        else if (state_ == EState::Attack && e.name == "hit") {
            if (move_ && move_->projectile) {
                if (!firedProjectile_ && env.spawns) {
                    Vector3 b, tp;
                    WeaponPoints(b, tp);
                    Vector3 dir = Vector3Normalize(Vector3Subtract(Vector3Add(env.playerPos, Vector3{0, 0.7f, 0}), tp));
                    env.spawns->push_back({tp, dir, id_, def_});
                    firedProjectile_ = true;
                    fx.push_back({FxType::Sound, tp, 1.0f, "mage_cast"});
                }
            } else {
                swing_.Start();
                swingStartedAt_ = now_;
                fx.push_back({FxType::Swing, pos_, def_->scale, ""});
            }
        } else if (e.name == "hit_end") {
            swing_.Stop();
        }
    }

    // Separation from other enemies.
    if (state_ != EState::Dead && state_ != EState::Dormant)
        for (const Vector3& o : env.others) {
            Vector3 dv = Vector3Subtract(pos_, o);
            dv.y = 0;
            float l = Vector3Length(dv);
            float minD = def_->radius * def_->scale * 2.4f;
            if (l > 0.001f && l < minD) vel_ = Vector3Add(vel_, Vector3Scale(dv, (minD - l) / l * 2.5f));
        }
    if (body_ && state_ != EState::Dead) {
        body_->Move(vel_, dt, -20.0f);
        pos_ = body_->Position();
    }
}

void Enemy::Draw(Renderer& r, float alpha, float time) const {
    Vector3 p = Vector3Lerp(prevPos_, pos_, alpha);
    float yaw = prevYaw_ + WrapAngle(yaw_ - prevYaw_) * alpha;
    DrawParams dp;
    dp.tint = tint_;
    dp.hitFlash = hitFlash_;
    float eyePulse = 0.85f + 0.15f * std::sin(time * 3.0f + float(id_));
    float eyeK = state_ == EState::Dead ? std::max(0.0f, 1.0f - stateTime_ * 0.8f) : eyePulse;
    if (state_ == EState::Dormant) eyeK *= 0.15f;
    dp.maskEmissive = Vector3Scale(def_->eyeColor, eyeK);
    Matrix w = WorldMatrix(p, yaw);
    vis_.Draw(r, w, dp);
    float blob = def_->radius * def_->scale * 1.7f;
    r.DrawBlobShadow(p, blob, 0.55f);
}

}  // namespace lk
