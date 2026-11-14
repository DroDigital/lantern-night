#pragma once
// Asset cache. Everything is loaded up front (Preload) and looked up by path afterwards;
// nothing is loaded during gameplay. glTF/GLB only.
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "raylib.h"

namespace lk {

struct ModelAsset {
    std::string path;
    Model model{};
    std::vector<std::string> meshNames;       // node name per mesh index
    std::vector<ModelAnimation> clips;         // owned array copied out of raylib
    ModelAnimation* rawClips = nullptr;
    int rawClipCount = 0;
    std::map<std::string, int> clipIndex;      // full clip name -> index
    std::vector<std::string> clipNames;
    BoundingBox bounds{};
    bool skinned = false;

    int FindMesh(const std::string& nodeName) const;
    int FindBone(const std::string& boneName) const;
    int FindClip(const std::string& name) const;
};

class AssetCache {
public:
    ~AssetCache();
    void UnloadAll();

    // Shaders used for every model material (static and skinned variants).
    void SetDefaultShaders(Shader staticShader, Shader skinnedShader);

    ModelAsset* LoadModel(const std::string& assetRel);   // path relative to assets/
    ModelAsset* GetModel(const std::string& assetRel) const;
    Texture2D LoadTexture(const std::string& assetRel, bool mipmaps = true);
    Texture2D GetTexture(const std::string& assetRel) const;
    Sound LoadSound(const std::string& assetRel);
    Font LoadFont(const std::string& assetRel, int size);
    Font GetFont(const std::string& assetRel, int size) const;

    int LoadedModelCount() const { return int(models_.size()); }
    int FailedCount() const { return failures_; }

    // Reassign shaders on every material (after a shader reload).
    void ApplyShaders();

private:
    std::map<std::string, std::unique_ptr<ModelAsset>> models_;
    std::map<std::string, Texture2D> textures_;
    std::map<std::string, Sound> sounds_;
    std::map<std::string, Font> fonts_;
    Shader static_{};
    Shader skinned_{};
    int failures_ = 0;
};

}  // namespace lk
