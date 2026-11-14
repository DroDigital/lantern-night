#pragma once
// A play session in one zone: world, player, enemies, camera, lights, effects and HUD.
// Runs on a fixed 60 Hz step; hitstop scales the simulation clock, never the frame rate.
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/input.h"
#include "engine/nav.h"
#include "engine/particles.h"
#include "engine/renderer.h"
#include "game/camera_rig.h"
#include "game/combat.h"
#include "game/context.h"
#include "game/enemies/enemy.h"
#include "game/lantern.h"
#include "game/lantern_mesh.h"
#include "game/player/player.h"
#include "game/ui/hud.h"
#include "game/world/world.h"

namespace lk {

struct ZoneLight {
    PointLight light;
    float flicker = 0.0f;
    float seed = 0.0f;
    float baseIntensity = 1.0f;
    Vector3 baseColor{1, 1, 1};
    std::string id;
    bool halo = false;
};

struct Projectile {
    Vector3 pos, vel;
    float life;
    int owner;
    const EnemyDef* def;
    bool alive = true;
};

class Session {
public:
    explicit Session(Ctx& ctx);
    ~Session();
    bool Load(const std::string& zoneFile);
    void ReloadTunables();

    // Real-time advance: runs as many fixed steps as the (hitstop-scaled) clock allows.
    int Advance(float realDt);
    void Step(const InputFrame& in, float dt);
    void Render(float alpha, int screenW, int screenH);

    // Hooks for tests, capture and the smoke test
    void SetFlameFraction(float f);
    float FlameFraction() const { return player_.FlameFrac(); }
    Vector3 PlayerPos() const { return player_.Pos(); }
    Player& GetPlayer() { return player_; }
    CameraRig& Camera() { return cam_; }
    World& GetWorld() { return world_; }
    std::vector<std::unique_ptr<Enemy>>& Enemies() { return enemies_; }
    int StepCount() const { return steps_; }
    float SimTime() const { return simTime_; }
    void SetShowDebug(bool on) { showDebug_ = on; }
    Enemy* LockTarget() const;
    void ForceLock(int enemyId) { lockId_ = enemyId; }
    int Kills() const { return kills_; }
    float Alpha() const { return alpha_; }

private:
    void BuildLights();
    void SpawnEnemies();
    const EnemyDef* Def(const std::string& kind);
    void GatherLights(std::vector<PointLight>& out, float t);
    void UpdateLockOn(const InputFrame& in, float dt);
    void ResolveCombat();
    void UpdateProjectiles(float dt);
    void ProcessFx();
    void Hitstop(float seconds);
    void RenderHud(int sw, int sh);
    void RenderDebug(int sw, int sh);

    Ctx& ctx_;
    World world_;
    Nav nav_;
    CameraRig cam_;
    LanternTuning lantern_;
    LanternState lanternState_;
    LanternMesh lanternMesh_;
    std::vector<ZoneLight> zoneLights_;
    Particles particles_;
    Shader trailShader_{};
    int trailIntensityLoc_ = -1;

    Player player_;
    std::map<std::string, std::unique_ptr<EnemyDef>> defs_;
    std::vector<std::unique_ptr<Enemy>> enemies_;
    std::vector<Projectile> projectiles_;
    std::vector<ProjectileSpawn> spawns_;
    AttackTokens tokens_;
    FxQueue fx_;
    json feel_;

    int lockId_ = -1;
    float lockLost_ = 0;
    double acc_ = 0;
    int steps_ = 0;
    float simTime_ = 0, realTime_ = 0;
    float hitstop_ = 0;
    float flash_ = 0, flashDecay_ = 4;
    Color flashColor_ = WHITE;
    float flameTrail_ = 1;
    float emberAcc_ = 0;
    int kills_ = 0;
    int embers_ = 0;
    float emberFlash_ = 0;
    bool showDebug_ = false;
    float drawDistance_ = 34.0f;
    Vector3 blobTune_{0.55f, 0.65f, 0};
    float alpha_ = 1.0f;
    std::map<int, float> barTimers_;
};

}  // namespace lk
