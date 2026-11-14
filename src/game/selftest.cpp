#include "game/selftest.h"

#include <cmath>
#include <functional>
#include <memory>

#include "engine/input.h"
#include "engine/log.h"
#include "game/context.h"
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

}  // namespace lk
