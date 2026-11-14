#include "game/validate.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <vector>

#include "core/zone_data.h"
#include "engine/data_store.h"
#include "engine/gltf_inspect.h"
#include "engine/log.h"
#include "engine/nav.h"
#include "engine/paths.h"

namespace fs = std::filesystem;

namespace lk {
namespace {

// Required top-level fields per data file (prefix match on the relative path).
const std::map<std::string, std::vector<std::string>> kRequired = {
    {"render.json", {"ambient", "fog_color", "fog_density", "bloom_strength", "exposure", "vignette", "lantern"}},
    {"camera.json", {"distance", "pivot_height", "pitch_min_deg", "pitch_max_deg", "collision_radius"}},
    {"audio.json", {"sfx", "music"}},
    {"player/stats.json",
     {"flame_max_base", "flame_per_vigor", "stamina_max_base", "stamina_per_endurance", "damage_per_might", "stamina_regen",
      "stamina_regen_delay", "costs", "roll", "heavy", "block", "flask", "lockon"}},
    {"player/leveling.json", {"cost_base", "cost_growth"}},
    {"weapons/", {"name", "damage_light", "damage_heavy", "posture_light", "posture_heavy", "anims", "hitbox"}},
    {"enemies/", {"name", "hp", "model", "anims", "moves", "embers"}},
    {"boss/", {"name", "hp", "model", "phases"}},
    {"anims/", {"model", "actions"}},
    {"levels/", {"cell_size", "model_scale", "layer_height", "legend", "layers", "player_spawn"}},
};

int CheckFields(const std::string& rel, const json& j) {
    int problems = 0;
    for (auto& [prefix, fields] : kRequired) {
        bool match = prefix.back() == '/' ? rel.rfind(prefix, 0) == 0 : rel == prefix;
        if (!match) continue;
        for (auto& f : fields)
            if (!j.contains(f)) {
                LK_ERROR("validate: %s missing field '%s'", rel.c_str(), f.c_str());
                ++problems;
            }
    }
    return problems;
}

int CheckAssetRefs(const json& j, const std::string& rel) {
    // Any string value that looks like an asset path must exist.
    int problems = 0;
    if (j.is_object())
        for (auto& [k, v] : j.items()) problems += CheckAssetRefs(v, rel);
    else if (j.is_array())
        for (auto& v : j) problems += CheckAssetRefs(v, rel);
    else if (j.is_string()) {
        const std::string s = j.get<std::string>();
        static const char* exts[] = {".glb", ".gltf", ".png", ".ogg", ".wav", ".ttf"};
        for (const char* e : exts)
            if (s.size() > 4 && s.rfind(e) == s.size() - std::char_traits<char>::length(e) && s.find(' ') == std::string::npos) {
                if (!fs::exists(paths::Asset(s))) {
                    LK_ERROR("validate: %s references missing asset %s", rel.c_str(), s.c_str());
                    ++problems;
                }
                break;
            }
    }
    return problems;
}
}  // namespace

int ValidateContent() {
    int problems = 0;
    // 1. Every glTF/GLB parses.
    int models = 0;
    for (auto& e : fs::recursive_directory_iterator(paths::AssetDir())) {
        if (!e.is_regular_file()) continue;
        auto ext = e.path().extension().string();
        if (ext != ".gltf" && ext != ".glb") continue;
        ++models;
        auto r = InspectGltf(e.path().string());
        if (!r.ok) {
            LK_ERROR("validate: %s: %s", e.path().string().c_str(), r.error.c_str());
            ++problems;
        }
    }
    // 2. Every data file parses and has its fields.
    DataStore ds;
    if (!ds.LoadAll(paths::DataDir())) ++problems;
    std::map<std::string, std::set<std::string>> clipCache;
    for (const auto& rel : ds.Files()) {
        const json& j = ds.Get(rel);
        problems += CheckFields(rel, j);
        problems += CheckAssetRefs(j, rel);
        if (rel.rfind("levels/", 0) == 0) {
            ZoneData z;
            std::string err;
            if (!ParseZone(j, z, err)) {
                LK_ERROR("validate: %s: %s", rel.c_str(), err.c_str());
                ++problems;
            } else if (rel == "levels/zone.json") {
                // Connectivity: everything that matters must be reachable from the spawn (all gates open).
                Nav nav;
                nav.Build(z);
                Vector3 spawn{z.playerSpawn.x, z.playerSpawn.y + 0.1f, z.playerSpawn.z};
                auto reach = [&](const char* what, const std::string& id, Vec3f p) {
                    Vector3 t{p.x, p.y + 0.1f, p.z};
                    if (nav.Path(spawn, t).empty() && !nav.Walkable(t)) {
                        LK_ERROR("validate: zone: %s '%s' at (%.1f, %.1f, %.1f) is not on the nav grid", what, id.c_str(), p.x, p.y, p.z);
                        ++problems;
                    } else if (nav.Path(spawn, t).empty()) {
                        LK_ERROR("validate: zone: %s '%s' is unreachable from the spawn", what, id.c_str());
                        ++problems;
                    }
                };
                for (const auto& s : z.shrines) reach("shrine", s.id, s.pos);
                for (const auto& s : z.items) reach("item", s.id, s.pos);
                for (const auto& s : z.braziers) reach("brazier", s.id, s.pos);
                for (const auto& s : z.enemies) reach("enemy", s.id, s.pos);
                for (const auto& s : z.gates) reach("gate", s.id, s.pos);
                if (z.hasFogGate) reach("fog gate", z.fogGate.id, z.fogGate.pos);
                LK_INFO("validate: zone connectivity checked (%d nav nodes)", nav.NodeCount());
            }
        }
        if (rel.rfind("anims/", 0) == 0 && j.contains("model") && j.contains("actions")) {
            std::string model = j["model"].get<std::string>();
            auto& clips = clipCache[model];
            if (clips.empty())
                for (auto& n : GltfAnimationNames(paths::Asset(model))) clips.insert(n);
            for (auto& [action, a] : j["actions"].items()) {
                std::string clip = a.value("clip", "");
                if (!clips.count(clip)) {
                    LK_ERROR("validate: %s action '%s' uses unknown clip '%s'", rel.c_str(), action.c_str(), clip.c_str());
                    ++problems;
                }
            }
        }
    }
    LK_INFO("validate: %d models, %zu data files, %d problems", models, ds.Files().size(), problems);
    return problems;
}

}  // namespace lk
