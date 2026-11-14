#include "game/app.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "engine/audio.h"
#include "engine/gltf_inspect.h"
#include "engine/log.h"
#include "engine/paths.h"
#include "game/game_state.h"
#include "game/save_system.h"
#include "game/session.h"
#include "game/validate.h"
#include "rlgl.h"

namespace fs = std::filesystem;

namespace lk {

App::App(GameState& state, SaveSystem& save, AudioManager& audio) : state_(state), save_(save), audio_(audio) {}

App::~App() { ShutdownEngine(); }

bool App::InitEngine(int w, int h, bool hidden) {
    unsigned flags = FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE;
    if (!opt_.smokeTest && opt_.captureScene.empty()) flags |= FLAG_VSYNC_HINT;
    if (hidden) flags |= FLAG_WINDOW_HIDDEN;
    SetConfigFlags(flags);
    std::string title = std::string("Lantern Knight ") + LK_VERSION;
    InitWindow(w, h, title.c_str());
    if (!IsWindowReady()) {
        LK_ERROR("could not create a window / OpenGL 3.3 context");
        return false;
    }
    SetExitKey(KEY_NULL);
    Image icon = LoadImage(paths::Asset("icon.png").c_str());
    if (icon.data) {
        SetWindowIcon(icon);
        UnloadImage(icon);
    }
    if (!data_.LoadAll(paths::DataDir())) return false;
    if (!renderer_.Init(GetRenderWidth(), GetRenderHeight())) return false;
    assets_.SetDefaultShaders(renderer_.StaticShader(), renderer_.SkinnedShader());
    physics_.Init();
    if (!opt_.smokeTest && opt_.captureScene.empty()) audio_.Init(data_.Get("audio.json"));
    else if (opt_.smokeTest) audio_.Init(data_.Get("audio.json"));  // still load every sound
    ctx_ = std::make_unique<Ctx>(Ctx{data_, assets_, renderer_, physics_, input_, audio_, save_, state_});
    engineReady_ = true;
    return true;
}

void App::ShutdownEngine() {
    if (!engineReady_) return;
    session_.reset();
    physics_.Shutdown();
    assets_.UnloadAll();
    renderer_.Shutdown();
    audio_.Shutdown();
    CloseWindow();
    engineReady_ = false;
}

int App::Run(const AppOptions& opt) {
    opt_ = opt;
    if (!opt.userDir.empty()) paths::SetUserDir(opt.userDir);
    if (!opt.captureScene.empty()) return RunCapture();
    if (opt.smokeTest) return RunSmoke();
    return RunInteractive();
}

bool App::SaveScreenshot(const std::string& path) {
    rlDrawRenderBatchActive();
    Image img = LoadImageFromScreen();
    bool ok = img.data && ExportImage(img, path.c_str());
    UnloadImage(img);
    if (ok) LK_INFO("capture: wrote %s", path.c_str());
    else LK_ERROR("capture: failed to write %s", path.c_str());
    return ok;
}

void App::RenderFrame(float alpha) {
    BeginDrawing();
    ClearBackground(BLACK);
    if (session_) session_->Render(alpha, GetScreenWidth(), GetScreenHeight());
    EndDrawing();
}

int App::RunInteractive() {
    if (!InitEngine(opt_.width, opt_.height, false)) return 3;
    session_ = std::make_unique<Session>(*ctx_);
    if (!session_->Load(opt_.zone)) return 1;
    const double step = 1.0 / 60.0;
    while (!WindowShouldClose()) {
        float frameDt = std::min(GetFrameTime(), 0.25f);
#if defined(LK_DEBUG_TOOLS)
        if (IsKeyPressed(KEY_F5)) {
            data_.Reload();
            renderer_.ReloadShaders();
            assets_.SetDefaultShaders(renderer_.StaticShader(), renderer_.SkinnedShader());
            assets_.ApplyShaders();
            session_->ReloadTunables();
        }
#endif
        if (IsWindowResized()) renderer_.Resize(GetRenderWidth(), GetRenderHeight());
        input_.Poll(frameDt, true);
        accumulator_ += frameDt;
        while (accumulator_ >= step) {
            session_->Step(input_.ConsumeStep(), float(step));
            accumulator_ -= step;
        }
        audio_.Update(frameDt);
        RenderFrame(float(accumulator_ / step));
    }
    return 0;
}

int App::RunCapture() {
    if (!InitEngine(1920, 1080, false)) return 3;
    std::error_code ec;
    fs::create_directories(opt_.captureOut, ec);
    std::vector<std::string> scenes;
    if (opt_.captureScene == "all") scenes = {"lighting_full", "lighting_half", "lighting_low", "zone_overview"};
    else scenes = {opt_.captureScene};
    input_.SetScripted(true);
    int failures = 0;
    for (const auto& scene : scenes) {
        session_ = std::make_unique<Session>(*ctx_);
        std::string zone = scene.rfind("lighting", 0) == 0 ? "lighting_test.json" : opt_.zone;
        if (!session_->Load(zone)) {
            ++failures;
            continue;
        }
        if (scene == "lighting_full") session_->SetFlameFraction(1.0f);
        else if (scene == "lighting_half") session_->SetFlameFraction(0.5f);
        else if (scene == "lighting_low") session_->SetFlameFraction(0.15f);
        for (int f = 0; f < opt_.captureFrames; ++f) {
            InputFrame in;
            if (scene == "zone_overview" && f == 0) in.look = {0.0f, 0.25f};
            // Walk toward the far wall, then idle for the last part so the pose settles.
            if (f < opt_.captureFrames * 2 / 3) in.move = {0.0f, 0.5f};
            input_.PushScripted(in);
            session_->Step(input_.ConsumeStep(), 1.0f / 60.0f);
            if (opt_.verbose && f % 20 == 0) {
                Vector3 p = session_->PlayerPos();
                LK_INFO("capture %s frame %d player (%.2f, %.2f, %.2f)", scene.c_str(), f, p.x, p.y, p.z);
            }
            BeginDrawing();
            ClearBackground(BLACK);
            session_->Render(1.0f, GetScreenWidth(), GetScreenHeight());
            if (f == opt_.captureFrames - 1)
                if (!SaveScreenshot((fs::path(opt_.captureOut) / (scene + ".png")).string())) ++failures;
            EndDrawing();
        }
    }
    return failures == 0 && log::ErrorCount() == 0 ? 0 : 1;
}

int App::RunSmoke() {
    if (!InitEngine(1280, 720, false)) return 3;
    session_ = std::make_unique<Session>(*ctx_);
    if (!session_->Load(opt_.zone)) return 1;
    input_.SetScripted(true);
    for (int i = 0; i < 3600; ++i) {
        InputFrame in;
        in.move = {0.3f * std::sin(float(i) * 0.01f), 0.8f};
        input_.PushScripted(in);
        session_->Step(input_.ConsumeStep(), 1.0f / 60.0f);
        Vector3 p = session_->PlayerPos();
        if (std::isnan(p.x) || std::isnan(p.y) || std::isnan(p.z)) {
            LK_ERROR("smoke: NaN player position at step %d", i);
            return 1;
        }
        if (i % 30 == 0) RenderFrame(1.0f);
    }
    if (log::ErrorCount() > 0) {
        LK_ERROR("smoke: %d errors (last: %s)", log::ErrorCount(), log::LastError().c_str());
        return 1;
    }
    LK_INFO("smoke: OK");
    return 0;
}

}  // namespace lk
