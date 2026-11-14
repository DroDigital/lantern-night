# Lantern Knight

A small, cute knight explores a crypt where darkness is the enemy. The knight's lantern is
their life: its flame is your health and your light. Take damage and the world gets darker.
Rest at lantern shrines, die, and go back for your embers. It looks friendly and plays hard.

A single-player third-person soulslike written in C++20 on raylib with no game engine:
the engine-level systems (animation, rendering, physics glue, navigation, input, audio, UI,
editor) live in `src/engine/`.

## Download and play

Grab the archive for your OS from the [Releases](../../releases) page (or the latest CI run's artifacts):

| OS | Archive | Launch |
|---|---|---|
| Windows 10+ x64 | `LanternKnight-<version>-windows-x64.zip` | Unzip, run `LanternKnight.exe`. If SmartScreen warns, click *More info → Run anyway*. |
| macOS 11+ (Apple Silicon and Intel) | `LanternKnight-<version>-macos-universal.zip` | Unzip, open `Lantern Knight.app`. If it is not notarized: right-click → *Open*, or *System Settings → Privacy & Security → Open Anyway*. |
| Linux x64 | `LanternKnight-<version>-linux-x64.tar.gz` | `tar xzf …` then `./LanternKnight`. |

No installer or runtime is required. Controls are in `packaging/README.txt` (shipped as README.txt).

## Build from source

Requirements: CMake 3.24+, a C++20 compiler (MSVC 2022, Xcode 14+, GCC 11+/Clang 14+), Git.
On Linux also: `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libasound2-dev`.

```bash
cmake --preset release
```

```bash
cmake --build --preset release
```

```bash
ctest --preset release
```

The game is `build/release/bin/LanternKnight` (`LanternKnight.exe`, or `LanternKnight.app` on macOS).
Debug builds (`--preset debug`) enable F1 (debug overlay; G toggles god mode), F2 (level editor) and F5 (hot-reload data and shaders).

**F2 level editor:** hold the right mouse button to look, WASD/Q/E to fly (Shift = fast). `1` tiles, `2` props, `3` markers; PgUp/PgDn change the layer. Tiles: mouse wheel picks a tile, left click paints, X erases, R rotates stairs. Props: wheel picks, left click places, R rotates, X deletes the nearest. Markers: click to select, click the floor to move it, R rotates, X deletes, N adds an enemy. Ctrl+S writes `data/levels/zone.json`.

Package the archive for the current OS into `build/release/dist/`:

```bash
cmake --build --preset release --target package_game
```

### Useful command lines

| Command | What it does |
|---|---|
| `LanternKnight --validate` | Parses every glTF and data file (no window). |
| `LanternKnight --smoke-test` | Combat self-test, then a scripted 3,600-step run; non-zero exit on any error. |
| `LanternKnight --selftest` | Deterministic combat checks only (hits, combo, block, parry, roll, riposte, buffer). |
| `LanternKnight --anim-report docs/anim_timing.md` | Measures attack swing windows from the clips. |
| `LanternKnight --capture all --frames 90 --out shots` | Renders the capture scenes and saves PNGs. |
| `asset_report assets docs/asset_report.md` | Regenerates the asset report. |

## Repository layout

- `src/engine/`: game-agnostic systems (assets, animator, renderer, physics, nav, input, audio, ui, editor, debug)
- `src/core/`: pure rules and data (unit-tested, no raylib)
- `src/game/`: player, enemies, boss, world, ui
- `data/`: every tunable number (JSON), hot-reloaded with F5 in debug builds
- `assets/`: CC0 art and audio (see CREDITS.md)
- `tools/`: asset report, import scripts, packaging
- `docs/`: progress, decisions, asset report

See `docs/PROGRESS.md` for milestone status and `docs/DECISIONS.md` for design decisions.
