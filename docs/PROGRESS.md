# Progress

## M0 Setup: done
- CMake project with `debug` / `release` presets; raylib 6.2, Jolt 5.6.0, nlohmann/json 3.12.0, doctest 2.5.3 via FetchContent (pinned tags).
- CI (`.github/workflows/build.yml`): Windows, macOS (universal), Ubuntu 22.04. Configure, build, unit tests, `--validate`, smoke test (Linux: xvfb + llvmpipe; Windows: mesa-dist-win llvmpipe next to a copy of the exe; macOS: smoke test if a GL context exists, otherwise `--validate`), package, upload; GitHub Release on `v*` tags.
- Assets imported (`tools/import_assets.py`), `tools/asset_report` writes `docs/asset_report.md` (343 glTF files, 0 parse failures).
- Engine: logging, paths (exe-relative content, per-user data dir), data store with F5 hot reload, asset cache, animator (sampling, crossfade, events, bone matrices, GPU skinning), renderer (lit shader, 8 lights, fog, rim, emissive, hit flash, blob shadows, halos, shafts, HDR MSAA, bloom, vignette, ACES/AgX), Jolt wrapper (static compound, toggles, ray/sphere casts, CharacterVirtual), input (actions, dead zones, hot-plug, last device, pad family), audio (pools, pitch jitter, positional, music crossfade, buses).
- Zone tile loading: stacked ASCII layers + legend → chunk-merged meshes, auto walls, railings, stairs ramps, colliders.
- Lighting test: knight with lantern walking through a dark room; captures `lighting_full/half/low` and `zone_overview` tuned to be moody but readable.
- Unit tests: zone parsing, save round-trip and migration.

### Known issues
- The smoke test is a movement soak at this point; the full scripted path (attack, kill, rest, save/reload) arrives with M1–M2.
- `zone.json` is a stairs test room until M4.

## M1 Player: done
- Knight controller (`src/game/player/`): walk/run/sprint, roll with i-frames (0.08 to 0.33 s), 3-hit light combo with cancel windows, charged heavy (up to 1 s, +60%), block (x0.5 damage, stamina cost, frontal arc), parry (block pressed within 0.15 s before the hit lands; staggers the attacker), riposte (3x on a staggered enemy), hit and guard-break reactions, exhaustion at 0 stamina (walk only), 0.15 s input buffer, attacks commit outside their cancel windows.
- Camera: orbit, smooth follow, Jolt sphere-cast collision, lock-on (toggle, flick or wheel to switch, released when out of range or hidden for 2 s) with an over-the-shoulder offset.
- Combat queries: weapon capsules swept between steps vs hurtbox capsules; per-swing hit lists.
- Hit frames measured, not guessed: `--anim-report` samples every attack's weapon-tip speed and writes `docs/anim_timing.md`; the anim maps use those windows.
- Training skeleton (`data/enemies/training.json`) swings on a timer with a held, glinting wind-up.
- Feel: hitstop scales the sim clock (70 ms hits, 120 ms parries/crits), camera shake, white hit flash, blue-white parry flash, sparks on blocks, ribbon weapon trails, embers drifting from the lantern.
- HUD: flame bar with lantern icon, stamina bar, flasks, embers, enemy health/posture bars, lock-on reticle; Kenney prompts per device (PlayStation face buttons are drawn placeholders).
- `--selftest` (also run first in `--smoke-test`): 20 deterministic combat checks covering hits, combo, block, parry (and an early tap that must not parry), roll i-frames, riposte, heavy charge, exhaustion, commit and the input buffer. All pass.

### Known issues
- Gamepad feel was tuned from data and the self-test only; no physical pad was attached in the build environment.

## Next
- M2 Lantern loop: flame as health, flasks, shrine, death and ember pile, leveling, save and load.
