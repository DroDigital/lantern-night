#pragma once
// A skinned character: model, animator, the action->clip map from data/anims/<name>.json,
// hidden/emissive meshes by node name and items attached to bones.
#include <map>
#include <string>
#include <vector>

#include "engine/animator.h"
#include "engine/data_store.h"
#include "engine/renderer.h"

namespace lk {

class AssetCache;
struct ModelAsset;

struct ActionInfo {
    int def = -1;
    float speed = 1.0f;
    bool loop = false;
    // Clip-time markers (seconds of clip time). Negative = absent.
    float hit = -1, hitEnd = -1, cancelOpen = -1, cancelClose = -1, chargeHold = -1;
    float duration = 0.0f;   // clip seconds
    float RealDuration() const { return speed > 0 ? duration / speed : duration; }
    float Real(float clipT) const { return speed > 0 ? clipT / speed : clipT; }
};

struct Attachment {
    ModelAsset* asset = nullptr;
    int bone = -1;
    Matrix local = MatrixIdentity();
    bool visible = true;
    std::string name;
};

class CharacterVisual {
public:
    bool Init(AssetCache& assets, const json& animMap, const std::string& modelOverride = "");
    bool Valid() const { return asset_ != nullptr; }

    // Play an action by name; returns false if unknown.
    bool Play(const std::string& action, float fade = 0.1f, bool restart = false, float startClipTime = 0.0f);
    const ActionInfo* Info(const std::string& action) const;
    bool Has(const std::string& action) const { return actions_.count(action) > 0; }
    const std::string& Current() const { return current_; }
    void Update(float dt) { anim_.Update(dt); }

    void HideMesh(const std::string& node, bool hidden = true);
    void SetEmissiveMesh(const std::string& node, bool on = true);
    int Attach(AssetCache& assets, const std::string& modelRel, const std::string& bone, Matrix local = MatrixIdentity(),
               const std::string& name = "");
    Attachment* GetAttachment(const std::string& name);

    // world = model scale * rotation * translation
    void Draw(Renderer& r, const Matrix& world, DrawParams p) const;
    Matrix BoneWorld(const std::string& bone, const Matrix& world) const;
    Matrix BoneWorld(int bone, const Matrix& world) const { return anim_.BoneWorld(bone, world); }
    int Bone(const std::string& name) const;

    Animator& Anim() { return anim_; }
    const Animator& Anim() const { return anim_; }
    ModelAsset* Asset() const { return asset_; }
    float Height() const;   // model units

private:
    ModelAsset* asset_ = nullptr;
    Animator anim_;
    std::map<std::string, ActionInfo> actions_;
    std::string current_;
    uint64_t hidden_ = 0, emissive_ = 0;
    std::vector<Attachment> attachments_;
};

}  // namespace lk
