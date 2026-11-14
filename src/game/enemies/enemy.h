#pragma once
// Skeleton enemies. One data-driven class: the "brain" (training, melee, rogue, mage, boss)
// selects behaviour; everything tunable comes from data/enemies/*.json.
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/combat_rules.h"
#include "engine/data_store.h"
#include "game/character_visual.h"
#include "game/combat.h"

namespace lk {

struct Ctx;
class Character;
class Physics;
class Nav;

struct EnemyMove {
    std::string name, anim, combo;
    float rangeMin = 0, rangeMax = 2;
    float damage = 10, posturePlayer = 0;
    float holdMin = 0.4f, holdMax = 0.4f;
    float lunge = 0.5f;
    float weight = 1;
    float comboChance = 0;
    bool projectile = false;
    bool aoe = false;          // boss shockwave: hits around the impact point
    float aoeRadius = 0;
    std::string fx;            // extra fx tag
};

struct EnemyDef {
    std::string key, name, model, brain;
    json anims;
    float hp = 50, posture = 50, postureDecay = 10, postureDelay = 1.6f, staggerTime = 1.5f;
    int embers = 20;
    float scale = 1, radius = 0.32f, height = 1.15f;
    float turnSpeed = 300, walkSpeed = 1.1f, runSpeed = 2.4f, circleSpeed = 1.6f;
    float aggroRange = 9, loseRange = 18;
    float strafeMin = 2.2f, strafeMax = 3.6f;
    float cooldownMin = 1.2f, cooldownMax = 2.2f;
    float swingInterval = 3.0f;
    bool flinch = true;
    bool hasShield = false, shieldBlocksLight = false;
    float shieldArc = 120, shieldPostureMult = 0.6f;
    std::string weaponModel, weaponBone, offhandModel, offhandBone;
    Vector3 hitBase{0, 0.1f, 0}, hitTip{0, 1.1f, 0};
    float hitRadius = 0.16f;
    std::vector<EnemyMove> moves;
    Vector3 eyeColor{3.2f, 0.9f, 0.25f};
    Vector3 glintColor{4, 3.2f, 2.2f};
    Color tint = WHITE;
    // mage
    float teleportRange = 3, teleportMin = 6, teleportMax = 9, teleportCooldown = 4;
    float boltSpeed = 7, boltDamage = 16, boltRadius = 0.22f, boltLife = 3, boltTurn = 25;
    Vector3 boltColor{0.6f, 1.2f, 4.0f};
    json raw;

    bool Load(const std::string& key, const json& j, const json& anims);
};

// At most N enemies attack at once.
class AttackTokens {
public:
    int max = 2;
    bool Request(int id);
    void Release(int id) { holders_.erase(id); }
    bool Holds(int id) const { return holders_.count(id) > 0; }
    int Count() const { return int(holders_.size()); }
    void Clear() { holders_.clear(); }

private:
    std::set<int> holders_;
};

struct ProjectileSpawn {
    Vector3 pos, dir;
    int owner;
    const EnemyDef* def;
};

struct EnemyEnv {
    Vector3 playerPos{};
    Vector3 playerFacing{0, 0, 1};
    bool playerAlive = true;
    bool playerAttacking = false;
    Physics* physics = nullptr;
    const Nav* nav = nullptr;
    AttackTokens* tokens = nullptr;
    std::vector<ProjectileSpawn>* spawns = nullptr;
    std::vector<Vector3> others;   // positions of other live enemies (separation)
    float time = 0;
};

struct PlayerStrike {
    float damage = 0;
    float posture = 0;
    bool heavy = false;
    Vector3 from{};
    Vector3 point{};
};

struct StrikeResult {
    bool landed = false;
    bool blocked = false;
    bool critical = false;
    bool killed = false;
    bool postureBroken = false;
    float damageDealt = 0;
};

enum class EState { Idle, Alert, Approach, Strafe, Attack, Recover, Hit, Stagger, Dead, Teleport, Dormant };
const char* EStateName(EState s);

class Enemy {
public:
    Enemy();
    ~Enemy();
    bool Init(Ctx& ctx, const EnemyDef& def, Vector3 pos, float yawDeg, int id, const std::string& spawnId);
    void Step(const EnemyEnv& env, float dt, FxQueue& fx);
    void Draw(class Renderer& r, float alpha, float time) const;
    void Respawn();

    // Player strike on this enemy (already confirmed to overlap the hurtbox).
    StrikeResult TakeStrike(const PlayerStrike& s, float critMult, FxQueue& fx);
    // Parry: the player deflected our attack.
    void OnParried(FxQueue& fx);

    geom::Capsule Hurtbox() const;
    bool AttackActive() const { return state_ == EState::Attack && swing_.active; }
    SwingTracker& Swing() { return swing_; }
    void WeaponPoints(Vector3& base, Vector3& tip) const;
    const EnemyMove* CurrentMove() const { return move_; }

    int Id() const { return id_; }
    const std::string& SpawnId() const { return spawnId_; }
    const EnemyDef& Def() const { return *def_; }
    bool Alive() const { return state_ != EState::Dead; }
    bool Lockable() const { return Alive() && state_ != EState::Dormant; }
    bool Staggered() const { return state_ == EState::Stagger; }
    EState State() const { return state_; }
    Vector3 Pos() const { return pos_; }
    Vector3 LockPoint() const;
    Vector3 Facing() const;
    float Hp() const { return hp_; }
    float HpMax() const { return def_->hp; }
    const Posture& GetPosture() const { return posture_; }
    float LastHurtTime() const { return lastHurt_; }
    bool Aggro() const { return aggro_; }
    void SetDormant(bool d);
    void SetTint(Color c) { tint_ = c; }
    void SetPos(Vector3 p);
    // Tests: the training brain swings on its next step.
    void TriggerAttackSoon() { swingTimer_ = 1e6f; }
    float SwingStartedAt() const { return swingStartedAt_; }
    float Scale() const { return def_->scale; }

private:
    void Think(const EnemyEnv& env, float dt, FxQueue& fx);
    void StartAttack(const EnemyMove& m, FxQueue& fx);
    const EnemyMove* PickMove(float dist) const;
    void FaceToward(Vector3 target, float dt, float speedMult = 1.0f);
    void MoveToward(Vector3 target, float speed, const EnemyEnv& env, float dt);
    void ChangeState(EState s);
    Matrix WorldMatrix(Vector3 p, float yaw) const;
    float Rand();

    Ctx* ctx_ = nullptr;
    const EnemyDef* def_ = nullptr;
    CharacterVisual vis_;
    std::unique_ptr<Character> body_;
    int id_ = 0;
    std::string spawnId_;
    Vector3 spawnPos_{};
    float spawnYaw_ = 0;
    Vector3 pos_{}, prevPos_{}, vel_{};
    float yaw_ = 0, prevYaw_ = 0;
    EState state_ = EState::Idle;
    float stateTime_ = 0;
    float hp_ = 1;
    Posture posture_;
    bool aggro_ = false;
    float cooldown_ = 1.0f;
    float strafeDir_ = 1.0f;
    float strafeSwitch_ = 2.0f;
    float lostSight_ = 0;
    const EnemyMove* move_ = nullptr;
    float hold_ = 0, holdLeft_ = 0;
    bool holding_ = false, holdDone_ = false, firedProjectile_ = false;
    float lungeLeft_ = 0;
    SwingTracker swing_;
    float hitFlash_ = 0;
    float lastHurt_ = -100;
    float now_ = 0;
    float teleportCd_ = 0;
    float swingTimer_ = 0;
    float swingStartedAt_ = -1;
    float glint_ = 0;
    Color tint_ = WHITE;
    unsigned rng_ = 1;
    int weaponBone_ = -1;
};

}  // namespace lk
