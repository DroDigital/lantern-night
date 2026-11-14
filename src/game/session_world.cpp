// Session: shrines, resting, leveling, death and the ember pile, menus and save/load.
#include <algorithm>
#include <cmath>

#include "core/combat_rules.h"
#include "engine/assets.h"
#include "engine/audio.h"
#include "engine/data_store.h"
#include "engine/log.h"
#include "engine/ui.h"
#include "game/game_state.h"
#include "game/save_system.h"
#include "game/session.h"

namespace lk {

static Vector3 V3w(const json& j, Vector3 d) {
    if (!j.is_array() || j.size() < 3) return d;
    return Vector3{j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
}

void Session::BuildInteractables() {
    world_cfg_ = ctx_.data.Get("world.json");
    const json& sc = world_cfg_.value("shrine", json::object());
    shrines_.clear();
    for (const auto& p : world_.Zone().shrines) {
        Shrine s;
        s.id = p.id;
        s.name = p.extra.value("name", std::string("Lantern Shrine"));
        s.pos = Vector3{p.pos.x, p.pos.y, p.pos.z};
        s.yaw = p.rot;
        ZoneLight zl;
        zl.id = "shrine:" + s.id;
        zl.light.pos = Vector3Add(s.pos, Vector3{0, sc.value("lantern_height", 0.95f) + 0.2f, 0});
        zl.light.range = 3.0f;
        zl.flicker = 0.05f;
        zl.seed = float(zoneLights_.size()) * 1.7f;
        s.light = int(zoneLights_.size());
        zoneLights_.push_back(zl);
        shrines_.push_back(s);
        ctx_.assets.LoadModel(sc.value("pedestal", "kaykit_dungeon/column.gltf.glb"));
    }
}

// Bring the world in line with saved progress (lit shrines, stats, flasks, weapon, pile).
void Session::ApplyProgress() {
    SaveData& p = ctx_.state.progress;
    const json& sc = world_cfg_.value("shrine", json::object());
    const json& lit = sc.value("light_lit", json::object());
    const json& unlit = sc.value("light_unlit", json::object());
    for (auto& s : shrines_) {
        bool on = p.litShrines.count(s.id) > 0;
        ZoneLight& zl = zoneLights_[size_t(s.light)];
        const json& L = on ? lit : unlit;
        zl.baseColor = V3w(L.value("color", json()), Vector3{1, 0.7f, 0.35f});
        zl.baseIntensity = L.value("intensity", 1.0f);
        zl.light.range = L.value("range", 5.0f);
        zl.halo = on;
    }
    player_.ApplyStats(p.stats.vigor, p.stats.endurance, p.stats.might);
    player_.flaskMax = p.flaskMax;
    player_.flasks = std::min(p.flasks, p.flaskMax);
    if (p.weapon != player_.Weapon().key) EquipWeapon(p.weapon);
}

bool Session::EquipWeapon(const std::string& key) {
    std::string file = "weapons/" + key + ".json";
    if (!ctx_.data.Has(file)) return false;
    WeaponDef w;
    w.Load(key, ctx_.data.Get(file));
    if (!player_.Equip(w)) return false;
    ctx_.state.progress.weapon = key;
    return true;
}

Vector3 Session::RespawnPoint(float* yawDeg) const {
    const SaveData& p = ctx_.state.progress;
    for (const auto& s : shrines_)
        if (s.id == p.lastShrine) {
            float off = world_cfg_.value("shrine", json::object()).value("respawn_offset", 1.3f);
            float y = s.yaw * DEG2RAD;
            if (yawDeg) *yawDeg = s.yaw;
            return Vector3Add(s.pos, Vector3{std::sin(y) * off, 0, std::cos(y) * off});
        }
    const ZoneData& z = world_.Zone();
    if (yawDeg) *yawDeg = z.playerYaw;
    return Vector3{z.playerSpawn.x, z.playerSpawn.y, z.playerSpawn.z};
}

void Session::RespawnWorld() {
    for (auto& e : enemies_)
        if (!ctx_.state.progress.defeatedBosses.count(e->SpawnId()) && e->Def().brain != "boss") e->Respawn();
    tokens_.Clear();
    projectiles_.clear();
    lockId_ = -1;
}

void Session::RestAtShrine(const Shrine& s) {
    SaveData& p = ctx_.state.progress;
    p.lastShrine = s.id;
    player_.flame = player_.flameMax;
    player_.stamina.value = player_.stamina.max;
    player_.flasks = player_.flaskMax = p.flaskMax;
    p.flasks = p.flaskMax;
    RespawnWorld();
    ++restCount_;
    ctx_.audio.PlayAt("rest", s.pos);
    SaveProgress();
}

bool Session::SaveProgress() {
    SaveData& p = ctx_.state.progress;
    p.flasks = player_.flasks;
    p.flaskMax = player_.flaskMax;
    p.weapon = player_.Weapon().key;
    return ctx_.save.Save(p);
}

void Session::UpdateInteract(const InputFrame& in, float dt) {
    (void)in;
    SaveData& p = ctx_.state.progress;
    promptText_.clear();
    promptTarget_.clear();
    pileGlow_ += dt;
    if (!player_.Alive() || menu_ != Menu::None) return;

    // Ember pile: walking over it returns the embers.
    if (p.pile.active) {
        Vector3 pp{p.pile.x, p.pile.y, p.pile.z};
        float r = world_cfg_.value("ember_pile", json::object()).value("pickup_range", 1.1f);
        if (Vector3Distance(pp, player_.Pos()) < r) {
            EmberState es{p.embers, p.pile.active, p.pile.amount, p.pile.x, p.pile.y, p.pile.z};
            int got = RecoverPile(es);
            p.embers = es.carried;
            p.pile.active = false;
            p.pile.amount = 0;
            emberFlash_ = 1.0f;
            fx_.push_back({FxType::Embers, pp, 3.0f, ""});
            ctx_.audio.PlayAt("embers", pp);
            LK_INFO("recovered %d embers", got);
        }
    }

    // Finish a pending interaction once the knight's animation completes.
    if (!pendingInteract_.empty() && player_.State() == PState::Move) {
        std::string id = pendingInteract_;
        pendingInteract_.clear();
        for (const auto& s : shrines_)
            if (s.id == id && !p.litShrines.count(id)) {
                p.litShrines.insert(id);
                p.lastShrine = id;
                ApplyProgress();
                fx_.push_back({FxType::Sound, s.pos, 1.0f, "ignite"});
                fx_.push_back({FxType::Embers, Vector3Add(s.pos, Vector3{0, 1.0f, 0}), 2.0f, ""});
                SaveProgress();
            }
    }

    // What is in reach?
    const Shrine* near = nullptr;
    float range = world_cfg_.value("shrine", json::object()).value("interact_range", 1.8f);
    for (const auto& s : shrines_)
        if (Vector3Distance(s.pos, player_.Pos()) < range && std::fabs(s.pos.y - player_.Pos().y) < 1.0f) near = &s;
    if (near && player_.State() == PState::Move) {
        bool lit = p.litShrines.count(near->id) > 0;
        promptText_ = lit ? "Rest" : "Light the shrine";
        promptTarget_ = near->id;
        if (player_.WantsInteract()) {
            if (!lit) {
                player_.StartInteract(world_cfg_.value("shrine", json::object()).value("light_time", 1.0f));
                pendingInteract_ = near->id;
            } else {
                player_.StartRest();
                restingAt_ = near;
                RestAtShrine(*near);
                menu_ = Menu::Shrine;
                shrineMenu_ = ShrineMenuState{};
                shrineMenu_.shrineName = near->name;
                ctx_.ui.ResetFocus();
            }
        }
    }
}

void Session::UpdateDeath(float dt) {
    if (player_.Alive()) {
        deathTimer_ = 0;
        return;
    }
    if (deathTimer_ == 0.0f) {
        // Drop every carried ember where the flame went out; an older pile is lost.
        SaveData& p = ctx_.state.progress;
        EmberState es{p.embers, p.pile.active, p.pile.amount, p.pile.x, p.pile.y, p.pile.z};
        Vector3 at = player_.Pos();
        int lost = OnPlayerDeath(es, at.x, at.y, at.z);
        p.embers = es.carried;
        p.pile = EmberPile{es.pileActive, es.pileX, es.pileY, es.pileZ, es.pileAmount};
        ++deathCount_;
        lockId_ = -1;
        if (lost > 0) LK_INFO("%d embers lost with the old pile", lost);
    }
    deathTimer_ += dt;
    float respawnAfter = world_cfg_.value("death", json::object()).value("respawn_after", 4.0f);
    if (deathTimer_ >= respawnAfter) {
        float yaw = 0;
        Vector3 at = RespawnPoint(&yaw);
        player_.Revive(at, yaw);
        player_.flasks = player_.flaskMax = ctx_.state.progress.flaskMax;
        RespawnWorld();
        cam_.Reset(at, yaw * DEG2RAD + PI);
        deathTimer_ = 0;
        SaveProgress();
    }
}

void Session::UpdateMenus(const InputFrame& in) {
    if (menu_ == Menu::None && in.Pressed(Act::Pause) && player_.Alive()) OpenPause();
}

void Session::RenderMenus() {
    Ui& ui = ctx_.ui;
    SaveData& p = ctx_.state.progress;
    if (menu_ == Menu::Shrine) {
        bool hasAxe = p.pickedItems.count("greataxe") > 0;
        MenuAction a = DrawShrineMenu(ui, shrineMenu_, p, ctx_.data.Get("player/leveling.json"), hasAxe, player_.Weapon().name);
        const json& lv = ctx_.data.Get("player/leveling.json");
        auto levelUp = [&](int& stat) {
            int cost = LevelUpCost(p.stats.Level(), lv.value("cost_base", 50.0f), lv.value("cost_growth", 1.15f));
            if (p.embers < cost || stat >= lv.value("max_level_per_stat", 30)) return;
            p.embers -= cost;
            ++stat;
            player_.ApplyStats(p.stats.vigor, p.stats.endurance, p.stats.might);
            player_.flame = player_.flameMax;
            ctx_.audio.Play("level_up");
            SaveProgress();
        };
        switch (a) {
            case MenuAction::LevelVigor: levelUp(p.stats.vigor); break;
            case MenuAction::LevelEndurance: levelUp(p.stats.endurance); break;
            case MenuAction::LevelMight: levelUp(p.stats.might); break;
            case MenuAction::SwapWeapon:
                EquipWeapon(player_.Weapon().key == "sword" ? "greataxe" : "sword");
                player_.StartRest();
                ctx_.audio.Play("armor");
                SaveProgress();
                break;
            case MenuAction::Close:
                menu_ = Menu::None;
                player_.EndRest();
                ctx_.audio.Play("ui_back");
                break;
            default: break;
        }
        if (ui.MovedFocus()) ctx_.audio.Play("ui_move");
    } else if (menu_ == Menu::Pause) {
        MenuAction a = DrawPauseMenu(ui);
        if (a == MenuAction::Resume) menu_ = Menu::None;
        if (a == MenuAction::QuitToDesktop) {
            SaveProgress();
            wantsQuit_ = true;
        }
        if (ui.MovedFocus()) ctx_.audio.Play("ui_move");
    }
    // Death banner.
    if (!player_.Alive()) {
        const json& d = world_cfg_.value("death", json::object());
        float t = deathTimer_ - d.value("banner_delay", 0.6f);
        float a = std::clamp(t * 1.5f, 0.0f, 1.0f) * std::clamp((d.value("respawn_after", 4.0f) - deathTimer_) * 2.0f, 0.0f, 1.0f);
        if (a > 0) DrawBanner(ui, d.value("banner_text", "YOUR FLAME GOES OUT"), a, Color{235, 120, 60, 255});
    }
}

void Session::DrawInteractables(Renderer& r) {
    const json& sc = world_cfg_.value("shrine", json::object());
    ModelAsset* ped = ctx_.assets.GetModel(sc.value("pedestal", "kaykit_dungeon/column.gltf.glb"));
    const SaveData& p = ctx_.state.progress;
    for (const auto& s : shrines_) {
        bool lit = p.litShrines.count(s.id) > 0;
        if (ped) {
            Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(0.5f, 0.5f, 0.5f), MatrixRotateY(s.yaw * DEG2RAD)),
                                      MatrixTranslate(s.pos.x, s.pos.y, s.pos.z));
            r.DrawModel(*ped, m, nullptr, DrawParams{});
        }
        float h = sc.value("lantern_height", 0.95f);
        float sc3 = sc.value("lantern_scale", 3.2f);
        Vector3 lp = Vector3Add(s.pos, Vector3{0, h + 0.1f * sc3, 0});
        float flick = Flicker(realTime_, 7.0f, 0.06f, s.pos.x);
        lanternMesh_.Draw(r, lp, s.yaw * DEG2RAD, lit ? 1.0f : 0.05f, flick, Vector3{1.0f, 0.6f, 0.25f}, sc3);
    }
}

}  // namespace lk
