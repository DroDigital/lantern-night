#include "core/zone_data.h"

#include <cmath>

namespace lk {

using nlohmann::json;

int ZoneLayer::Width() const {
    size_t w = 0;
    for (auto& r : rows) w = std::max(w, r.size());
    return int(w);
}

char ZoneLayer::At(int cx, int cz) const {
    int lx = cx - originX, lz = cz - originZ;
    if (lz < 0 || lz >= int(rows.size())) return ' ';
    const auto& r = rows[size_t(lz)];
    if (lx < 0 || lx >= int(r.size())) return ' ';
    return r[size_t(lx)];
}

const LegendEntry* ZoneData::Legend(char c) const {
    auto it = legend.find(c);
    return it == legend.end() ? nullptr : &it->second;
}

const ZoneLayer* ZoneData::Layer(int level) const {
    for (auto& l : layers)
        if (l.level == level) return &l;
    return nullptr;
}

TileKind ZoneData::KindAt(int level, int cx, int cz) const {
    const ZoneLayer* l = Layer(level);
    if (!l) return TileKind::Void;
    const LegendEntry* e = Legend(l->At(cx, cz));
    return e ? e->kind : TileKind::Void;
}

Vec3f ZoneData::CellCenter(int level, int cx, int cz) const {
    return Vec3f{(float(cx) + 0.5f) * cellSize, float(level) * layerHeight, (float(cz) + 0.5f) * cellSize};
}

void ZoneData::WorldToCell(float x, float z, int& cx, int& cz) const {
    cx = int(std::floor(x / cellSize));
    cz = int(std::floor(z / cellSize));
}

static Vec3f ReadVec(const json& j) {
    Vec3f v;
    if (j.is_array() && j.size() >= 3) {
        v.x = j[0].get<float>();
        v.y = j[1].get<float>();
        v.z = j[2].get<float>();
    }
    return v;
}

static json WriteVec(const Vec3f& v) { return json::array({v.x, v.y, v.z}); }

static bool ReadPlacements(const json& j, const char* key, std::vector<Placement>& out, std::string& err) {
    if (!j.contains(key)) return true;
    if (!j[key].is_array()) {
        err = std::string(key) + " must be an array";
        return false;
    }
    for (auto& e : j[key]) {
        Placement p;
        p.type = key;
        p.id = e.value("id", "");
        p.model = e.value("model", e.value("kind", ""));
        if (!e.contains("pos")) {
            err = std::string(key) + " entry missing pos";
            return false;
        }
        p.pos = ReadVec(e["pos"]);
        p.rot = e.value("rot", 0.0f);
        p.scale = e.value("scale", 1.0f);
        p.extra = e;
        out.push_back(std::move(p));
    }
    return true;
}

bool ParseZone(const json& j, ZoneData& z, std::string& err) {
    z = ZoneData{};
    try {
        z.version = j.value("version", 1);
        z.name = j.value("name", "zone");
        z.cellSize = j.at("cell_size").get<float>();
        z.modelScale = j.at("model_scale").get<float>();
        z.layerHeight = j.at("layer_height").get<float>();
        z.ambientMusic = j.value("music", "ambient");
        for (auto& [k, v] : j.at("legend").items()) {
            if (k.size() != 1) {
                err = "legend key must be one character: '" + k + "'";
                return false;
            }
            LegendEntry e;
            e.ch = k[0];
            std::string kind = v.at("kind").get<std::string>();
            if (kind == "floor") e.kind = TileKind::Floor;
            else if (kind == "wall") e.kind = TileKind::Wall;
            else if (kind == "stairs") e.kind = TileKind::Stairs;
            else if (kind == "void") e.kind = TileKind::Void;
            else {
                err = "unknown tile kind '" + kind + "'";
                return false;
            }
            e.piece = v.value("piece", "");
            e.rot = v.value("rot", 0);
            e.water = v.value("water", false);
            e.dark = v.value("dark", false);
            if (v.contains("variants")) e.variants = v["variants"].get<std::vector<std::string>>();
            if ((e.kind == TileKind::Floor || e.kind == TileKind::Stairs) && e.piece.empty()) {
                err = std::string("legend '") + e.ch + "' needs a piece";
                return false;
            }
            z.legend[e.ch] = e;
        }
        if (!z.legend.count(' ')) {
            LegendEntry v;
            v.ch = ' ';
            z.legend[' '] = v;
        }
        for (auto& lj : j.at("layers")) {
            ZoneLayer l;
            l.name = lj.value("name", "");
            l.level = lj.at("level").get<int>();
            if (lj.contains("origin")) {
                l.originX = lj["origin"][0].get<int>();
                l.originZ = lj["origin"][1].get<int>();
            }
            l.rows = lj.at("rows").get<std::vector<std::string>>();
            for (auto& r : l.rows)
                for (char c : r)
                    if (!z.legend.count(c)) {
                        err = std::string("layer '") + l.name + "' uses unknown tile '" + c + "'";
                        return false;
                    }
            z.layers.push_back(std::move(l));
        }
        const json& sp = j.at("player_spawn");
        z.playerSpawn = ReadVec(sp.at("pos"));
        z.playerYaw = sp.value("rot", 0.0f);
        if (!ReadPlacements(j, "props", z.props, err) || !ReadPlacements(j, "lights", z.lights, err) ||
            !ReadPlacements(j, "enemies", z.enemies, err) || !ReadPlacements(j, "braziers", z.braziers, err) ||
            !ReadPlacements(j, "shrines", z.shrines, err) || !ReadPlacements(j, "gates", z.gates, err) ||
            !ReadPlacements(j, "triggers", z.triggers, err) || !ReadPlacements(j, "hints", z.hints, err) ||
            !ReadPlacements(j, "items", z.items, err))
            return false;
        if (j.contains("fog_gate")) {
            std::vector<Placement> tmp;
            json wrap = {{"fog_gate", json::array({j["fog_gate"]})}};
            if (!ReadPlacements(wrap, "fog_gate", tmp, err)) return false;
            z.fogGate = tmp[0];
            z.hasFogGate = true;
        }
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
    return true;
}

json PlacementToJson(const Placement& p) {
    json e = p.extra.is_object() ? p.extra : json::object();
    if (!p.id.empty()) e["id"] = p.id;
    if (!p.model.empty()) {
        if (e.contains("kind")) e["kind"] = p.model;
        else e["model"] = p.model;
    }
    e["pos"] = WriteVec(p.pos);
    e["rot"] = p.rot;
    if (p.scale != 1.0f) e["scale"] = p.scale;
    else e.erase("scale");
    return e;
}

json ZoneToJson(const ZoneData& z) {
    json j;
    j["version"] = z.version;
    j["name"] = z.name;
    j["cell_size"] = z.cellSize;
    j["model_scale"] = z.modelScale;
    j["layer_height"] = z.layerHeight;
    j["music"] = z.ambientMusic;
    json legend = json::object();
    for (auto& [c, e] : z.legend) {
        if (c == ' ' && e.kind == TileKind::Void && e.piece.empty()) continue;
        json v;
        switch (e.kind) {
            case TileKind::Floor: v["kind"] = "floor"; break;
            case TileKind::Wall: v["kind"] = "wall"; break;
            case TileKind::Stairs: v["kind"] = "stairs"; break;
            case TileKind::Void: v["kind"] = "void"; break;
        }
        if (!e.piece.empty()) v["piece"] = e.piece;
        if (e.rot) v["rot"] = e.rot;
        if (e.water) v["water"] = true;
        if (e.dark) v["dark"] = true;
        if (!e.variants.empty()) v["variants"] = e.variants;
        legend[std::string(1, c)] = v;
    }
    j["legend"] = legend;
    json layers = json::array();
    for (auto& l : z.layers)
        layers.push_back({{"name", l.name}, {"level", l.level}, {"origin", {l.originX, l.originZ}}, {"rows", l.rows}});
    j["layers"] = layers;
    j["player_spawn"] = {{"pos", WriteVec(z.playerSpawn)}, {"rot", z.playerYaw}};
    auto dump = [&](const char* key, const std::vector<Placement>& v) {
        json a = json::array();
        for (auto& p : v) a.push_back(PlacementToJson(p));
        j[key] = a;
    };
    dump("props", z.props);
    dump("lights", z.lights);
    dump("enemies", z.enemies);
    dump("braziers", z.braziers);
    dump("shrines", z.shrines);
    dump("gates", z.gates);
    dump("triggers", z.triggers);
    dump("hints", z.hints);
    dump("items", z.items);
    if (z.hasFogGate) j["fog_gate"] = PlacementToJson(z.fogGate);
    return j;
}

}  // namespace lk
