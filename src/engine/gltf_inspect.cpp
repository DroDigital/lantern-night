#include "engine/gltf_inspect.h"

#include <algorithm>
#include <cfloat>
#include <functional>

#include "cgltf.h"  // implementation is compiled inside raylib

namespace lk {

static void Grow(GltfAabb& b, const float p[3]) {
    if (!b.valid) {
        for (int i = 0; i < 3; ++i) b.min[i] = b.max[i] = p[i];
        b.valid = true;
        return;
    }
    for (int i = 0; i < 3; ++i) {
        b.min[i] = std::min(b.min[i], p[i]);
        b.max[i] = std::max(b.max[i], p[i]);
    }
}

static const cgltf_accessor* FindPosition(const cgltf_primitive& prim) {
    for (cgltf_size a = 0; a < prim.attributes_count; ++a)
        if (prim.attributes[a].type == cgltf_attribute_type_position) return prim.attributes[a].data;
    return nullptr;
}

GltfReport InspectGltf(const std::string& path) {
    GltfReport r;
    r.path = path;
    cgltf_options opt{};
    cgltf_data* data = nullptr;
    cgltf_result res = cgltf_parse_file(&opt, path.c_str(), &data);
    if (res != cgltf_result_success) {
        r.error = "cgltf_parse_file failed (" + std::to_string(int(res)) + ")";
        return r;
    }
    res = cgltf_load_buffers(&opt, data, path.c_str());
    if (res != cgltf_result_success) {
        r.error = "cgltf_load_buffers failed (" + std::to_string(int(res)) + ")";
        cgltf_free(data);
        return r;
    }
    res = cgltf_validate(data);
    if (res != cgltf_result_success) {
        r.error = "cgltf_validate failed (" + std::to_string(int(res)) + ")";
        cgltf_free(data);
        return r;
    }

    r.skins = int(data->skins_count);
    r.materials = int(data->materials_count);
    r.images = int(data->images_count);

    auto isJoint = [&](const cgltf_node* n) {
        for (cgltf_size s = 0; s < data->skins_count; ++s)
            for (cgltf_size j = 0; j < data->skins[s].joints_count; ++j)
                if (data->skins[s].joints[j] == n) return true;
        return false;
    };

    std::function<void(const cgltf_node*, int)> walk = [&](const cgltf_node* n, int depth) {
        GltfNodeInfo info;
        info.name = n->name ? n->name : "(unnamed)";
        info.depth = depth;
        info.mesh = n->mesh ? (n->mesh->name ? n->mesh->name : "(mesh)") : "";
        info.isJoint = isJoint(n);
        r.nodes.push_back(info);
        if (n->mesh && !n->skin) {
            float world[16];
            cgltf_node_transform_world(n, world);
            for (cgltf_size p = 0; p < n->mesh->primitives_count; ++p) {
                const cgltf_accessor* pos = FindPosition(n->mesh->primitives[p]);
                if (!pos) continue;
                for (cgltf_size v = 0; v < pos->count; ++v) {
                    float lp[3];
                    cgltf_accessor_read_float(pos, v, lp, 3);
                    float wp[3];
                    for (int i = 0; i < 3; ++i)
                        wp[i] = world[0 * 4 + i] * lp[0] + world[1 * 4 + i] * lp[1] + world[2 * 4 + i] * lp[2] + world[12 + i];
                    Grow(r.sceneAabb, wp);
                }
            }
        } else if (n->mesh && n->skin) {
            // Skinned mesh: bind-pose positions are already in model space.
            for (cgltf_size p = 0; p < n->mesh->primitives_count; ++p) {
                const cgltf_accessor* pos = FindPosition(n->mesh->primitives[p]);
                if (!pos) continue;
                for (cgltf_size v = 0; v < pos->count; ++v) {
                    float lp[3];
                    cgltf_accessor_read_float(pos, v, lp, 3);
                    Grow(r.sceneAabb, lp);
                }
            }
        }
        for (cgltf_size c = 0; c < n->children_count; ++c) walk(n->children[c], depth + 1);
    };
    const cgltf_scene* scene = data->scene ? data->scene : (data->scenes_count ? &data->scenes[0] : nullptr);
    if (scene) {
        for (cgltf_size i = 0; i < scene->nodes_count; ++i) walk(scene->nodes[i], 0);
    } else {
        for (cgltf_size i = 0; i < data->nodes_count; ++i)
            if (!data->nodes[i].parent) walk(&data->nodes[i], 0);
    }

    for (cgltf_size m = 0; m < data->meshes_count; ++m) {
        const cgltf_mesh& mesh = data->meshes[m];
        GltfMeshInfo mi;
        mi.name = mesh.name ? mesh.name : "(unnamed)";
        mi.primitives = int(mesh.primitives_count);
        for (cgltf_size p = 0; p < mesh.primitives_count; ++p) {
            const cgltf_accessor* pos = FindPosition(mesh.primitives[p]);
            if (!pos) continue;
            mi.vertices += int(pos->count);
            if (pos->has_min && pos->has_max) {
                Grow(mi.aabb, pos->min);
                Grow(mi.aabb, pos->max);
            }
        }
        r.meshes.push_back(mi);
    }

    if (data->skins_count > 0) {
        const cgltf_skin& skin = data->skins[0];
        for (cgltf_size j = 0; j < skin.joints_count; ++j)
            r.bones.push_back(skin.joints[j]->name ? skin.joints[j]->name : "(unnamed)");
    }

    for (cgltf_size a = 0; a < data->animations_count; ++a) {
        const cgltf_animation& anim = data->animations[a];
        GltfClipInfo ci;
        ci.name = anim.name ? anim.name : "(unnamed)";
        ci.channels = int(anim.channels_count);
        for (cgltf_size s = 0; s < anim.samplers_count; ++s) {
            const cgltf_accessor* in = anim.samplers[s].input;
            if (in && in->has_max) ci.duration = std::max(ci.duration, in->max[0]);
            else if (in && in->count) {
                float t = 0;
                cgltf_accessor_read_float(in, in->count - 1, &t, 1);
                ci.duration = std::max(ci.duration, t);
            }
        }
        r.clips.push_back(ci);
    }

    cgltf_free(data);
    r.ok = true;
    return r;
}

std::vector<std::string> GltfMeshNodeNames(const std::string& path) {
    std::vector<std::string> names;
    cgltf_options opt{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &data) != cgltf_result_success) return names;
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const cgltf_node& n = data->nodes[i];
        if (!n.mesh) continue;
        for (cgltf_size p = 0; p < n.mesh->primitives_count; ++p)
            if (n.mesh->primitives[p].type == cgltf_primitive_type_triangles) names.push_back(n.name ? n.name : "");
    }
    cgltf_free(data);
    return names;
}

std::vector<std::string> GltfAnimationNames(const std::string& path) {
    std::vector<std::string> names;
    cgltf_options opt{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &data) != cgltf_result_success) return names;
    if (data->skins_count > 0)
        for (cgltf_size i = 0; i < data->animations_count; ++i)
            names.push_back(data->animations[i].name ? data->animations[i].name : "");
    cgltf_free(data);
    return names;
}

}  // namespace lk
