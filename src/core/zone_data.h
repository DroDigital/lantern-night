#pragma once
// Zone description loaded from data/levels/*.json: stacked height layers of ASCII tile grids
// with a legend, plus placements (props, lights, enemies, braziers, shrines, gates, triggers,
// floor hints). Pure data: no rendering or physics here, so it can be unit-tested.
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace lk {

struct Vec3f {
    float x = 0, y = 0, z = 0;
};

enum class TileKind { Void, Wall, Floor, Stairs };

struct LegendEntry {
    char ch = ' ';
    TileKind kind = TileKind::Void;
    std::string piece;              // KayKit dungeon piece (file stem) for floors/stairs
    int rot = 0;                    // degrees (0, 90, 180, 270)
    std::vector<std::string> variants;  // walls: random pick per edge
    bool water = false;             // flooded floor (visual + slows nothing in v1)
    bool dark = false;              // marks a dark area (no ambient bounce)
};

struct ZoneLayer {
    std::string name;
    int level = 0;                  // height index; y = level * layerHeight
    int originX = 0, originZ = 0;   // cell offset of row 0 / column 0
    std::vector<std::string> rows;
    int Width() const;
    int Depth() const { return int(rows.size()); }
    char At(int cx, int cz) const;  // in global cell coords; ' ' outside
};

struct Placement {
    std::string id;
    std::string type;               // prop/enemy/brazier/shrine/gate/...
    std::string model;              // asset path (props) or subtype (enemy kind)
    Vec3f pos;
    float rot = 0.0f;               // yaw degrees
    float scale = 1.0f;
    nlohmann::json extra;           // anything else (flags, links, text)
};

struct ZoneData {
    int version = 1;
    std::string name;
    float cellSize = 2.0f;
    float modelScale = 0.5f;
    float layerHeight = 2.0f;
    std::map<char, LegendEntry> legend;
    std::vector<ZoneLayer> layers;
    std::vector<Placement> props, lights, enemies, braziers, shrines, gates, triggers, hints, items;
    Placement fogGate;
    bool hasFogGate = false;
    Vec3f playerSpawn;
    float playerYaw = 0.0f;
    std::string ambientMusic = "ambient";

    const LegendEntry* Legend(char c) const;
    TileKind KindAt(int level, int cx, int cz) const;
    // World-space centre of a cell (floor height).
    Vec3f CellCenter(int level, int cx, int cz) const;
    // Cell containing a world point (x,z) - floor division.
    void WorldToCell(float x, float z, int& cx, int& cz) const;
    const ZoneLayer* Layer(int level) const;
};

// Parse; returns false and fills err on schema problems (missing fields, unknown legend chars).
bool ParseZone(const nlohmann::json& j, ZoneData& out, std::string& err);
// Set one tile, growing (or creating) the layer as needed. Used by the F2 editor.
void SetTile(ZoneData& z, int level, int cx, int cz, char c);
nlohmann::json ZoneToJson(const ZoneData& z);
nlohmann::json PlacementToJson(const Placement& p);

}  // namespace lk
