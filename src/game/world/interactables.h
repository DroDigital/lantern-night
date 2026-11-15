#pragma once
// World interactables built from zone placements: braziers (light once, saved, may reveal a
// hidden path), gates (shortcuts that open from one side only, hidden walls, the mini-boss
// gate), item pickups (Flask Shards, the greataxe), floor hints and area triggers.
#include <string>
#include <vector>

#include "raylib.h"

namespace lk {

struct Brazier {
    std::string id;
    Vector3 pos{};
    std::string reveals;     // gate id opened when lit (hidden path)
    int light = -1;          // zoneLights_ index
    bool lit = false;
    bool arena = false;      // boss arena brazier (snuffed in phase 2)
    bool snuffed = false;
};

enum class GateKind { Shortcut, Hidden, Locked, Fog };

struct Gate {
    std::string id;
    GateKind kind = GateKind::Shortcut;
    Vector3 pos{};
    float yaw = 0;
    Vector3 openDir{0, 0, 1};   // shortcuts open only from this side
    std::string unlockBoss;     // Locked: opens once this boss is defeated
    std::string model;
    int body = -1;              // physics toggle handle
    bool open = false;
    float anim = 0;             // 0 closed .. 1 open (visual)
    int navLevel = 0, navX = 0, navZ = 0;
};

struct Pickup {
    std::string id, kind, model, label;
    Vector3 pos{};
    float yaw = 0;
    bool taken = false;
};

struct FloorHint {
    Vector3 pos{};
    std::string text;
    float radius = 2.6f;
    std::string action;   // optional prompt glyph action name
};

struct AreaTrigger {
    std::string id, name, music;
    BoundingBox box{};
};

}  // namespace lk
