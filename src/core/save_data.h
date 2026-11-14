#pragma once
// Persistent progress. Serialised to save.json in the user data folder.
// Version history:
//   1: initial (shrines, braziers, gates, bosses, stats, embers, flasks, pile, weapon)
//   2: + flask shards collected, items picked up, secrets found, play time, last shrine id
#include <set>
#include <string>

#include <nlohmann/json.hpp>

namespace lk {

constexpr int kSaveVersion = 2;

struct Stats {
    int vigor = 0;
    int endurance = 0;
    int might = 0;
    int Level() const { return vigor + endurance + might; }
};

struct EmberPile {
    bool active = false;
    float x = 0, y = 0, z = 0;
    int amount = 0;
};

struct SaveData {
    int version = kSaveVersion;
    std::set<std::string> litShrines;
    std::set<std::string> litBraziers;
    std::set<std::string> openedGates;
    std::set<std::string> defeatedBosses;
    std::set<std::string> pickedItems;     // flask shards, greataxe, rewards
    std::set<std::string> secretsFound;
    Stats stats;
    int embers = 0;
    int flasks = 3;
    int flaskMax = 3;
    int flaskShards = 0;
    EmberPile pile;
    std::string weapon = "sword";
    std::string lastShrine;                // respawn point ("" = zone start)
    double playTime = 0.0;
    bool gameCleared = false;
};

nlohmann::json SaveToJson(const SaveData& s);
// Accepts any older version and migrates it forward. Returns false on unusable data.
bool SaveFromJson(const nlohmann::json& j, SaveData& out, std::string* err = nullptr);
bool operator==(const SaveData& a, const SaveData& b);

}  // namespace lk
