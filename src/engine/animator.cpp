#include "engine/animator.h"

#include <algorithm>
#include <cmath>

#include "engine/assets.h"
#include "engine/log.h"

namespace lk {

static constexpr float kBakeFps = 60.0f;  // raylib bakes glTF animations at 60 fps

Matrix TransformToMatrix(const Transform& t) {
    return MatrixMultiply(MatrixMultiply(MatrixScale(t.scale.x, t.scale.y, t.scale.z), QuaternionToMatrix(t.rotation)),
                          MatrixTranslate(t.translation.x, t.translation.y, t.translation.z));
}

void Animator::Init(ModelAsset* asset) {
    asset_ = asset;
    defs_.clear();
    cur_ = prev_ = Layer{};
    fade_ = fadeDur_ = 0.0f;
    if (!asset || asset->model.skeleton.boneCount == 0) {
        asset_ = nullptr;
        return;
    }
    const auto& sk = asset->model.skeleton;
    pose_.assign(sk.bindPose, sk.bindPose + sk.boneCount);
    tmp_ = pose_;
    poseMat_.resize(sk.boneCount);
    invBind_.resize(sk.boneCount);
    skin_.assign(sk.boneCount, MatrixIdentity());
    for (unsigned i = 0; i < sk.boneCount; ++i) {
        invBind_[i] = MatrixInvert(TransformToMatrix(sk.bindPose[i]));
        poseMat_[i] = TransformToMatrix(pose_[i]);
    }
}

int Animator::AddClip(const std::string& clipName, float speed, bool loop, std::vector<AnimEvent> events) {
    if (!asset_) return -1;
    int idx = asset_->FindClip(clipName);
    if (idx < 0) {
        LK_ERROR("animator: clip '%s' not found in %s", clipName.c_str(), asset_->path.c_str());
        return -1;
    }
    AnimClipDef d;
    d.clip = idx;
    d.speed = speed;
    d.loop = loop;
    d.events = std::move(events);
    std::sort(d.events.begin(), d.events.end(), [](auto& a, auto& b) { return a.time < b.time; });
    defs_.push_back(std::move(d));
    return int(defs_.size()) - 1;
}

float Animator::Duration(int def) const {
    if (def < 0 || def >= int(defs_.size())) return 0.0f;
    const auto& a = asset_->clips[size_t(defs_[size_t(def)].clip)];
    return std::max(0.0f, float(a.keyframeCount - 1) / kBakeFps);
}

float Animator::Normalized() const {
    float d = Duration(cur_.def);
    return d > 0 ? std::clamp(cur_.time / d, 0.0f, 1.0f) : 1.0f;
}

bool Animator::Finished() const {
    if (cur_.def < 0) return true;
    const auto& d = defs_[size_t(cur_.def)];
    return !d.loop && cur_.time >= Duration(cur_.def) - 1e-4f;
}

void Animator::Play(int def, float fade, bool restart, float startTime) {
    if (def < 0 || def >= int(defs_.size())) return;
    if (def == cur_.def && !restart) return;
    if (cur_.def >= 0 && fade > 0.0f) {
        prev_ = cur_;
        fadeDur_ = fade;
        fade_ = 0.0f;
    } else {
        prev_ = Layer{};
        fadeDur_ = 0.0f;
    }
    cur_.def = def;
    cur_.time = startTime;
}

void Animator::Sample(const Layer& l, std::vector<Transform>& out) const {
    const auto& d = defs_[size_t(l.def)];
    const ModelAnimation& a = asset_->clips[size_t(d.clip)];
    if (a.keyframeCount <= 0) return;
    float f = l.time * kBakeFps;
    int f0 = int(std::floor(f));
    float t = f - float(f0);
    int last = a.keyframeCount - 1;
    int f1 = f0 + 1;
    if (d.loop && last > 0) {
        f0 %= last + 1;
        f1 %= last + 1;
    } else {
        f0 = std::clamp(f0, 0, last);
        f1 = std::clamp(f1, 0, last);
    }
    const Transform* p0 = a.keyframePoses[f0];
    const Transform* p1 = a.keyframePoses[f1];
    unsigned n = std::min<unsigned>(a.boneCount, unsigned(out.size()));
    for (unsigned b = 0; b < n; ++b) {
        out[b].translation = Vector3Lerp(p0[b].translation, p1[b].translation, t);
        out[b].rotation = QuaternionSlerp(p0[b].rotation, p1[b].rotation, t);
        out[b].scale = Vector3Lerp(p0[b].scale, p1[b].scale, t);
    }
}

void Animator::Update(float dt) {
    fired_.clear();
    if (!Valid() || cur_.def < 0) return;
    const auto& d = defs_[size_t(cur_.def)];
    float dur = Duration(cur_.def);
    float before = cur_.time;
    float after = before + dt * d.speed * speedScale_;
    // Fire events crossed in (before, after], handling loop wrap.
    auto fireRange = [&](float a, float b) {
        for (const auto& e : d.events)
            if (e.time > a && e.time <= b) fired_.push_back({e.name, cur_.def});
    };
    if (d.loop && dur > 0.0f && after >= dur) {
        fireRange(before, dur);
        after = std::fmod(after, dur);
        fireRange(-1.0f, after);
    } else {
        if (before == 0.0f) fireRange(-1.0f, std::min(after, dur));
        else fireRange(before, std::min(after, dur));
        if (!d.loop) after = std::min(after, dur);
    }
    cur_.time = after;

    if (prev_.def >= 0) {
        const auto& pd = defs_[size_t(prev_.def)];
        float pdur = Duration(prev_.def);
        prev_.time += dt * pd.speed * speedScale_;
        if (pd.loop && pdur > 0) prev_.time = std::fmod(prev_.time, pdur);
        else prev_.time = std::min(prev_.time, pdur);
        fade_ += dt;
        if (fade_ >= fadeDur_) prev_.def = -1;
    }

    Sample(cur_, pose_);
    if (prev_.def >= 0 && fadeDur_ > 0.0f) {
        Sample(prev_, tmp_);
        float w = std::clamp(fade_ / fadeDur_, 0.0f, 1.0f);
        w = w * w * (3.0f - 2.0f * w);
        for (size_t b = 0; b < pose_.size(); ++b) {
            pose_[b].translation = Vector3Lerp(tmp_[b].translation, pose_[b].translation, w);
            pose_[b].rotation = QuaternionSlerp(tmp_[b].rotation, pose_[b].rotation, w);
            pose_[b].scale = Vector3Lerp(tmp_[b].scale, pose_[b].scale, w);
        }
    }
    for (size_t b = 0; b < pose_.size(); ++b) {
        poseMat_[b] = TransformToMatrix(pose_[b]);
        skin_[b] = MatrixMultiply(invBind_[b], poseMat_[b]);
    }
}

Matrix Animator::BoneModel(int bone) const {
    if (bone < 0 || bone >= int(poseMat_.size())) return MatrixIdentity();
    return poseMat_[size_t(bone)];
}

Matrix Animator::BoneWorld(int bone, const Matrix& modelWorld) const {
    return MatrixMultiply(BoneModel(bone), modelWorld);
}

}  // namespace lk
