#pragma once
// --selftest: deterministic combat scenarios on the training level (light hit, combo, block,
// parry, roll i-frames, riposte, heavy charge, exhaustion). Also run at the start of --smoke-test.
namespace lk {
struct Ctx;
// Returns the number of failed checks.
int RunCombatSelfTest(Ctx& ctx);
// Enemy AI scenarios on data/levels/test_arena.json (tokens, shield, posture, mage, rogue, embers).
int RunEnemySelfTest(Ctx& ctx);
// World scenarios on data/levels/zone.json (braziers, secret, shortcuts, pickups, mini-boss).
int RunWorldSelfTest(Ctx& ctx);
}  // namespace lk
