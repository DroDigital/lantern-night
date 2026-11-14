#pragma once
// --selftest: deterministic combat scenarios on the training level (light hit, combo, block,
// parry, roll i-frames, riposte, heavy charge, exhaustion). Also run at the start of --smoke-test.
namespace lk {
struct Ctx;
// Returns the number of failed checks.
int RunCombatSelfTest(Ctx& ctx);
}  // namespace lk
