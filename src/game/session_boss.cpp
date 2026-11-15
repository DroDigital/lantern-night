// Session: the Hollow Warden fight - fog gate, phases, shockwaves, charge, reset, victory, credits.
#include <algorithm>
#include <cmath>

#include "engine/audio.h"
#include "engine/data_store.h"
#include "engine/log.h"
#include "engine/ui.h"
#include "game/game_state.h"
#include "game/session.h"

namespace lk {

static const char* kBossGroup = "hollow_warden";

Enemy* Session::Boss() {
    for (auto& e : enemies_)
        if (e->Def().brain == "boss") return e.get();
    return nullptr;
}

StrikeResult Session::DebugStrike(Enemy* e, float damage, float posture) {
    PlayerStrike s;
    s.damage = damage;
    s.posture = posture;
    s.heavy = true;
    s.from = player_.Pos();
    s.point = e->LockPoint();
    StrikeResult r = e->TakeStrike(s, 3.0f, fx_);
    if (r.killed) ++kills_;
    return r;
}

Gate* Session::FindGate(const std::string& id) {
    for (auto& g : gates_)
        if (g.id == id) return &g;
    return nullptr;
}

void Session::DebugEnterFog() {
    for (auto& g : gates_)
        if (g.kind == GateKind::Fog) EnterFogGate(g);
}

void Session::EnterFogGate(Gate& g) {
    // Walk through the fog into the arena; the fog seals behind until the Warden falls.
    Vector3 through = Vector3Subtract(g.pos, Vector3Scale(g.openDir, 1.8f));
    player_.Teleport(through, g.yaw + 180.0f);
    cam_.Reset(through, (g.yaw + 180.0f) * DEG2RAD + PI);
    fx_.push_back({FxType::Sound, g.pos, 1.0f, "ignite"});
    StartBossFight();
}

void Session::StartBossFight() {
    Enemy* b = Boss();
    if (!b || !b->Alive() || ctx_.state.progress.defeatedBosses.count(kBossGroup)) return;
    ctx_.state.inBossFight = true;
    b->SetDormant(false);
    b->ForceAggro();
    ctx_.audio.PlayMusic("boss", 1.0f);
    areaBanner_ = b->Def().bossName;
    areaBannerTime_ = 3.0f;
    fx_.push_back({FxType::Sound, b->LockPoint(), 1.0f, "boss_roar"});
}

void Session::ResetBossFight() {
    if (!ctx_.state.inBossFight) return;
    ctx_.state.inBossFight = false;
    if (Enemy* b = Boss()) {
        b->Respawn();
        b->SetDormant(true);
    }
    // Summoned adds vanish; arena braziers burn again.
    enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(),
                                  [](const std::unique_ptr<Enemy>& e) { return e->SpawnId().rfind("summon_", 0) == 0; }),
                   enemies_.end());
    SnuffArenaBraziers(false);
    waves_.clear();
    ctx_.audio.PlayMusic(world_.Zone().ambientMusic, 2.0f);
}

void Session::OnBossPhase(const FxEvent& e) {
    Enemy* b = Boss();
    if (!b) return;
    const EnemyDef& d = b->Def();
    SnuffArenaBraziers(true);
    cam_.AddShake(0.9f);
    flash_ = 0.25f;
    flashColor_ = Color{40, 10, 10, 255};
    flashDecay_ = 1.5f;
    if (!summoned_ && !d.summonKind.empty()) {
        summoned_ = true;
        for (int i = 0; i < d.summonCount; ++i) {
            float a = float(i) / float(std::max(1, d.summonCount)) * 2.0f * PI + 0.4f;
            Vector3 p = Vector3Add(b->Pos(), Vector3{std::cos(a) * d.summonRadius, 0, std::sin(a) * d.summonRadius});
            if (!nav_.Walkable(p)) p = Vector3Add(b->Pos(), Vector3{std::cos(a) * 2.5f, 0, std::sin(a) * 2.5f});
            if (Enemy* m = SpawnEnemy(d.summonKind, p, 0.0f, "summon_" + std::to_string(i))) {
                m->ForceAggro();
                fx_.push_back({FxType::Embers, Vector3Add(p, Vector3{0, 0.5f, 0}), 2.0f, ""});
            }
        }
    }
    (void)e;
}

void Session::UpdateShockwaves(float dt) {
    unsigned seed = unsigned(steps_ * 747796405u);
    auto rnd = [&]() {
        seed = seed * 1664525u + 1013904223u;
        return float((seed >> 8) & 0xFFFF) / 65535.0f;
    };
    for (auto& w : waves_) {
        w.radius += w.speed * dt;
        // Ring of dust and sparks along the front.
        int n = int(w.radius * 4.0f);
        for (int i = 0; i < n; ++i) {
            if (rnd() > 0.35f) continue;
            float a = rnd() * 2.0f * PI;
            ParticleDesc d;
            d.pos = Vector3{w.center.x + std::cos(a) * w.radius, w.center.y + 0.1f, w.center.z + std::sin(a) * w.radius};
            d.vel = Vector3{std::cos(a) * 1.5f, 1.2f + rnd(), std::sin(a) * 1.5f};
            d.life = 0.35f;
            d.size0 = 0.12f;
            d.size1 = 0.0f;
            d.color0 = Vector4{4.0f, 2.0f, 0.8f, 0.9f};
            d.color1 = Vector4{0.5f, 0.3f, 0.2f, 0.0f};
            d.gravity = Vector3{0, -6, 0};
            particles_.Emit(d);
        }
        // Hit the knight when the front passes their feet (roll i-frames avoid it).
        Vector3 pp = player_.Pos();
        float dist = Vector2Distance({pp.x, pp.z}, {w.center.x, w.center.z});
        if (!w.hitPlayer && std::fabs(dist - w.radius) < 0.5f && std::fabs(pp.y - w.center.y) < 0.6f) {
            IncomingHit h;
            h.damage = w.damage;
            h.from = w.center;
            h.parryable = false;
            h.blockable = false;
            IncomingResult r = player_.ReceiveHit(h, ctx_.state.godMode, fx_);
            if (!r.ignored) w.hitPlayer = true;
        }
    }
    waves_.erase(std::remove_if(waves_.begin(), waves_.end(), [](const Shockwave& w) { return w.radius > w.maxRadius; }),
                 waves_.end());
    // Shield charge: the Warden's body is the hitbox.
    for (auto& e : enemies_) {
        if (!e->ChargeActive() || e->Swing().hitIds.count(0)) continue;
        geom::Capsule body = e->Hurtbox();
        if (!geom::CapsuleOverlap(body, player_.Hurtbox())) continue;
        e->Swing().hitIds.insert(0);
        IncomingHit h;
        h.damage = e->CurrentMove() ? e->CurrentMove()->damage : 25.0f;
        h.posture = 25.0f;
        h.from = e->Pos();
        h.parryable = false;
        player_.ReceiveHit(h, ctx_.state.godMode, fx_);
        cam_.AddShake(0.6f);
    }
}

void Session::OnBossDefeated(const EnemyDef& d) {
    SaveData& p = ctx_.state.progress;
    const json& v = d.raw.value("victory", json::object());
    ctx_.state.inBossFight = false;
    SnuffArenaBraziers(false);
    for (auto& g : gates_)
        if (g.kind == GateKind::Fog) SetGateOpen(g, true, false);
    for (auto& e : enemies_)
        if (e->SpawnId().rfind("summon_", 0) == 0 && e->Alive()) e->Kill();
    p.stats.vigor += v.value("reward_vigor", 2);
    p.pickedItems.insert("warden_ember");
    p.gameCleared = true;
    player_.ApplyStats(p.stats.vigor, p.stats.endurance, p.stats.might);
    player_.flame = player_.flameMax;
    victoryBanner_ = v.value("banner", "FLAME REKINDLED");
    victoryTime_ = 0.001f;
    creditsAfter_ = v.value("credits_after", 6.0f);
    ctx_.audio.PlayMusic(world_.Zone().ambientMusic, 3.0f);
    ctx_.audio.Play("level_up");
    SaveProgress();
}

void Session::UpdateVictory(float dt) {
    if (victoryTime_ <= 0) return;
    victoryTime_ += dt;
    if (victoryTime_ > creditsAfter_ && menu_ == Menu::None && !creditsShown_) {
        creditsShown_ = true;
        menu_ = Menu::Credits;
        creditsScroll_ = 0;
    }
}

void Session::RenderVictory() {
    Ui& ui = ctx_.ui;
    if (victoryTime_ > 0 && victoryTime_ < creditsAfter_ + 0.5f) {
        float a = std::clamp(std::min(victoryTime_ * 1.2f, (creditsAfter_ + 0.5f - victoryTime_) * 1.5f), 0.0f, 1.0f);
        DrawBanner(ui, victoryBanner_, a, Color{255, 190, 90, 255}, 104);
        if (victoryTime_ > 1.5f)
            ui.Text("The Warden's ember feeds your lantern (+2 Vigor)", ui.W() * 0.5f, ui.H() * 0.42f + 130, 30,
                    Color{240, 220, 190, (unsigned char)(a * 255)}, Align::Center);
    }
    if (menu_ == Menu::Credits) {
        creditsScroll_ += GetFrameTime() * 60.0f;
        ui.Rect({0, 0, ui.W(), ui.H()}, Color{0, 0, 0, 220});
        const json& c = ctx_.data.Get("credits.json");
        float y = ui.H() - creditsScroll_;
        ui.Text(c.value("title", "Lantern Knight"), ui.W() * 0.5f, y, 72, Color{255, 200, 120, 255}, Align::Center);
        y += 140;
        for (const auto& sec : c.value("sections", json::array())) {
            std::string h = sec.value("heading", "");
            if (!h.empty()) {
                ui.Text(h, ui.W() * 0.5f, y, 40, Color{255, 180, 100, 255}, Align::Center);
                y += 64;
            }
            for (const auto& line : sec.value("lines", json::array())) {
                ui.Text(line.get<std::string>(), ui.W() * 0.5f, y, 28, Color{230, 225, 215, 255}, Align::Center);
                y += 44;
            }
            y += 50;
        }
        ui.Prompt(Act::Confirm, "Continue", ui.W() - 300, ui.H() - 90, 44);
        if (ui.BackPressed() || ctx_.input.Peek().Pressed(Act::Confirm) || y < -50) menu_ = Menu::None;
    }
}

}  // namespace lk
