#include "game/ui/menus.h"

#include "core/combat_rules.h"

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

}  // namespace lk
