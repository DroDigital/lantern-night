#include "engine/log.h"

#include <cstdio>
#include <cstring>

#include "raylib.h"

namespace lk::log {
namespace {
bool g_verbose = false;
int g_errors = 0;
int g_warnings = 0;
std::string g_lastError;
FILE* g_file = nullptr;

const char* Tag(Level l) {
    switch (l) {
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warn: return "WARN";
        case Level::Error: return "ERROR";
    }
    return "?";
}

void RaylibCallback(int logLevel, const char* text, va_list args) {
    Level l = Level::Debug;
    if (logLevel == LOG_INFO) l = Level::Info;
    else if (logLevel == LOG_WARNING) l = Level::Warn;
    else if (logLevel >= LOG_ERROR) l = Level::Error;
    if (l == Level::Debug && !g_verbose) return;
    if (l == Level::Info && !g_verbose) return;  // raylib is chatty; keep the console clean
    char buf[1024];
    vsnprintf(buf, sizeof buf, text, args);
    Write(l, "[raylib] %s", buf);
}
}  // namespace

void Init(bool verbose) { g_verbose = verbose; }

void WriteV(Level lvl, const char* fmt, va_list args) {
    if (lvl == Level::Debug && !g_verbose) return;
    char buf[2048];
    vsnprintf(buf, sizeof buf, fmt, args);
    if (lvl == Level::Error) {
        ++g_errors;
        g_lastError = buf;
    } else if (lvl == Level::Warn) {
        ++g_warnings;
    }
    FILE* out = (lvl >= Level::Warn) ? stderr : stdout;
    std::fprintf(out, "[%s] %s\n", Tag(lvl), buf);
    std::fflush(out);
}

void Write(Level lvl, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    WriteV(lvl, fmt, args);
    va_end(args);
}

int ErrorCount() { return g_errors; }
int WarningCount() { return g_warnings; }
void ResetCounts() {
    g_errors = 0;
    g_warnings = 0;
}
const std::string& LastError() { return g_lastError; }

void HookRaylib() {
    SetTraceLogCallback(RaylibCallback);
    SetTraceLogLevel(g_verbose ? LOG_INFO : LOG_WARNING);
    (void)g_file;
}

}  // namespace lk::log
