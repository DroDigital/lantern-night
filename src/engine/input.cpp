#include "engine/input.h"

#include <cctype>
#include <cmath>

namespace lk {

Vector2 ApplyRadialDeadZone(Vector2 v, float dz) {
    float m = std::sqrt(v.x * v.x + v.y * v.y);
    if (m < dz || m < 1e-6f) return Vector2{0, 0};
    float scaled = std::fmin(1.0f, (m - dz) / (1.0f - dz));
    return Vector2{v.x / m * scaled, v.y / m * scaled};
}

static PadFamily FamilyFromName(const std::string& raw) {
    std::string n;
    for (char c : raw) n.push_back(char(std::tolower((unsigned char)c)));
    if (n.find("ps3") != std::string::npos || n.find("ps4") != std::string::npos || n.find("ps5") != std::string::npos ||
        n.find("playstation") != std::string::npos || n.find("dualshock") != std::string::npos ||
        n.find("dualsense") != std::string::npos || n.find("sony") != std::string::npos)
        return PadFamily::PlayStation;
    if (n.find("nintendo") != std::string::npos || n.find("switch") != std::string::npos ||
        n.find("pro controller") != std::string::npos || n.find("joy-con") != std::string::npos)
        return PadFamily::Switch;
    return PadFamily::Xbox;
}

void Input::Poll(float frameDt, bool captureMouse) {
    if (scripted_) return;
    // Hot-plug: pick the first connected gamepad each frame.
    int pad = -1;
    for (int i = 0; i < 4; ++i)
        if (IsGamepadAvailable(i)) {
            pad = i;
            break;
        }
    if (pad != pad_) {
        pad_ = pad;
        padName_ = pad >= 0 ? GetGamepadName(pad) : "";
        family_ = FamilyFromName(padName_);
    }

    InputFrame f;
    uint32_t held = 0;
    auto set = [&](Act a, bool on) {
        if (on) held |= 1u << uint32_t(a);
    };
    bool kbmUsed = false, padUsed = false;

    // ---------------------------------------------------------------- keyboard + mouse
    Vector2 kmove{0, 0};
    if (IsKeyDown(KEY_W)) kmove.y += 1;
    if (IsKeyDown(KEY_S)) kmove.y -= 1;
    if (IsKeyDown(KEY_D)) kmove.x += 1;
    if (IsKeyDown(KEY_A)) kmove.x -= 1;
    float km = std::sqrt(kmove.x * kmove.x + kmove.y * kmove.y);
    if (km > 0) {
        kmove.x /= km;
        kmove.y /= km;
        kbmUsed = true;
    }
    bool walkMod = IsKeyDown(KEY_LEFT_ALT);
    if (walkMod) {
        kmove.x *= 0.4f;
        kmove.y *= 0.4f;
    }
    set(Act::Attack, IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsKeyDown(KEY_LEFT_CONTROL));
    set(Act::Heavy, IsKeyDown(KEY_F) || (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_LEFT_CONTROL)));
    set(Act::Roll, IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_LEFT_SHIFT));
    set(Act::Block, IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
    set(Act::Interact, IsKeyDown(KEY_E));
    set(Act::Flask, IsKeyDown(KEY_R));
    set(Act::LockOn, IsKeyDown(KEY_Q) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE));
    set(Act::TargetLeft, IsKeyDown(KEY_Z));
    set(Act::TargetRight, IsKeyDown(KEY_C));
    set(Act::Pause, IsKeyDown(KEY_ESCAPE) || IsKeyDown(KEY_TAB));
    set(Act::Confirm, IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_E));
    set(Act::Back, IsKeyDown(KEY_ESCAPE) || IsKeyDown(KEY_BACKSPACE));
    set(Act::Up, IsKeyDown(KEY_UP) || IsKeyDown(KEY_W));
    set(Act::Down, IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S));
    set(Act::Left, IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A));
    set(Act::Right, IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D));
    if (held) kbmUsed = true;
    float wheel = GetMouseWheelMove();
    if (wheel > 0.5f) set(Act::TargetLeft, true);
    if (wheel < -0.5f) set(Act::TargetRight, true);

    Vector2 look{0, 0};
    Vector2 md = GetMouseDelta();
    if (captureMouse) {
        if (std::fabs(md.x) + std::fabs(md.y) > 0.5f) kbmUsed = true;
        look.x += md.x * settings_.mouseSensitivity * settings_.sensitivity;
        look.y += md.y * settings_.mouseSensitivity * settings_.sensitivity;
    } else if (std::fabs(md.x) + std::fabs(md.y) > 2.0f) {
        kbmUsed = true;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) kbmUsed = true;

    // ---------------------------------------------------------------- gamepad
    Vector2 pmove{0, 0};
    if (pad_ >= 0) {
        Vector2 ls = ApplyRadialDeadZone(
            {GetGamepadAxisMovement(pad_, GAMEPAD_AXIS_LEFT_X), GetGamepadAxisMovement(pad_, GAMEPAD_AXIS_LEFT_Y)},
            settings_.stickDeadZone);
        Vector2 rs = ApplyRadialDeadZone(
            {GetGamepadAxisMovement(pad_, GAMEPAD_AXIS_RIGHT_X), GetGamepadAxisMovement(pad_, GAMEPAD_AXIS_RIGHT_Y)},
            settings_.stickDeadZone);
        pmove = {ls.x, -ls.y};
        float rt = GetGamepadAxisMovement(pad_, GAMEPAD_AXIS_RIGHT_TRIGGER);
        float lt = GetGamepadAxisMovement(pad_, GAMEPAD_AXIS_LEFT_TRIGGER);
        // Triggers rest at -1 on most backends; map to 0..1.
        float rtv = (rt + 1.0f) * 0.5f, ltv = (lt + 1.0f) * 0.5f;
        uint32_t ph = 0;
        auto pset = [&](Act a, bool on) {
            if (on) ph |= 1u << uint32_t(a);
        };
        pset(Act::Attack, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_RIGHT_TRIGGER_1));
        pset(Act::Heavy, rtv > settings_.triggerThreshold || IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_RIGHT_TRIGGER_2));
        pset(Act::Block, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_LEFT_TRIGGER_1) || ltv > settings_.triggerThreshold ||
                             IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_LEFT_TRIGGER_2));
        pset(Act::Roll, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT));
        pset(Act::Interact, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
        pset(Act::Flask, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_RIGHT_FACE_LEFT));
        pset(Act::LockOn, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_RIGHT_THUMB));
        pset(Act::Pause, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_MIDDLE_RIGHT));
        pset(Act::Confirm, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
        pset(Act::Back, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT));
        pset(Act::Up, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_LEFT_FACE_UP) || ls.y < -0.6f);
        pset(Act::Down, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_LEFT_FACE_DOWN) || ls.y > 0.6f);
        pset(Act::Left, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_LEFT_FACE_LEFT) || ls.x < -0.6f);
        pset(Act::Right, IsGamepadButtonDown(pad_, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || ls.x > 0.6f);
        // Right stick flick while locked on switches targets (latched until the stick recentres).
        if (std::fabs(rs.x) < 0.3f) flickArmed_ = true;
        if (flickArmed_ && std::fabs(rs.x) > 0.8f) {
            pset(rs.x < 0 ? Act::TargetLeft : Act::TargetRight, true);
            flickArmed_ = false;
        }
        look.x += rs.x * settings_.padLookSpeed * settings_.sensitivity * frameDt;
        look.y += rs.y * settings_.padLookSpeed * settings_.sensitivity * frameDt * 0.7f;
        if (ph || std::fabs(ls.x) + std::fabs(ls.y) + std::fabs(rs.x) + std::fabs(rs.y) > 0.0f) padUsed = true;
        held |= ph;
    }
    if (padUsed) lastDevice_ = Device::Gamepad;
    else if (kbmUsed) lastDevice_ = Device::KeyboardMouse;

    f.move = (std::fabs(pmove.x) + std::fabs(pmove.y) > std::fabs(kmove.x) + std::fabs(kmove.y)) ? pmove : kmove;
    if (settings_.invertX) look.x = -look.x;
    if (settings_.invertY) look.y = -look.y;

    // Accumulate look across frames until consumed; edges latch until the first sim step reads them.
    InputFrame next;
    next.move = f.move;
    next.look = {frame_.look.x * (edgesConsumed_ ? 0.0f : 1.0f) + look.x, frame_.look.y * (edgesConsumed_ ? 0.0f : 1.0f) + look.y};
    next.held = held;
    uint32_t newPressed = held & ~prevHeld_;
    uint32_t newReleased = prevHeld_ & ~held;
    next.pressed = newPressed | (edgesConsumed_ ? 0u : frame_.pressed);
    next.released = newReleased | (edgesConsumed_ ? 0u : frame_.released);
    prevHeld_ = held;
    frame_ = next;
    edgesConsumed_ = false;
}

InputFrame Input::ConsumeStep() {
    InputFrame f = frame_;
    if (edgesConsumed_) {
        f.pressed = 0;
        f.released = 0;
        f.look = {0, 0};
    }
    edgesConsumed_ = true;
    return f;
}

}  // namespace lk
