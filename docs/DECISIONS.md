# Decisions

Ambiguities resolved during development, simplest option first.

## Engine and build
- **raylib 6.2, Jolt 5.6.0, nlohmann/json 3.12.0, doctest 2.5.3**, latest release tags in November 2026, fetched with FetchContent over `GIT_REPOSITORY` (shallow) because tarball downloads were blocked in the development environment.
- **Baseline CPU:** Jolt is built without AVX/AVX2/AVX-512/LZCNT/TZCNT/F16C/FMA but keeps SSE4.1/4.2 (x86-64-v2, every x86-64 CPU since ~2009). The macOS universal binary builds both slices in one pass: Jolt detects SIMD from compiler predefines, so no lipo merge was needed.
- **Jolt compute (DX12/Vulkan/Metal/CPU compute) is disabled.** We only use collision queries and CharacterVirtual.
- **cgltf** comes from raylib's bundled copy (implementation compiled inside raylib); `tools/asset_report` and `--validate` link raylib for it but never open a window.
- **Windows console:** the release build uses the GUI subsystem with `mainCRTStartup`, so `main()` is shared; it attaches to a parent console only when no std handles were inherited, so CI pipes keep working.

## Assets
- **Only the Knight and Barbarian** are copied from the Adventures pack (plus all item models); the other adventurers are unused. All skeleton and dungeon models are copied.
- **Animations live inside the character GLBs** (the asset report confirms it), and raylib 6.2 loads them correctly, so the cgltf fallback loader was not needed. raylib bakes clips at 60 fps in model space, and the Animator samples and crossfades those poses.
- **Quaternius textures are downscaled to 512 px.** The lit shader only uses base color.
- **Kenney packs come from the OpenGameArt mirror**; kenney.nl and the itch.io music bundle could not be downloaded from the build machine. See TODO_ASSETS.md.
- **Hand lantern** is a small procedural mesh (cylinder + cone + torus) because neither KayKit pack has one; the Quaternius wall lantern has a wall bracket.

## World scale and tiles
- **World scale 0.5:** KayKit dungeon tiles are 4×4 model units with 4-unit walls (asset report), and the knight is 2.47 units tall. At 0.5 a tile is 2 m, a wall 2 m and the knight 1.23 m (small and cute), which makes the lantern ranges (4–12 m) and the 4.5 m camera distance read well. `model_scale` and `cell_size` live in the zone file.
- **Walls are auto-tiled.** A `#` cell is solid; a wall mesh is placed on every edge between an open cell and a solid or void cell. Legend variants pick wall meshes per edge with a stable hash. Upper-layer floors next to a drop get a railing (`barrier`) on top and a wall face below. Corner posts (`wall_corner_small`) hide seams at turns.
- **Stairs** are a legend tile with a rotation; the KayKit `stairs` mesh rises toward its local −Z. Physics uses a 45° ramp slab (extended under the floor at the low end so its end face never acts as a wall).
- **Static level collision** is one Jolt StaticCompoundShape of boxes; gates and fog walls are separate toggleable static bodies.

## Rendering
- **HDR pipeline:** scene into a 4× MSAA RGBA16F framebuffer (renderbuffers created through `rlGetProcAddress`), resolved into an RGBA16F texture, then bloom (bright pass plus two blurred mips), vignette, ACES (AgX available via `"tonemapper": "agx"`) and gamma. FLAG_MSAA_4X_HINT is still set for the default framebuffer.
- **Lighting is in linear space** with a wrapped diffuse, a smooth range window and a tunable quadratic falloff (`light_falloff`). The lantern is always light 0, followed by the 7 lights whose reach is closest to the drawn object.
- **The beyond-the-walls void** clears to a near-black `void_color` instead of the fog color, so the crypt reads as enclosed darkness.
- **Characters** get a minimum light level and a cold rim term so they are never pure black.

## Physics / character
- **CharacterVirtual** uses grounded velocity = ground velocity (no gravity while grounded) so characters neither pin into creases nor creep down slopes.

## Combat (M1)
- **Hit frames are measured.** `LanternKnight --anim-report docs/anim_timing.md` samples the weapon tip (handslot.r) through every attack clip; `hit`/`hit_end` are the span where tip speed stays above 45% of peak (minus 0.02 s / plus 0.07 s follow-through). Cancel windows open 0.04 s after `hit_end`. Enemy wind-up holds start 0.14 s before the fast arc.
- **Roll is on button release** (tap < 0.28 s); holding the same button sprints. This is the standard soulslike mapping.
- **Parry spends its 10 stamina when block is pressed**; the window is 0.15 s with a 0.25 s re-press cooldown, so mashing does not keep it open.
- **Riposte** is simply attacking a staggered enemy (from a parry or a broken posture): the hit is a 3x critical, and if the enemy is in front and close the knight uses the riposte (stab) animation.
- **Greataxe has no shield:** the block button only parries (a short deflect animation).
- **Enemy shields** (warrior) block light attacks from the front within their arc; heavies go through and deal big posture damage.
- **Lock-on** adds an over-the-shoulder camera offset so the target is not hidden behind the knight.
- **Hitstop** sets the simulation time scale to `hitstop_time_scale` (0) for the event's duration in real time; rendering keeps running at full frame rate.
