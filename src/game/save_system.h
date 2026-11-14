#pragma once
// Writes save.json to the user data folder: temp file + rename so a crash cannot corrupt it,
// and keeps one backup (save.json.bak) of the previous save.
#include <string>

#include "core/save_data.h"

namespace lk {

class SaveSystem {
public:
    explicit SaveSystem(std::string fileName = "save.json") : fileName_(std::move(fileName)) {}
    bool Save(const SaveData& data);
    // Loads save.json, falling back to the backup if the main file is unreadable.
    bool Load(SaveData& out);
    bool Exists() const;
    void Delete();
    std::string Path() const;
    int SaveCount() const { return saves_; }

private:
    std::string fileName_;
    int saves_ = 0;
};

}  // namespace lk
