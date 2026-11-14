#pragma once
// Shared combat types: hit/hurt capsules, attack descriptions and the FX event queue that
// gameplay code pushes into and the Session turns into hitstop, shake, particles and sound.
#include <set>
#include <string>
#include <vector>

#include "core/geom.h"
#include "raylib.h"
#include "raymath.h"

namespace lk {

inline geom::V3 G(Vector3 v) { return geom::V3{v.x, v.y, v.z}; }
inline Vector3 R(geom::V3 v) { return Vector3{v.x, v.y, v.z}; }

// A weapon hitbox active for one swing: capsule between two points on the weapon, swept
// from the previous step so fast swings do not tunnel. Each swing hits each target once.
struct SwingTracker {
    bool active = false;
    bool havePrev = false;
    Vector3 prevBase{}, prevTip{};
    std::set<int> hitIds;
    void Start() {
        active = true;
        havePrev = false;
        hitIds.clear();
    }
    void Stop() { active = false; }
    // Capsules to test this step (current blade, and the sweep from the previous tip).
    void Capsules(Vector3 base, Vector3 tip, float radius, std::vector<geom::Capsule>& out);
};

enum class FxType {
    HitFlesh,      // player hit an enemy
    HitCrit,       // critical (riposte / posture break)
    Blocked,       // shield block sparks
    Parry,         // blue-white flash
    PlayerHurt,
    EnemyBlocked,  // enemy shield blocked the player
    PostureBreak,
    Glint,         // weapon glint on telegraph
    Swing,
    SwingHeavy,
    Footstep,
    Rattle,
    Death,
    EnemyDeath,
    Roll,
    Embers,
    Sound,         // generic named sound at pos
    Shake,
};

struct FxEvent {
    FxType type;
    Vector3 pos{};
    float strength = 1.0f;
    std::string name;   // sound name for FxType::Sound
};

using FxQueue = std::vector<FxEvent>;

}  // namespace lk
