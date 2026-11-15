#include "game/ui/menus.h"

#include <algorithm>
#include <cmath>

#include "core/combat_rules.h"
#include "game/settings.h"

namespace lk {

MenuAction DrawShrineMenu(Ui& ui, ShrineMenuState& st, const SaveData& save, const json& lv, bool hasGreataxe,
                          const std::string& weaponName) {
    MenuAction act = MenuAction::None;
    float w = 640, x = 120, y = ui.H() * 0.5f - 300;
    ui.Panel({x, y, w, 600});
    ui.Text(st.shrineName.empty() ? "Lantern Shrine" : st.shrineName, x + 40, y + 34, 44, Color{255, 210, 150, 255});
    ui.Text("Embers  " + std::to_string(save.embers), x + 40, y + 96, 28, Color{230, 200, 160, 255});
    if (st.page == ShrineMenuState::Page::Main) {
        float by = y + 160;
        if (ui.Button("Level up", {x + 40, by, w - 80, 72})) {
            st.page = ShrineMenuState::Page::LevelUp;
            ui.ResetFocus();
        }
        by += 90;
        std::string swap = hasGreataxe ? "Swap weapon (" + weaponName + ")" : "Swap weapon";
        if (ui.Button(swap, {x + 40, by, w - 80, 72}, hasGreataxe)) act = MenuAction::SwapWeapon;
        by += 90;
        if (ui.Button("Leave", {x + 40, by, w - 80, 72}) || ui.BackPressed()) act = MenuAction::Close;
        ui.Text("Resting refills your flame and flasks.", x + 40, y + 540, 22, Color{170, 165, 160, 255});
        ui.Text("The dead rise again.", x + 40, y + 566, 22, Color{170, 165, 160, 255});
    } else {
        int level = save.stats.Level();
        int cost = LevelUpCost(level, lv.value("cost_base", 50.0f), lv.value("cost_growth", 1.15f));
        ui.Text("Level " + std::to_string(level + 1) + "   next costs " + std::to_string(cost), x + 40, y + 140, 28,
                Color{230, 225, 215, 255});
        bool afford = save.embers >= cost;
        float by = y + 200;
        const json& stats = lv.value("stats", json::object());
        auto row = [&](const char* key, int value, MenuAction a) {
            const json& s = stats.value(key, json::object());
            std::string label = s.value("label", std::string(key)) + "  " + std::to_string(value) + "   " + s.value("desc", "");
            if (ui.Button(label, {x + 40, by, w - 80, 72}, afford)) act = a;
            by += 88;
        };
        row("vigor", save.stats.vigor, MenuAction::LevelVigor);
        row("endurance", save.stats.endurance, MenuAction::LevelEndurance);
        row("might", save.stats.might, MenuAction::LevelMight);
        if (ui.Button("Back", {x + 40, by + 10, w - 80, 64}) || ui.BackPressed()) {
            st.page = ShrineMenuState::Page::Main;
            ui.ResetFocus();
        }
    }
    return act;
}

MenuAction DrawPauseMenu(Ui& ui) {
    MenuAction act = MenuAction::None;
    ui.Rect({0, 0, ui.W(), ui.H()}, Color{0, 0, 0, 140});
    float w = 520, x = (ui.W() - w) * 0.5f, y = ui.H() * 0.5f - 230;
    ui.Panel({x, y, w, 440});
    ui.Text("Paused", ui.W() * 0.5f, y + 36, 48, Color{255, 210, 150, 255}, Align::Center);
    float by = y + 120;
    if (ui.Button("Resume", {x + 40, by, w - 80, 70}) || ui.BackPressed()) act = MenuAction::Resume;
    by += 88;
    if (ui.Button("Settings", {x + 40, by, w - 80, 70})) act = MenuAction::Settings;
    by += 88;
    if (ui.Button("Save and quit", {x + 40, by, w - 80, 70})) act = MenuAction::QuitToDesktop;
    return act;
}

MenuAction DrawSettingsMenu(Ui& ui, Settings& s, bool& changed) {
    MenuAction act = MenuAction::None;
    ui.Rect({0, 0, ui.W(), ui.H()}, Color{0, 0, 0, 150});
    float w = 900, x = (ui.W() - w) * 0.5f, y = ui.H() * 0.5f - 470;
    ui.Panel({x, y, w, 940});
    ui.Text("Settings", ui.W() * 0.5f, y + 30, 48, Color{255, 210, 150, 255}, Align::Center);
    float ry = y + 110, rh = 62, gap = 68;
    auto pct = [](float v) { return std::to_string(int(std::lround(v * 100))) + "%"; };
    auto vol = [&](const char* label, float& v) {
        int d = ui.Stepper(label, pct(v), {x + 40, ry, w - 80, rh});
        if (d) {
            v = std::clamp(v + 0.1f * float(d), 0.0f, 1.0f);
            changed = true;
        }
        ry += gap;
    };
    auto toggle = [&](const char* label, bool& v) {
        if (ui.Stepper(label, v ? "On" : "Off", {x + 40, ry, w - 80, rh})) {
            v = !v;
            changed = true;
        }
        ry += gap;
    };
    vol("Master volume", s.master);
    vol("Music volume", s.music);
    vol("Effects volume", s.sfx);
    toggle("Camera shake", s.cameraShake);
    {
        char buf[16];
        std::snprintf(buf, sizeof buf, "%.1f", s.sensitivity);
        int d = ui.Stepper("Camera sensitivity", buf, {x + 40, ry, w - 80, rh});
        if (d) {
            s.sensitivity = std::clamp(s.sensitivity + 0.1f * float(d), 0.2f, 3.0f);
            changed = true;
        }
        ry += gap;
    }
    toggle("Invert camera X", s.invertX);
    toggle("Invert camera Y", s.invertY);
    {
        if (ui.Stepper("Display", s.fullscreen ? "Fullscreen" : "Windowed", {x + 40, ry, w - 80, rh})) {
            s.fullscreen = !s.fullscreen;
            changed = true;
        }
        ry += gap;
    }
    {
        int d = ui.Stepper("Resolution", kResolutions[s.resolution].label, {x + 40, ry, w - 80, rh});
        if (d) {
            s.resolution = (s.resolution + d + kResolutionCount) % kResolutionCount;
            changed = true;
        }
        ry += gap;
    }
    toggle("Vertical sync", s.vsync);
    toggle("Lantern shadows", s.lanternShadows);
    if (ui.Button("Back", {x + 40, ry + 6, w - 80, 64}) || ui.BackPressed()) act = MenuAction::Close;
    return act;
}

MenuAction DrawTitleMenu(Ui& ui, bool hasSave, const std::string& version, float time) {
    MenuAction act = MenuAction::None;
    float cx = ui.W() * 0.5f;
    float glow = 0.85f + 0.15f * std::sin(time * 2.0f);
    ui.Text("LANTERN KNIGHT", cx, ui.H() * 0.18f, 110, Color{255, (unsigned char)(170 * glow + 40), 90, 255}, Align::Center);
    ui.Text("a small knight, a dark crypt, a lantern that is your life", cx, ui.H() * 0.18f + 130, 28, Color{220, 210, 200, 220},
            Align::Center);
    float w = 520, x = cx - w * 0.5f, y = ui.H() * 0.50f;
    if (hasSave && ui.Button("Continue", {x, y, w, 72})) act = MenuAction::Continue;
    if (hasSave) y += 88;
    if (ui.Button("New game", {x, y, w, 72})) act = MenuAction::NewGame;
    y += 88;
    if (ui.Button("Settings", {x, y, w, 72})) act = MenuAction::Settings;
    y += 88;
    if (ui.Button("Quit", {x, y, w, 72})) act = MenuAction::QuitToDesktop;
    ui.Text("v" + version, ui.W() - 40, ui.H() - 60, 26, Color{200, 195, 190, 200}, Align::Right);
    ui.Prompt(Act::Confirm, "Select", 40, ui.H() - 76, 44);
    return act;
}

}  // namespace lk
