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

## M2 Lantern loop: done
- Flame is health; the lantern light (range 4 to 12 m, intensity, flicker) follows flame/max, sputters below 25%, the vignette tightens and a crackle loop plays.
- Oil flasks: 45 flame over a 1.2 s drink (heal lands at 0.56 s; a hit before that wastes the flask). Start with 3; shrines refill.
- Lantern shrines (`data/world.json`, zone `shrines`): light (1 s interact), rest (refill flame and flasks, respawn non-boss enemies, set the respawn point, save), shrine menu with level up (Vigor/Endurance/Might, cost 50 x 1.15^level) and weapon swap.
- Death: "YOUR FLAME GOES OUT", all embers drop as a glowing pile (light, halo, sparks) at the death spot; respawn at the last shrine; touching the pile returns them; dying again first loses them (rules in `core/combat_rules`, unit-tested).
- Save: `save.json` in the per-user folder on light/rest/level/death/quit; temp file + rename, one `.bak` backup, versioned with v1 to v2 migration (unit-tested).
- Pause menu (resume, settings placeholder, save and quit).
- `--smoke-test` now does everything the spec lists: combat self-test, loads every model/texture and decodes every sound, validates all data, then a bot drives the knight in god mode (kill an enemy, light and rest at a shrine, save, read the save back and compare, rebuild the session from disk, wander the nav graph) for 3,600 fixed steps; fails on any error, missing asset, NaN position or save mismatch.

### Known issues
- The settings entry in the pause menu is a placeholder until M6.

## M3 Enemies: done
- Four skeleton types, all data-driven (`data/enemies/*.json`, `data/anims/skeleton_*.json`):
  - Minion (40 HP): slow two-hit combo, groups of 2 to 3.
  - Warrior (90 HP): sword and large shield; the shield blocks light attacks from the front (any time it is not committed to a swing), heavies go through with big posture damage, flanking works.
  - Rogue (60 HP): circles at mid range, delayed lunge with a randomised hold (mix-up timing), fast gap closer.
  - Mage (50 HP): homing bolts (emissive, lit, particle tail), teleports away when the knight closes in.
- AI states: idle/patrol, alert (scream), approach (A* on the tile grid across layers), strafe, attack (move picked by distance and weight, combos), recover, hit, stagger, dead, teleport. Separation between enemies.
- Telegraphs: wind-up held 0.3 to 0.8 s at the clip's wind-up time, with a weapon glint (bright particle + sound).
- Attack tokens: at most 2 enemies attack at once.
- Posture: heavies and parries fill it; a full bar staggers for 1.5 s and the next hit is a 3x critical.
- Embers drop on death; enemies respawn when the knight rests or dies.
- Enemy self-test (`RunEnemySelfTest`, in `--selftest` and `--smoke-test`): tokens, warrior shield vs light/heavy, posture break then critical, mage bolts hit and teleport, rogue lunge speed, ember drops. All pass.

## Next
- M4 World: F2 level editor, three areas, braziers, shortcuts, mini-boss, Flask Shards, greataxe.
