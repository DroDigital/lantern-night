#include "engine/data_store.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "engine/log.h"

namespace fs = std::filesystem;

namespace lk {

bool LoadJsonFile(const std::string& path, json& out, std::string* err) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    try {
        out = json::parse(ss.str(), nullptr, true, /*ignore_comments=*/true);
    } catch (const std::exception& e) {
        if (err) *err = path + ": " + e.what();
        return false;
    }
    return true;
}

bool WriteJsonFileAtomic(const std::string& path, const json& j, int indent) {
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << j.dump(indent);
        out.flush();
        if (!out) return false;
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        // Windows cannot rename over an existing file on some filesystems: remove then rename.
        fs::remove(path, ec);
        fs::rename(tmp, path, ec);
    }
    return !ec;
}

bool DataStore::LoadAll(const std::string& dataDir) {
    dir_ = dataDir;
    return Reload();
}

bool DataStore::Reload() {
    std::map<std::string, json> fresh;
    bool ok = true;
    std::error_code ec;
    for (auto& e : fs::recursive_directory_iterator(dir_, ec)) {
        if (!e.is_regular_file() || e.path().extension() != ".json") continue;
        std::string rel = fs::relative(e.path(), dir_).generic_string();
        json j;
        std::string err;
        if (!LoadJsonFile(e.path().string(), j, &err)) {
            LK_ERROR("data: %s", err.c_str());
            ok = false;
            continue;
        }
        fresh[rel] = std::move(j);
    }
    if (ec) {
        LK_ERROR("data: cannot list %s: %s", dir_.c_str(), ec.message().c_str());
        return false;
    }
    if (ok || docs_.empty()) docs_ = std::move(fresh);
    ++version_;
    LK_INFO("data: loaded %zu files (version %d)", docs_.size(), version_);
    return ok;
}

const json& DataStore::Get(const std::string& rel) const {
    static const json kEmpty = json::object();
    auto it = docs_.find(rel);
    if (it == docs_.end()) {
        LK_ERROR("data: missing file %s", rel.c_str());
        return kEmpty;
    }
    return it->second;
}

bool DataStore::Has(const std::string& rel) const { return docs_.count(rel) > 0; }

std::vector<std::string> DataStore::Files() const {
    std::vector<std::string> v;
    for (auto& [k, _] : docs_) v.push_back(k);
    return v;
}

}  // namespace lk
