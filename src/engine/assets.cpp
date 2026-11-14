#include "engine/assets.h"

#include <cstring>
#include <filesystem>

#include "engine/gltf_inspect.h"
#include "engine/log.h"
#include "engine/paths.h"
#include "rlgl.h"

namespace lk {

int ModelAsset::FindMesh(const std::string& nodeName) const {
    for (size_t i = 0; i < meshNames.size(); ++i)
        if (meshNames[i] == nodeName) return int(i);
    return -1;
}

int ModelAsset::FindBone(const std::string& boneName) const {
    for (unsigned i = 0; i < model.skeleton.boneCount; ++i)
        if (boneName == model.skeleton.bones[i].name) return int(i);
    return -1;
}

int ModelAsset::FindClip(const std::string& name) const {
    auto it = clipIndex.find(name);
    return it == clipIndex.end() ? -1 : it->second;
}

AssetCache::~AssetCache() { UnloadAll(); }

void AssetCache::UnloadAll() {
    if (!IsWindowReady()) return;
    for (auto& [k, m] : models_) {
        if (m->rawClips) UnloadModelAnimations(m->rawClips, m->rawClipCount);
        // Materials share our shaders: detach before raylib unloads them.
        for (int i = 0; i < m->model.materialCount; ++i) m->model.materials[i].shader = Shader{rlGetShaderIdDefault(), rlGetShaderLocsDefault()};
        UnloadModel(m->model);
    }
    models_.clear();
    for (auto& [k, t] : textures_) UnloadTexture(t);
    textures_.clear();
    for (auto& [k, s] : sounds_) UnloadSound(s);
    sounds_.clear();
    for (auto& [k, f] : fonts_) UnloadFont(f);
    fonts_.clear();
}

void AssetCache::SetDefaultShaders(Shader staticShader, Shader skinnedShader) {
    static_ = staticShader;
    skinned_ = skinnedShader;
}

void AssetCache::ApplyShaders() {
    for (auto& [k, m] : models_)
        for (int i = 0; i < m->model.materialCount; ++i) m->model.materials[i].shader = m->skinned ? skinned_ : static_;
}

ModelAsset* AssetCache::GetModel(const std::string& rel) const {
    auto it = models_.find(rel);
    return it == models_.end() ? nullptr : it->second.get();
}

ModelAsset* AssetCache::LoadModel(const std::string& rel) {
    if (auto* m = GetModel(rel)) return m;
    std::string full = paths::Asset(rel);
    if (!std::filesystem::exists(full)) {
        LK_ERROR("assets: missing model %s", rel.c_str());
        ++failures_;
        return nullptr;
    }
    auto asset = std::make_unique<ModelAsset>();
    asset->path = rel;
    asset->model = ::LoadModel(full.c_str());
    if (asset->model.meshCount == 0) {
        LK_ERROR("assets: failed to load model %s", rel.c_str());
        ++failures_;
        return nullptr;
    }
    asset->meshNames = GltfMeshNodeNames(full);
    if (int(asset->meshNames.size()) != asset->model.meshCount) {
        LK_WARN("assets: %s mesh name count %zu != mesh count %d", rel.c_str(), asset->meshNames.size(),
                asset->model.meshCount);
        asset->meshNames.resize(size_t(asset->model.meshCount));
    }
    asset->skinned = asset->model.skeleton.boneCount > 0;
    if (asset->skinned) {
        asset->rawClips = LoadModelAnimations(full.c_str(), &asset->rawClipCount);
        auto names = GltfAnimationNames(full);
        for (int i = 0; i < asset->rawClipCount; ++i) {
            asset->clips.push_back(asset->rawClips[i]);
            std::string n = (size_t(i) < names.size()) ? names[size_t(i)] : asset->rawClips[i].name;
            asset->clipNames.push_back(n);
            asset->clipIndex[n] = i;
            if (!IsModelAnimationValid(asset->model, asset->rawClips[i]))
                LK_WARN("assets: %s clip %s does not match skeleton", rel.c_str(), n.c_str());
        }
    }
    asset->bounds = GetModelBoundingBox(asset->model);
    for (int i = 0; i < asset->model.materialCount; ++i) {
        Material& mat = asset->model.materials[i];
        mat.shader = asset->skinned ? skinned_ : static_;
        Texture2D& t = mat.maps[MATERIAL_MAP_DIFFUSE].texture;
        if (t.id > 0 && t.id != rlGetTextureIdDefault()) {
            // KayKit uses a small gradient atlas: bilinear without mips keeps swatches clean.
            bool kaykit = rel.rfind("kaykit", 0) == 0;
            if (!kaykit) GenTextureMipmaps(&t);
            SetTextureFilter(t, kaykit ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_TRILINEAR);
            SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
        }
    }
    LK_DEBUG("assets: model %s (%d meshes, %u bones, %d clips)", rel.c_str(), asset->model.meshCount,
             asset->model.skeleton.boneCount, asset->rawClipCount);
    auto* ptr = asset.get();
    models_[rel] = std::move(asset);
    return ptr;
}

Texture2D AssetCache::LoadTexture(const std::string& rel, bool mipmaps) {
    auto it = textures_.find(rel);
    if (it != textures_.end()) return it->second;
    std::string full = paths::Asset(rel);
    Texture2D t = ::LoadTexture(full.c_str());
    if (t.id == 0) {
        LK_ERROR("assets: failed to load texture %s", rel.c_str());
        ++failures_;
        return t;
    }
    if (mipmaps) {
        GenTextureMipmaps(&t);
        SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    } else {
        SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    }
    textures_[rel] = t;
    return t;
}

Texture2D AssetCache::GetTexture(const std::string& rel) const {
    auto it = textures_.find(rel);
    return it == textures_.end() ? Texture2D{} : it->second;
}

Sound AssetCache::LoadSound(const std::string& rel) {
    auto it = sounds_.find(rel);
    if (it != sounds_.end()) return it->second;
    std::string full = paths::Asset(rel);
    Sound s = ::LoadSound(full.c_str());
    if (s.frameCount == 0) {
        LK_ERROR("assets: failed to load sound %s", rel.c_str());
        ++failures_;
    }
    sounds_[rel] = s;
    return s;
}

Font AssetCache::LoadFont(const std::string& rel, int size) {
    std::string key = rel + "@" + std::to_string(size);
    auto it = fonts_.find(key);
    if (it != fonts_.end()) return it->second;
    std::string full = paths::Asset(rel);
    Font f = LoadFontEx(full.c_str(), size, nullptr, 0);
    if (f.texture.id == 0) {
        LK_ERROR("assets: failed to load font %s", rel.c_str());
        ++failures_;
        f = GetFontDefault();
    } else {
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    }
    fonts_[key] = f;
    return f;
}

Font AssetCache::GetFont(const std::string& rel, int size) const {
    auto it = fonts_.find(rel + "@" + std::to_string(size));
    return it == fonts_.end() ? GetFontDefault() : it->second;
}

}  // namespace lk
