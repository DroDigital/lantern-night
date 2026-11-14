#pragma once
// Application shell: window, engine systems, the fixed 60 Hz simulation loop with interpolated
// rendering (hitstop scales the simulation clock), and the special run modes
// (--validate, --smoke-test, --capture).
#include <memory>
#include <string>
#include <vector>

#include "engine/assets.h"
#include "engine/data_store.h"
#include "engine/input.h"
#include "engine/physics.h"
#include "engine/renderer.h"
#include "engine/ui.h"
#include "game/context.h"

namespace lk {

class AudioManager;
class SaveSystem;
struct GameState;
class Session;

struct AppOptions {
    bool smokeTest = false;
    bool validate = false;
    bool verbose = false;
    std::string captureScene;     // "" = interactive
    int captureFrames = 90;
    std::string captureOut = "shots";
    int width = 1600, height = 900;
    bool windowedOverride = false;
    std::string zone = "zone.json";
    std::string userDir;          // override user data folder
    std::string animReport;       // --anim-report <out.md>
    bool selfTest = false;
};


class App {
public:
    App(GameState& state, SaveSystem& save, AudioManager& audio);
    ~App();
    int Run(const AppOptions& opt);

private:
    bool InitEngine(int w, int h, bool hidden);
    void ShutdownEngine();
    int RunInteractive();
    int RunCapture();
    int RunSmoke();
    int RunAnimReport();
    int PreloadEverything();
    bool SaveScreenshot(const std::string& path);
    void RenderFrame(float alpha);
    InputFrame CaptureScript(const std::string& scene, int frame, int total);

    GameState& state_;
    SaveSystem& save_;
    AudioManager& audio_;
    AppOptions opt_;

    DataStore data_;
    AssetCache assets_;
    Renderer renderer_;
    Physics physics_;
    Input input_;
    Ui ui_;
    std::unique_ptr<Ctx> ctx_;
    std::unique_ptr<Session> session_;
    bool engineReady_ = false;
};

}  // namespace lk
