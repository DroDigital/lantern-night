#include <doctest/doctest.h>

#include "core/save_data.h"

using namespace lk;
using nlohmann::json;

TEST_CASE("save round-trips every field") {
    SaveData s;
    s.litShrines = {"shrine_entrance", "shrine_ossuary"};
    s.litBraziers = {"b1"};
    s.openedGates = {"gate_loop"};
    s.defeatedBosses = {"twin_blades"};
    s.pickedItems = {"shard_1", "greataxe"};
    s.secretsFound = {"hidden_alcove"};
    s.stats = {3, 2, 1};
    s.embers = 1234;
    s.flasks = 2;
    s.flaskMax = 4;
    s.flaskShards = 1;
    s.pile = {true, 1.5f, 2.0f, -3.25f, 800};
    s.weapon = "greataxe";
    s.lastShrine = "shrine_ossuary";
    s.gameCleared = true;
    SaveData r;
    std::string err;
    REQUIRE(SaveFromJson(SaveToJson(s), r, &err));
    CHECK(r == s);
    CHECK(r.version == kSaveVersion);
}

TEST_CASE("version 1 saves migrate forward") {
    json v1 = {{"version", 1},
               {"lit_shrines", {"shrine_entrance"}},
               {"lit_braziers", json::array()},
               {"opened_gates", json::array()},
               {"defeated_bosses", json::array()},
               {"stats", {{"vigor", 2}, {"endurance", 0}, {"might", 1}}},
               {"embers", 50},
               {"flasks", 4},
               {"flask_max", 5},
               {"ember_pile", {{"active", false}, {"pos", {0, 0, 0}}, {"amount", 0}}},
               {"weapon", "sword"},
               {"respawn_shrine", "shrine_entrance"}};
    SaveData s;
    std::string err;
    REQUIRE(SaveFromJson(v1, s, &err));
    CHECK(s.version == kSaveVersion);
    CHECK(s.flaskShards == 2);
    CHECK(s.lastShrine == "shrine_entrance");
    CHECK(s.stats.vigor == 2);
    CHECK(s.pickedItems.empty());
}

TEST_CASE("unknown versions are rejected") {
    SaveData s;
    std::string err;
    CHECK_FALSE(SaveFromJson(json{{"version", 99}, {"stats", json::object()}}, s, &err));
    CHECK_FALSE(SaveFromJson(json{{"stats", json::object()}}, s, &err));
}
