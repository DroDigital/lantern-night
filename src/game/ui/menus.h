#pragma once
// Shrine menu (rest, level up, swap weapon) and pause menu, driven by the immediate-mode UI.
#include <string>

#include "core/save_data.h"
#include "engine/data_store.h"
#include "engine/ui.h"

namespace lk {

enum class MenuAction { None, Close, LevelVigor, LevelEndurance, LevelMight, SwapWeapon, Resume, Settings, QuitToDesktop };

struct ShrineMenuState {
    enum class Page { Main, LevelUp } page = Page::Main;
    std::string shrineName;
};

MenuAction DrawShrineMenu(Ui& ui, ShrineMenuState& st, const SaveData& save, const json& leveling, bool hasGreataxe,
                          const std::string& weaponName);
MenuAction DrawPauseMenu(Ui& ui);

}  // namespace lk
