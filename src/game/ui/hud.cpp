#include "game/ui/hud.h"

#include <algorithm>
#include <cmath>

namespace lk {

void DrawLanternIcon(Ui& ui, float x, float y, float size, float glow) {
    float s = size;
    Color metal{58, 52, 60, 255};
    unsigned char g = (unsigned char)std::clamp(120.0f + 135.0f * glow, 0.0f, 255.0f);
    // halo
    for (int i = 4; i > 0; --i) {
        float r = s * (0.25f + 0.12f * float(i)) * (0.5f + 0.5f * glow);
        ui.Rect({x + s * 0.5f - r, y + s * 0.55f - r, 2 * r, 2 * r}, Color{255, 150, 60, (unsigned char)(18 * glow * float(5 - i))});
    }
    ui.Rect({x + s * 0.30f, y + s * 0.18f, s * 0.40f, s * 0.08f}, metal);
    ui.Rect({x + s * 0.33f, y + s * 0.26f, s * 0.34f, s * 0.50f}, Color{255, g, (unsigned char)(g / 2), 255});
    ui.Rect({x + s * 0.47f, y + s * 0.26f, s * 0.06f, s * 0.50f}, metal);
    ui.Rect({x + s * 0.28f, y + s * 0.76f, s * 0.44f, s * 0.08f}, metal);
    ui.Rect({x + s * 0.44f, y + s * 0.06f, s * 0.12f, s * 0.12f}, metal);
}

void DrawHud(Ui& ui, const HudState& s, float time) {
    float x = 56, y = 52;
    float glow = std::clamp(s.flame, 0.0f, 1.0f);
    float pulse = s.lowFlamePulse * (0.5f + 0.5f * std::sin(time * 9.0f));
    DrawLanternIcon(ui, x - 8, y - 14, 72, glow * (1.0f - 0.5f * pulse));
    float bw = 300 + s.flameMax * 1.6f;
    Color flameCol{(unsigned char)(235 + 20 * pulse), (unsigned char)(140 - 60 * pulse), 50, 255};
    ui.Bar({x + 70, y + 6, bw, 18}, s.flame, flameCol, Color{30, 18, 14, 220}, s.flameTrail);
    float sw = 260 + s.staminaMax * 1.4f;
    Color stamCol = s.exhausted ? Color{120, 120, 110, 255} : Color{120, 190, 110, 255};
    ui.Bar({x + 70, y + 32, sw, 10}, s.stamina, stamCol, Color{18, 24, 18, 220});

    // Flasks: little bottles, filled = available.
    float fx = 60, fy = ui.H() - 128;
    for (int i = 0; i < s.flaskMax; ++i) {
        bool full = i < s.flasks;
        Color glass = full ? Color{255, 170, 70, 255} : Color{60, 55, 60, 200};
        float bx = fx + float(i) * 34;
        ui.Rect({bx + 8, fy, 10, 8}, Color{90, 70, 60, 255});
        ui.Rect({bx + 2, fy + 8, 22, 30}, glass);
        if (full) ui.Rect({bx + 4, fy + 10, 6, 24}, Color{255, 230, 180, 120});
    }
    ui.Prompt(Act::Flask, "", fx + float(s.flaskMax) * 34 + 6, fy + 2, 36);

    // Embers (currency), bottom right.
    std::string e = std::to_string(s.embers);
    float ex = ui.W() - 60, ey = ui.H() - 96;
    unsigned char a = (unsigned char)std::clamp(200.0f + 55.0f * s.emberFlash, 0.0f, 255.0f);
    ui.Panel({ex - 230, ey - 14, 230, 64}, Color{255, 255, 255, 200});
    ui.Rect({ex - 210, ey + 6, 18, 18}, Color{255, 140, 50, a});
    ui.Rect({ex - 206, ey + 10, 10, 10}, Color{255, 230, 160, a});
    ui.Text(e, ex - 24, ey, 36, Color{255, 220, 170, a}, Align::Right);

    if (s.bossActive) {
        float w = 1100, bx = (ui.W() - w) * 0.5f, by = ui.H() - 120;
        ui.Text(s.bossName, bx, by - 42, 34, Color{230, 220, 210, 255});
        ui.Bar({bx, by, w, 16}, s.bossHp, Color{190, 40, 30, 255}, Color{25, 10, 10, 230}, s.bossTrail);
    }
    if (!s.prompt.empty()) {
        float pw = ui.TextWidth(s.prompt, 30) + 120;
        float px = (ui.W() - pw) * 0.5f, py = ui.H() * 0.70f;
        ui.Panel({px, py, pw, 64}, Color{255, 255, 255, 220});
        ui.Prompt(s.promptAction, s.prompt, px + 20, py + 12, 40);
    }
}

void DrawEnemyBar(Ui& ui, Vector2 sp, float hp, float posture, float alpha) {
    unsigned char a = (unsigned char)std::clamp(alpha * 255.0f, 0.0f, 255.0f);
    float w = 90, x = sp.x - w * 0.5f, y = sp.y;
    ui.Bar({x, y, w, 7}, hp, Color{200, 50, 40, a}, Color{20, 10, 10, (unsigned char)(a * 0.8f)});
    if (posture > 0.01f) ui.Bar({x, y + 10, w, 4}, posture, Color{240, 200, 90, a}, Color{20, 18, 10, (unsigned char)(a * 0.6f)});
}

void DrawBanner(Ui& ui, const std::string& text, float alpha, Color color, float size) {
    unsigned char a = (unsigned char)std::clamp(alpha * 255.0f, 0.0f, 255.0f);
    float y = ui.H() * 0.42f;
    ui.Rect({0, y - 30, ui.W(), size + 60}, Color{0, 0, 0, (unsigned char)(a * 0.55f)});
    Color c = color;
    c.a = a;
    ui.Text(text, ui.W() * 0.5f, y, size, c, Align::Center);
}

}  // namespace lk
