#pragma once
// Small immediate-mode UI on raylib drawing: text, nine-slice panels, bars, icons and focusable
// buttons that work fully with a gamepad (Up/Down/Confirm/Back) and with the mouse.
// Layout is in a virtual 1920x1080 space scaled to the window.
#include <map>
#include <string>
#include <vector>

#include "engine/input.h"
#include "raylib.h"

namespace lk {

class AssetCache;

enum class Align { Left, Center, Right };

class Ui {
public:
    bool Init(AssetCache& assets);
    void Shutdown();

    // Call once per frame before drawing widgets. nav = this frame's input (edges).
    void Begin(int screenW, int screenH, const InputFrame& nav, Device device, PadFamily family);
    void End();

    float Scale() const { return scale_; }
    float W() const { return 1920.0f; }
    float H() const { return vh_; }

    void Text(const std::string& s, float x, float y, float size, Color c, Align a = Align::Left, bool shadow = true);
    float TextWidth(const std::string& s, float size) const;
    void Panel(Rectangle r, Color tint = Color{255, 255, 255, 235});
    void Bar(Rectangle r, float frac, Color fill, Color back, float trailFrac = -1.0f, Color trail = Color{255, 240, 200, 180});
    void Icon(Texture2D t, Rectangle r, Color tint = WHITE);
    void Rect(Rectangle r, Color c);

    // Focusable button; returns true when activated. Focus moves with Up/Down; mouse hover focuses.
    bool Button(const std::string& label, Rectangle r, bool enabled = true);
    // Left/Right adjustable row; returns -1/+1 when changed, 0 otherwise.
    int Stepper(const std::string& label, const std::string& value, Rectangle r);
    void ResetFocus() { focus_ = 0; }
    // Forget this frame's button presses (the press that opened a menu must not also act in it).
    // The suppressed buttons stay ignored until released.
    void DropPresses() {
        suppress_ |= nav_.held | nav_.pressed;
        nav_.pressed = 0;
        back_ = false;
    }
    int Focus() const { return focus_; }
    bool BackPressed() const { return back_; }
    bool MovedFocus() const { return moved_; }
    bool Activated() const { return activated_; }

    // Input prompt glyph for an action on the last-used device.
    void Glyph(Act a, float x, float y, float size);
    void Prompt(Act a, const std::string& label, float x, float y, float size);
    // Short button name for the last-used device ("RB", "R1", "LMB", ...).
    std::string ActionLabel(Act a) const;
    // Replace {Attack}, {Roll}, ... tokens with ActionLabel names.
    std::string ExpandTokens(const std::string& text) const;

    Font& GetFont() { return font_; }

private:
    Rectangle ToScreen(Rectangle r) const;
    Texture2D GlyphTexture(Act a) const;
    std::string KeyLabel(Act a) const;

    Font font_{};
    Font fontSmall_{};
    Texture2D panel_{};
    std::map<std::string, Texture2D> glyphs_;
    float scale_ = 1.0f;
    float vh_ = 1080.0f;
    float offX_ = 0, offY_ = 0;
    InputFrame nav_;
    uint32_t suppress_ = 0;
    Device device_ = Device::KeyboardMouse;
    PadFamily family_ = PadFamily::Xbox;
    int focus_ = 0, widget_ = 0, widgetCount_ = 0;
    bool back_ = false, moved_ = false, activated_ = false;
    bool mouseClicked_ = false;
    Vector2 mouse_{};
    AssetCache* assets_ = nullptr;
};

}  // namespace lk
