// Session: braziers, gates (shortcuts, hidden walls, the mini-boss gate), pickups,
// floor hints, area triggers and the mini-boss group.
#include <algorithm>
#include <cmath>

#include "engine/assets.h"
#include "engine/audio.h"
#include "engine/data_store.h"
#include "engine/log.h"
#include "engine/physics.h"
#include "engine/ui.h"
#include "game/game_state.h"
#include "game/session.h"

namespace lk {

static Vector3 V3i(const json& j, Vector3 d) {
    if (!j.is_array() || j.size() < 3) return d;
    return Vector3{j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

static Vector3 YawDir(float deg) { return Vector3{std::sin(deg * DEG2RAD), 0, std::cos(deg * DEG2RAD)}; }

void Session::BuildWorldObjects() {
    const ZoneData& z = world_.Zone();
    const json& r = ctx_.data.Get("render.json");
    const json& bl = r.value("brazier_light", json::object());
    braziers_.clear();
    for (const auto& p : z.braziers) {
        Brazier b;
        b.id = p.id;
        b.pos = Vector3{p.pos.x, p.pos.y, p.pos.z};
        b.reveals = p.extra.value("reveals", "");
        b.arena = p.extra.value("arena", false);
        ZoneLight zl;
        zl.id = "brazier:" + b.id;
        zl.light.pos = Vector3Add(b.pos, Vector3{0, world_cfg_.value("brazier", json::object()).value("fire_height", 0.55f) + 0.3f, 0});
        zl.baseColor = V3i(bl.value("color", json()), Vector3{1, 0.5f, 0.18f});
        zl.baseIntensity = 0.0f;
        zl.light.range = bl.value("range", 9.0f);
        zl.flicker = bl.value("flicker_amount", 0.08f);
        zl.seed = float(zoneLights_.size()) * 2.1f;
        b.light = int(zoneLights_.size());
        zoneLights_.push_back(zl);
        braziers_.push_back(b);
    }
    ctx_.assets.LoadModel(world_cfg_.value("brazier", json::object()).value("model", "quaternius_props/Cauldron.gltf"));

    gates_.clear();
    auto addGate = [&](const Placement& p, GateKind kind) {
        Gate g;
        g.id = p.id;
        g.kind = kind;
        g.pos = Vector3{p.pos.x, p.pos.y, p.pos.z};
        g.yaw = p.rot;
        g.openDir = YawDir(p.extra.value("open_side", p.rot));
        g.unlockBoss = p.extra.value("unlock_boss", "");
        g.model = p.extra.value("model", kind == GateKind::Hidden ? "kaykit_dungeon/wall_cracked.gltf.glb"
                                                                  : "kaykit_dungeon/wall_gated.gltf.glb");
        if (kind != GateKind::Fog) ctx_.assets.LoadModel(g.model);
        // Collider: a wall-sized slab across the passage.
        Vector3 he = Vector3{z.cellSize * 0.5f, z.layerHeight * 0.5f, 0.3f};
        float yawCol = p.rot;
        g.body = ctx_.physics.AddToggleBox(Vector3Add(g.pos, Vector3{0, z.layerHeight * 0.5f, 0}), he, yawCol, true);
        int cx, cz;
        z.WorldToCell(g.pos.x, g.pos.z, cx, cz);
        g.navLevel = int(std::lround(g.pos.y / z.layerHeight));
        g.navX = cx;
        g.navZ = cz;
        gates_.push_back(g);
    };
    for (const auto& p : z.gates) {
        std::string k = p.extra.value("kind", "shortcut");
        addGate(p, k == "hidden" ? GateKind::Hidden : (k == "locked" ? GateKind::Locked : GateKind::Shortcut));
    }
    if (z.hasFogGate) addGate(z.fogGate, GateKind::Fog);

    pickups_.clear();
    for (const auto& p : z.items) {
        Pickup k;
        k.id = p.id;
        k.kind = p.extra.value("kind", "flask_shard");
        k.model = p.model;
        k.label = p.extra.value("label", k.kind == "weapon" ? "Take the greataxe" : "Pick up");
        k.pos = Vector3{p.pos.x, p.pos.y, p.pos.z};
        k.yaw = p.rot;
        if (!k.model.empty()) ctx_.assets.LoadModel(k.model);
        pickups_.push_back(k);
    }
    hints_.clear();
    for (const auto& p : z.hints) {
        FloorHint h;
        h.pos = Vector3{p.pos.x, p.pos.y, p.pos.z};
        h.text = p.extra.value("text", "");
        h.radius = p.extra.value("radius", 2.6f);
        h.action = p.extra.value("action", "");
        hints_.push_back(h);
    }
    triggers_.clear();
    for (const auto& p : z.triggers) {
        AreaTrigger t;
        t.id = p.id;
        t.name = p.extra.value("name", "");
        t.music = p.extra.value("music", "");
        Vector3 size = V3i(p.extra.value("size", json()), Vector3{6, 3, 6});
        Vector3 c{p.pos.x, p.pos.y, p.pos.z};
        t.box = BoundingBox{Vector3Subtract(c, Vector3Scale(size, 0.5f)), Vector3Add(c, Vector3Scale(size, 0.5f))};
        triggers_.push_back(t);
    }
}

void Session::ApplyWorldProgress() {
    const SaveData& p = ctx_.state.progress;
    const json& bl = ctx_.data.Get("render.json").value("brazier_light", json::object());
    for (auto& b : braziers_) {
        b.lit = p.litBraziers.count(b.id) > 0;
        if (b.arena && !p.defeatedBosses.count("hollow_warden")) b.lit = true;  // arena braziers burn until phase 2
        zoneLights_[size_t(b.light)].baseIntensity = (b.lit && !b.snuffed) ? bl.value("intensity", 6.0f) : 0.0f;
        zoneLights_[size_t(b.light)].halo = b.lit && !b.snuffed;
    }
    for (auto& g : gates_) {
        bool open = p.openedGates.count(g.id) > 0;
        if (g.kind == GateKind::Locked && !g.unlockBoss.empty() && p.defeatedBosses.count(g.unlockBoss)) open = true;
        if (g.kind == GateKind::Fog) open = p.defeatedBosses.count("hollow_warden") > 0;
        SetGateOpen(g, open, true);
    }
    for (auto& k : pickups_) k.taken = p.pickedItems.count(k.id) > 0;
    // Defeated mini-boss groups stay dead.
    for (auto& e : enemies_)
        if (!e->Def().bossGroup.empty() && p.defeatedBosses.count(e->Def().bossGroup)) e->Kill();
}

void Session::SetGateOpen(Gate& g, bool open, bool instant) {
    g.open = open;
    if (instant) g.anim = open ? 1.0f : 0.0f;
    ctx_.physics.SetToggle(g.body, !open);
    nav_.SetBlocked(g.navLevel, g.navX, g.navZ, !open);
}

void Session::LightBrazier(Brazier& b) {
    if (b.lit) return;
    b.lit = true;
    ctx_.state.progress.litBraziers.insert(b.id);
    const json& bl = ctx_.data.Get("render.json").value("brazier_light", json::object());
    zoneLights_[size_t(b.light)].baseIntensity = bl.value("intensity", 6.0f);
    zoneLights_[size_t(b.light)].halo = true;
    fx_.push_back({FxType::Sound, b.pos, 1.0f, "ignite"});
    fx_.push_back({FxType::Embers, Vector3Add(b.pos, Vector3{0, 0.6f, 0}), 3.0f, ""});
    if (!b.reveals.empty())
        for (auto& g : gates_)
            if (g.id == b.reveals && !g.open) {
                SetGateOpen(g, true, false);
                ctx_.state.progress.openedGates.insert(g.id);
                ctx_.state.progress.secretsFound.insert(g.id);
                fx_.push_back({FxType::Sound, g.pos, 1.0f, "slam"});
                fx_.push_back({FxType::Shake, g.pos, 0.3f, ""});
                ShowToast("A hidden passage opens");
            }
    SaveProgress();
}

void Session::ShowToast(const std::string& text) {
    toast_ = text;
    toastTime_ = 3.0f;
}

void Session::SnuffArenaBraziers(bool snuff) {
    const json& bl = ctx_.data.Get("render.json").value("brazier_light", json::object());
    for (auto& b : braziers_) {
        if (!b.arena) continue;
        b.snuffed = snuff;
        b.lit = !snuff;
        zoneLights_[size_t(b.light)].baseIntensity = snuff ? 0.0f : bl.value("intensity", 6.0f);
        zoneLights_[size_t(b.light)].halo = !snuff;
        if (snuff) fx_.push_back({FxType::Embers, Vector3Add(b.pos, Vector3{0, 0.6f, 0}), 2.0f, ""});
    }
}

// Candidate interactions in reach; returns true if one was handled this step.
bool Session::UpdateWorldInteract() {
    SaveData& p = ctx_.state.progress;
    Vector3 pp = player_.Pos();
    float best = 1e9f;
    enum class K { None, Brazier, Gate, Pickup } kind = K::None;
    int idx = -1;
    float br = world_cfg_.value("brazier", json::object()).value("interact_range", 1.6f);
    for (size_t i = 0; i < braziers_.size(); ++i) {
        const Brazier& b = braziers_[i];
        float d = Vector3Distance(b.pos, pp);
        if (!b.lit && !b.arena && d < br && d < best && std::fabs(b.pos.y - pp.y) < 1.0f) {
            best = d;
            kind = K::Brazier;
            idx = int(i);
        }
    }
    float gr = world_cfg_.value("gate", json::object()).value("interact_range", 1.8f);
    for (size_t i = 0; i < gates_.size(); ++i) {
        const Gate& g = gates_[i];
        if (g.open || g.kind == GateKind::Hidden) continue;
        float d = Vector3Distance(g.pos, pp);
        if (d < gr && d < best && std::fabs(g.pos.y - pp.y) < 1.0f) {
            best = d;
            kind = K::Gate;
            idx = int(i);
        }
    }
    float pr = world_cfg_.value("pickup_range", 1.4f);
    for (size_t i = 0; i < pickups_.size(); ++i) {
        const Pickup& k = pickups_[i];
        float d = Vector3Distance(k.pos, pp);
        if (!k.taken && d < pr && d < best && std::fabs(k.pos.y - pp.y) < 1.0f) {
            best = d;
            kind = K::Pickup;
            idx = int(i);
        }
    }
    if (kind == K::None || player_.State() != PState::Move) return false;
    bool press = player_.WantsInteract();
    if (kind == K::Brazier) {
        promptText_ = "Light the brazier";
        promptTarget_ = braziers_[size_t(idx)].id;
        if (press) {
            player_.StartInteract(world_cfg_.value("brazier", json::object()).value("light_time", 1.0f));
            pendingBrazier_ = braziers_[size_t(idx)].id;
        }
    } else if (kind == K::Gate) {
        Gate& g = gates_[size_t(idx)];
        promptTarget_ = g.id;
        if (g.kind == GateKind::Fog) {
            promptText_ = "Pass through the fog";
            if (press) EnterFogGate(g);
        } else {
            bool fromOpenSide = Vector3DotProduct(Vector3Subtract(pp, g.pos), g.openDir) > 0.0f;
            bool canOpen = g.kind == GateKind::Shortcut && fromOpenSide;
            promptText_ = canOpen ? "Open the gate" : "Examine the gate";
            if (press) {
                if (canOpen) {
                    player_.StartInteract(world_cfg_.value("gate", json::object()).value("open_time", 1.0f));
                    pendingGate_ = g.id;
                } else {
                    ShowToast(g.kind == GateKind::Locked ? "Sealed. Something guards the way." : "It does not open from this side");
                    fx_.push_back({FxType::Sound, g.pos, 1.0f, "gate_locked"});
                }
            }
        }
    } else if (kind == K::Pickup) {
        Pickup& k = pickups_[size_t(idx)];
        promptText_ = k.label;
        promptTarget_ = k.id;
        if (press) {
            k.taken = true;
            p.pickedItems.insert(k.id);
            if (k.kind == "flask_shard") {
                p.flaskShards++;
                int maxF = player_.Tuning().flaskMaxShards;
                p.flaskMax = std::min(maxF, p.flaskMax + 1);
                player_.flaskMax = p.flaskMax;
                player_.flasks = std::min(player_.flasks + 1, player_.flaskMax);
                ShowToast("Flask Shard: oil flasks +1");
            } else if (k.kind == "weapon") {
                ShowToast("Barbarian's Greataxe. Swap weapons at a shrine.");
            } else if (k.kind == "embers") {
                p.embers += 200;
                emberFlash_ = 1;
                ShowToast("A cache of embers");
            }
            fx_.push_back({FxType::Sound, k.pos, 1.0f, "pickup"});
            player_.StartInteract(0.6f);
            SaveProgress();
        }
    }
    return true;
}

void Session::FinishWorldInteract() {
    if (player_.State() != PState::Move) return;
    if (!pendingBrazier_.empty()) {
        for (auto& b : braziers_)
            if (b.id == pendingBrazier_) LightBrazier(b);
        pendingBrazier_.clear();
    }
    if (!pendingGate_.empty()) {
        for (auto& g : gates_)
            if (g.id == pendingGate_ && !g.open) {
                SetGateOpen(g, true, false);
                ctx_.state.progress.openedGates.insert(g.id);
                fx_.push_back({FxType::Sound, g.pos, 1.0f, "gate"});
                ShowToast("A shortcut opens");
                SaveProgress();
            }
        pendingGate_.clear();
    }
}

void Session::UpdateWorldObjects(float dt) {
    for (auto& g : gates_) g.anim = std::clamp(g.anim + (g.open ? dt : -dt) * 1.2f, 0.0f, 1.0f);
    toastTime_ = std::max(0.0f, toastTime_ - dt);
    areaBannerTime_ = std::max(0.0f, areaBannerTime_ - dt);
    // Area triggers: name banner and music.
    Vector3 pp = player_.Pos();
    for (const auto& t : triggers_) {
        bool inside = pp.x >= t.box.min.x && pp.x <= t.box.max.x && pp.y >= t.box.min.y && pp.y <= t.box.max.y &&
                      pp.z >= t.box.min.z && pp.z <= t.box.max.z;
        if (inside && currentArea_ != t.id) {
            currentArea_ = t.id;
            ctx_.state.currentArea = t.name;
            if (!t.name.empty()) {
                areaBanner_ = t.name;
                areaBannerTime_ = 3.5f;
            }
            if (!t.music.empty() && !ctx_.state.inBossFight) ctx_.audio.PlayMusic(t.music, 3.0f);
        }
    }
    // Floor hints.
    hintText_.clear();
    for (const auto& h : hints_)
        if (Vector3Distance(h.pos, pp) < h.radius && std::fabs(h.pos.y - pp.y) < 1.0f) hintText_ = h.text;
    // Brazier fire.
    unsigned seed = unsigned(steps_ * 2246822519u);
    auto rnd = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return float((seed >> 8) & 0xFFFF) / 65535.0f;
    };
    float fh = world_cfg_.value("brazier", json::object()).value("fire_height", 0.55f);
    for (const auto& b : braziers_) {
        if (!b.lit || b.snuffed || Vector3Distance(b.pos, pp) > drawDistance_) continue;
        for (int k = 0; k < 2; ++k) {
            ParticleDesc d;
            d.pos = Vector3{b.pos.x + (rnd() - 0.5f) * 0.35f, b.pos.y + fh, b.pos.z + (rnd() - 0.5f) * 0.35f};
            d.vel = Vector3{(rnd() - 0.5f) * 0.2f, 0.9f + rnd() * 0.7f, (rnd() - 0.5f) * 0.2f};
            d.life = 0.5f + rnd() * 0.4f;
            d.size0 = 0.16f + rnd() * 0.08f;
            d.size1 = 0.02f;
            d.color0 = Vector4{6.0f, 2.4f, 0.6f, 0.9f};
            d.color1 = Vector4{2.0f, 0.3f, 0.05f, 0.0f};
            particles_.Emit(d);
        }
    }
    // Mini-boss groups: boss bar while engaged, record the defeat.
    UpdateBossGroups();
}

void Session::UpdateBossGroups() {
    SaveData& p = ctx_.state.progress;
    bossBar_ = BossBar{};
    std::map<std::string, std::vector<Enemy*>> groups;
    for (auto& e : enemies_)
        if (!e->Def().bossGroup.empty()) groups[e->Def().bossGroup].push_back(e.get());
    for (auto& [group, members] : groups) {
        if (p.defeatedBosses.count(group)) continue;
        float hp = 0, hpMax = 0;
        bool engaged = false, anyAlive = false;
        for (Enemy* e : members) {
            hp += e->Hp();
            hpMax += e->HpMax();
            engaged = engaged || (e->Aggro() && e->Alive());
            anyAlive = anyAlive || e->Alive();
        }
        if (!anyAlive) {
            p.defeatedBosses.insert(group);
            if (members.front()->Def().brain == "boss") {
                OnBossDefeated(members.front()->Def());
                continue;
            }
            for (auto& g : gates_)
                if (g.kind == GateKind::Locked && g.unlockBoss == group && !g.open) {
                    SetGateOpen(g, true, false);
                    fx_.push_back({FxType::Sound, g.pos, 1.0f, "gate"});
                }
            areaBanner_ = members.front()->Def().bossName + " defeated";
            areaBannerTime_ = 4.0f;
            SaveProgress();
            continue;
        }
        if (engaged) {
            bossBar_.active = true;
            bossBar_.name = members.front()->Def().bossName;
            bossBar_.hp = hpMax > 0 ? hp / hpMax : 0;
        }
    }
}

void Session::DrawWorldObjects(Renderer& r) {
    const json& bc = world_cfg_.value("brazier", json::object());
    ModelAsset* cauldron = ctx_.assets.GetModel(bc.value("model", "quaternius_props/Cauldron.gltf"));
    float bs = bc.value("scale", 1.3f) * 0.5f;
    Vector3 cam = cam_.Cam().position;
    for (const auto& b : braziers_) {
        if (!cauldron || Vector3Distance(b.pos, cam) > drawDistance_) continue;
        Matrix m = MatrixMultiply(MatrixScale(bs * 2.0f, bs * 2.0f, bs * 2.0f), MatrixTranslate(b.pos.x, b.pos.y, b.pos.z));
        DrawParams dp;
        if (b.lit && !b.snuffed) dp.emissive = Vector3{0.15f, 0.05f, 0.01f};
        r.DrawModel(*cauldron, m, nullptr, dp);
    }
    const ZoneData& z = world_.Zone();
    for (const auto& g : gates_) {
        if (g.kind == GateKind::Fog || g.anim >= 0.999f) continue;
        ModelAsset* a = ctx_.assets.GetModel(g.model);
        if (!a) continue;
        // Shortcut and locked gates slide down into the floor; hidden walls sink and fade.
        float drop = g.anim * z.layerHeight * 1.05f;
        Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(0.5f, 0.5f, 0.5f), MatrixRotateY(g.yaw * DEG2RAD)),
                                  MatrixTranslate(g.pos.x, g.pos.y - drop, g.pos.z));
        r.DrawModel(*a, m, nullptr, DrawParams{});
    }
    for (const auto& k : pickups_) {
        if (k.taken) continue;
        ModelAsset* a = k.model.empty() ? nullptr : ctx_.assets.GetModel(k.model);
        float bob = std::sin(realTime_ * 2.0f + k.pos.x) * 0.06f;
        if (a) {
            float s = k.kind == "weapon" ? 0.5f : 1.6f;
            Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(s, s, s), MatrixRotateY(realTime_ * 0.8f + k.yaw * DEG2RAD)),
                                      MatrixTranslate(k.pos.x, k.pos.y + 0.45f + bob, k.pos.z));
            DrawParams dp;
            dp.emissive = k.kind == "flask_shard" ? Vector3{1.2f, 0.5f, 0.15f} : Vector3{0.15f, 0.12f, 0.08f};
            r.DrawModel(*a, m, nullptr, dp);
        }
    }
}

void Session::DrawWorldAdditive(Renderer& r) {
    const ZoneData& z = world_.Zone();
    for (const auto& b : braziers_)
        if (b.lit && !b.snuffed)
            r.DrawHalo(Vector3Add(b.pos, Vector3{0, 0.7f, 0}), 1.6f, Color{255, 140, 60, 255}, 1.6f * Flicker(realTime_, 9.0f, 0.1f, b.pos.x));
    for (const auto& k : pickups_)
        if (!k.taken) r.DrawHalo(Vector3Add(k.pos, Vector3{0, 0.5f, 0}), 0.9f, Color{255, 200, 120, 255}, 1.0f + 0.4f * std::sin(realTime_ * 3.0f));
    // Fog gate: layered drifting veils.
    for (const auto& g : gates_) {
        if (g.kind != GateKind::Fog || g.open) continue;
        Vector3 c = Vector3Add(g.pos, Vector3{0, z.layerHeight * 0.5f, 0});
        Vector3 side = Vector3{std::cos(g.yaw * DEG2RAD), 0, -std::sin(g.yaw * DEG2RAD)};
        for (int i = -3; i <= 3; ++i) {
            float t = realTime_ * 0.6f + float(i);
            Vector3 p = Vector3Add(c, Vector3Add(Vector3Scale(side, float(i) * 0.3f), Vector3{0, std::sin(t) * 0.25f, 0}));
            r.DrawHalo(p, 2.2f, Color{170, 190, 230, 255}, 0.55f);
        }
    }
    // Floor hint runes.
    for (const auto& h : hints_)
        r.DrawHalo(Vector3Add(h.pos, Vector3{0, 0.05f, 0}), 0.8f, Color{255, 170, 90, 255}, 0.5f + 0.2f * std::sin(realTime_ * 2.0f));
}

}  // namespace lk
