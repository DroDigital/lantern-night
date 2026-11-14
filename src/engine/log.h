#pragma once
// Tiny logging layer. Also captures raylib's TraceLog so the smoke test can fail on
// any warning/error (missing texture, failed shader, ...).
#include <cstdarg>
#include <string>

namespace lk::log {

enum class Level { Debug, Info, Warn, Error };

void Init(bool verbose);
void Write(Level lvl, const char* fmt, ...);
void WriteV(Level lvl, const char* fmt, va_list args);

int ErrorCount();
int WarningCount();
void ResetCounts();
const std::string& LastError();

// Route raylib TraceLog messages through this logger.
void HookRaylib();

}  // namespace lk::log

#define LK_INFO(...) ::lk::log::Write(::lk::log::Level::Info, __VA_ARGS__)
#define LK_WARN(...) ::lk::log::Write(::lk::log::Level::Warn, __VA_ARGS__)
#define LK_ERROR(...) ::lk::log::Write(::lk::log::Level::Error, __VA_ARGS__)
#define LK_DEBUG(...) ::lk::log::Write(::lk::log::Level::Debug, __VA_ARGS__)
