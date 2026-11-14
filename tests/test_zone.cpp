#include <doctest/doctest.h>

#include "core/zone_data.h"

using namespace lk;
using nlohmann::json;

static json MiniZone() {
    return json::parse(R"({
      "cell_size": 2.0, "model_scale": 0.5, "layer_height": 2.0,
      "legend": { "#": {"kind": "wall"}, ".": {"kind": "floor", "piece": "floor_tile_large"},
                  "^": {"kind": "stairs", "piece": "stairs", "rot": 0} },
      "layers": [ {"name": "g", "level": 0, "origin": [0, 0], "rows": ["###", "#^#", "#.#", "###"]},
                  {"name": "u", "level": 1, "origin": [1, 0], "rows": ["."]} ],
      "player_spawn": {"pos": [3, 0, 5], "rot": 90},
      "enemies": [ {"id": "e1", "kind": "minion", "pos": [1, 0, 1]} ]
    })");
}

TEST_CASE("zone parses layers, legend and placements") {
    ZoneData z;
    std::string err;
    REQUIRE(ParseZone(MiniZone(), z, err));
    CHECK(z.cellSize == 2.0f);
    CHECK(z.layers.size() == 2);
    CHECK(z.KindAt(0, 1, 1) == TileKind::Stairs);
    CHECK(z.KindAt(0, 1, 2) == TileKind::Floor);
    CHECK(z.KindAt(0, 0, 0) == TileKind::Wall);
    CHECK(z.KindAt(0, 9, 9) == TileKind::Void);
    CHECK(z.KindAt(1, 1, 0) == TileKind::Floor);  // origin offset applies
    CHECK(z.enemies.size() == 1);
    CHECK(z.enemies[0].model == "minion");
    Vec3f c = z.CellCenter(1, 1, 0);
    CHECK(c.x == doctest::Approx(3.0f));
    CHECK(c.y == doctest::Approx(2.0f));
    int cx, cz;
    z.WorldToCell(3.9f, 5.1f, cx, cz);
    CHECK(cx == 1);
    CHECK(cz == 2);
}

TEST_CASE("zone rejects unknown tiles") {
    json j = MiniZone();
    j["layers"][0]["rows"][2] = "#X#";
    ZoneData z;
    std::string err;
    CHECK_FALSE(ParseZone(j, z, err));
    CHECK(err.find("unknown tile") != std::string::npos);
}

TEST_CASE("zone round-trips through json") {
    ZoneData a, b;
    std::string err;
    REQUIRE(ParseZone(MiniZone(), a, err));
    REQUIRE(ParseZone(ZoneToJson(a), b, err));
    CHECK(b.layers.size() == a.layers.size());
    CHECK(b.layers[1].rows == a.layers[1].rows);
    CHECK(b.enemies[0].pos.x == a.enemies[0].pos.x);
    CHECK(b.playerYaw == a.playerYaw);
}

TEST_CASE("SetTile grows layers in every direction") {
    ZoneData z;
    std::string err;
    REQUIRE(ParseZone(MiniZone(), z, err));
    SetTile(z, 0, -2, -1, '.');
    CHECK(z.KindAt(0, -2, -1) == TileKind::Floor);
    CHECK(z.KindAt(0, 1, 1) == TileKind::Stairs);  // existing tiles keep their world cells
    SetTile(z, 0, 9, 7, '#');
    CHECK(z.KindAt(0, 9, 7) == TileKind::Wall);
    SetTile(z, 3, 4, 4, '.');
    CHECK(z.KindAt(3, 4, 4) == TileKind::Floor);
}
