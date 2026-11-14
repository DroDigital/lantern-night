#pragma once
// --anim-report: samples every attack action's weapon tip through its clip and writes
// docs/anim_timing.md with the measured fast-swing window (suggested hit / hit_end).
#include <string>

namespace lk {
struct Ctx;
int WriteAnimReport(Ctx& ctx, const std::string& outPath);
}  // namespace lk
