#include "game/save_system.h"

#include <filesystem>

#include "engine/data_store.h"
#include "engine/log.h"
#include "engine/paths.h"

namespace fs = std::filesystem;

namespace lk {

std::string SaveSystem::Path() const { return paths::User(fileName_); }

bool SaveSystem::Exists() const {
    std::error_code ec;
    return fs::exists(Path(), ec) || fs::exists(Path() + ".bak", ec);
}

void SaveSystem::Delete() {
    std::error_code ec;
    fs::remove(Path(), ec);
    fs::remove(Path() + ".bak", ec);
}

bool SaveSystem::Save(const SaveData& data) {
    std::string path = Path();
    std::error_code ec;
    if (fs::exists(path, ec)) fs::copy_file(path, path + ".bak", fs::copy_options::overwrite_existing, ec);
    if (!WriteJsonFileAtomic(path, SaveToJson(data))) {
        LK_ERROR("save: failed to write %s", path.c_str());
        return false;
    }
    ++saves_;
    LK_INFO("save: wrote %s", path.c_str());
    return true;
}

bool SaveSystem::Load(SaveData& out) {
    for (const std::string& p : {Path(), Path() + ".bak"}) {
        json j;
        std::string err;
        std::error_code ec;
        if (!fs::exists(p, ec)) continue;
        if (!LoadJsonFile(p, j, &err)) {
            LK_WARN("save: %s unreadable (%s)", p.c_str(), err.c_str());
            continue;
        }
        if (!SaveFromJson(j, out, &err)) {
            LK_WARN("save: %s invalid (%s)", p.c_str(), err.c_str());
            continue;
        }
        return true;
    }
    return false;
}

}  // namespace lk
