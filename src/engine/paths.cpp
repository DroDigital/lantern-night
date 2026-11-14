#include "engine/paths.h"

#include <cstdlib>
#include <filesystem>
#include <vector>

#include "engine/log.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace lk::paths {
namespace {
std::string g_root;
std::string g_assets;
std::string g_data;
std::string g_user;

fs::path ExecutablePath(const char* argv0) {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, DWORD(sizeof buf / sizeof buf[0]));
    if (n > 0) return fs::path(std::wstring(buf, n));
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) == 0) return fs::weakly_canonical(fs::path(buf));
#else
    std::error_code ec;
    auto p = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return p;
#endif
    std::error_code ec2;
    return fs::absolute(fs::path(argv0 ? argv0 : "."), ec2);
}

bool HasContent(const fs::path& dir) {
    std::error_code ec;
    return fs::is_directory(dir / "assets", ec) && fs::is_directory(dir / "data", ec);
}
}  // namespace

bool Init(const char* argv0) {
    fs::path exeDir = ExecutablePath(argv0).parent_path();
    std::vector<fs::path> candidates;
    candidates.push_back(exeDir);
    candidates.push_back(exeDir / ".." / "Resources");  // macOS bundle
    // Dev builds: walk up from build/<preset>/bin (and from inside a dev .app).
    fs::path p = exeDir;
    for (int i = 0; i < 8 && p.has_parent_path() && p != p.parent_path(); ++i) {
        p = p.parent_path();
        candidates.push_back(p);
    }
    for (auto& c : candidates) {
        if (HasContent(c)) {
            std::error_code ec;
            g_root = fs::weakly_canonical(c, ec).string();
            g_assets = (fs::path(g_root) / "assets").string();
            g_data = (fs::path(g_root) / "data").string();
            LK_INFO("Content root: %s", g_root.c_str());
            return true;
        }
    }
    LK_ERROR("Could not find assets/ and data/ near %s", exeDir.string().c_str());
    return false;
}

const std::string& ContentRoot() { return g_root; }
const std::string& AssetDir() { return g_assets; }
const std::string& DataDir() { return g_data; }
std::string Asset(const std::string& rel) { return (fs::path(g_assets) / rel).string(); }
std::string Data(const std::string& rel) { return (fs::path(g_data) / rel).string(); }

const std::string& UserDir() {
    if (g_user.empty()) {
        fs::path base;
#if defined(_WIN32)
        const char* appdata = std::getenv("APPDATA");
        base = fs::path(appdata ? appdata : ".") / "LanternKnight";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        base = fs::path(home ? home : ".") / "Library" / "Application Support" / "LanternKnight";
#else
        const char* xdg = std::getenv("XDG_DATA_HOME");
        if (xdg && *xdg) base = fs::path(xdg) / "lantern-knight";
        else {
            const char* home = std::getenv("HOME");
            base = fs::path(home ? home : ".") / ".local" / "share" / "lantern-knight";
        }
#endif
        g_user = base.string();
    }
    std::error_code ec;
    fs::create_directories(g_user, ec);
    return g_user;
}

std::string User(const std::string& rel) { return (fs::path(UserDir()) / rel).string(); }

void SetUserDir(const std::string& dir) { g_user = dir; }

}  // namespace lk::paths
