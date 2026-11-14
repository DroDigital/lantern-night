#include "engine/audio.h"

#include <algorithm>
#include <cmath>

#include "engine/log.h"
#include "engine/paths.h"
#include "raymath.h"

namespace lk {

float AudioManager::Rand() {
    rng_ = rng_ * 1664525u + 1013904223u;
    return float((rng_ >> 8) & 0xFFFF) / 65535.0f;
}

bool AudioManager::Init(const json& j) {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) {
        LK_WARN("audio: no audio device; running silent");
        return false;
    }
    if (j.contains("sfx"))
        for (auto& [name, s] : j["sfx"].items()) {
            Pool p;
            if (s.is_string()) p.def.files = {s.get<std::string>()};
            else if (s.is_array()) p.def.files = s.get<std::vector<std::string>>();
            else {
                p.def.files = s.value("files", std::vector<std::string>{});
                p.def.volume = s.value("volume", 1.0f);
                p.def.pitchJitter = s.value("pitch_jitter", p.def.pitchJitter);
                p.def.voices = s.value("voices", p.def.voices);
                p.def.maxDistance = s.value("max_distance", p.def.maxDistance);
            }
            for (auto& f : p.def.files) {
                std::string full = paths::Asset(f);
                Sound base = LoadSound(full.c_str());
                if (base.frameCount == 0) {
                    LK_ERROR("audio: missing %s", f.c_str());
                    ++missing_;
                    continue;
                }
                std::vector<Sound> voices{base};
                for (int v = 1; v < p.def.voices; ++v) voices.push_back(LoadSoundAlias(base));
                p.variants.push_back(std::move(voices));
                p.next.push_back(0);
            }
            pools_[name] = std::move(p);
        }
    if (j.contains("music"))
        for (auto& [name, f] : j["music"].items()) {
            Track t;
            std::string full = paths::Asset(f.get<std::string>());
            t.music = LoadMusicStream(full.c_str());
            t.loaded = t.music.frameCount > 0;
            if (!t.loaded) {
                LK_ERROR("audio: missing music %s", f.get<std::string>().c_str());
                ++missing_;
            }
            t.music.looping = true;
            music_tracks_[name] = t;
        }
    if (j.contains("loops"))
        for (auto& [name, f] : j["loops"].items()) {
            Loop l;
            std::string full = paths::Asset(f.get<std::string>());
            l.s = LoadSound(full.c_str());
            l.loaded = l.s.frameCount > 0;
            if (!l.loaded) {
                LK_ERROR("audio: missing loop %s", f.get<std::string>().c_str());
                ++missing_;
            }
            loops_[name] = l;
        }
    return true;
}

void AudioManager::Shutdown() {
    if (!ready_) return;
    for (auto& [n, p] : pools_)
        for (auto& v : p.variants) {
            for (size_t i = 1; i < v.size(); ++i) UnloadSoundAlias(v[i]);
            if (!v.empty()) UnloadSound(v[0]);
        }
    pools_.clear();
    for (auto& [n, t] : music_tracks_)
        if (t.loaded) UnloadMusicStream(t.music);
    music_tracks_.clear();
    for (auto& [n, l] : loops_)
        if (l.loaded) UnloadSound(l.s);
    loops_.clear();
    CloseAudioDevice();
    ready_ = false;
}

std::vector<std::string> AudioManager::Names() const {
    std::vector<std::string> v;
    for (auto& [k, _] : pools_) v.push_back(k);
    return v;
}

void AudioManager::SetListener(Vector3 pos, Vector3 forward, Vector3 right) {
    lpos_ = pos;
    lfwd_ = forward;
    lright_ = right;
}

void AudioManager::Play(const std::string& name, const Vector3* pos, float volume, float pitch) {
    if (!ready_) return;
    auto it = pools_.find(name);
    if (it == pools_.end() || it->second.variants.empty()) {
        static std::map<std::string, bool> warned;
        if (!warned[name]) {
            LK_WARN("audio: unknown sfx '%s'", name.c_str());
            warned[name] = true;
        }
        return;
    }
    Pool& p = it->second;
    size_t vi = size_t(Rand() * float(p.variants.size())) % p.variants.size();
    auto& voices = p.variants[vi];
    int& nx = p.next[vi];
    Sound s = voices[size_t(nx)];
    nx = (nx + 1) % int(voices.size());
    float vol = p.def.volume * volume * sfx_ * master_;
    float pan = 0.5f;
    if (pos) {
        Vector3 d = Vector3Subtract(*pos, lpos_);
        float dist = Vector3Length(d);
        float att = std::clamp(1.0f - dist / p.def.maxDistance, 0.0f, 1.0f);
        vol *= att * att;
        if (dist > 0.01f) {
            float side = Vector3DotProduct(Vector3Scale(d, 1.0f / dist), lright_);
            pan = 0.5f - side * 0.4f;  // raylib: 0.5 centre, 1.0 left, 0.0 right
        }
    }
    if (vol <= 0.001f) return;
    SetSoundVolume(s, vol);
    SetSoundPan(s, pan);
    SetSoundPitch(s, pitch * (1.0f + (Rand() * 2.0f - 1.0f) * p.def.pitchJitter));
    PlaySound(s);
}

void AudioManager::SetLoop(const std::string& name, float volume) {
    if (!ready_) return;
    auto it = loops_.find(name);
    if (it == loops_.end() || !it->second.loaded) return;
    it->second.target = volume;
}

void AudioManager::PlayMusic(const std::string& name, float fade) {
    if (!ready_ || name == currentMusic_) return;
    for (auto& [n, t] : music_tracks_) {
        t.target = 0.0f;
        t.speed = fade > 0 ? 1.0f / fade : 100.0f;
    }
    auto it = music_tracks_.find(name);
    if (it != music_tracks_.end() && it->second.loaded) {
        Track& t = it->second;
        if (!IsMusicStreamPlaying(t.music)) {
            SeekMusicStream(t.music, 0.0f);
            PlayMusicStream(t.music);
            t.vol = 0.0f;
        }
        t.target = 1.0f;
    }
    currentMusic_ = name;
}

void AudioManager::StopMusic(float fade) {
    for (auto& [n, t] : music_tracks_) {
        t.target = 0.0f;
        t.speed = fade > 0 ? 1.0f / fade : 100.0f;
    }
    currentMusic_.clear();
}

void AudioManager::Update(float dt) {
    if (!ready_) return;
    for (auto& [n, t] : music_tracks_) {
        if (!t.loaded) continue;
        if (t.vol < t.target) t.vol = std::min(t.target, t.vol + t.speed * dt);
        else if (t.vol > t.target) t.vol = std::max(t.target, t.vol - t.speed * dt);
        bool playing = IsMusicStreamPlaying(t.music);
        if (t.vol <= 0.0f && t.target <= 0.0f) {
            if (playing) StopMusicStream(t.music);
            continue;
        }
        if (!playing) PlayMusicStream(t.music);
        SetMusicVolume(t.music, t.vol * music_ * master_);
        UpdateMusicStream(t.music);
    }
    for (auto& [n, l] : loops_) {
        if (!l.loaded) continue;
        l.vol += (l.target - l.vol) * std::min(1.0f, dt * 6.0f);
        if (l.vol > 0.01f) {
            if (!IsSoundPlaying(l.s)) PlaySound(l.s);
            SetSoundVolume(l.s, l.vol * sfx_ * master_);
        } else if (IsSoundPlaying(l.s)) {
            StopSound(l.s);
        }
    }
}

void AudioManager::SetVolumes(float master, float music, float sfx) {
    master_ = std::clamp(master, 0.0f, 1.0f);
    music_ = std::clamp(music, 0.0f, 1.0f);
    sfx_ = std::clamp(sfx, 0.0f, 1.0f);
}

}  // namespace lk
