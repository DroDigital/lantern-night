#include "game/selftest.h"

#include <cmath>
#include <functional>
#include <map>
#include <memory>

#include "engine/input.h"
#include "engine/log.h"
#include "game/context.h"
#include "game/game_state.h"
#include "game/session.h"

namespace lk {
namespace {

struct Harness {
    Ctx& ctx;
    std::unique_ptr<Session> s;
    int failures = 0;

    explicit Harness(Ctx& c) : ctx(c) {}

    bool Fresh() {
        s.reset();
        s = std::make_unique<Session>(ctx);
        if (!s->Load("training.json")) return false;
        Enemy* e = Dummy();
        // Stand 1 m in front of the dummy, facing it.
        Vector3 ep = e->Pos();
        s->GetPlayer().Teleport(Vector3{ep.x, ep.y, ep.z + 1.0f}, 180.0f);
        s->ForceLock(e->Id());
        Step({});
        return true;
    }
    Enemy* Dummy() { return s->Enemies().empty() ? nullptr : s->Enemies()[0].get(); }
    Player& P() { return s->GetPlayer(); }

    // Run exactly one simulation step with this input (hitstop is skipped through).
    void Step(const InputFrame& in) {
        int before = s->StepCount();
        ctx.input.PushScripted(in);
        for (int k = 0; k < 64 && s->StepCount() == before; ++k) s->Advance(1.0f / 60.0f);
    }
    void Run(int n, const std::function<InputFrame(int)>& f = nullptr) {
        for (int i = 0; i < n; ++i) Step(f ? f(i) : InputFrame{});
    }
    void Check(bool ok, const char* what) {
        if (ok) LK_INFO("selftest: PASS %s", what);
        else {
            LK_ERROR("selftest: FAIL %s", what);
            ++failures;
        }
    }
    static InputFrame Press(Act a) {
        InputFrame f;
        f.Press(a);
        return f;
    }
    // Steps from TriggerAttackSoon() until the dummy's blade becomes active (player idle).
    int MeasureSwingDelay() {
        Fresh();
        Dummy()->TriggerAttackSoon();
        for (int i = 0; i < 240; ++i) {
            Step({});
            if (Dummy()->AttackActive()) return i;
        }
        return -1;
    }
};

}  // namespace

int RunCombatSelfTest(Ctx& ctx) {
    Harness h(ctx);
    ctx.input.SetScripted(true);
    if (!h.Fresh()) {
        LK_ERROR("selftest: training level failed to load");
        return 1;
    }
    const float hpMax = h.Dummy()->HpMax();
    const float light = h.P().Weapon().dmgLight;

    // 1. A light attack lands.
    h.Step(Harness::Press(Act::Attack));
    h.Run(45);
    h.Check(std::fabs((hpMax - h.Dummy()->Hp()) - light) < 0.01f, "light attack deals weapon damage once");

    // 2. Three buffered presses chain the full combo.
    h.Fresh();
    int maxCombo = 0;
    h.Run(110, [&](int i) {
        maxCombo = std::max(maxCombo, h.P().ComboIndex());
        return (i == 0 || i == 24 || i == 48) ? Harness::Press(Act::Attack) : InputFrame{};
    });
    h.Check(maxCombo == 2, "three presses reach the third combo hit");
    h.Check(hpMax - h.Dummy()->Hp() >= light * 3 - 0.01f, "all three combo hits land");
    h.Check(h.P().stamina.value < h.P().stamina.max, "attacks spend stamina");

    int delay = h.MeasureSwingDelay();
    h.Check(delay > 10, "dummy swing has a readable wind-up");

    // 3. A clean hit on the knight costs flame (and tells us the step the hit lands on).
    h.Fresh();
    h.Dummy()->TriggerAttackSoon();
    int land = -1;
    h.Run(delay + 20, [&](int i) {
        if (land < 0 && h.P().flame < h.P().flameMax) land = i - 1;
        return InputFrame{};
    });
    LK_INFO("selftest: swing activates after %d steps, lands after %d", delay, land);
    float dmg = h.Dummy()->Def().moves[0].damage;
    h.Check(std::fabs((h.P().flameMax - h.P().flame) - dmg) < 0.01f, "unguarded hit removes the move's damage in flame");

    // 4. Blocking halves damage and costs stamina.
    h.Fresh();
    h.Dummy()->TriggerAttackSoon();
    h.Run(delay + 20, [&](int) {
        InputFrame f;
        f.Set(Act::Block, true);
        return f;
    });
    h.Check(std::fabs((h.P().flameMax - h.P().flame) - dmg * 0.5f) < 0.01f, "block halves damage");
    h.Check(h.P().stamina.value < h.P().stamina.max, "block costs stamina");

    // 5. Block pressed 0.1 s before the hit parries: the dummy staggers, no damage.
    h.Fresh();
    h.Dummy()->TriggerAttackSoon();
    bool staggered = false;
    h.Run(delay + 20, [&](int i) {
        staggered = staggered || h.Dummy()->Staggered();
        return i == land - 6 ? Harness::Press(Act::Block) : InputFrame{};
    });
    h.Check(staggered, "parry staggers the attacker");
    h.Check(h.P().flame == h.P().flameMax, "parry takes no damage");

    // 6. Riposte on the staggered dummy deals a critical (3x).
    float before = h.Dummy()->Hp();
    if (h.Dummy()->Staggered()) {
        h.Step(Harness::Press(Act::Attack));
        h.Run(60);
    }
    h.Check(before - h.Dummy()->Hp() >= light * 3 - 0.01f, "riposte deals a 3x critical");

    // 7. Rolling through the swing (i-frames 0.08-0.33 s) avoids damage.
    h.Fresh();
    h.Dummy()->TriggerAttackSoon();
    h.Run(delay + 20, [&](int i) {
        InputFrame f;
        if (i == land - 9) f.Press(Act::Roll);  // tap: released on the next step
        return f;
    });
    h.Check(h.P().flame == h.P().flameMax, "roll i-frames avoid the hit");

    // 8. A charged heavy hits harder than an uncharged one.
    h.Fresh();
    h.Step(Harness::Press(Act::Heavy));
    h.Run(50);
    float quick = hpMax - h.Dummy()->Hp();
    h.Fresh();
    h.Run(110, [&](int i) {
        InputFrame f;
        if (i == 0) f.Press(Act::Heavy);
        else if (i < 75) f.Set(Act::Heavy, true);
        return f;
    });
    float charged = hpMax - h.Dummy()->Hp();
    h.Check(quick > 0 && charged > quick * 1.3f, "charged heavy deals more damage");

    // 9. Rolling until empty exhausts the knight: no more rolls until stamina returns.
    h.Fresh();
    int rolls = 0;
    PState last = PState::Move;
    h.Run(240, [&](int i) {
        if (h.P().State() == PState::Roll && last != PState::Roll) ++rolls;
        last = h.P().State();
        InputFrame f;
        if (i % 20 == 0) f.Press(Act::Roll);
        return f;
    });
    h.Check(rolls >= 5 && rolls <= 8, "stamina limits consecutive rolls");

    // 10. A block tap 0.3 s early is not a parry (and without holding, the hit lands).
    h.Fresh();
    h.Dummy()->TriggerAttackSoon();
    h.Run(delay + 20, [&](int i) { return i == land - 18 ? Harness::Press(Act::Block) : InputFrame{}; });
    h.Check(!h.Dummy()->Staggered() && h.P().flame < h.P().flameMax, "early block tap does not parry");

    // 11. Attacks commit: a roll pressed during the wind-up does not cancel the swing.
    h.Fresh();
    bool rolledEarly = false;
    h.Run(30, [&](int i) {
        if (i > 1 && i < 12 && h.P().State() == PState::Roll) rolledEarly = true;
        if (i == 0) return Harness::Press(Act::Attack);
        if (i == 3) return Harness::Press(Act::Roll);
        return InputFrame{};
    });
    h.Check(!rolledEarly, "roll cannot cancel an attack before its hit frame");

    // 12. Input buffer: a second attack pressed shortly before the cancel window chains;
    //     one pressed long before is dropped.
    auto comboAfter = [&](int pressStep) {
        h.Fresh();
        int best = 0;
        h.Run(70, [&](int i) {
            best = std::max(best, h.P().ComboIndex());
            if (i == 0 || i == pressStep) return Harness::Press(Act::Attack);
            return InputFrame{};
        });
        return best;
    };
    const ActionInfo* a1 = nullptr;
    {
        h.Fresh();
        (void)a1;
    }
    // light_1: cancel window opens at cancel_open / speed seconds after the press.
    const json& km = ctx.data.Get("anims/knight.json")["actions"]["attack_light_1"];
    int openStep = int(std::lround(km.value("cancel_open", 0.5f) / km.value("speed", 1.0f) * 60.0f));
    h.Check(comboAfter(openStep - 6) == 1, "attack buffered 0.1 s before the window chains");
    h.Check(comboAfter(openStep - 20) == 0, "attack pressed 0.33 s before the window is dropped");

    h.s.reset();
    ctx.input.SetScripted(false);
    LK_INFO("selftest: %d failures", h.failures);
    return h.failures;
}

// ------------------------------------------------------------------ enemies (M3)
int RunEnemySelfTest(Ctx& ctx) {
    Harness h(ctx);
    ctx.input.SetScripted(true);
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        if (ok) LK_INFO("selftest: PASS %s", what);
        else {
            LK_ERROR("selftest: FAIL %s", what);
            ++failures;
        }
    };
    auto arena = [&]() {
        h.s.reset();
        h.s = std::make_unique<Session>(ctx);
        bool ok = h.s->Load("test_arena.json");
        h.s->GetPlayer().Teleport(Vector3{14, 0, 14}, 0.0f);
        return ok;
    };
    if (!arena()) return 1;
    const bool god = ctx.state.godMode;
    ctx.state.godMode = true;

    // 1. Attack tokens: five minions around the knight, never more than two attacking at once.
    for (int i = 0; i < 5; ++i) {
        float a = float(i) / 5.0f * 2.0f * PI;
        h.s->SpawnEnemy("minion", Vector3{14 + std::cos(a) * 3.0f, 0, 14 + std::sin(a) * 3.0f}, 0);
    }
    int maxAttacking = 0, everAttacked = 0;
    h.Run(900, [&](int) {
        int n = 0;
        for (auto& e : h.s->Enemies())
            if (e->State() == EState::Attack) ++n;
        maxAttacking = std::max(maxAttacking, n);
        everAttacked += n > 0;
        return InputFrame{};
    });
    check(everAttacked > 0, "minions aggro and attack");
    check(maxAttacking <= 2, "at most 2 enemies attack at once");

    // 2. Warrior shield: light attacks from the front are blocked, heavies go through.
    arena();
    Enemy* w = h.s->SpawnEnemy("warrior", Vector3{14, 0, 15.0f}, 180.0f);
    w->SetFrozenAI(true);
    h.s->GetPlayer().Teleport(Vector3{14, 0, 14}, 0.0f);
    h.s->ForceLock(w->Id());
    float hp0 = w->Hp();
    h.Step(Harness::Press(Act::Attack));
    h.Run(40);
    check(w->Hp() == hp0, "warrior shield blocks a frontal light attack");
    h.Run(60);
    h.Step(Harness::Press(Act::Heavy));
    h.Run(70);
    check(w->Hp() < hp0, "a heavy attack breaks through the shield");

    // 3. Posture: repeated heavies break the warrior's posture; the next hit is a critical.
    arena();
    w = h.s->SpawnEnemy("warrior", Vector3{14, 0, 15.0f}, 180.0f);
    w->SetFrozenAI(true);
    h.s->GetPlayer().Teleport(Vector3{14, 0, 14}, 0.0f);
    h.s->ForceLock(w->Id());
    bool broke = false;
    for (int k = 0; k < 6 && !broke; ++k) {
        h.Step(Harness::Press(Act::Heavy));
        for (int i = 0; i < 70; ++i) {
            h.Step({});
            broke = broke || w->Staggered();
            h.s->GetPlayer().stamina.value = h.s->GetPlayer().stamina.max;
        }
    }
    check(broke, "heavies fill posture and stagger the warrior");
    float hpS = w->Hp();
    h.Step(Harness::Press(Act::Attack));
    h.Run(40);
    float dealt = hpS - w->Hp();
    LK_INFO("selftest: crit check hp %.1f -> %.1f state %s player %s", hpS, w->Hp(), EStateName(w->State()), PStateName(h.s->GetPlayer().State()));
    check(dealt >= std::min(hpS, h.s->GetPlayer().Weapon().dmgLight * 3) - 0.01f && dealt > 0, "hit on a staggered enemy is a 3x critical");

    // 4. Mage keeps distance with bolts and teleports away when the knight closes in.
    arena();
    ctx.state.godMode = false;
    Enemy* m = h.s->SpawnEnemy("mage", Vector3{14, 0, 6.0f}, 0.0f);
    h.s->GetPlayer().Teleport(Vector3{14, 0, 14}, 180.0f);
    bool sawBolt = false;
    float flame0 = h.s->GetPlayer().flame;
    h.Run(600, [&](int) {
        sawBolt = sawBolt || !h.s->Projectiles().empty();
        return InputFrame{};
    });
    check(sawBolt, "mage casts bolts at range");
    check(h.s->GetPlayer().flame < flame0, "mage bolts hit the knight");
    ctx.state.godMode = true;
    Vector3 before = m->Pos();
    h.s->GetPlayer().Teleport(Vector3Add(m->Pos(), Vector3{0, 0, 1.5f}), 180.0f);
    bool teleported = false;
    h.Run(240, [&](int) {
        teleported = teleported || Vector3Distance(m->Pos(), before) > 4.0f;
        return InputFrame{};
    });
    check(teleported, "mage teleports away when the knight gets close");

    // 5. Rogue: closes distance quickly with its lunge.
    arena();
    Enemy* r = h.s->SpawnEnemy("rogue", Vector3{14, 0, 9.0f}, 0.0f);
    h.s->GetPlayer().Teleport(Vector3{14, 0, 14}, 180.0f);
    float maxStep = 0;
    Vector3 last = r->Pos();
    bool rogueAttacked = false;
    h.Run(900, [&](int) {
        maxStep = std::max(maxStep, Vector3Distance(r->Pos(), last) * 60.0f);
        last = r->Pos();
        rogueAttacked = rogueAttacked || r->State() == EState::Attack;
        return InputFrame{};
    });
    check(rogueAttacked, "rogue attacks");
    check(maxStep > 4.0f, "rogue lunge is fast (> 4 m/s)");

    // 6. Kills drop embers.
    arena();
    Enemy* mn = h.s->SpawnEnemy("minion", Vector3{14, 0, 15.0f}, 180.0f);
    h.s->GetPlayer().Teleport(Vector3{14, 0, 14}, 0.0f);
    h.s->ForceLock(mn->Id());
    int embers0 = ctx.state.progress.embers;
    for (int k = 0; k < 8 && mn->Alive(); ++k) {
        h.Step(Harness::Press(Act::Attack));
        h.Run(36);
        h.s->GetPlayer().stamina.value = h.s->GetPlayer().stamina.max;
    }
    check(!mn->Alive(), "minion dies");
    check(ctx.state.progress.embers == embers0 + mn->Def().embers, "dead enemy drops its embers");

    ctx.state.godMode = god;
    h.s.reset();
    ctx.input.SetScripted(false);
    LK_INFO("selftest (enemies): %d failures", failures);
    return failures;
}

}  // namespace lk

namespace lk {

// ------------------------------------------------------------------ world (M4)
int RunWorldSelfTest(Ctx& ctx) {
    Harness h(ctx);
    ctx.input.SetScripted(true);
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        if (ok) LK_INFO("selftest: PASS %s", what);
        else {
            LK_ERROR("selftest: FAIL %s", what);
            ++failures;
        }
    };
    SaveData saved = ctx.state.progress;
    bool god = ctx.state.godMode;
    ctx.state.ResetProgress();
    ctx.state.godMode = true;
    h.s = std::make_unique<Session>(ctx);
    if (!h.s->Load("zone.json")) return 1;
    auto& P = h.s->GetPlayer();
    auto interactAt = [&](Vector3 p, float yaw) {
        P.Teleport(p, yaw);
        h.Run(2);
        h.Step(Harness::Press(Act::Interact));
        h.Run(90);
    };
    auto find = [&](auto& list, const std::string& id) -> decltype(&list[0]) {
        for (auto& x : list)
            if (x.id == id) return &x;
        return nullptr;
    };
    // Kill everything nearby so tests are not interrupted.
    for (auto& e : h.s->Enemies())
        if (e->Def().bossGroup.empty()) e->Kill();
        else e->SetFrozenAI(true);

    // 1. Lighting the hall D brazier reveals the hidden wall.
    Brazier* b = find(h.s->Braziers(), "b_hallD");
    Gate* secret = find(h.s->Gates(), "secret_wall");
    check(b && secret && !secret->open, "secret wall starts closed");
    if (b && secret) {
        interactAt(Vector3Add(b->pos, Vector3{-1.0f, 0, 0}), 90);
        check(b->lit && ctx.state.progress.litBraziers.count("b_hallD"), "brazier lights and is saved");
        check(secret->open && ctx.state.progress.secretsFound.count("secret_wall"), "brazier reveals the hidden path");
    }

    // 2. Shortcut gate: does not open from the locked side, opens from the open side.
    Gate* g = find(h.s->Gates(), "gate_shortcut_entrance");
    if (g) {
        interactAt(Vector3Subtract(g->pos, Vector3Scale(g->openDir, 1.2f)), 90);
        check(!g->open, "shortcut does not open from the far side");
        interactAt(Vector3Add(g->pos, Vector3Scale(g->openDir, 1.2f)), 270);
        check(g->open && ctx.state.progress.openedGates.count(g->id), "shortcut opens from its side and is saved");
    } else {
        check(false, "shortcut gate exists");
    }

    // 3. Pickups: a Flask Shard raises the flask maximum; the greataxe is recorded.
    int maxBefore = P.flaskMax;
    if (Pickup* k = find(h.s->Pickups(), "shard_secret")) interactAt(Vector3Add(k->pos, Vector3{-0.8f, 0, 0}), 90);
    check(P.flaskMax == maxBefore + 1 && ctx.state.progress.flaskShards == 1, "flask shard adds a flask");
    if (Pickup* k = find(h.s->Pickups(), "greataxe")) interactAt(Vector3Add(k->pos, Vector3{0, 0, 0.9f}), 180);
    check(ctx.state.progress.pickedItems.count("greataxe") > 0, "greataxe can be picked up");

    // 4. The Twin Blades: when both fall, the sealed gate opens and the defeat is saved.
    Gate* sealed = find(h.s->Gates(), "gate_sealed");
    check(sealed && !sealed->open, "sealed gate starts closed");
    P.Teleport(Vector3{99, 2, 56}, 180);
    h.Run(30);
    for (auto& e : h.s->Enemies())
        if (e->Def().bossGroup == "twin_blades") e->Kill();
    h.Run(5);
    check(ctx.state.progress.defeatedBosses.count("twin_blades") > 0, "Twin Blades defeat is recorded");
    check(sealed && sealed->open, "defeating the Twin Blades opens the sealed gate");

    // 5. State survives a reload.
    SaveData snapshot = ctx.state.progress;
    h.s = std::make_unique<Session>(ctx);
    h.s->Load("zone.json");
    Gate* s2 = find(h.s->Gates(), "secret_wall");
    Gate* g2 = find(h.s->Gates(), "gate_shortcut_entrance");
    check(s2 && s2->open && g2 && g2->open, "opened gates stay open after reload");
    bool twinsDead = true;
    for (auto& e : h.s->Enemies())
        if (e->Def().bossGroup == "twin_blades" && e->Alive()) twinsDead = false;
    check(twinsDead, "defeated mini-boss stays dead after reload");

    h.s.reset();
    ctx.state.progress = saved;
    ctx.state.godMode = god;
    ctx.input.SetScripted(false);
    LK_INFO("selftest (world): %d failures", failures);
    return failures;
}

}  // namespace lk

namespace lk {

// ------------------------------------------------------------------ boss (M5)
int RunBossSelfTest(Ctx& ctx) {
    Harness h(ctx);
    ctx.input.SetScripted(true);
    int failures = 0;
    auto check = [&](bool ok, const char* what) {
        if (ok) LK_INFO("selftest: PASS %s", what);
        else {
            LK_ERROR("selftest: FAIL %s", what);
            ++failures;
        }
    };
    SaveData saved = ctx.state.progress;
    bool god = ctx.state.godMode;
    ctx.state.ResetProgress();
    ctx.state.progress.litShrines.insert("shrine_warden");
    ctx.state.progress.lastShrine = "shrine_warden";
    ctx.state.progress.defeatedBosses.insert("twin_blades");
    ctx.state.godMode = true;
    h.s = std::make_unique<Session>(ctx);
    if (!h.s->Load("zone.json")) return 1;
    Enemy* boss = h.s->Boss();
    check(boss && boss->State() == EState::Dormant, "the Warden waits dormant behind the fog");
    if (!boss) return failures;
    auto arenaLit = [&]() {
        int lit = 0;
        for (auto& b : h.s->Braziers())
            if (b.arena && b.lit && !b.snuffed) ++lit;
        return lit;
    };
    check(arenaLit() == 4, "arena braziers burn before the fight");

    h.s->DebugEnterFog();
    h.Run(30);
    check(ctx.state.inBossFight && boss->Aggro(), "passing the fog gate starts the fight");
    check(h.s->GetBossBar().active && h.s->GetBossBar().name == "The Hollow Warden", "boss bar shows his name");

    // Phase 1 attacks: run and watch for a slam shockwave or a charge.
    bool sawWave = false, sawCharge = false, sawSweep = false;
    std::map<std::string, int> picked;
    const EnemyMove* lastMove = nullptr;
    h.Run(1500, [&](int) {
        static EState lastState = EState::Idle;
        if (boss->State() == EState::Attack && (lastState != EState::Attack || boss->CurrentMove() != lastMove) && boss->CurrentMove())
            picked[boss->CurrentMove()->name]++;
        lastState = boss->State();
        lastMove = boss->CurrentMove();
        sawWave = sawWave || !h.s->Waves().empty();
        sawCharge = sawCharge || boss->ChargeActive();
        sawSweep = sawSweep || (boss->CurrentMove() && boss->CurrentMove()->name == "sweep" && boss->State() == EState::Attack);
        return InputFrame{};
    });
    check(sawSweep, "phase 1: sweeping combo");
    check(sawWave, "phase 1: overhead slam sends a shockwave");
    check(sawCharge, "phase 1: shield charge when the knight keeps away");
    LK_INFO("selftest: boss charge observed: %s", sawCharge ? "yes" : "no");
    for (auto& [n, c] : picked) LK_INFO("selftest: boss move %s x%d", n.c_str(), c);
    LK_INFO("selftest: boss state %s dist %.1f", EStateName(boss->State()), Vector3Distance(boss->Pos(), h.s->PlayerPos()));

    // Phase 2 below 50%.
    h.s->DebugStrike(boss, boss->HpMax() * 0.55f, 0);
    h.Run(10);
    check(boss->Phase() == 2, "below 50% the Warden enters phase 2");
    check(arenaLit() == 0, "phase 2 snuffs every arena brazier");
    int summons = 0;
    for (auto& e : h.s->Enemies())
        if (e->SpawnId().rfind("summon_", 0) == 0) ++summons;
    check(summons == 3, "phase 2 summons three minions");
    bool sawDelayed = false;
    h.Run(1500, [&](int) {
        sawDelayed = sawDelayed || (boss->CurrentMove() && boss->CurrentMove()->name == "slam_delayed");
        return InputFrame{};
    });
    check(sawDelayed, "phase 2 adds the delayed slam");

    // Dying resets the fight.
    FxQueue fx;
    h.s->GetPlayer().Kill(fx);
    h.Run(300);
    summons = 0;
    for (auto& e : h.s->Enemies())
        if (e->SpawnId().rfind("summon_", 0) == 0) ++summons;
    check(!ctx.state.inBossFight && boss->Hp() == boss->HpMax() && boss->Phase() == 1 && boss->State() == EState::Dormant,
          "dying resets the Warden");
    check(arenaLit() == 4 && summons == 0, "reset relights the arena and removes the adds");

    // Victory.
    h.s->DebugEnterFog();
    h.Run(10);
    h.s->DebugStrike(boss, boss->HpMax() * 0.6f, 0);
    h.Run(200);  // phase 2 roar (strikes are ignored while he roars)
    h.s->DebugStrike(boss, boss->HpMax(), 0);
    h.Run(20);
    check(ctx.state.progress.defeatedBosses.count("hollow_warden") > 0, "Warden defeat is recorded");
    check(h.s->VictoryShown(), "FLAME REKINDLED banner");
    check(arenaLit() == 4, "victory relights the braziers");
    Gate* fog = h.s->FindGate("fog_gate");
    check(fog && fog->open, "the fog lifts");
    check(ctx.state.progress.gameCleared, "game marked cleared");
    check(!ctx.state.inBossFight, "fight over");

    h.s.reset();
    ctx.state.progress = saved;
    ctx.state.godMode = god;
    ctx.state.inBossFight = false;
    ctx.input.SetScripted(false);
    LK_INFO("selftest (boss): %d failures", failures);
    return failures;
}

}  // namespace lk
