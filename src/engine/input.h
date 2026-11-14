#pragma once
// Input: reads raylib keyboard, mouse and gamepad directly. Gamepad first; KB+M fully supported.
// Handles dead zones, hot-plugging, and tracks the last-used device for glyphs.
// Edges (pressed) are latched per render frame and consumed by the first fixed sim step.
#include <cstdint>
#include <string>

#include "raylib.h"

namespace lk {

enum class Act : uint32_t {
    Attack = 0,     // light attack
    Heavy,          // heavy attack (hold to charge)
    Roll,           // tap = roll, hold = sprint
    Block,          // block / parry
    Interact,
    Flask,
    LockOn,
    TargetLeft,
    TargetRight,
    Pause,
    Confirm,
    Back,
    Up,
    Down,
    Left,
    Right,
    Count
};

enum class Device { KeyboardMouse, Gamepad };
enum class PadFamily { Xbox, PlayStation, Switch };

struct InputFrame {
    Vector2 move{0, 0};      // left stick / WASD, y up = forward
    Vector2 look{0, 0};      // camera delta (already scaled by sensitivity), radians-ish per frame
    uint32_t held = 0;
    uint32_t pressed = 0;
    uint32_t released = 0;
    bool Held(Act a) const { return held & (1u << uint32_t(a)); }
    bool Pressed(Act a) const { return pressed & (1u << uint32_t(a)); }
    bool Released(Act a) const { return released & (1u << uint32_t(a)); }
    void Set(Act a, bool h) {
        if (h) held |= 1u << uint32_t(a);
        else held &= ~(1u << uint32_t(a));
    }
    void Press(Act a) { pressed |= 1u << uint32_t(a); held |= 1u << uint32_t(a); }
};

struct InputSettings {
    float stickDeadZone = 0.2f;
    float triggerThreshold = 0.35f;
    float mouseSensitivity = 0.0025f;
    float padLookSpeed = 2.6f;       // rad/s at full deflection
    float sensitivity = 1.0f;        // user setting multiplier
    bool invertX = false;
    bool invertY = false;
};

class Input {
public:
    // Poll devices once per render frame.
    void Poll(float frameDt, bool captureMouse);
    // Frame for the next fixed step; edges are cleared after the first call per Poll.
    InputFrame ConsumeStep();
    // Inject a scripted frame instead of devices (smoke test / capture).
    void SetScripted(bool on) { scripted_ = on; }
    bool Scripted() const { return scripted_; }
    void PushScripted(const InputFrame& f) { frame_ = f; edgesConsumed_ = false; }

    InputSettings& Settings() { return settings_; }
    Device LastDevice() const { return lastDevice_; }
    PadFamily Family() const { return family_; }
    int Gamepad() const { return pad_; }
    const std::string& GamepadName() const { return padName_; }
    // Right stick flick (for lock-on target switching): -1 left, +1 right, 0 none. Latched like edges.
    // (Folded into TargetLeft/TargetRight.)

private:
    InputSettings settings_;
    InputFrame frame_;
    bool edgesConsumed_ = false;
    bool scripted_ = false;
    uint32_t prevHeld_ = 0;
    Device lastDevice_ = Device::KeyboardMouse;
    PadFamily family_ = PadFamily::Xbox;
    int pad_ = -1;
    std::string padName_;
    bool flickArmed_ = true;
    Vector2 prevMouse_{0, 0};
    bool havePrevMouse_ = false;
};

Vector2 ApplyRadialDeadZone(Vector2 v, float dz);

}  // namespace lk
