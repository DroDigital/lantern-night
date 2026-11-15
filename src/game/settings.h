#pragma once
// Player settings, persisted to settings.json in the per-user data folder.
#include <string>

namespace lk {

class AudioManager;
class Input;
class Renderer;

struct Settings {
    float master = 0.8f, music = 0.6f, sfx = 0.8f;
    bool cameraShake = true;
    float sensitivity = 1.0f;
    bool invertX = false, invertY = false;
    bool fullscreen = false;
    int resolution = 1;        // index into kResolutions
    bool vsync = true;
    bool lanternShadows = false;

    bool Load();               // false if missing/unreadable (defaults kept)
    bool Save() const;
    // Push audio volumes, input sensitivity/inversion and the shadow toggle to the systems.
    void ApplyRuntime(AudioManager& audio, Input& input, Renderer& renderer) const;
    // Window mode, resolution and vsync (needs a window).
    void ApplyWindow() const;
};

struct Resolution {
    int w, h;
    const char* label;
};
extern const Resolution kResolutions[];
extern const int kResolutionCount;

}  // namespace lk
