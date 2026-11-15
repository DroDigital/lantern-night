#include "game/settings.h"

#include <algorithm>

#include "engine/audio.h"
#include "engine/data_store.h"
#include "engine/input.h"
#include "engine/log.h"
#include "engine/paths.h"
#include "engine/renderer.h"
#include "raylib.h"

namespace lk {

const Resolution kResolutions[] = {{1280, 720, "1280 x 720"}, {1600, 900, "1600 x 900"}, {1920, 1080, "1920 x 1080"},
                                   {2560, 1440, "2560 x 1440"}};
const int kResolutionCount = int(sizeof(kResolutions) / sizeof(kResolutions[0]));

bool Settings::Load() {
    json j;
    if (!LoadJsonFile(paths::User("settings.json"), j)) return false;
    master = std::clamp(j.value("master", master), 0.0f, 1.0f);
    music = std::clamp(j.value("music", music), 0.0f, 1.0f);
    sfx = std::clamp(j.value("sfx", sfx), 0.0f, 1.0f);
    cameraShake = j.value("camera_shake", cameraShake);
    sensitivity = std::clamp(j.value("sensitivity", sensitivity), 0.2f, 3.0f);
    invertX = j.value("invert_x", invertX);
    invertY = j.value("invert_y", invertY);
    fullscreen = j.value("fullscreen", fullscreen);
    resolution = std::clamp(j.value("resolution", resolution), 0, kResolutionCount - 1);
    vsync = j.value("vsync", vsync);
    lanternShadows = j.value("lantern_shadows", lanternShadows);
    return true;
}

bool Settings::Save() const {
    json j = {{"version", 1},          {"master", master},       {"music", music},        {"sfx", sfx},
              {"camera_shake", cameraShake}, {"sensitivity", sensitivity}, {"invert_x", invertX}, {"invert_y", invertY},
              {"fullscreen", fullscreen}, {"resolution", resolution}, {"vsync", vsync},     {"lantern_shadows", lanternShadows}};
    bool ok = WriteJsonFileAtomic(paths::User("settings.json"), j);
    if (!ok) LK_WARN("settings: could not write %s", paths::User("settings.json").c_str());
    return ok;
}

void Settings::ApplyRuntime(AudioManager& audio, Input& input, Renderer& renderer) const {
    audio.SetVolumes(master, music, sfx);
    input.Settings().sensitivity = sensitivity;
    input.Settings().invertX = invertX;
    input.Settings().invertY = invertY;
    renderer.Settings().lanternShadows = lanternShadows;
}

void Settings::ApplyWindow() const {
    if (!IsWindowReady()) return;
    if (vsync) SetWindowState(FLAG_VSYNC_HINT);
    else ClearWindowState(FLAG_VSYNC_HINT);
    bool isFull = IsWindowFullscreen() || IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    if (fullscreen && !isFull) {
        ToggleBorderlessWindowed();
    } else if (!fullscreen && isFull) {
        ToggleBorderlessWindowed();
    }
    if (!fullscreen) {
        const Resolution& r = kResolutions[std::clamp(resolution, 0, kResolutionCount - 1)];
        int mon = GetCurrentMonitor();
        int mw = GetMonitorWidth(mon), mh = GetMonitorHeight(mon);
        int w = std::min(r.w, mw > 0 ? mw : r.w), h = std::min(r.h, mh > 0 ? mh : r.h);
        if (GetScreenWidth() != w || GetScreenHeight() != h) {
            SetWindowSize(w, h);
            if (mw > 0) SetWindowPosition((mw - w) / 2, (mh - h) / 2);
        }
    }
}

}  // namespace lk
