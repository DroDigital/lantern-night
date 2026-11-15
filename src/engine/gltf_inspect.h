#pragma once
// Reads a glTF/GLB file with cgltf (no window needed) and summarises it:
// node tree, meshes, bones, AABBs and animation clips. Used by tools/asset_report
// and by the game's --validate mode.
#include <string>
#include <vector>

namespace lk {

struct GltfAabb {
    float min[3] = {0, 0, 0};
    float max[3] = {0, 0, 0};
    bool valid = false;
};

struct GltfNodeInfo {
    std::string name;
    int depth = 0;
    std::string mesh;   // empty if none
    bool isJoint = false;
};

struct GltfClipInfo {
    std::string name;
    float duration = 0.0f;
    int channels = 0;
};

struct GltfMeshInfo {
    std::string name;
    int primitives = 0;
    int vertices = 0;
    GltfAabb aabb;   // local (mesh space)
};

struct GltfReport {
    std::string path;
    bool ok = false;
    std::string error;
    std::vector<GltfNodeInfo> nodes;
    std::vector<GltfMeshInfo> meshes;
    std::vector<std::string> bones;
    std::vector<GltfClipInfo> clips;
    GltfAabb sceneAabb;  // world-space AABB of all mesh positions under node transforms
    int skins = 0;
    int materials = 0;
    int images = 0;
};

GltfReport InspectGltf(const std::string& path);

// Node name for each raylib Model mesh index (raylib emits one mesh per triangle primitive,
// walking data->nodes in file order). Lets the game hide/show/tint meshes by name.
std::vector<std::string> GltfMeshNodeNames(const std::string& path);
// Full (untruncated) animation names in file order; raylib truncates to 31 chars.
std::vector<std::string> GltfAnimationNames(const std::string& path);

}  // namespace lk
