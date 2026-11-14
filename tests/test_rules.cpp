#include <doctest/doctest.h>

#include "core/combat_rules.h"
#include "core/geom.h"

using namespace lk;

TEST_CASE("stamina: use, regen delay, regen rate, exhaustion") {
    Stamina s;
    s.max = s.value = 100;
    s.regenPerSec = 30;
    s.regenDelay = 0.6f;
    CHECK(s.TryUse(12));
    CHECK(s.value == doctest::Approx(88));
    s.Update(0.5f);  // still inside the delay
    CHECK(s.value == doctest::Approx(88));
    s.Update(0.2f);  // 0.7 s since use: regen for this step
    CHECK(s.value == doctest::Approx(94));
    s.Update(1.0f);
    CHECK(s.value == doctest::Approx(100));
    // Drive to zero: the last action still comes out, then exhaustion.
    s.value = 10;
    CHECK(s.TryUse(25));
    CHECK(s.value == 0);
    CHECK(s.exhausted);
    CHECK_FALSE(s.CanAct());
    CHECK_FALSE(s.TryUse(12));
    for (int i = 0; i < 120; ++i) s.Update(1.0f / 60.0f);
    CHECK(s.CanAct());
}

TEST_CASE("stamina: sprint drains continuously") {
    Stamina s;
    for (int i = 0; i < 60; ++i) {
        s.Drain(15.0f / 60.0f);
        s.Update(1.0f / 60.0f);
    }
    CHECK(s.value == doctest::Approx(85).epsilon(0.01));
}

TEST_CASE("roll i-frames are 0.08 to 0.33 s") {
    RollTiming r;
    CHECK_FALSE(r.Invulnerable(0.0f));
    CHECK_FALSE(r.Invulnerable(0.079f));
    CHECK(r.Invulnerable(0.08f));
    CHECK(r.Invulnerable(0.2f));
    CHECK(r.Invulnerable(0.329f));
    CHECK_FALSE(r.Invulnerable(0.33f));
    CHECK_FALSE(r.Invulnerable(0.45f));
}

TEST_CASE("parry window is 0.15 s before the hit") {
    ParryTiming p;
    p.window = 0.15f;
    p.cooldown = 0.25f;
    CHECK_FALSE(p.Active());
    p.Press();
    CHECK(p.Active());
    p.Update(0.14f);
    CHECK(p.Active());
    p.Update(0.02f);
    CHECK_FALSE(p.Active());
    // Mashing: a press inside the cooldown does not reopen the window.
    p.Press();
    CHECK_FALSE(p.Active());
    p.Update(0.2f);
    p.Press();
    CHECK(p.Active());
}

TEST_CASE("hit resolution: i-frames, parry, block, clean hit") {
    BlockTuning b{0.5f, 0.6f};
    DefenseState d;
    d.invulnerable = true;
    CHECK(ResolveHitOnPlayer(20, d, b).ignored);
    d = {};
    d.parryActive = true;
    d.facingAttacker = true;
    CHECK(ResolveHitOnPlayer(20, d, b).parried);
    d.parryActive = false;
    d.blocking = true;
    HitResult r = ResolveHitOnPlayer(20, d, b);
    CHECK(r.blocked);
    CHECK(r.damage == doctest::Approx(10));
    CHECK(r.stamina == doctest::Approx(12));
    d.facingAttacker = false;  // attacked from behind: shield does not help
    r = ResolveHitOnPlayer(20, d, b);
    CHECK_FALSE(r.blocked);
    CHECK(r.damage == doctest::Approx(20));
}

TEST_CASE("posture fills, staggers for its duration, then resets") {
    Posture p;
    p.max = 100;
    p.staggerDuration = 1.5f;
    CHECK_FALSE(p.Add(60));
    CHECK(p.Add(50));
    CHECK(p.Staggered());
    CHECK_FALSE(p.Add(10));  // no extra posture while staggered
    p.Update(1.0f);
    CHECK(p.Staggered());
    p.Update(0.6f);
    CHECK_FALSE(p.Staggered());
    CHECK(p.value == 0);
}

TEST_CASE("posture decays after a delay") {
    Posture p;
    p.Add(30);
    p.Update(1.0f);
    CHECK(p.value == doctest::Approx(30));
    p.Update(1.0f);  // 0.5 s past the 1.5 s delay
    CHECK(p.value < 30);
}

TEST_CASE("damage: might, charge and critical multipliers") {
    CHECK(PlayerDamage(20, 0, 0.05f, 0, 0.6f, 1) == doctest::Approx(20));
    CHECK(PlayerDamage(20, 2, 0.05f, 0, 0.6f, 1) == doctest::Approx(22));
    CHECK(PlayerDamage(20, 0, 0.05f, 1, 0.6f, 1) == doctest::Approx(32));
    CHECK(PlayerDamage(20, 0, 0.05f, 0, 0.6f, 3) == doctest::Approx(60));
}

TEST_CASE("level-up cost is 50 x 1.15^level") {
    CHECK(LevelUpCost(0) == 50);
    CHECK(LevelUpCost(1) == 58);   // 57.5 rounds to 58
    CHECK(LevelUpCost(5) == 101);  // 100.57
    CHECK(LevelUpCost(10) == 202); // 202.28
    CHECK(MaxFlame(3) == doctest::Approx(124));
    CHECK(MaxStamina(2) == doctest::Approx(112));
}

TEST_CASE("ember pile: drop, recover, lose on second death") {
    EmberState e;
    e.carried = 300;
    CHECK(OnPlayerDeath(e, 1, 0, 2) == 0);
    CHECK(e.carried == 0);
    CHECK(e.pileActive);
    CHECK(e.pileAmount == 300);
    CHECK(RecoverPile(e) == 300);
    CHECK(e.carried == 300);
    CHECK_FALSE(e.pileActive);
    // Die, then die again before recovering: the first pile is gone.
    OnPlayerDeath(e, 1, 0, 2);
    e.carried = 40;
    CHECK(OnPlayerDeath(e, 5, 0, 5) == 300);
    CHECK(e.pileAmount == 40);
    CHECK(e.pileX == 5);
    // Dying with nothing carried leaves no pile (and loses the old one).
    CHECK(OnPlayerDeath(e, 7, 0, 7) == 40);
    CHECK_FALSE(e.pileActive);
    CHECK(RecoverPile(e) == 0);
}

TEST_CASE("capsule overlap and shield arc") {
    using namespace lk::geom;
    Capsule a{{0, 0, 0}, {0, 1, 0}, 0.3f};
    Capsule b{{0.5f, 0.5f, 0}, {1.5f, 0.5f, 0}, 0.25f};
    CHECK(CapsuleOverlap(a, b));
    b.a.x = 0.6f;
    CHECK_FALSE(CapsuleOverlap(a, b));
    CHECK(WithinArc({0, 0, 1}, {0.2f, 0, 1}, 55));
    CHECK_FALSE(WithinArc({0, 0, 1}, {0, 0, -1}, 55));
    CHECK_FALSE(WithinArc({0, 0, 1}, {1, 0, 0}, 55));
}
