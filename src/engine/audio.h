#pragma once
// Audio on raylib: per-SFX voice pools so the same sound can overlap, slight pitch randomisation,
// positional volume/pan by distance to the listener, music streams with crossfade, and
// Master / Music / SFX volume buses.
#include <map>
#include <string>
#include <vector>

#include "engine/data_store.h"
#include "raylib.h"

namespace lk {

struct SfxDef {
    std::vector<std::string> files;   // random pick per play
    float volume = 1.0f;
    float pitchJitter = 0.06f;
    int voices = 4;
    float maxDistance = 18.0f;
};

class AudioManager {
public:
    bool Init(const json& audioData);   // data/audio.json
    void Shutdown();
    bool Enabled() const { return ready_; }

    void SetListener(Vector3 pos, Vector3 forward, Vector3 right);
    // Play a named SFX at a world position (positional) or flat (pos == nullptr).
    void Play(const std::string& name, const Vector3* pos = nullptr, float volume = 1.0f, float pitch = 1.0f);
    void PlayAt(const std::string& name, Vector3 pos, float volume = 1.0f) { Play(name, &pos, volume); }

    // Looping sound (lantern crackle): set target volume each frame; 0 = silent.
    void SetLoop(const std::string& name, float volume);

    void PlayMusic(const std::string& name, float fadeSeconds = 2.0f);
    void StopMusic(float fadeSeconds = 2.0f);
    void Update(float dt);

    void SetVolumes(float master, float music, float sfx);
    float Master() const { return master_; }
    float MusicVol() const { return music_; }
    float SfxVol() const { return sfx_; }
    int MissingCount() const { return missing_; }
    std::vector<std::string> Names() const;

private:
    struct Pool {
        SfxDef def;
        std::vector<std::vector<Sound>> variants;  // [file][voice]
        std::vector<int> next;
    };
    struct Track {
        Music music{};
        bool loaded = false;
        float vol = 0.0f, target = 0.0f, speed = 0.5f;
    };
    struct Loop {
        Sound s{};
        bool loaded = false;
        float vol = 0.0f, target = 0.0f;
    };
    bool ready_ = false;
    std::map<std::string, Pool> pools_;
    std::map<std::string, Track> music_tracks_;
    std::map<std::string, Loop> loops_;
    std::string currentMusic_;
    Vector3 lpos_{}, lfwd_{0, 0, -1}, lright_{1, 0, 0};
    float master_ = 0.8f, music_ = 0.6f, sfx_ = 0.8f;
    int missing_ = 0;
    unsigned rng_ = 12345u;
    float Rand();
};

}  // namespace lk
