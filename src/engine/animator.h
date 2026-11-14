#pragma once
// Skeletal animator: samples a clip by time, crossfades between clips, fires events at
// set clip times, supports per-clip speed and exposes bone matrices for attachments.
// Poses come from raylib's baked glTF keyframes (model-space transforms at 60 fps).
#include <string>
#include <vector>

#include "raylib.h"
#include "raymath.h"

namespace lk {

struct ModelAsset;

struct AnimEvent {
    float time = 0.0f;       // seconds into the clip
    std::string name;        // "hit", "cancel_open", "cancel_close", "footstep", ...
};

struct AnimClipDef {
    int clip = -1;           // index into ModelAsset::clips
    float speed = 1.0f;
    bool loop = true;
    std::vector<AnimEvent> events;
};

struct FiredEvent {
    std::string name;
    int clipDef = -1;
};

class Animator {
public:
    void Init(ModelAsset* asset);
    bool Valid() const { return asset_ != nullptr && !pose_.empty(); }

    // Register a playable clip (by full glTF clip name). Returns its id or -1.
    int AddClip(const std::string& clipName, float speed, bool loop, std::vector<AnimEvent> events = {});

    // Switch to a clip with a crossfade. restart=false keeps playing if already current.
    void Play(int clipDef, float fade = 0.1f, bool restart = false, float startTime = 0.0f);
    void SetSpeedScale(float s) { speedScale_ = s; }

    // Advance time, fire events, rebuild the pose and skinning matrices.
    void Update(float dt);

    int Current() const { return cur_.def; }
    float Time() const { return cur_.time; }
    float Duration(int clipDef) const;
    float Normalized() const;          // 0..1 through the current clip
    bool Finished() const;             // non-looping clip reached its end

    const std::vector<FiredEvent>& Events() const { return fired_; }

    // Model-space bone pose matrix (row-vector convention used by raymath).
    Matrix BoneModel(int bone) const;
    // World matrix of a bone given the model's world transform.
    Matrix BoneWorld(int bone, const Matrix& modelWorld) const;

    const Matrix* SkinMatrices() const { return skin_.data(); }
    int BoneCount() const { return int(skin_.size()); }

private:
    struct Layer {
        int def = -1;
        float time = 0.0f;
    };
    void Sample(const Layer& l, std::vector<Transform>& out) const;

    ModelAsset* asset_ = nullptr;
    std::vector<AnimClipDef> defs_;
    Layer cur_, prev_;
    float fade_ = 0.0f, fadeDur_ = 0.0f;
    float speedScale_ = 1.0f;
    std::vector<Transform> pose_, tmp_;
    std::vector<Matrix> poseMat_;
    std::vector<Matrix> invBind_;
    std::vector<Matrix> skin_;
    std::vector<FiredEvent> fired_;
};

Matrix TransformToMatrix(const Transform& t);

}  // namespace lk
