#include "core/combat_rules.h"

#include <algorithm>
#include <cmath>

namespace lk {

bool Stamina::TryUse(float cost) {
    if (!CanAct()) return false;
    value = std::max(0.0f, value - cost);
    sinceUse = 0.0f;
    if (value <= 0.0f) exhausted = true;
    return true;
}

void Stamina::Drain(float amount) {
    value = std::max(0.0f, value - amount);
    sinceUse = 0.0f;
    if (value <= 0.0f) exhausted = true;
}

void Stamina::Update(float dt) {
    sinceUse += dt;
    if (sinceUse >= regenDelay) value = std::min(max, value + regenPerSec * dt);
    // Exhaustion clears once a usable chunk has come back.
    if (exhausted && value >= std::min(max, 20.0f)) exhausted = false;
}

void ParryTiming::Press() {
    if (sinceLastWindow < cooldown) return;
    sincePress = 0.0f;
    sinceLastWindow = 0.0f;
}

void ParryTiming::Update(float dt) {
    sincePress += dt;
    sinceLastWindow += dt;
}

bool Posture::Add(float amount) {
    if (Staggered()) return false;
    value += amount;
    sinceHit = 0.0f;
    if (value >= max) {
        value = max;
        staggerLeft = staggerDuration;
        return true;
    }
    return false;
}

void Posture::Update(float dt) {
    sinceHit += dt;
    if (staggerLeft > 0.0f) {
        staggerLeft -= dt;
        if (staggerLeft <= 0.0f) {
            staggerLeft = 0.0f;
            value = 0.0f;
        }
        return;
    }
    if (sinceHit >= decayDelay) value = std::max(0.0f, value - decayPerSec * dt);
}

HitResult ResolveHitOnPlayer(float raw, const DefenseState& d, const BlockTuning& b) {
    HitResult r;
    if (d.invulnerable) {
        r.ignored = true;
        return r;
    }
    if (d.parryActive && d.facingAttacker && d.canParry) {
        r.parried = true;
        return r;
    }
    if (d.blocking && d.facingAttacker) {
        r.blocked = true;
        r.damage = raw * b.damageMult;
        r.stamina = raw * b.staminaPerDamage;
        return r;
    }
    r.damage = raw;
    return r;
}

float PlayerDamage(float base, int might, float perMight, float chargeFrac, float chargeBonus, float critMult) {
    float c = std::clamp(chargeFrac, 0.0f, 1.0f);
    return base * (1.0f + float(might) * perMight) * (1.0f + c * chargeBonus) * critMult;
}

int LevelUpCost(int level, float base, float growth) {
    return int(std::lround(base * std::pow(growth, float(level))));
}

float MaxFlame(int vigor, float base, float perVigor) { return base + float(vigor) * perVigor; }
float MaxStamina(int endurance, float base, float perEndurance) { return base + float(endurance) * perEndurance; }

int OnPlayerDeath(EmberState& s, float x, float y, float z) {
    int lost = s.pileActive ? s.pileAmount : 0;
    s.pileActive = s.carried > 0;
    s.pileAmount = s.carried;
    s.pileX = x;
    s.pileY = y;
    s.pileZ = z;
    s.carried = 0;
    return lost;
}

int RecoverPile(EmberState& s) {
    if (!s.pileActive) return 0;
    int n = s.pileAmount;
    s.carried += n;
    s.pileActive = false;
    s.pileAmount = 0;
    return n;
}

}  // namespace lk
