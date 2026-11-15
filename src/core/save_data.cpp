#include "core/save_data.h"

namespace lk {

using nlohmann::json;

json SaveToJson(const SaveData& s) {
    json j;
    j["version"] = kSaveVersion;
    j["lit_shrines"] = s.litShrines;
    j["lit_braziers"] = s.litBraziers;
    j["opened_gates"] = s.openedGates;
    j["defeated_bosses"] = s.defeatedBosses;
    j["picked_items"] = s.pickedItems;
    j["secrets_found"] = s.secretsFound;
    j["stats"] = {{"vigor", s.stats.vigor}, {"endurance", s.stats.endurance}, {"might", s.stats.might}};
    j["embers"] = s.embers;
    j["flasks"] = s.flasks;
    j["flask_max"] = s.flaskMax;
    j["flask_shards"] = s.flaskShards;
    j["ember_pile"] = {{"active", s.pile.active}, {"pos", {s.pile.x, s.pile.y, s.pile.z}}, {"amount", s.pile.amount}};
    j["weapon"] = s.weapon;
    j["last_shrine"] = s.lastShrine;
    j["play_time"] = s.playTime;
    j["game_cleared"] = s.gameCleared;
    return j;
}

static void Migrate1To2(json& j) {
    // v1 stored the flask maximum but not the shards; infer shards from the maximum.
    int fmax = j.value("flask_max", 3);
    j["flask_shards"] = std::max(0, fmax - 3);
    j["picked_items"] = json::array();
    j["secrets_found"] = json::array();
    j["play_time"] = 0.0;
    j["last_shrine"] = j.value("respawn_shrine", "");
    j["game_cleared"] = false;
    j["version"] = 2;
}

bool SaveFromJson(const json& in, SaveData& s, std::string* err) {
    try {
        json j = in;
        int v = j.value("version", 0);
        if (v < 1 || v > kSaveVersion) {
            if (err) *err = "unsupported save version " + std::to_string(v);
            return false;
        }
        if (v == 1) Migrate1To2(j);
        s = SaveData{};
        s.litShrines = j.value("lit_shrines", std::set<std::string>{});
        s.litBraziers = j.value("lit_braziers", std::set<std::string>{});
        s.openedGates = j.value("opened_gates", std::set<std::string>{});
        s.defeatedBosses = j.value("defeated_bosses", std::set<std::string>{});
        s.pickedItems = j.value("picked_items", std::set<std::string>{});
        s.secretsFound = j.value("secrets_found", std::set<std::string>{});
        const json& st = j.at("stats");
        s.stats.vigor = st.value("vigor", 0);
        s.stats.endurance = st.value("endurance", 0);
        s.stats.might = st.value("might", 0);
        s.embers = j.value("embers", 0);
        s.flasks = j.value("flasks", 3);
        s.flaskMax = j.value("flask_max", 3);
        s.flaskShards = j.value("flask_shards", 0);
        if (j.contains("ember_pile")) {
            const json& p = j["ember_pile"];
            s.pile.active = p.value("active", false);
            if (p.contains("pos") && p["pos"].size() == 3) {
                s.pile.x = p["pos"][0].get<float>();
                s.pile.y = p["pos"][1].get<float>();
                s.pile.z = p["pos"][2].get<float>();
            }
            s.pile.amount = p.value("amount", 0);
        }
        s.weapon = j.value("weapon", "sword");
        s.lastShrine = j.value("last_shrine", "");
        s.playTime = j.value("play_time", 0.0);
        s.gameCleared = j.value("game_cleared", false);
        s.version = kSaveVersion;
    } catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
    return true;
}

bool operator==(const SaveData& a, const SaveData& b) {
    return a.litShrines == b.litShrines && a.litBraziers == b.litBraziers && a.openedGates == b.openedGates &&
           a.defeatedBosses == b.defeatedBosses && a.pickedItems == b.pickedItems && a.secretsFound == b.secretsFound &&
           a.stats.vigor == b.stats.vigor && a.stats.endurance == b.stats.endurance && a.stats.might == b.stats.might &&
           a.embers == b.embers && a.flasks == b.flasks && a.flaskMax == b.flaskMax && a.flaskShards == b.flaskShards &&
           a.pile.active == b.pile.active && a.pile.x == b.pile.x && a.pile.y == b.pile.y && a.pile.z == b.pile.z &&
           a.pile.amount == b.pile.amount && a.weapon == b.weapon && a.lastShrine == b.lastShrine &&
           a.gameCleared == b.gameCleared;
}

}  // namespace lk
