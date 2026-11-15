#pragma once
// Minimal HUD: flame bar with a lantern icon, stamina bar, flask count, embers, boss bar,
// small health bars over damaged enemies and the floating prompt.
#include <string>

#include "engine/ui.h"

namespace lk {

struct HudState {
    float flame = 1, flameTrail = 1, flameMax = 100;
    float stamina = 1, staminaMax = 100;
    bool exhausted = false;
    int flasks = 3, flaskMax = 3;
    int embers = 0;
    float emberFlash = 0;
    bool bossActive = false;
    std::string bossName;
    float bossHp = 1, bossTrail = 1;
    std::string prompt;          // e.g. "Rest at shrine"
    Act promptAction = Act::Interact;
    float lowFlamePulse = 0;     // 0..1 when flame is low
};

void DrawHud(Ui& ui, const HudState& s, float time);
void DrawEnemyBar(Ui& ui, Vector2 screenPos, float hpFrac, float postureFrac, float alpha);
void DrawBanner(Ui& ui, const std::string& text, float alpha, Color color, float size = 96);
void DrawLanternIcon(Ui& ui, float x, float y, float size, float glow);

}  // namespace lk
