#pragma once
// Where things live on disk.
//  - Read-only content (assets/, data/) is found relative to the executable
//    (Contents/Resources inside a macOS .app), falling back to parent folders for dev builds.
//  - Settings and saves go to the per-user data folder; never next to the executable.
#include <string>

namespace lk::paths {

// Call once at startup with argv[0]. Returns false if assets/ or data/ could not be found.
bool Init(const char* argv0);

const std::string& ContentRoot();  // folder containing assets/ and data/
std::string Asset(const std::string& rel);  // <root>/assets/<rel>
std::string Data(const std::string& rel);   // <root>/data/<rel>
const std::string& DataDir();               // <root>/data
const std::string& AssetDir();              // <root>/assets

// %APPDATA%\LanternKnight, ~/Library/Application Support/LanternKnight,
// $XDG_DATA_HOME/lantern-knight or ~/.local/share/lantern-knight. Created on demand.
const std::string& UserDir();
std::string User(const std::string& rel);
// Override the user folder (tests / smoke test use a temp folder).
void SetUserDir(const std::string& dir);

}  // namespace lk::paths
