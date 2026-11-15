#pragma once
// Pure combat and progression rules (no rendering, no physics). Unit-tested in tests/.
#include <cstdint>

namespace lk {

// ---------------------------------------------------------------- Stamina ----
struct Stamina {
    float value = 100.0f;
    float max = 100.0f;
    float regenPerSec = 30.0f;
    float regenDelay = 0.6f;   // pause after any use before regen starts
    float sinceUse = 999.0f;
    bool exhausted = false;    // hit 0: can only walk until it regenerates a bit

    // Actions need at least 1 stamina; they may drive stamina to 0 ("last action").
    bool CanAct() const { return value > 0.0f && !exhausted; }
    bool TryUse(float cost);           // returns false if no stamina (value <= 0)
    void Drain(float amount);          // continuous use (sprint): drains, no failure
    void Update(float dt);
};

// ------------------------------------------------------------------- Roll ----
struct RollTiming {
    float duration = 0.5f;
    float iframeStart = 0.08f;
    float iframeEnd = 0.33f;
    bool Invulnerable(float t) const { return t >= iframeStart && t < iframeEnd; }
};

// ------------------------------------------------------------------ Parry ----
// A parry happens when block was pressed within `window` seconds before a hit lands.
struct ParryTiming {
    float window = 0.15f;
    float cooldown = 0.25f;   // pressing again too soon does not re-open the window
    float sincePress = 999.0f;
    float sinceLastWindow = 999.0f;
    void Press();
    void Update(float dt);
    bool Active() const { return sincePress <= window; }
};

// ---------------------------------------------------------------- Posture ----
struct Posture {
    float value = 0.0f;
    float max = 100.0f;
    float decayPerSec = 12.0f;
    float decayDelay = 1.5f;
    float staggerDuration = 1.5f;
    float sinceHit = 999.0f;
    float staggerLeft = 0.0f;
    bool Staggered() const { return staggerLeft > 0.0f; }
    // Adds posture damage; returns true if this broke the posture (enters stagger).
    bool Add(float amount);
    void Update(float dt);
    void Reset() { value = 0; staggerLeft = 0; sinceHit = 999.0f; }
};

// --------------------------------------------------------------- Damage ------
struct HitResult {
    float damage = 0.0f;
    float stamina = 0.0f;      // stamina spent by the defender (blocking)
    bool blocked = false;
    bool parried = false;
    bool ignored = false;      // i-frames
};

struct DefenseState {
    bool invulnerable = false;
    bool blocking = false;
    bool facingAttacker = false;   // within the shield arc
    bool parryActive = false;
    bool canParry = true;
};

struct BlockTuning {
    float damageMult = 0.5f;
    float staminaPerDamage = 0.6f;
};

HitResult ResolveHitOnPlayer(float rawDamage, const DefenseState& d, const BlockTuning& b);

// Damage dealt by the player: base * (1 + might bonus) * (1 + charge bonus) * crit multiplier.
float PlayerDamage(float base, int might, float perMight, float chargeFrac, float chargeBonus, float critMult);

// ---------------------------------------------------------------- Leveling ---
// Cost to buy the next level when the current total level is `level` (0-based).
int LevelUpCost(int level, float base = 50.0f, float growth = 1.15f);
float MaxFlame(int vigor, float base = 100.0f, float perVigor = 8.0f);
float MaxStamina(int endurance, float base = 100.0f, float perEndurance = 6.0f);

// --------------------------------------------------------------- Embers ------
// Death drops all carried embers as a pile at the death spot. Dying again before
// touching it destroys the old pile (its embers are lost). Touching it returns them.
struct EmberState {
    int carried = 0;
    bool pileActive = false;
    int pileAmount = 0;
    float pileX = 0, pileY = 0, pileZ = 0;
};
// Returns the number of embers permanently lost by this death.
int OnPlayerDeath(EmberState& s, float x, float y, float z);
// Returns the number recovered (0 if no pile).
int RecoverPile(EmberState& s);

}  // namespace lk
