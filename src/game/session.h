#pragma once
// A play session in one zone: world, player, enemies, camera, lights, effects.
#include <memory>
#include <string>
#include <vector>

#include "engine/input.h"
#include "engine/renderer.h"
#include "game/camera_rig.h"
#include "game/character_visual.h"
#include "game/context.h"
#include "game/lantern.h"
#include "game/lantern_mesh.h"
#include "game/world/world.h"

namespace lk {

class Character;

struct ZoneLight {
    PointLight light;
    float flicker = 0.0f;
    float seed = 0.0f;
    float baseIntensity = 1.0f;
    Vector3 baseColor{1, 1, 1};
    std::string id;
};

class Session {
public:
    explicit Session(Ctx& ctx);
    ~Session();
    bool Load(const std::string& zoneFile);
    void Step(const InputFrame& in, float dt);
    void Render(float alpha, int screenW, int screenH);
    void ReloadTunables();

    // Test / capture hooks
    void SetFlameFraction(float f) { flameFraction_ = f; }
    float FlameFraction() const { return flameFraction_; }
    Vector3 PlayerPos() const;
    CameraRig& Camera() { return cam_; }
    World& GetWorld() { return world_; }

private:
    void BuildLights();
    void GatherLights(std::vector<PointLight>& out, float t);

    Ctx& ctx_;
    World world_;
    CameraRig cam_;
    LanternTuning lantern_;
    LanternState lanternState_;
    LanternMesh lanternMesh_;
    Vector3 lanternPos_{};
    std::vector<ZoneLight> zoneLights_;

    // Player (M0: movement only; replaced by game/player in M1)
    CharacterVisual knight_;
    std::unique_ptr<Character> body_;
    Vector3 pos_{}, prevPos_{};
    float yaw_ = 0.0f, prevYaw_ = 0.0f;
    float flameFraction_ = 1.0f;
    float time_ = 0.0f;
    float drawDistance_ = 34.0f;
    Vector3 blobTune_{0.55f, 0.65f, 0};
};

}  // namespace lk
