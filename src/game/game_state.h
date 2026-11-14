#pragma once
// GameState: the one place that holds progress (persisted via SaveSystem) plus run-time flags.
// Created once in main() and passed by reference.
#include <string>

#include "core/save_data.h"

namespace lk {

struct GameState {
    SaveData progress;
    // Run-time only
    bool godMode = false;
    bool paused = false;
    bool inBossFight = false;
    std::string currentArea;
    void ResetProgress() { progress = SaveData{}; }
};

}  // namespace lk
