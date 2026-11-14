#pragma once
// References to the long-lived services and engine systems. Built once in App (whose services
// come from main) and passed by reference; nothing here is a global.
namespace lk {

class DataStore;
class AssetCache;
class Renderer;
class Physics;
class Input;
class AudioManager;
class SaveSystem;
struct GameState;

struct Ctx {
    DataStore& data;
    AssetCache& assets;
    Renderer& renderer;
    Physics& physics;
    Input& input;
    AudioManager& audio;
    SaveSystem& save;
    GameState& state;
};

}  // namespace lk
