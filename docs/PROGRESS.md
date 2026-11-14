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

## Next
- M1 Player: movement, camera, lock-on, stamina, roll, combos, block, parry vs a training skeleton.
