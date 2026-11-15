#pragma once
// The knight: movement, sprint, roll (i-frames), light 3-hit combo, charged heavy, block/parry,
// hit reactions, flask drinking, interact/rest and death. Tunables from data/player/stats.json
// and data/weapons/*.json; clip timings from data/anims/*.json.
#include <memory>
#include <string>

#include "core/combat_rules.h"
#include "engine/data_store.h"
#include "engine/input.h"
#include "engine/trails.h"
#include "game/character_visual.h"
#include "game/combat.h"

namespace lk {

struct Ctx;
class Character;
class Renderer;

struct PlayerTuning {
    float flameBase = 100, flamePerVigor = 8, staminaBase = 100, staminaPerEnd = 6, damagePerMight = 0.05f;
    float staminaRegen = 30, staminaDelay = 0.6f;
    float walkSpeed = 1.6f, runSpeed = 3.6f, sprintSpeed = 5.4f, exhaustedSpeed = 1.6f;
    float turnSpeed = 720, accel = 22, gravity = -20, radius = 0.3f, height = 1.2f, walkThreshold = 0.55f;
    float costLight = 12, costHeavy = 25, costRoll = 20, costSprint = 15, costParry = 10;
    RollTiming roll;
    float rollDistance = 3.6f, rollTapMax = 0.28f, rollRecoveryCancel = 0.4f;
    float comboWindow = 0.45f, inputBuffer = 0.15f;
    float chargeMax = 1.0f, chargeBonus = 0.6f, chargeMin = 0.18f;
    BlockTuning block;
    float blockArc = 110, blockWalk = 1.2f, parryWindow = 0.15f, parryCooldown = 0.25f;
    float riposteMult = 3.0f, critMult = 3.0f, hitStun = 0.45f, guardBreak = 1.0f;
    float lungeLight = 0.9f, lungeHeavy = 1.4f, lungeRiposte = 1.0f;
    float flaskHeal = 45, drinkTime = 1.2f, healAt = 0.56f, drinkWalk = 0.9f;
    int flaskStart = 3, flaskMaxShards = 5;
    float interactRange = 1.6f, invulnAfterHit = 0.25f;
    float lockRange = 14, lockLostTime = 2, lockMaxAngle = 75;
    float softAimRange = 2.6f, softAimAngle = 70;   // unlocked attacks turn toward a close enemy in front
    void Load(const json& j);
};

struct WeaponDef {
    std::string key, name, anims;
    std::vector<std::string> show, hide;
    std::string attachModel, attachBone;
    bool shield = true;
    float dmgLight = 16, dmgHeavy = 28, postureLight = 9, postureHeavy = 30, staminaLight = 12, staminaHeavy = 25;
    std::string hitBone = "handslot.r";
    Vector3 hitBase{0, 0.15f, 0}, hitTip{0, 1.45f, 0};
    float hitRadius = 0.16f;
    Color trailColor{255, 225, 180, 255};
    float trailIntensity = 1.6f;
    bool Load(const std::string& key, const json& j);
};

enum class PState { Move, Roll, Attack, Parry, BlockHit, Hit, GuardBreak, Drink, Interact, Rest, Dead };
const char* PStateName(PState s);

struct PlayerEnv {
    Vector3 camForward{0, 0, -1}, camRight{1, 0, 0};
    const Vector3* lockTarget = nullptr;
    const Vector3* softTarget = nullptr;   // nearest enemy in the soft-aim cone (no lock-on)
    bool godMode = false;
    bool riposteReady = false;   // a staggered enemy is in front and in reach
    float time = 0;
};

struct IncomingHit {
    float damage = 0;
    float posture = 0;       // extra stamina damage when blocked (shield bash, boss)
    Vector3 from{};
    bool parryable = true;
    bool blockable = true;
};

struct IncomingResult {
    bool landed = false, blocked = false, parried = false, ignored = false, guardBroken = false, killed = false;
};

class Player {
public:
    Player();
    ~Player();
    bool Init(Ctx& ctx, Vector3 pos, float yawDeg);
    void LoadTuning(const json& stats);
    bool Equip(const WeaponDef& w);
    void ApplyStats(int vigor, int endurance, int might);

    void Step(const InputFrame& in, const PlayerEnv& env, float dt, FxQueue& fx);
    void Draw(Renderer& r, float alpha) const;
    void DrawTrail(float now, Shader trailShader, int intensityLoc) const;

    IncomingResult ReceiveHit(const IncomingHit& h, bool godMode, FxQueue& fx);

    // Offensive state for the session's hit tests.
    bool AttackActive() const { return state_ == PState::Attack && swing_.active; }
    SwingTracker& Swing() { return swing_; }
    void WeaponPoints(Vector3& base, Vector3& tip) const;
    float WeaponRadius() const { return weapon_.hitRadius; }
    float StrikeDamage() const;
    float StrikePosture() const;
    bool StrikeHeavy() const { return attackKind_ == AttackKind::Heavy; }
    float CritMult() const { return tune_.critMult; }

    geom::Capsule Hurtbox() const;
    Vector3 Pos() const { return pos_; }
    Vector3 RenderPos(float alpha) const;
    float Yaw() const { return yaw_; }
    Vector3 Facing() const;
    PState State() const { return state_; }
    bool Alive() const { return state_ != PState::Dead; }
    bool Invulnerable() const;
    bool Blocking() const { return blocking_; }
    void Teleport(Vector3 p, float yawDeg);

    // Lantern
    Vector3 LanternPos() const { return lanternPos_; }

    // Flame (health), stamina, flasks
    float flame = 100, flameMax = 100;
    Stamina stamina;
    int flasks = 3, flaskMax = 3;
    float FlameFrac() const { return flameMax > 0 ? flame / flameMax : 0; }
    float StaminaFrac() const { return stamina.max > 0 ? stamina.value / stamina.max : 0; }
    int Might() const { return might_; }

    // Interact/rest are started by the session (it knows what is in range).
    bool WantsInteract() const { return interactPressed_; }
    void StartInteract(float duration);
    void StartRest();
    void EndRest();
    bool Resting() const { return state_ == PState::Rest; }
    bool InteractDone() const { return state_ == PState::Move && interactFinished_; }
    void Revive(Vector3 p, float yawDeg);
    void Kill(FxQueue& fx);

    const PlayerTuning& Tuning() const { return tune_; }
    const WeaponDef& Weapon() const { return weapon_; }
    int ComboIndex() const { return combo_; }
    float StateTime() const { return stateTime_; }
    bool Drinking() const { return state_ == PState::Drink; }

private:
    enum class AttackKind { Light, Heavy, Riposte };
    enum class Buffered { None, Attack, Heavy, Roll, Flask };
    void Buffer(Buffered b, float now);
    Buffered TakeBuffer(float now);
    void StartAttack(AttackKind k, int combo, const PlayerEnv& env, FxQueue& fx);
    void StartRoll(Vector3 dir, FxQueue& fx);
    void StartDrink(FxQueue& fx);
    void TurnToward(Vector3 dir, float dt, float speedMult = 1.0f);
    Vector3 WishDir(const InputFrame& in, const PlayerEnv& env) const;
    Matrix WorldMatrix(Vector3 p, float yaw) const;
    void UpdateLantern();
    void Enter(PState s);

    Ctx* ctx_ = nullptr;
    PlayerTuning tune_;
    WeaponDef weapon_;
    CharacterVisual vis_;
    std::unique_ptr<Character> body_;
    Vector3 pos_{}, prevPos_{}, vel_{};
    float yaw_ = 0, prevYaw_ = 0;
    PState state_ = PState::Move;
    float stateTime_ = 0;
    float now_ = 0;
    int might_ = 0;

    // Combat
    AttackKind attackKind_ = AttackKind::Light;
    int combo_ = 0;
    float lungeLeft_ = 0;
    bool charging_ = false;
    float charge_ = 0;
    bool heavyHeld_ = false;
    bool hitFramePassed_ = false;
    SwingTracker swing_;
    Trail trail_;
    ParryTiming parry_;
    bool blocking_ = false;
    float invuln_ = 0;
    Buffered buffered_ = Buffered::None;
    float bufferedAt_ = -10;
    float rollHeld_ = 0;
    bool rollHeldActive_ = false;
    bool sprinting_ = false;
    Vector3 rollDir_{0, 0, 1};
    float stunTime_ = 0;
    bool healed_ = false;
    bool interactPressed_ = false;
    bool interactFinished_ = false;
    float interactDuration_ = 1.0f;
    int handBone_ = -1, lanternBone_ = -1;
    Vector3 lanternOffset_{0.3f, 0.02f, 0.1f};
    Vector3 lanternPos_{};
    float hitFlash_ = 0;
};

}  // namespace lk
