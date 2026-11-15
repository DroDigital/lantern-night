#include "game/character_visual.h"

#include "engine/assets.h"
#include "engine/log.h"

namespace lk {

bool CharacterVisual::Init(AssetCache& assets, const json& map, const std::string& modelOverride) {
    std::string model = modelOverride.empty() ? map.value("model", "") : modelOverride;
    asset_ = assets.LoadModel(model);
    if (!asset_) return false;
    anim_.Init(asset_);
    actions_.clear();
    if (!map.contains("actions")) {
        LK_ERROR("anim map for %s has no actions", model.c_str());
        return false;
    }
    for (auto& [name, a] : map["actions"].items()) {
        ActionInfo info;
        info.speed = a.value("speed", 1.0f);
        info.loop = a.value("loop", false);
        info.hit = a.value("hit", -1.0f);
        info.hitEnd = a.value("hit_end", -1.0f);
        info.cancelOpen = a.value("cancel_open", -1.0f);
        info.cancelClose = a.value("cancel_close", -1.0f);
        info.chargeHold = a.value("charge_hold", -1.0f);
        std::vector<AnimEvent> events;
        if (a.contains("events"))
            for (auto& e : a["events"]) events.push_back({e.value("t", 0.0f), e.value("e", "")});
        if (info.hit >= 0) events.push_back({info.hit, "hit"});
        if (info.hitEnd >= 0) events.push_back({info.hitEnd, "hit_end"});
        if (info.cancelOpen >= 0) events.push_back({info.cancelOpen, "cancel_open"});
        if (info.cancelClose >= 0) events.push_back({info.cancelClose, "cancel_close"});
        info.def = anim_.AddClip(a.value("clip", ""), info.speed, info.loop, std::move(events));
        if (info.def < 0) continue;
        info.duration = anim_.Duration(info.def);
        actions_[name] = info;
    }
    return true;
}

bool CharacterVisual::Play(const std::string& action, float fade, bool restart, float startClipTime) {
    auto it = actions_.find(action);
    if (it == actions_.end()) return false;
    anim_.Play(it->second.def, fade, restart, startClipTime);
    current_ = action;
    return true;
}

const ActionInfo* CharacterVisual::Info(const std::string& action) const {
    auto it = actions_.find(action);
    return it == actions_.end() ? nullptr : &it->second;
}

void CharacterVisual::HideMesh(const std::string& node, bool hidden) {
    if (!asset_) return;
    for (size_t i = 0; i < asset_->meshNames.size() && i < 64; ++i)
        if (asset_->meshNames[i] == node) {
            if (hidden) hidden_ |= 1ull << i;
            else hidden_ &= ~(1ull << i);
        }
}

void CharacterVisual::SetEmissiveMesh(const std::string& node, bool on) {
    if (!asset_) return;
    for (size_t i = 0; i < asset_->meshNames.size() && i < 64; ++i)
        if (asset_->meshNames[i] == node) {
            if (on) emissive_ |= 1ull << i;
            else emissive_ &= ~(1ull << i);
        }
}

int CharacterVisual::Bone(const std::string& name) const { return asset_ ? asset_->FindBone(name) : -1; }

int CharacterVisual::Attach(AssetCache& assets, const std::string& modelRel, const std::string& bone, Matrix local,
                            const std::string& name) {
    Attachment a;
    a.asset = assets.LoadModel(modelRel);
    a.bone = Bone(bone);
    a.local = local;
    a.name = name.empty() ? modelRel : name;
    if (!a.asset || a.bone < 0) {
        LK_ERROR("attach %s to bone %s failed", modelRel.c_str(), bone.c_str());
        return -1;
    }
    attachments_.push_back(a);
    return int(attachments_.size()) - 1;
}

Attachment* CharacterVisual::GetAttachment(const std::string& name) {
    for (auto& a : attachments_)
        if (a.name == name) return &a;
    return nullptr;
}

Matrix CharacterVisual::BoneWorld(const std::string& bone, const Matrix& world) const {
    return anim_.BoneWorld(Bone(bone), world);
}

float CharacterVisual::Height() const { return asset_ ? asset_->bounds.max.y - asset_->bounds.min.y : 1.0f; }

void CharacterVisual::Draw(Renderer& r, const Matrix& world, DrawParams p) const {
    if (!asset_) return;
    p.hiddenMask |= hidden_;
    p.emissiveMask |= emissive_;
    p.character = true;
    if (!p.hasProbe) {
        p.lightProbe = Vector3{world.m12, world.m13 + 0.6f, world.m14};
        p.hasProbe = true;
    }
    r.DrawModel(*asset_, world, &anim_, p);
    for (const auto& a : attachments_) {
        if (!a.visible) continue;
        Matrix m = MatrixMultiply(a.local, anim_.BoneWorld(a.bone, world));
        DrawParams ap = p;
        ap.hiddenMask = 0;
        ap.emissiveMask = 0;
        r.DrawModel(*a.asset, m, nullptr, ap);
    }
}

}  // namespace lk
