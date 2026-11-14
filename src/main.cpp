// Lantern Knight entry point. Creates the long-lived services (GameState, SaveSystem,
// AudioManager) once and passes them by reference into the App.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "engine/audio.h"
#include "engine/log.h"
#include "engine/paths.h"
#include "game/app.h"
#include "game/game_state.h"
#include "game/save_system.h"
#include "game/validate.h"

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall AttachConsole(unsigned long);
extern "C" __declspec(dllimport) void* __stdcall GetStdHandle(unsigned long);
#endif

static void PrintUsage() {
    std::printf(
        "Lantern Knight %s\n"
        "  --validate                 parse every glTF and data file (no window)\n"
        "  --smoke-test               scripted 3600-step run; non-zero exit on any error\n"
        "  --capture <scene|all>      render a scene and save PNG screenshots\n"
        "  --frames N                 frames to render per capture scene (default 90)\n"
        "  --out DIR                  capture output folder (default shots)\n"
        "  --zone FILE                zone file under data/levels (default zone.json)\n"
        "  --size WxH                 window size\n"
        "  --user-dir DIR             override the save/settings folder\n"
        "  --verbose                  more logging\n",
        LK_VERSION);
}

int main(int argc, char** argv) {
#if defined(_WIN32)
    // GUI-subsystem build: if launched from a terminal without redirected output, attach to it so
    // --validate / --smoke-test print. Redirected handles (CI pipes) are left alone.
    void* out = GetStdHandle(static_cast<unsigned long>(-11));  // STD_OUTPUT_HANDLE
    if ((out == nullptr || out == reinterpret_cast<void*>(-1)) && AttachConsole(static_cast<unsigned long>(-1))) {
        std::freopen("CONOUT$", "w", stdout);
        std::freopen("CONOUT$", "w", stderr);
    }
#endif
    lk::AppOptions opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--smoke-test") opt.smokeTest = true;
        else if (a == "--validate") opt.validate = true;
        else if (a == "--verbose") opt.verbose = true;
        else if (a == "--capture") opt.captureScene = next();
        else if (a == "--frames") opt.captureFrames = std::atoi(next().c_str());
        else if (a == "--out") opt.captureOut = next();
        else if (a == "--zone") opt.zone = next();
        else if (a == "--anim-report") opt.animReport = next();
        else if (a == "--selftest") opt.selfTest = true;
        else if (a == "--user-dir") opt.userDir = next();
        else if (a == "--size") {
            std::string s = next();
            std::sscanf(s.c_str(), "%dx%d", &opt.width, &opt.height);
        } else if (a == "--help" || a == "-h") {
            PrintUsage();
            return 0;
        } else if (a.rfind("-psn_", 0) == 0) {
            // macOS Finder process serial number: ignore.
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", a.c_str());
            PrintUsage();
            return 2;
        }
    }
    lk::log::Init(opt.verbose);
    lk::log::HookRaylib();
    LK_INFO("Lantern Knight %s", LK_VERSION);
    if (!lk::paths::Init(argc > 0 ? argv[0] : nullptr)) return 1;

    if (opt.validate) {
        int problems = lk::ValidateContent();
        std::printf("validate: %s (%d problems)\n", problems == 0 ? "OK" : "FAILED", problems);
        return problems == 0 ? 0 : 1;
    }

    lk::GameState state;
    lk::SaveSystem save;
    lk::AudioManager audio;
    lk::App app(state, save, audio);
    int rc = app.Run(opt);
    std::printf("exit code %d\n", rc);
    return rc;
}
