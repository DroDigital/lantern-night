#pragma once
// A play session in one zone: world, player, enemies, camera, lights, effects and HUD.
// Runs on a fixed 60 Hz step; hitstop scales the simulation clock, never the frame rate.
#include <functional>
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
#include "game/ui/menus.h"
#include "game/world/interactables.h"
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

struct Shrine {
    std::string id, name;
    Vector3 pos{};
    float yaw = 0;
    int light = -1;   // index into zoneLights_
};

struct BossBar {
    bool active = false;
    std::string name;
    float hp = 1;
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
    bool LoadZone(const ZoneData& zone);
    // Editor hooks: render from another camera and draw an overlay inside the 3D pass.
    void SetCameraOverride(const Camera3D* cam) { camOverride_ = cam; }
    void SetOverlay(std::function<void(Renderer&)> f) { overlay_ = std::move(f); }
    void SetHudVisible(bool v) { hudVisible_ = v; }
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
    const Nav& GetNav() const { return nav_; }
    std::vector<std::unique_ptr<Enemy>>& Enemies() { return enemies_; }
    int StepCount() const { return steps_; }
    float SimTime() const { return simTime_; }
    void SetShowDebug(bool on) { showDebug_ = on; }
    Enemy* LockTarget() const;
    void ForceLock(int enemyId) { lockId_ = enemyId; }
    // Spawn an enemy at runtime (tests, boss summons). Returns nullptr on failure.
    Enemy* SpawnEnemy(const std::string& kind, Vector3 pos, float yawDeg, const std::string& id = "");
    const std::vector<Projectile>& Projectiles() const { return projectiles_; }
    const AttackTokens& Tokens() const { return tokens_; }
    int Kills() const { return kills_; }
    float Alpha() const { return alpha_; }

    // Progress (M2)
    bool SaveProgress();
    bool MenuOpen() const { return menu_ != Menu::None; }
    bool WantsQuit() const { return wantsQuit_; }
    bool PlayerDead() const { return !player_.Alive(); }
    int RestCount() const { return restCount_; }
    int DeathCount() const { return deathCount_; }
    const std::vector<Shrine>& Shrines() const { return shrines_; }
    std::vector<Brazier>& Braziers() { return braziers_; }
    std::vector<Gate>& Gates() { return gates_; }
    std::vector<Pickup>& Pickups() { return pickups_; }
    const BossBar& Boss() const { return bossBar_; }
    // Interact with whatever is in range right now (also used by the smoke-test bot).
    std::string InteractPromptTarget() const { return promptTarget_; }
    void OpenPause() { menu_ = Menu::Pause; ctx_.ui.ResetFocus(); }

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
    // session_world.cpp
    void BuildInteractables();
    void ApplyProgress();
    void UpdateInteract(const InputFrame& in, float dt);
    void UpdateDeath(float dt);
    void UpdateMenus(const InputFrame& in);
    void RestAtShrine(const Shrine& s);
    void RespawnWorld();
    Vector3 RespawnPoint(float* yawDeg) const;
    void DrawInteractables(Renderer& r);
    void RenderMenus();
    bool EquipWeapon(const std::string& key);
    // session_interactables.cpp
    void BuildWorldObjects();
    void ApplyWorldProgress();
    void SetGateOpen(Gate& g, bool open, bool instant);
    void LightBrazier(Brazier& b);
    void ShowToast(const std::string& text);
    void SnuffArenaBraziers(bool snuff);
    bool UpdateWorldInteract();
    void FinishWorldInteract();
    void UpdateWorldObjects(float dt);
    void UpdateBossGroups();
    void DrawWorldObjects(Renderer& r);
    void DrawWorldAdditive(Renderer& r);
    void EnterFogGate(Gate& g);
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
    int nextEnemyId_ = 1;
    float emberFlash_ = 0;
    // Shrines, death, menus
    enum class Menu { None, Shrine, Pause };
    Menu menu_ = Menu::None;
    ShrineMenuState shrineMenu_;
    std::vector<Shrine> shrines_;
    const Shrine* restingAt_ = nullptr;
    std::string promptText_, promptTarget_;
    float deathTimer_ = 0;
    int restCount_ = 0, deathCount_ = 0;
    bool wantsQuit_ = false;
    std::string pendingInteract_;
    float pileGlow_ = 0;
    json world_cfg_;
    std::vector<Brazier> braziers_;
    std::vector<Gate> gates_;
    std::vector<Pickup> pickups_;
    std::vector<FloorHint> hints_;
    std::vector<AreaTrigger> triggers_;
    std::string pendingBrazier_, pendingGate_;
    std::string toast_, areaBanner_, hintText_, currentArea_;
    float toastTime_ = 0, areaBannerTime_ = 0;
    BossBar bossBar_;
    float bossTrail_ = 1;
    bool showDebug_ = false;
    float drawDistance_ = 34.0f;
    Vector3 blobTune_{0.55f, 0.65f, 0};
    float alpha_ = 1.0f;
    std::map<int, float> barTimers_;
    const Camera3D* camOverride_ = nullptr;
    std::function<void(Renderer&)> overlay_;
    bool hudVisible_ = true;
};

}  // namespace lk
