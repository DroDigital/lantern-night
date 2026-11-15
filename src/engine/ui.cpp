#include "engine/ui.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "engine/assets.h"
#include "engine/paths.h"

namespace lk {
namespace {
// Physical position of the pad button bound to each action.
enum class PadPos { FaceDown, FaceRight, FaceLeft, FaceUp, LB, RB, LT, RT, RStick, Start, None };

PadPos PadPosFor(Act a) {
    switch (a) {
        case Act::Attack: return PadPos::RB;
        case Act::Heavy: return PadPos::RT;
        case Act::Roll: return PadPos::FaceRight;
        case Act::Block: return PadPos::LB;
        case Act::Interact: return PadPos::FaceDown;
        case Act::Flask: return PadPos::FaceLeft;
        case Act::LockOn: return PadPos::RStick;
        case Act::Pause: return PadPos::Start;
        case Act::Confirm: return PadPos::FaceDown;
        case Act::Back: return PadPos::FaceRight;
        default: return PadPos::None;
    }
}

std::string PadFile(PadFamily f, PadPos p) {
    if (f == PadFamily::Switch) {
        switch (p) {
            case PadPos::FaceDown: return "switch/switch_button_b.png";
            case PadPos::FaceRight: return "switch/switch_button_a.png";
            case PadPos::FaceLeft: return "switch/switch_button_y.png";
            case PadPos::FaceUp: return "switch/switch_button_x.png";
            case PadPos::LB: return "switch/switch_button_l.png";
            case PadPos::RB: return "switch/switch_button_r.png";
            case PadPos::LT: return "switch/switch_button_zl.png";
            case PadPos::RT: return "switch/switch_button_zr.png";
            case PadPos::RStick: return "switch/switch_stick_r_press.png";
            case PadPos::Start: return "switch/switch_button_plus.png";
            default: return "";
        }
    }
    // Xbox glyphs; PlayStation uses Xbox shoulder/stick art and drawn shape placeholders for the face buttons.
    switch (p) {
        case PadPos::FaceDown: return "xbox/xbox_button_a.png";
        case PadPos::FaceRight: return "xbox/xbox_button_b.png";
        case PadPos::FaceLeft: return "xbox/xbox_button_x.png";
        case PadPos::FaceUp: return "xbox/xbox_button_y.png";
        case PadPos::LB: return "xbox/xbox_lb.png";
        case PadPos::RB: return "xbox/xbox_rb.png";
        case PadPos::LT: return "xbox/xbox_lt.png";
        case PadPos::RT: return "xbox/xbox_rt.png";
        case PadPos::RStick: return "xbox/xbox_stick_r_press.png";
        case PadPos::Start: return "xbox/xbox_button_menu.png";
        default: return "";
    }
}

std::string KeyFile(Act a) {
    switch (a) {
        case Act::Attack: return "keyboard/mouse_left.png";
        case Act::Heavy: return "keyboard/keyboard_f.png";
        case Act::Roll: return "keyboard/keyboard_space.png";
        case Act::Block: return "keyboard/mouse_right.png";
        case Act::Interact: return "keyboard/keyboard_e.png";
        case Act::Flask: return "keyboard/keyboard_r.png";
        case Act::LockOn: return "keyboard/keyboard_q.png";
        case Act::Pause: return "keyboard/keyboard_escape.png";
        case Act::Confirm: return "keyboard/keyboard_enter.png";
        case Act::Back: return "keyboard/keyboard_escape.png";
        default: return "";
    }
}
}  // namespace

bool Ui::Init(AssetCache& assets) {
    assets_ = &assets;
    font_ = assets.LoadFont("fonts/kenvector_future.ttf", 64);
    fontSmall_ = assets.LoadFont("fonts/kenvector_future_thin.ttf", 40);
    // Procedural nine-slice panel: dark slate with a warm hairline border and soft corners.
    Image img = GenImageColor(48, 48, BLANK);
    ImageDrawRectangle(&img, 2, 2, 44, 44, Color{14, 16, 24, 230});
    ImageDrawRectangleLinesEx(&img, Rectangle{2, 2, 44, 44}, 2, Color{150, 110, 70, 255});
    ImageDrawRectangleLinesEx(&img, Rectangle{5, 5, 38, 38}, 1, Color{60, 50, 45, 255});
    for (int c = 0; c < 4; ++c) {  // knock the corner pixels out for a rounded feel
        int x = (c & 1) ? 45 : 2, y = (c & 2) ? 45 : 2;
        ImageDrawPixel(&img, x, y, BLANK);
    }
    panel_ = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(panel_, TEXTURE_FILTER_BILINEAR);
    // Preload every glyph so nothing is loaded mid-game.
    for (int a = 0; a < int(Act::Count); ++a) {
        Act act = Act(a);
        for (PadFamily f : {PadFamily::Xbox, PadFamily::Switch}) {
            std::string file = PadFile(f, PadPosFor(act));
            if (!file.empty()) glyphs_[file] = assets.LoadTexture("prompts/" + file, true);
        }
        std::string k = KeyFile(act);
        if (!k.empty()) glyphs_[k] = assets.LoadTexture("prompts/" + k, true);
    }
    return true;
}

void Ui::Shutdown() {
    if (panel_.id) UnloadTexture(panel_);
    panel_ = Texture2D{};
}

void Ui::Begin(int sw, int sh, const InputFrame& nav, Device device, PadFamily family) {
    // Uniform scale fitting 1920 wide; virtual height follows the aspect ratio.
    scale_ = std::min(float(sw) / 1920.0f, float(sh) / 1080.0f);
    vh_ = float(sh) / scale_;
    offX_ = (float(sw) - 1920.0f * scale_) * 0.5f;
    offY_ = 0;
    nav_ = nav;
    suppress_ &= nav.held | nav.pressed;   // released buttons work again
    nav_.pressed &= ~suppress_;
    device_ = device;
    family_ = family;
    widgetCount_ = widget_;
    widget_ = 0;
    back_ = nav_.Pressed(Act::Back);
    moved_ = false;
    activated_ = false;
    if (widgetCount_ > 0) {
        if (nav_.Pressed(Act::Down)) {
            focus_ = (focus_ + 1) % widgetCount_;
            moved_ = true;
        }
        if (nav_.Pressed(Act::Up)) {
            focus_ = (focus_ + widgetCount_ - 1) % widgetCount_;
            moved_ = true;
        }
        focus_ = std::clamp(focus_, 0, widgetCount_ - 1);
    }
    mouse_ = GetMousePosition();
    mouseClicked_ = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void Ui::End() {}

Rectangle Ui::ToScreen(Rectangle r) const {
    return Rectangle{offX_ + r.x * scale_, offY_ + r.y * scale_, r.width * scale_, r.height * scale_};
}

float Ui::TextWidth(const std::string& s, float size) const {
    const Font& f = size >= 30 ? font_ : fontSmall_;
    return MeasureTextEx(f, s.c_str(), size, size * 0.06f).x;
}

void Ui::Text(const std::string& s, float x, float y, float size, Color c, Align a, bool shadow) {
    const Font& f = size >= 30 ? font_ : fontSmall_;
    float w = TextWidth(s, size);
    if (a == Align::Center) x -= w * 0.5f;
    else if (a == Align::Right) x -= w;
    Vector2 p{offX_ + x * scale_, offY_ + y * scale_};
    float ss = size * scale_;
    if (shadow) DrawTextEx(f, s.c_str(), Vector2{p.x + 2 * scale_, p.y + 2 * scale_}, ss, ss * 0.06f, Color{0, 0, 0, (unsigned char)(c.a * 0.7f)});
    DrawTextEx(f, s.c_str(), p, ss, ss * 0.06f, c);
}

void Ui::Panel(Rectangle r, Color tint) {
    NPatchInfo n{Rectangle{0, 0, 48, 48}, 12, 12, 12, 12, NPATCH_NINE_PATCH};
    DrawTextureNPatch(panel_, n, ToScreen(r), Vector2{0, 0}, 0.0f, tint);
}

void Ui::Rect(Rectangle r, Color c) { DrawRectangleRec(ToScreen(r), c); }

void Ui::Bar(Rectangle r, float frac, Color fill, Color back, float trailFrac, Color trail) {
    Rectangle s = ToScreen(r);
    DrawRectangleRec(Rectangle{s.x - 2 * scale_, s.y - 2 * scale_, s.width + 4 * scale_, s.height + 4 * scale_}, Color{0, 0, 0, 160});
    DrawRectangleRec(s, back);
    frac = std::clamp(frac, 0.0f, 1.0f);
    if (trailFrac > frac) DrawRectangleRec(Rectangle{s.x, s.y, s.width * std::min(trailFrac, 1.0f), s.height}, trail);
    DrawRectangleRec(Rectangle{s.x, s.y, s.width * frac, s.height}, fill);
    DrawRectangleRec(Rectangle{s.x, s.y, s.width * frac, s.height * 0.35f}, Color{255, 255, 255, 40});
}

void Ui::Icon(Texture2D t, Rectangle r, Color tint) {
    if (t.id == 0) return;
    DrawTexturePro(t, Rectangle{0, 0, float(t.width), float(t.height)}, ToScreen(r), Vector2{0, 0}, 0.0f, tint);
}

bool Ui::Button(const std::string& label, Rectangle r, bool enabled) {
    int id = widget_++;
    Rectangle s = ToScreen(r);
    bool hover = CheckCollisionPointRec(mouse_, s);
    if (hover && device_ == Device::KeyboardMouse && std::fabs(GetMouseDelta().x) + std::fabs(GetMouseDelta().y) > 0.1f) focus_ = id;
    bool focused = focus_ == id;
    Panel(r, focused ? Color{255, 220, 170, 255} : Color{200, 200, 210, 220});
    if (focused) Rect(Rectangle{r.x + 6, r.y + 6, 6, r.height - 12}, Color{255, 170, 80, 255});
    Text(label, r.x + r.width * 0.5f, r.y + r.height * 0.5f - 16, 32, enabled ? (focused ? Color{255, 230, 190, 255} : Color{200, 200, 210, 255}) : Color{110, 110, 120, 255},
         Align::Center);
    bool act = enabled && ((focused && nav_.Pressed(Act::Confirm)) || (hover && mouseClicked_));
    if (act) activated_ = true;
    return act;
}

int Ui::Stepper(const std::string& label, const std::string& value, Rectangle r) {
    int id = widget_++;
    Rectangle s = ToScreen(r);
    bool hover = CheckCollisionPointRec(mouse_, s);
    if (hover && device_ == Device::KeyboardMouse && std::fabs(GetMouseDelta().x) + std::fabs(GetMouseDelta().y) > 0.1f) focus_ = id;
    bool focused = focus_ == id;
    Panel(r, focused ? Color{255, 220, 170, 255} : Color{200, 200, 210, 200});
    Color tc = focused ? Color{255, 230, 190, 255} : Color{200, 200, 210, 255};
    Text(label, r.x + 28, r.y + r.height * 0.5f - 14, 28, tc);
    Text("<  " + value + "  >", r.x + r.width - 28, r.y + r.height * 0.5f - 14, 28, tc, Align::Right);
    int d = 0;
    if (focused) {
        if (nav_.Pressed(Act::Left)) d = -1;
        if (nav_.Pressed(Act::Right) || nav_.Pressed(Act::Confirm)) d = +1;
    }
    if (hover && mouseClicked_) d = (mouse_.x < s.x + s.width * 0.5f) ? -1 : +1;
    return d;
}

Texture2D Ui::GlyphTexture(Act a) const {
    std::string file;
    if (device_ == Device::Gamepad) file = PadFile(family_ == PadFamily::Switch ? PadFamily::Switch : PadFamily::Xbox, PadPosFor(a));
    else file = KeyFile(a);
    auto it = glyphs_.find(file);
    return it == glyphs_.end() ? Texture2D{} : it->second;
}

void Ui::Glyph(Act a, float x, float y, float size) {
    PadPos pos = PadPosFor(a);
    bool psFace = device_ == Device::Gamepad && family_ == PadFamily::PlayStation &&
                  (pos == PadPos::FaceDown || pos == PadPos::FaceRight || pos == PadPos::FaceLeft || pos == PadPos::FaceUp);
    if (psFace) {
        // Placeholder PlayStation face glyphs (see TODO_ASSETS.md): dark disc + shape.
        Vector2 c{offX_ + (x + size * 0.5f) * scale_, offY_ + (y + size * 0.5f) * scale_};
        float r = size * 0.45f * scale_;
        DrawCircleV(c, r, Color{30, 30, 36, 255});
        DrawCircleLinesV(c, r, Color{220, 220, 230, 255});
        float k = r * 0.5f;
        if (pos == PadPos::FaceDown) {
            DrawLineEx({c.x - k, c.y - k}, {c.x + k, c.y + k}, 3 * scale_, Color{120, 160, 255, 255});
            DrawLineEx({c.x - k, c.y + k}, {c.x + k, c.y - k}, 3 * scale_, Color{120, 160, 255, 255});
        } else if (pos == PadPos::FaceRight) {
            DrawCircleLinesV(c, k, Color{255, 110, 110, 255});
        } else if (pos == PadPos::FaceLeft) {
            DrawRectangleLinesEx(Rectangle{c.x - k, c.y - k, 2 * k, 2 * k}, 2.5f * scale_, Color{240, 140, 220, 255});
        } else {
            DrawTriangleLines({c.x, c.y - k}, {c.x - k, c.y + k * 0.8f}, {c.x + k, c.y + k * 0.8f}, Color{110, 230, 180, 255});
        }
        return;
    }
    Texture2D t = GlyphTexture(a);
    if (t.id) Icon(t, Rectangle{x, y, size, size});
}

std::string Ui::ActionLabel(Act a) const {
    if (device_ == Device::KeyboardMouse) {
        switch (a) {
            case Act::Attack: return "LMB";
            case Act::Heavy: return "F";
            case Act::Roll: return "Space";
            case Act::Block: return "RMB";
            case Act::Interact: return "E";
            case Act::Flask: return "R";
            case Act::LockOn: return "Q";
            case Act::Pause: return "Esc";
            default: return "?";
        }
    }
    PadPos p = PadPosFor(a);
    static const char* xbox[] = {"A", "B", "X", "Y", "LB", "RB", "LT", "RT", "R3", "Menu", "?"};
    static const char* ps[] = {"Cross", "Circle", "Square", "Triangle", "L1", "R1", "L2", "R2", "R3", "Options", "?"};
    static const char* sw[] = {"B", "A", "Y", "X", "L", "R", "ZL", "ZR", "R3", "+", "?"};
    const char** t = family_ == PadFamily::PlayStation ? ps : (family_ == PadFamily::Switch ? sw : xbox);
    return t[int(p)];
}

std::string Ui::ExpandTokens(const std::string& text) const {
    static const std::pair<const char*, Act> tokens[] = {
        {"{Attack}", Act::Attack}, {"{Heavy}", Act::Heavy}, {"{Roll}", Act::Roll}, {"{Block}", Act::Block},
        {"{Interact}", Act::Interact}, {"{Flask}", Act::Flask}, {"{LockOn}", Act::LockOn}, {"{Pause}", Act::Pause}};
    std::string out = text;
    for (auto& [tok, act] : tokens) {
        size_t pos;
        while ((pos = out.find(tok)) != std::string::npos) out.replace(pos, std::strlen(tok), "[" + ActionLabel(act) + "]");
    }
    return out;
}

void Ui::Prompt(Act a, const std::string& label, float x, float y, float size) {
    Glyph(a, x, y, size);
    Text(label, x + size + 8, y + size * 0.5f - size * 0.32f, size * 0.62f, Color{230, 225, 215, 255});
}

}  // namespace lk
