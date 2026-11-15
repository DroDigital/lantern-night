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
- The settings entry in the pause menu is a placeholder until M6 (resolved in M6).

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

## M4 World: done
- `data/levels/zone.json`: one connected zone on two height layers (58 x 36 cells of 2 m), authored with `tools/zone_src/build_zone.py` (hand-placed rooms and placements, no randomness) and tunable in the F2 editor.
  - **Crypt Entrance** (upper, west): floor hints engraved in the floor (glowing runes + text with the current device's button names), minion pairs, Entrance Shrine, a shortcut gate that only opens from the far side.
  - **Flooded Ossuary** (lower, centre): dark, flooded halls (translucent water with light glints), braziers, rogues and mages, the greataxe room (warrior guard), the Ossuary Shrine, an optional Drowned Chapel with a Flask Shard, and a hidden wall revealed by lighting the hall D brazier (secret alcove with the second Flask Shard). The ossuary loops back up to the Entrance Shrine through the shortcut gate.
  - **Warden's Hall** (upper, east): warrior gauntlet, The Twin Blades (two red-tinted rogues, 250 HP each, one boss bar), a second shortcut back down to the ossuary, a sealed gate that opens when the Twin Blades fall, the Warden's Shrine and the fog gate to the boss arena.
  - Totals: 3 shrines, 2 shortcuts, 1 optional side area with a Flask Shard, 1 secret in a dark area, 11 braziers (4 in the boss arena), 29 enemies.
- Braziers: 1 s interact, permanent and saved, add a point light with fire particles and halo; one reveals a hidden path.
- Gates: shortcuts (open from one side only, saved), hidden walls, the boss-locked gate; colliders and nav cells toggle with them.
- Pickups: Flask Shards (+1 max flask, up to 5), the greataxe (swap at shrines), an ember cache.
- Area triggers show the area name.
- F2 level editor (debug builds): free camera, layer select, paint/erase/rotate tiles, place/rotate/delete props, select/move/rotate/delete markers, add enemies, Ctrl+S saves `zone.json`; the session rebuilds from the edited data.
- `--validate` checks zone connectivity (every shrine, item, brazier, gate and enemy reachable from the spawn on the nav graph).
- World self-test: brazier reveals the secret, shortcuts open only from their side, pickups, Twin Blades open the sealed gate, everything persists across a reload.

### Known issues
- Rooms are sparsely decorated; more props come with the balance pass.

## M5 Boss: done
- The Hollow Warden (`data/boss/hollow_warden.json`, `data/anims/hollow_warden.json`): Skeleton Warrior x2.5, 1,100 HP (1,500 before the M6 balance pass), boss bar with his name.
  - Phase 1: sweeping two-hit combo, overhead slam (0.9 s telegraph) that sends an expanding ground shockwave (roll through it; it cannot be blocked or parried), shield charge when the knight keeps their distance.
  - Phase 2 (below 50%): roars (ignores hits while roaring), snuffs every arena brazier so only the lantern lights the fight, summons three minions once, adds a delayed slam with a 1.35 to 1.75 s hold.
- Fog gate: interact to pass through; the fight starts (boss music), the fog seals until victory. Dying resets the Warden to full health in phase 1, relights the arena and removes the adds.
- Victory: braziers relight, "FLAME REKINDLED", +2 Vigor (the Warden's ember) and his 3,000 embers, the fog lifts, the save is marked cleared, then scrolling credits.
- Camera: lock-on pulls back and up for big targets; when walls squeeze the camera it tilts up to look over the knight.
- Boss self-test: dormant until the fog, sweep/slam/shockwave/charge in phase 1, phase 2 snuff + 3 summons + delayed slam, reset on death, victory state. Captures: `boss_phase1`, `boss_phase2`, `victory`.

## M6 Polish and ship: done
- Title screen: the zone behind a slow orbit camera, Continue / New game / Settings / Quit, the build version (`git describe`) in the corner.
- Settings (`src/game/settings.*`, `settings.json` in the per-user folder): master/music/effects volume, camera shake, sensitivity, invert X/Y, fullscreen (borderless), resolution, vsync, lantern shadows. Reachable from the title and pause menus; applied immediately and saved.
- Lantern shadows: an R32F distance cube map rendered from the lantern each frame (walls, props, enemies cast; the knight, who holds the light, does not), 4-tap PCF in the lit shader. Off by default; the setting turns it on.
- Audio: every sound referenced by the game exists in `data/audio.json` (checked), menu move/confirm/back sounds, positional SFX, ambient/boss music crossfades, lantern crackle loop at low flame.
- Particles (built in M1 to M5): lantern embers, hit and block sparks, brazier fire, mage bolt tails, death bursts, ember piles; weapon ribbon trails.
- Dressing pass: `build_zone.py` adds 57 deterministic props (low clutter along walls, barrels/crates/kegs/trunks in room corners) away from interactables, stairs and the boss arena.
- Balance pass: the Hollow Warden drops from 1,500 to 1,100 HP (about 70 light hits instead of 94, fewer with Might levels and ripostes); minion hits drop from 12 to 10 because the first room sends up to three at a fresh knight (playing it, mashing attack died twice; blocking and countering got through with flame to spare).
- Camera (found while playing the packaged build): against walls the boom snaps in to the wall hit instead of stopping at a 0.6 m floor that let it pass through the wall; the squeeze tilt now picks whichever boom (level or tilted) is clearer; when the knight backs into a wall so neither clears, the camera rises over his head and looks down; the knight is hidden only when the camera is within 0.8 m of him.
- Soft aim: without lock-on, an attack turns toward the closest enemy within 2.6 m and 70 degrees of the stick (or the knight's facing); `soft_aim` in `data/player/stats.json`.
- App icons (Windows .ico in the exe, macOS .icns in the bundle, Linux .png + .desktop), README with download and launch steps per OS, `v0.1.0` tag with a GitHub Release carrying the Windows, macOS and Linux archives.

### Known issues
- Gamepad feel and the overall difficulty were tuned from data, captures and scripted runs; no human playtest happened in the build environment.
- macOS builds are ad-hoc signed unless the `APPLE_*` secrets are configured, so Gatekeeper asks once (see README).
- Lantern shadows cost six extra scene passes over nearby geometry; they are off by default.

## Next
- Playtest-driven tuning of enemy timings and boss damage.
- More dressing variety (wall banners and bones) and a second weapon moveset.
