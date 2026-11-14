#include "game/player/player.h"

#include <algorithm>
#include <cmath>

#include "engine/assets.h"
#include "engine/data_store.h"
#include "engine/log.h"
#include "engine/physics.h"
#include "engine/renderer.h"
#include "game/context.h"

namespace lk {

namespace {
constexpr float kModelScale = 0.5f;
Vector3 V3(const json& j, Vector3 d) {
    if (!j.is_array() || j.size() < 3) return d;
    return Vector3{j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}
float WrapAngle(float a) {
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}
}  // namespace

const char* PStateName(PState s) {
    switch (s) {
        case PState::Move: return "move";
        case PState::Roll: return "roll";
        case PState::Attack: return "attack";
        case PState::Parry: return "parry";
        case PState::BlockHit: return "block_hit";
        case PState::Hit: return "hit";
        case PState::GuardBreak: return "guard_break";
        case PState::Drink: return "drink";
        case PState::Interact: return "interact";
        case PState::Rest: return "rest";
        case PState::Dead: return "dead";
    }
    return "?";
}

void PlayerTuning::Load(const json& j) {
    flameBase = j.value("flame_max_base", flameBase);
    flamePerVigor = j.value("flame_per_vigor", flamePerVigor);
    staminaBase = j.value("stamina_max_base", staminaBase);
    staminaPerEnd = j.value("stamina_per_endurance", staminaPerEnd);
    damagePerMight = j.value("damage_per_might", damagePerMight);
    staminaRegen = j.value("stamina_regen", staminaRegen);
    staminaDelay = j.value("stamina_regen_delay", staminaDelay);
    walkSpeed = j.value("walk_speed", walkSpeed);
    runSpeed = j.value("run_speed", runSpeed);
    sprintSpeed = j.value("sprint_speed", sprintSpeed);
    exhaustedSpeed = j.value("exhausted_speed", exhaustedSpeed);
    turnSpeed = j.value("turn_speed_deg", turnSpeed);
    accel = j.value("acceleration", accel);
    gravity = j.value("gravity", gravity);
    radius = j.value("capsule_radius", radius);
    height = j.value("capsule_height", height);
    walkThreshold = j.value("walk_threshold", walkThreshold);
    if (j.contains("costs")) {
        const json& c = j["costs"];
        costLight = c.value("light", costLight);
        costHeavy = c.value("heavy", costHeavy);
        costRoll = c.value("roll", costRoll);
        costSprint = c.value("sprint_per_s", costSprint);
        costParry = c.value("parry", costParry);
    }
    if (j.contains("roll")) {
        const json& r = j["roll"];
        roll.duration = r.value("duration", roll.duration);
        roll.iframeStart = r.value("iframe_start", roll.iframeStart);
        roll.iframeEnd = r.value("iframe_end", roll.iframeEnd);
        rollDistance = r.value("distance", rollDistance);
        rollTapMax = r.value("tap_max", rollTapMax);
        rollRecoveryCancel = r.value("recovery_cancel", rollRecoveryCancel);
    }
    comboWindow = j.value("light_combo_window", comboWindow);
    inputBuffer = j.value("input_buffer", inputBuffer);
    if (j.contains("heavy")) {
        chargeMax = j["heavy"].value("charge_max", chargeMax);
        chargeBonus = j["heavy"].value("charge_bonus", chargeBonus);
        chargeMin = j["heavy"].value("charge_min", chargeMin);
    }
    if (j.contains("block")) {
        const json& b = j["block"];
        block.damageMult = b.value("damage_mult", block.damageMult);
        block.staminaPerDamage = b.value("stamina_per_damage", block.staminaPerDamage);
        blockArc = b.value("front_angle_deg", blockArc);
        blockWalk = b.value("walk_speed", blockWalk);
        parryWindow = b.value("parry_window", parryWindow);
        parryCooldown = b.value("parry_cooldown", parryCooldown);
    }
    riposteMult = j.value("riposte_mult", riposteMult);
    critMult = j.value("critical_mult", critMult);
    hitStun = j.value("hit_stun", hitStun);
    guardBreak = j.value("guard_break_stun", guardBreak);
    if (j.contains("attack_lunge")) {
        lungeLight = j["attack_lunge"].value("light", lungeLight);
        lungeHeavy = j["attack_lunge"].value("heavy", lungeHeavy);
        lungeRiposte = j["attack_lunge"].value("riposte", lungeRiposte);
    }
    if (j.contains("flask")) {
        const json& f = j["flask"];
        flaskHeal = f.value("heal", flaskHeal);
        drinkTime = f.value("drink_time", drinkTime);
        healAt = f.value("heal_at", healAt);
        drinkWalk = f.value("walk_speed", drinkWalk);
        flaskStart = f.value("start", flaskStart);
        flaskMaxShards = f.value("max_with_shards", flaskMaxShards);
    }
    interactRange = j.value("interact_range", interactRange);
    invulnAfterHit = j.value("invuln_after_hit", invulnAfterHit);
    if (j.contains("lockon")) {
        lockRange = j["lockon"].value("range", lockRange);
        lockLostTime = j["lockon"].value("lost_time", lockLostTime);
        lockMaxAngle = j["lockon"].value("max_angle_deg", lockMaxAngle);
    }
}

bool WeaponDef::Load(const std::string& k, const json& j) {
    key = k;
    name = j.value("name", k);
    anims = j.value("anims", "anims/knight.json");
    show = j.value("show_meshes", std::vector<std::string>{});
    hide = j.value("hide_meshes", std::vector<std::string>{});
    if (j.contains("attach") && j["attach"].is_object()) {
        attachModel = j["attach"].value("model", "");
        attachBone = j["attach"].value("bone", "handslot.r");
    }
    shield = j.value("shield", true);
    dmgLight = j.value("damage_light", dmgLight);
    dmgHeavy = j.value("damage_heavy", dmgHeavy);
    postureLight = j.value("posture_light", postureLight);
    postureHeavy = j.value("posture_heavy", postureHeavy);
    staminaLight = j.value("stamina_light", staminaLight);
    staminaHeavy = j.value("stamina_heavy", staminaHeavy);
    if (j.contains("hitbox")) {
        hitBone = j["hitbox"].value("bone", hitBone);
        hitBase = V3(j["hitbox"].value("base", json()), hitBase);
        hitTip = V3(j["hitbox"].value("tip", json()), hitTip);
        hitRadius = j["hitbox"].value("radius", hitRadius);
    }
    if (j.contains("trail_color")) {
        auto c = j["trail_color"];
        trailColor = Color{(unsigned char)c[0].get<int>(), (unsigned char)c[1].get<int>(), (unsigned char)c[2].get<int>(), 255};
    }
    trailIntensity = j.value("trail_intensity", trailIntensity);
    return true;
}

Player::Player() = default;
Player::~Player() = default;

bool Player::Init(Ctx& ctx, Vector3 pos, float yawDeg) {
    ctx_ = &ctx;
    LoadTuning(ctx.data.Get("player/stats.json"));
    WeaponDef sword;
    sword.Load("sword", ctx.data.Get("weapons/sword.json"));
    if (!Equip(sword)) return false;
    CharacterDesc cd;
    cd.radius = tune_.radius;
    cd.height = tune_.height;
    body_ = ctx.physics.CreateCharacter(cd, pos);
    if (!body_) return false;
    const json& lr = ctx.data.Get("render.json");
    if (lr.contains("lantern")) lanternOffset_ = V3(lr["lantern"].value("offset", json()), lanternOffset_);
    Teleport(pos, yawDeg);
    flame = flameMax;
    stamina.value = stamina.max;
    flasks = flaskMax = tune_.flaskStart;
    return true;
}

void Player::LoadTuning(const json& stats) {
    tune_.Load(stats);
    stamina.regenPerSec = tune_.staminaRegen;
    stamina.regenDelay = tune_.staminaDelay;
    parry_.window = tune_.parryWindow;
    parry_.cooldown = tune_.parryCooldown;
}

void Player::ApplyStats(int vigor, int endurance, int might) {
    float oldMax = flameMax;
    flameMax = MaxFlame(vigor, tune_.flameBase, tune_.flamePerVigor);
    stamina.max = MaxStamina(endurance, tune_.staminaBase, tune_.staminaPerEnd);
    stamina.value = std::min(stamina.value, stamina.max);
    if (oldMax > 0 && flame > 0) flame = std::min(flameMax, flame + (flameMax - oldMax));
    might_ = might;
}

bool Player::Equip(const WeaponDef& w) {
    weapon_ = w;
    const json& anims = ctx_->data.Get(w.anims);
    std::string cur = vis_.Valid() ? vis_.Current() : "";
    CharacterVisual fresh;
    if (!fresh.Init(ctx_->assets, anims)) return false;
    vis_ = std::move(fresh);
    for (const auto& h : w.hide) vis_.HideMesh(h, true);
    for (const auto& s : w.show) vis_.HideMesh(s, false);
    if (!w.attachModel.empty()) vis_.Attach(ctx_->assets, w.attachModel, w.attachBone, MatrixIdentity(), "weapon");
    handBone_ = vis_.Bone(w.hitBone);
    const json& lr = ctx_->data.Get("render.json");
    lanternBone_ = vis_.Bone(lr.contains("lantern") ? lr["lantern"].value("bone", "hips") : "hips");
    if (lanternBone_ < 0) lanternBone_ = vis_.Bone("hips");
    trail_.SetColor(w.trailColor, w.trailIntensity);
    vis_.Play("idle", 0.0f, true);
    (void)cur;
    return true;
}

Matrix Player::WorldMatrix(Vector3 p, float yaw) const {
    return MatrixMultiply(MatrixMultiply(MatrixScale(kModelScale, kModelScale, kModelScale), MatrixRotateY(yaw)),
                          MatrixTranslate(p.x, p.y, p.z));
}

void Player::Teleport(Vector3 p, float yawDeg) {
    pos_ = prevPos_ = p;
    yaw_ = prevYaw_ = yawDeg * DEG2RAD;
    vel_ = {0, 0, 0};
    if (body_) body_->SetPosition(p);
    swing_.Stop();
    trail_.Clear();
    UpdateLantern();
}

void Player::Revive(Vector3 p, float yawDeg) {
    Teleport(p, yawDeg);
    flame = flameMax;
    stamina.value = stamina.max;
    stamina.exhausted = false;
    Enter(PState::Move);
    vis_.Play("idle", 0.0f, true);
}

void Player::Kill(FxQueue& fx) {
    if (state_ == PState::Dead) return;
    flame = 0;
    swing_.Stop();
    Enter(PState::Dead);
    vis_.Play("death", 0.1f, true);
    fx.push_back({FxType::Death, pos_, 1.0f, ""});
}

Vector3 Player::Facing() const { return Vector3{std::sin(yaw_), 0, std::cos(yaw_)}; }

Vector3 Player::RenderPos(float alpha) const { return Vector3Lerp(prevPos_, pos_, alpha); }

bool Player::Invulnerable() const {
    if (invuln_ > 0) return true;
    if (state_ == PState::Roll) return tune_.roll.Invulnerable(stateTime_);
    return false;
}

geom::Capsule Player::Hurtbox() const {
    return geom::Capsule{G(Vector3Add(pos_, Vector3{0, 0.3f, 0})), G(Vector3Add(pos_, Vector3{0, 0.95f, 0})), 0.32f};
}

void Player::WeaponPoints(Vector3& base, Vector3& tip) const {
    Matrix bw = vis_.BoneWorld(handBone_, WorldMatrix(pos_, yaw_));
    base = Vector3Transform(weapon_.hitBase, bw);
    tip = Vector3Transform(weapon_.hitTip, bw);
}

float Player::StrikeDamage() const {
    float base = attackKind_ == AttackKind::Light ? weapon_.dmgLight : weapon_.dmgHeavy;
    float chargeFrac = attackKind_ == AttackKind::Heavy ? std::clamp(charge_ / tune_.chargeMax, 0.0f, 1.0f) : 0.0f;
    return PlayerDamage(base, might_, tune_.damagePerMight, chargeFrac, tune_.chargeBonus, 1.0f);
}

float Player::StrikePosture() const {
    float base = attackKind_ == AttackKind::Light ? weapon_.postureLight : weapon_.postureHeavy;
    float chargeFrac = attackKind_ == AttackKind::Heavy ? std::clamp(charge_ / tune_.chargeMax, 0.0f, 1.0f) : 0.0f;
    return base * (1.0f + chargeFrac * tune_.chargeBonus);
}

void Player::Enter(PState s) {
    state_ = s;
    stateTime_ = 0;
    if (s != PState::Attack) {
        swing_.Stop();
        charging_ = false;
        vis_.Anim().SetSpeedScale(1.0f);
    }
    if (s != PState::Move) {
        blocking_ = false;
        sprinting_ = false;
    }
}

void Player::Buffer(Buffered b, float now) {
    buffered_ = b;
    bufferedAt_ = now;
}

Player::Buffered Player::TakeBuffer(float now) {
    if (buffered_ == Buffered::None) return Buffered::None;
    if (now - bufferedAt_ > tune_.inputBuffer) {
        buffered_ = Buffered::None;
        return Buffered::None;
    }
    Buffered b = buffered_;
    buffered_ = Buffered::None;
    return b;
}

Vector3 Player::WishDir(const InputFrame& in, const PlayerEnv& env) const {
    return Vector3Add(Vector3Scale(env.camForward, in.move.y), Vector3Scale(env.camRight, in.move.x));
}

void Player::TurnToward(Vector3 dir, float dt, float speedMult) {
    if (dir.x * dir.x + dir.z * dir.z < 1e-5f) return;
    float want = std::atan2(dir.x, dir.z);
    float d = WrapAngle(want - yaw_);
    float step = tune_.turnSpeed * DEG2RAD * speedMult * dt;
    yaw_ = WrapAngle(yaw_ + std::clamp(d, -step, step));
}

void Player::StartAttack(AttackKind k, int combo, const PlayerEnv& env, FxQueue& fx) {
    attackKind_ = k;
    combo_ = combo;
    charge_ = 0;
    hitFramePassed_ = false;
    std::string action = k == AttackKind::Heavy ? "attack_heavy" : (k == AttackKind::Riposte ? "riposte" : "attack_light_" + std::to_string(combo + 1));
    Enter(PState::Attack);
    vis_.Play(action, 0.08f, true);
    charging_ = k == AttackKind::Heavy;
    heavyHeld_ = k == AttackKind::Heavy;
    lungeLeft_ = k == AttackKind::Heavy ? tune_.lungeHeavy : (k == AttackKind::Riposte ? tune_.lungeRiposte : tune_.lungeLight);
    // Snap toward the lock target (or keep facing) at the start of the swing.
    if (env.lockTarget) {
        Vector3 to = Vector3Subtract(*env.lockTarget, pos_);
        yaw_ = std::atan2(to.x, to.z);
    }
    swing_.Stop();
    (void)fx;
}

void Player::StartRoll(Vector3 dir, FxQueue& fx) {
    if (!stamina.TryUse(tune_.costRoll)) return;
    Enter(PState::Roll);
    if (dir.x * dir.x + dir.z * dir.z < 1e-4f) dir = Facing();
    rollDir_ = Vector3Normalize(Vector3{dir.x, 0, dir.z});
    yaw_ = std::atan2(rollDir_.x, rollDir_.z);
    vis_.Play("roll", 0.05f, true);
    fx.push_back({FxType::Roll, pos_, 1.0f, ""});
}

void Player::StartDrink(FxQueue& fx) {
    if (flasks <= 0) {
        fx.push_back({FxType::Sound, pos_, 1.0f, "gate_locked"});
        return;
    }
    flasks--;
    healed_ = false;
    Enter(PState::Drink);
    vis_.Play("drink", 0.1f, true);
    fx.push_back({FxType::Sound, pos_, 1.0f, "drink"});
}

void Player::StartInteract(float duration) {
    interactDuration_ = duration;
    interactFinished_ = false;
    Enter(PState::Interact);
    vis_.Play("interact", 0.1f, true);
}

void Player::StartRest() {
    Enter(PState::Rest);
    vis_.Play("rest", 0.15f, true);
}

void Player::EndRest() {
    if (state_ != PState::Rest) return;
    Enter(PState::Interact);
    interactDuration_ = 0.9f;
    vis_.Play("rest_up", 0.15f, true);
}

IncomingResult Player::ReceiveHit(const IncomingHit& h, bool godMode, FxQueue& fx) {
    IncomingResult r;
    if (state_ == PState::Dead) return r;
    DefenseState d;
    d.invulnerable = Invulnerable() || state_ == PState::Rest;
    d.blocking = blocking_ && weapon_.shield && h.blockable;
    Vector3 to = Vector3Subtract(h.from, pos_);
    d.facingAttacker = geom::WithinArc(G(Facing()), G(to), tune_.blockArc * 0.5f);
    d.parryActive = parry_.Active() && (state_ == PState::Move || state_ == PState::Parry);
    d.canParry = h.parryable;
    HitResult hr = ResolveHitOnPlayer(h.damage, d, tune_.block);
    if (hr.ignored) {
        r.ignored = true;
        return r;
    }
    r.landed = true;
    if (hr.parried) {
        r.parried = true;
        Enter(PState::Parry);
        vis_.Play("parry", 0.04f, true);
        fx.push_back({FxType::Parry, Vector3Add(pos_, Vector3Add(Vector3Scale(Facing(), 0.45f), Vector3{0, 0.75f, 0})), 1.0f, ""});
        return r;
    }
    float dmg = godMode ? 0.0f : hr.damage;
    if (hr.blocked) {
        r.blocked = true;
        stamina.Drain(hr.stamina + h.posture);
        flame = std::max(0.0f, flame - dmg);
        Vector3 spark = Vector3Add(pos_, Vector3Add(Vector3Scale(Facing(), 0.4f), Vector3{0, 0.7f, 0}));
        fx.push_back({FxType::Blocked, spark, 1.0f, ""});
        if (stamina.value <= 0.0f) {
            r.guardBroken = true;
            Enter(PState::GuardBreak);
            stunTime_ = tune_.guardBreak;
            vis_.Play("stagger", 0.05f, true);
        } else {
            Enter(PState::BlockHit);
            stunTime_ = 0.28f;
            vis_.Play("block_hit", 0.04f, true);
        }
    } else {
        flame = std::max(0.0f, flame - dmg);
        hitFlash_ = 1.0f;
        fx.push_back({FxType::PlayerHurt, Vector3Add(pos_, Vector3{0, 0.7f, 0}), dmg, ""});
        if (flame > 0) {
            Enter(PState::Hit);
            stunTime_ = tune_.hitStun;
            vis_.Play("hit", 0.04f, true);
            invuln_ = tune_.invulnAfterHit;
        }
    }
    if (flame <= 0 && !godMode) {
        r.killed = true;
        Kill(fx);
    }
    return r;
}

void Player::UpdateLantern() {
    Matrix w = WorldMatrix(pos_, yaw_);
    Matrix hip = vis_.BoneWorld(lanternBone_, w);
    Vector3 off = Vector3Transform(lanternOffset_, MatrixRotateY(yaw_));
    lanternPos_ = Vector3Add(Vector3{hip.m12, hip.m13, hip.m14}, off);
}

void Player::Step(const InputFrame& in, const PlayerEnv& env, float dt, FxQueue& fx) {
    prevPos_ = pos_;
    prevYaw_ = yaw_;
    now_ = env.time;
    stateTime_ += dt;
    invuln_ = std::max(0.0f, invuln_ - dt);
    hitFlash_ = std::max(0.0f, hitFlash_ - dt * 6.0f);
    parry_.Update(dt);
    interactPressed_ = false;

    // ---------------------------------------------------------------- input edges -> buffer
    if (state_ != PState::Dead && state_ != PState::Rest) {
        if (in.Pressed(Act::Attack)) Buffer(Buffered::Attack, now_);
        if (in.Pressed(Act::Heavy)) Buffer(Buffered::Heavy, now_);
        if (in.Pressed(Act::Flask)) Buffer(Buffered::Flask, now_);
        if (in.Pressed(Act::Interact)) interactPressed_ = true;
        if (in.Pressed(Act::Roll)) {
            rollHeldActive_ = true;
            rollHeld_ = 0;
        }
        if (rollHeldActive_) {
            if (in.Held(Act::Roll)) rollHeld_ += dt;
            if (in.Released(Act::Roll) || !in.Held(Act::Roll)) {
                if (rollHeld_ <= tune_.rollTapMax) Buffer(Buffered::Roll, now_);
                rollHeldActive_ = false;
                rollHeld_ = 0;
            }
        }
    }
    heavyHeld_ = in.Held(Act::Heavy);

    Vector3 wish = WishDir(in, env);
    float mag = std::min(1.0f, Vector3Length(wish));
    Vector3 wishN = mag > 0.01f ? Vector3Normalize(wish) : Vector3{0, 0, 0};
    Vector3 desiredVel{0, 0, 0};
    bool regenBlocked = false;

    switch (state_) {
        case PState::Move: {
            bool canAct = stamina.CanAct();
            bool wantBlock = in.Held(Act::Block);
            if (in.Pressed(Act::Block) && canAct) {
                if (stamina.TryUse(tune_.costParry)) parry_.Press();
                if (!weapon_.shield) {
                    // No shield: a block press is a parry attempt (short deflect animation).
                    Enter(PState::Parry);
                    vis_.Play("parry", 0.05f, true);
                    break;
                }
            }
            blocking_ = wantBlock && weapon_.shield;
            sprinting_ = rollHeldActive_ && rollHeld_ > tune_.rollTapMax && mag > 0.3f && canAct && !blocking_;
            // Act on buffered input.
            Buffered b = TakeBuffer(now_);
            if (b == Buffered::Roll && canAct) {
                StartRoll(mag > 0.1f ? wishN : Facing(), fx);
                break;
            }
            if ((b == Buffered::Attack || b == Buffered::Heavy) && canAct) {
                bool heavy = b == Buffered::Heavy;
                float cost = heavy ? weapon_.staminaHeavy : weapon_.staminaLight;
                if (stamina.TryUse(cost)) {
                    if (mag > 0.1f && !env.lockTarget) yaw_ = std::atan2(wishN.x, wishN.z);
                    StartAttack(heavy ? AttackKind::Heavy : (env.riposteReady ? AttackKind::Riposte : AttackKind::Light), 0, env, fx);
                    break;
                }
            }
            if (b == Buffered::Flask) {
                StartDrink(fx);
                break;
            }
            float speed = 0;
            if (mag > 0.05f) {
                if (!canAct || stamina.exhausted) speed = tune_.exhaustedSpeed * std::min(1.0f, mag / tune_.walkThreshold);
                else if (blocking_) speed = tune_.blockWalk * mag;
                else if (sprinting_) speed = tune_.sprintSpeed;
                else if (mag < tune_.walkThreshold) speed = tune_.walkSpeed * (mag / tune_.walkThreshold);
                else speed = tune_.runSpeed;
            }
            if (sprinting_) stamina.Drain(tune_.costSprint * dt);
            if (blocking_) regenBlocked = true;
            desiredVel = Vector3Scale(wishN, speed);
            if (env.lockTarget && !sprinting_) {
                TurnToward(Vector3Subtract(*env.lockTarget, pos_), dt);
            } else if (mag > 0.05f) {
                TurnToward(wishN, dt);
            }
            std::string anim = "idle";
            if (blocking_) anim = "block";
            else if (speed > tune_.runSpeed + 0.2f) anim = "sprint";
            else if (speed > tune_.walkSpeed + 0.3f) anim = "run";
            else if (speed > 0.05f) anim = "walk";
            vis_.Play(anim, 0.15f);
            break;
        }
        case PState::Roll: {
            float t = stateTime_ / tune_.roll.duration;
            float k = 1.0f - std::max(0.0f, (t - 0.6f) / 0.4f) * 0.7f;  // ease out at the end
            desiredVel = Vector3Scale(rollDir_, tune_.rollDistance / tune_.roll.duration * k);
            if (stateTime_ >= tune_.rollRecoveryCancel) {
                // Roll recovery can be cancelled into an attack or another roll.
                Buffered b = buffered_;
                if (b == Buffered::Attack || b == Buffered::Roll || b == Buffered::Heavy) {
                    Enter(PState::Move);
                    break;
                }
            }
            if (stateTime_ >= tune_.roll.duration) Enter(PState::Move);
            break;
        }
        case PState::Attack: {
            const ActionInfo* ai = vis_.Info(vis_.Current());
            float t = vis_.Anim().Time();
            if (!ai) {
                Enter(PState::Move);
                break;
            }
            // Heavy charge: hold the clip at charge_hold while the button is held.
            if (attackKind_ == AttackKind::Heavy && charging_) {
                if (ai->chargeHold >= 0 && t >= ai->chargeHold) {
                    if (heavyHeld_ && charge_ < tune_.chargeMax) {
                        vis_.Anim().SetSpeedScale(0.0f);
                        charge_ += dt;
                        if (int(charge_ * 10) % 3 == 0 && charge_ > tune_.chargeMin)
                            fx.push_back({FxType::Embers, Vector3Add(pos_, Vector3{0, 1.0f, 0}), 0.3f, ""});
                    } else {
                        charging_ = false;
                        vis_.Anim().SetSpeedScale(1.0f);
                    }
                }
            }
            bool beforeHit = t < ai->hit;
            if (beforeHit) {
                if (env.lockTarget) TurnToward(Vector3Subtract(*env.lockTarget, pos_), dt, 0.6f);
                else if (mag > 0.1f) TurnToward(wishN, dt, 0.35f);
                if (lungeLeft_ > 0 && !charging_) {
                    float remain = std::max(0.08f, ai->Real(ai->hit - t));
                    float sp = lungeLeft_ / remain;
                    if (env.lockTarget && Vector2Distance({env.lockTarget->x, env.lockTarget->z}, {pos_.x, pos_.z}) < 0.85f) sp = 0;
                    desiredVel = Vector3Scale(Facing(), sp);
                    lungeLeft_ -= sp * dt;
                }
            } else {
                hitFramePassed_ = true;
            }
            for (const auto& e : vis_.Anim().Events()) {
                if (e.name == "hit") {
                    swing_.Start();
                    trail_.Emit(true);
                    fx.push_back({attackKind_ == AttackKind::Light ? FxType::Swing : FxType::SwingHeavy, pos_, 1.0f, ""});
                } else if (e.name == "hit_end") {
                    swing_.Stop();
                    trail_.Emit(false);
                }
            }
            bool inWindow = ai->cancelOpen >= 0 && t >= ai->cancelOpen && (ai->cancelClose < 0 || t <= ai->cancelClose);
            if (inWindow) {
                Buffered b = TakeBuffer(now_);
                if (b == Buffered::Attack && attackKind_ == AttackKind::Light && combo_ < 2 && stamina.TryUse(weapon_.staminaLight)) {
                    if (mag > 0.1f && !env.lockTarget) yaw_ = std::atan2(wishN.x, wishN.z);
                    trail_.Emit(false);
                    StartAttack(AttackKind::Light, combo_ + 1, env, fx);
                    break;
                }
                if ((b == Buffered::Attack || b == Buffered::Heavy) && stamina.TryUse(b == Buffered::Heavy ? weapon_.staminaHeavy : weapon_.staminaLight)) {
                    trail_.Emit(false);
                    StartAttack(b == Buffered::Heavy ? AttackKind::Heavy : AttackKind::Light, 0, env, fx);
                    break;
                }
                if (b == Buffered::Roll && hitFramePassed_ && stamina.CanAct()) {
                    trail_.Emit(false);
                    StartRoll(mag > 0.1f ? wishN : Facing(), fx);
                    break;
                }
            }
            bool done = vis_.Anim().Finished() || (ai->cancelClose >= 0 && t > ai->cancelClose);
            if (done) {
                trail_.Emit(false);
                Enter(PState::Move);
            }
            regenBlocked = true;
            break;
        }
        case PState::Parry:
            if (stateTime_ > 0.38f) Enter(PState::Move);
            break;
        case PState::BlockHit:
        case PState::Hit:
        case PState::GuardBreak:
            stunTime_ -= dt;
            if (stunTime_ <= 0) Enter(PState::Move);
            break;
        case PState::Drink: {
            if (!healed_ && stateTime_ >= tune_.healAt) {
                healed_ = true;
                flame = std::min(flameMax, flame + tune_.flaskHeal);
                fx.push_back({FxType::Embers, lanternPos_, 1.0f, ""});
            }
            desiredVel = Vector3Scale(wishN, tune_.drinkWalk * mag);
            if (mag > 0.05f) TurnToward(wishN, dt, 0.5f);
            if (stateTime_ >= tune_.drinkTime) Enter(PState::Move);
            break;
        }
        case PState::Interact:
            if (stateTime_ >= interactDuration_) {
                interactFinished_ = true;
                Enter(PState::Move);
                interactFinished_ = true;
            }
            break;
        case PState::Rest:
            if (vis_.Current() == "rest" && vis_.Anim().Finished()) vis_.Play("rest_idle", 0.2f);
            break;
        case PState::Dead:
            break;
    }
    if (state_ == PState::Move && stateTime_ > 0.05f) interactFinished_ = false;

    if (!regenBlocked) stamina.Update(dt);
    else {
        stamina.sinceUse = std::min(stamina.sinceUse, stamina.regenDelay * 0.5f);
        stamina.Update(dt * 0.5f);
    }

    // Smooth horizontal velocity toward the desired one (snappy).
    if (state_ == PState::Roll || state_ == PState::Attack) vel_ = desiredVel;
    else vel_ = Vector3Lerp(vel_, desiredVel, std::min(1.0f, tune_.accel * dt));
    if (body_ && state_ != PState::Dead) {
        body_->Move(vel_, dt, tune_.gravity);
        pos_ = body_->Position();
    } else if (body_) {
        body_->Move(Vector3{0, 0, 0}, dt, tune_.gravity);
        pos_ = body_->Position();
    }

    vis_.Update(dt);
    for (const auto& e : vis_.Anim().Events())
        if (e.name == "footstep") fx.push_back({FxType::Footstep, pos_, sprinting_ ? 1.2f : 1.0f, ""});
    UpdateLantern();
    Vector3 b, t;
    WeaponPoints(b, t);
    trail_.Push(b, t, now_);
}

void Player::Draw(Renderer& r, float alpha) const {
    Vector3 p = Vector3Lerp(prevPos_, pos_, alpha);
    float yaw = prevYaw_ + WrapAngle(yaw_ - prevYaw_) * alpha;
    DrawParams dp;
    dp.hitFlash = hitFlash_ * 0.6f;
    if (Invulnerable() && state_ == PState::Roll) dp.emissive = Vector3{0.02f, 0.02f, 0.03f};
    vis_.Draw(r, WorldMatrix(p, yaw), dp);
}

void Player::DrawTrail(float now, Shader shader, int loc) const { trail_.Draw(now, shader, loc); }

}  // namespace lk
