#pragma once
// Every tunable number lives in JSON under data/. The DataStore loads all of it at startup
// and reloads on demand (F5 in debug builds). Systems re-read their typed structs when
// Version() changes.
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace lk {

using json = nlohmann::json;

class DataStore {
public:
    // Loads every *.json under dataDir. Returns false (and logs) on any parse error.
    bool LoadAll(const std::string& dataDir);
    bool Reload();

    // Path relative to data/, with forward slashes, e.g. "player/stats.json".
    const json& Get(const std::string& rel) const;
    bool Has(const std::string& rel) const;
    std::vector<std::string> Files() const;
    int Version() const { return version_; }

private:
    std::string dir_;
    std::map<std::string, json> docs_;
    int version_ = 0;
};

// Helpers that log a clear message when a required field is missing.
bool LoadJsonFile(const std::string& path, json& out, std::string* err = nullptr);
bool WriteJsonFileAtomic(const std::string& path, const json& j, int indent = 2);

}  // namespace lk
