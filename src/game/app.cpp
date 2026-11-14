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
#include "game/anim_report.h"
#include "game/selftest.h"
#include "game/smoke_bot.h"
#include "game/world/zone_editor.h"
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
    ui_.Init(assets_);
    ctx_ = std::make_unique<Ctx>(Ctx{data_, assets_, renderer_, physics_, input_, audio_, save_, state_, ui_});
    engineReady_ = true;
    return true;
}

void App::ShutdownEngine() {
    if (!engineReady_) return;
    session_.reset();
    ui_.Shutdown();
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
    if (!opt.animReport.empty()) return RunAnimReport();
    if (opt.selfTest) {
        if (!InitEngine(1280, 720, false)) return 3;
        int f = RunCombatSelfTest(*ctx_) + RunEnemySelfTest(*ctx_) + RunWorldSelfTest(*ctx_);
        return f == 0 ? 0 : 1;
    }
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
    ui_.Begin(GetScreenWidth(), GetScreenHeight(), input_.Peek(), input_.LastDevice(), input_.Family());
    if (session_) session_->Render(alpha, GetScreenWidth(), GetScreenHeight());
    ui_.End();
    EndDrawing();
}

int App::RunInteractive() {
    if (!InitEngine(opt_.width, opt_.height, false)) return 3;
    if (save_.Load(state_.progress)) LK_INFO("loaded save from %s", save_.Path().c_str());
    session_ = std::make_unique<Session>(*ctx_);
    if (!session_->Load(opt_.zone)) return 1;
    DisableCursor();
    bool cursorFree = false;
#if defined(LK_DEBUG_TOOLS)
    ZoneEditor editor;
    bool editing = false;
    Camera3D editorCam{};
#endif
    while (!WindowShouldClose() && !session_->WantsQuit()) {
        float frameDt = std::min(GetFrameTime(), 0.25f);
#if defined(LK_DEBUG_TOOLS)
        if (IsKeyPressed(KEY_F2) && opt_.zone == "zone.json") {
            editing = !editing;
            if (editing) {
                ZoneData z;
                std::string err;
                ParseZone(data_.Get("levels/zone.json"), z, err);
                editor.Open(z, session_->PlayerPos());
                EnableCursor();
            } else {
                session_->SetCameraOverride(nullptr);
                session_->SetOverlay(nullptr);
                session_->SetHudVisible(true);
                DisableCursor();
            }
        }
        if (editing) {
            if (editor.Update(frameDt, data_.Get("editor.json"))) {
                Vector3 keep = session_->PlayerPos();
                session_ = std::make_unique<Session>(*ctx_);
                session_->LoadZone(editor.Zone());
                session_->GetPlayer().Teleport(keep, 0.0f);
            }
            if (editor.SaveRequested()) {
                bool ok = WriteJsonFileAtomic(paths::Data("levels/zone.json"), ZoneToJson(editor.Zone()), 1);
                if (ok) data_.Reload();
                editor.SetStatus(ok ? "Saved data/levels/zone.json" : "Save FAILED");
            }
            editorCam = editor.Cam();
            session_->SetCameraOverride(&editorCam);
            session_->SetOverlay([&editor](Renderer& r) { editor.DrawOverlay3D(r); });
            session_->SetHudVisible(false);
            audio_.Update(frameDt);
            BeginDrawing();
            ClearBackground(BLACK);
            ui_.Begin(GetScreenWidth(), GetScreenHeight(), input_.Peek(), input_.LastDevice(), input_.Family());
            session_->Render(1.0f, GetScreenWidth(), GetScreenHeight());
            editor.DrawUi(ui_);
            EndDrawing();
            continue;
        }
#endif
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
        bool wantFree = session_->MenuOpen() || IsWindowFocused() == false;
        if (wantFree != cursorFree) {
            if (wantFree) EnableCursor();
            else DisableCursor();
            cursorFree = wantFree;
        }
        input_.Poll(frameDt, !cursorFree);
        session_->Advance(frameDt);
        audio_.Update(frameDt);
        RenderFrame(session_->Alpha());
    }
    session_->SaveProgress();  // save on quit
    return 0;
}

InputFrame App::CaptureScript(const std::string& scene, int f, int total) {
    InputFrame in;
    const json& shots = data_.Get("capture.json")["shots"];
    if (shots.contains(scene)) {
        if (f == 0) in.look = {shots[scene]["look"][0].get<float>(), shots[scene]["look"][1].get<float>()};
        return in;
    }
    if (scene.rfind("lighting", 0) == 0 || scene == "zone_overview") {
        if (scene == "zone_overview" && f == 0) in.look = {0.0f, 0.25f};
        // Walk toward the far wall, then idle for the last part so the pose settles.
        if (f < total * 2 / 3) in.move = {0.0f, 0.5f};
    } else if (scene == "enemies") {
        if (f < 40) in.move = {0.0f, 0.6f};
        if (f == 41) in.Press(Act::LockOn);
    } else if (scene == "shrine_menu") {
        if (f == 5 || f == 80) in.Press(Act::Interact);
    } else if (scene == "death") {
    } else if (scene == "swing") {
        if (f == 2) in.Press(Act::Attack);
    } else if (scene == "parry") {
        if (f == 65) in.Press(Act::Block);
    } else if (scene == "combat") {
        // Approach the training skeleton, lock on and swing.
        if (f < 78) in.move = {0.0f, 1.0f};
        if (f == 30) in.Press(Act::LockOn);
        if (f == 82 || f == 112 || f == 140) in.Press(Act::Attack);
        if (f > total - 12) in.move = {0.0f, 0.0f};
    } else if (scene == "block") {
        if (f < 40) in.move = {0.0f, 1.0f};
        if (f == 42) in.Press(Act::LockOn);
        if (f > 45) in.Set(Act::Block, true);
    } else if (scene == "hud") {
        if (f < 20) in.move = {0.2f, 0.6f};
    }
    return in;
}

int App::RunCapture() {
    if (!InitEngine(1920, 1080, false)) return 3;
    std::error_code ec;
    fs::create_directories(opt_.captureOut, ec);
    std::vector<std::string> scenes;
    if (opt_.captureScene == "all")
        scenes = {"lighting_full", "lighting_half", "lighting_low", "zone_overview", "combat", "swing", "parry", "hud", "shrine_menu", "death", "enemies"};
    if (opt_.captureScene == "all" || opt_.captureScene == "areas")
        for (auto& [name, shot] : data_.Get("capture.json")["shots"].items()) {
            (void)shot;
            if (opt_.captureScene == "areas") scenes.clear();
            scenes.push_back(name);
        }
    if (opt_.captureScene == "areas") {
        scenes.clear();
        for (auto& [name, shot] : data_.Get("capture.json")["shots"].items()) {
            (void)shot;
            scenes.push_back(name);
        }
    }
    else scenes = {opt_.captureScene};
    input_.SetScripted(true);
    int failures = 0;
    for (const auto& scene : scenes) {
        state_.ResetProgress();
        session_.reset();
        session_ = std::make_unique<Session>(*ctx_);
        std::string zone = opt_.zone;
        if (scene.rfind("lighting", 0) == 0) zone = "lighting_test.json";
        bool training = scene == "combat" || scene == "block" || scene == "hud" || scene == "swing" || scene == "parry";
        if (training) zone = "training.json";
        if (scene == "enemies") zone = "arena.json";
        if (scene == "editor") zone = "zone.json";
        const json& shots = data_.Get("capture.json")["shots"];
        const json* shot = shots.contains(scene) ? &shots[scene] : nullptr;
        if (shot) zone = shot->value("zone", zone);
        if (!session_->Load(zone)) {
            ++failures;
            continue;
        }
        if (scene == "lighting_full") session_->SetFlameFraction(1.0f);
        else if (scene == "lighting_half") session_->SetFlameFraction(0.5f);
        else if (scene == "lighting_low") session_->SetFlameFraction(0.15f);
        else if (scene == "hud") session_->SetFlameFraction(0.62f);
        int frames = opt_.captureFrames;
        if (scene == "shrine_menu" && !session_->Shrines().empty()) {
            const Shrine& sh = session_->Shrines().front();
            session_->GetPlayer().Teleport(Vector3Add(sh.pos, Vector3{-1.1f, 0, 0}), 90.0f);
            session_->Camera().Reset(session_->GetPlayer().Pos(), 90.0f * DEG2RAD + PI);
            state_.progress.embers = 240;
            frames = 140;
        }
#if defined(LK_DEBUG_TOOLS)
        if (scene == "editor") {
            ZoneEditor ed;
            ZoneData z;
            std::string err;
            ParseZone(data_.Get("levels/zone.json"), z, err);
            ed.Open(z, Vector3{20.0f, 2.0f, 34.0f});
            ed.Update(1.0f / 60.0f, data_.Get("editor.json"));
            Camera3D ec = ed.Cam();
            session_->SetCameraOverride(&ec);
            session_->SetOverlay([&ed](Renderer& r) { ed.DrawOverlay3D(r); });
            session_->SetHudVisible(false);
            BeginDrawing();
            ClearBackground(BLACK);
            ui_.Begin(GetScreenWidth(), GetScreenHeight(), input_.Peek(), input_.LastDevice(), input_.Family());
            session_->Render(1.0f, GetScreenWidth(), GetScreenHeight());
            ed.DrawUi(ui_);
            SaveScreenshot((fs::path(opt_.captureOut) / "editor.png").string());
            EndDrawing();
            session_->SetCameraOverride(nullptr);
            session_->SetOverlay(nullptr);
            continue;
        }
#endif
        if (shot) {
            Vector3 p{(*shot)["pos"][0].get<float>(), (*shot)["pos"][1].get<float>(), (*shot)["pos"][2].get<float>()};
            float yaw = shot->value("yaw", 0.0f);
            session_->GetPlayer().Teleport(p, yaw);
            session_->Camera().Reset(p, yaw * DEG2RAD + PI);
            frames = shot->value("frames", frames);
        }
        if (scene == "death") {
            FxQueue fx;
            session_->GetPlayer().Kill(fx);
            frames = 150;
        }
        if ((scene == "swing" || scene == "parry") && !session_->Enemies().empty()) {
            Enemy* d = session_->Enemies()[0].get();
            Vector3 ep = d->Pos();
            // Stand in front of the dummy at a slight angle so the camera sees both.
            session_->GetPlayer().Teleport(Vector3{ep.x + 0.35f, ep.y, ep.z + 0.95f}, 200.0f);
            session_->ForceLock(d->Id());
            session_->Camera().Reset(session_->GetPlayer().Pos(), 200.0f * DEG2RAD + PI);
            if (scene == "parry") d->TriggerAttackSoon();
            frames = scene == "swing" ? 19 : 73;
        }
        if (scene == "combat") frames = std::max(frames, 158);
        if (scene == "block") frames = std::max(frames, 262);
        for (int f = 0; f < frames; ++f) {
            input_.PushScripted(CaptureScript(scene, f, frames));
            session_->Advance(1.0f / 60.0f);
            if (opt_.verbose && scene == "combat" && f >= 80 && f % 3 == 0) {
                Player& pl = session_->GetPlayer();
                Vector3 b, t;
                pl.WeaponPoints(b, t);
                LK_INFO("dbg f%d state %s anim t active %d tip (%.2f %.2f %.2f) base (%.2f %.2f %.2f)", f, PStateName(pl.State()),
                        int(pl.AttackActive()), t.x, t.y, t.z, b.x, b.y, b.z);
            }
            if (opt_.verbose && f % 20 == 0) {
                Vector3 p = session_->PlayerPos();
                LK_INFO("capture %s frame %d player (%.2f, %.2f, %.2f)", scene.c_str(), f, p.x, p.y, p.z);
            }
            BeginDrawing();
            ClearBackground(BLACK);
            ui_.Begin(GetScreenWidth(), GetScreenHeight(), input_.Peek(), input_.LastDevice(), input_.Family());
            session_->Render(session_->Alpha(), GetScreenWidth(), GetScreenHeight());
            if (f == frames - 1 && opt_.verbose)
                for (auto& e : session_->Enemies())
                    LK_INFO("capture %s: enemy %s hp %.1f posture %.1f state %s | player flame %.1f stamina %.1f state %s", scene.c_str(),
                            e->Def().name.c_str(), e->Hp(), e->GetPosture().value, EStateName(e->State()),
                            session_->GetPlayer().flame, session_->GetPlayer().stamina.value, PStateName(session_->GetPlayer().State()));
            if (f == frames - 1)
                if (!SaveScreenshot((fs::path(opt_.captureOut) / (scene + ".png")).string())) ++failures;
            EndDrawing();
        }
    }
    return failures == 0 && log::ErrorCount() == 0 ? 0 : 1;
}

int App::RunAnimReport() {
    if (!InitEngine(640, 360, false)) return 3;
    return WriteAnimReport(*ctx_, opt_.animReport);
}

int App::PreloadEverything() {
    int before = assets_.FailedCount();
    int models = 0, textures = 0, sounds = 0;
    for (auto& e : fs::recursive_directory_iterator(paths::AssetDir())) {
        if (!e.is_regular_file()) continue;
        std::string rel = fs::relative(e.path(), paths::AssetDir()).generic_string();
        std::string ext = e.path().extension().string();
        if (ext == ".glb" || ext == ".gltf") {
            assets_.LoadModel(rel);
            ++models;
        } else if (ext == ".png" && rel.rfind("quaternius_props/", 0) != 0 && rel.rfind("kaykit", 0) != 0) {
            assets_.LoadTexture(rel, false);
            ++textures;
        } else if (ext == ".ogg" || ext == ".wav") {
            // Decode without needing an audio device (CI has none).
            Wave w = LoadWave(e.path().string().c_str());
            if (w.frameCount == 0) {
                LK_ERROR("smoke: cannot decode %s", rel.c_str());
            }
            UnloadWave(w);
            ++sounds;
        }
    }
    LK_INFO("smoke: loaded %d models, %d textures, decoded %d sounds", models, textures, sounds);
    return assets_.FailedCount() - before;
}

int App::RunSmoke() {
    if (!InitEngine(1280, 720, false)) return 3;
    if (RunCombatSelfTest(*ctx_) + RunEnemySelfTest(*ctx_) + RunWorldSelfTest(*ctx_) != 0) {
        LK_ERROR("smoke: self-test failed");
        return 1;
    }
    if (PreloadEverything() != 0) return 1;
    if (ValidateContent() != 0) return 1;

    state_.ResetProgress();
    state_.godMode = true;
    save_.Delete();
    input_.SetScripted(true);
    int totalSteps = 0;
    auto newSession = [&]() -> bool {
        session_.reset();
        session_ = std::make_unique<Session>(*ctx_);
        return session_->Load(opt_.zone);
    };
    if (!newSession()) return 1;

    SmokeBot bot;
    std::vector<SmokeBot::Goal> plan = {SmokeBot::Goal::Kill, SmokeBot::Goal::Rest, SmokeBot::Goal::Wander};
    size_t planIdx = 0;
    bot.Start(plan[0]);
    bool reloaded = false;
    int guard = 0;
    while (totalSteps < 3600 && guard++ < 40000) {
        bool done = false, failed = false;
        InputFrame in = bot.Think(*session_, done, failed);
        if (failed) {
            LK_ERROR("smoke: bot failed: %s", bot.Status().c_str());
            return 1;
        }
        if (done) {
            LK_INFO("smoke: %s (step %d)", bot.Status().c_str(), totalSteps);
            if (bot.Current() == SmokeBot::Goal::Rest && !reloaded) {
                // Save, then reload from disk and compare.
                if (!session_->SaveProgress()) return 1;
                SaveData disk;
                if (!save_.Load(disk)) {
                    LK_ERROR("smoke: save could not be read back");
                    return 1;
                }
                if (!(disk == state_.progress)) {
                    LK_ERROR("smoke: save mismatch after reload");
                    return 1;
                }
                SaveData before = state_.progress;
                state_.progress = disk;
                totalSteps += session_->StepCount();
                if (!newSession()) return 1;
                if (!state_.progress.litShrines.count(before.lastShrine) || state_.progress.embers != before.embers) {
                    LK_ERROR("smoke: progress lost on reload");
                    return 1;
                }
                LK_INFO("smoke: save round-trip OK (%zu shrines lit, %d embers)", state_.progress.litShrines.size(),
                        state_.progress.embers);
                reloaded = true;
                totalSteps -= session_->StepCount();
            }
            if (planIdx + 1 < plan.size()) bot.Start(plan[++planIdx]);
        }
        input_.PushScripted(in);
        int before = session_->StepCount();
        session_->Advance(1.0f / 60.0f);
        totalSteps += session_->StepCount() - before;
        Vector3 p = session_->PlayerPos();
        if (std::isnan(p.x) || std::isnan(p.y) || std::isnan(p.z)) {
            LK_ERROR("smoke: NaN player position at step %d", totalSteps);
            return 1;
        }
        for (auto& e : session_->Enemies()) {
            Vector3 q = e->Pos();
            if (std::isnan(q.x) || std::isnan(q.y) || std::isnan(q.z)) {
                LK_ERROR("smoke: NaN enemy position at step %d", totalSteps);
                return 1;
            }
        }
        if (p.y < -20.0f) {
            LK_ERROR("smoke: player fell out of the world at step %d", totalSteps);
            return 1;
        }
        if (totalSteps % 30 == 0 || session_->MenuOpen()) RenderFrame(1.0f);
    }
    if (!reloaded) {
        LK_ERROR("smoke: the scripted path never reached the save/reload step");
        return 1;
    }
    if (log::ErrorCount() > 0) {
        LK_ERROR("smoke: %d errors (last: %s)", log::ErrorCount(), log::LastError().c_str());
        return 1;
    }
    LK_INFO("smoke: OK (%d steps, %d kills)", totalSteps, session_->Kills());
    return 0;
}

}  // namespace lk
