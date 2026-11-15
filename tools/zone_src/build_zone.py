#!/usr/bin/env python3
"""Authoring script for data/levels/zone.json (Lantern Knight's one connected zone).

The zone is hand-designed: rooms are listed below as rectangles of cells per height layer,
plus explicit placements. This script only rasterises those rectangles into the ASCII layers
the game loads; nothing is random. After generating, the F2 editor can tune zone.json by eye
(re-running this script would overwrite those edits).

Usage: python3 -I tools/zone_src/build_zone.py data/levels/zone.json
Cell size 2 m. Level 0 = Flooded Ossuary (lower), level 1 = Crypt Entrance + Warden's Hall.
"""
import json
import sys

W, D = 58, 36
CELL, LH = 2.0, 2.0


def grid():
    return [[' '] * W for _ in range(D)]


def rect(g, x0, z0, x1, z1, ch='.'):
    for z in range(z0, z1 + 1):
        for x in range(x0, x1 + 1):
            g[z][x] = ch


def walls(g):
    """Every void cell touching an open cell becomes a solid wall cell ('#')."""
    out = [row[:] for row in g]
    for z in range(D):
        for x in range(W):
            if g[z][x] != ' ':
                continue
            for dz in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    zz, xx = z + dz, x + dx
                    if 0 <= zz < D and 0 <= xx < W and g[zz][xx] not in (' ', '#'):
                        out[z][x] = '#'
    return out


def C(x, z, level, dy=0.0):
    return [round((x + 0.5) * CELL, 2), round(level * LH + dy, 2), round((z + 0.5) * CELL, 2)]


def E(x, z):
    """Grid vertex / edge coordinate (cell corner)."""
    return x * CELL, z * CELL


# ------------------------------------------------------------------ level 1 (upper)
L1 = grid()
# Crypt Entrance
rect(L1, 4, 29, 8, 33)            # start hall
rect(L1, 5, 23, 7, 28)            # corridor north
rect(L1, 2, 15, 12, 22)           # room 1 (first fight)
rect(L1, 6, 12, 8, 14)            # passage
rect(L1, 2, 4, 12, 11)            # room 2 (shrine 1)
rect(L1, 13, 6, 16, 6)            # exit corridor east -> stairs down
rect(L1, 13, 18, 15, 18)          # shortcut corridor (gate at 13,18)
# Warden's Hall
rect(L1, 44, 18, 44, 18)          # landing at the top of the stairs from the ossuary
rect(L1, 45, 14, 55, 24)          # gauntlet hall
rect(L1, 49, 25, 50, 26)          # passage to the mini-boss room
rect(L1, 45, 27, 54, 34)          # Twin Blades room
rect(L1, 44, 30, 44, 30)          # shortcut 2 gate cell
rect(L1, 49, 12, 49, 13)          # sealed passage north (gate at 49,13)
rect(L1, 46, 9, 53, 11)           # antechamber (shrine 3)
rect(L1, 49, 8, 49, 8)            # fog gate
rect(L1, 44, 0, 55, 7)            # boss arena

# ------------------------------------------------------------------ level 0 (Flooded Ossuary)
L0 = grid()
rect(L0, 18, 2, 31, 11, ',')      # hall A (dark)
rect(L0, 22, 4, 26, 7, '.')       # old tiled floor in hall A
rect(L0, 32, 5, 32, 5)            # door to the greataxe room
rect(L0, 33, 3, 38, 8)            # greataxe room
rect(L0, 23, 12, 25, 15)          # passage south
rect(L0, 18, 16, 33, 26, '~')     # hall B (flooded)
rect(L0, 19, 22, 23, 26, '.')     # dry dais around shrine 2
rect(L0, 17, 18, 17, 18)          # foot of shortcut 1 stairs
rect(L0, 34, 18, 42, 18)          # corridor east to Warden's Hall
rect(L0, 20, 27, 20, 27)          # door to the drowned chapel
rect(L0, 18, 28, 23, 33, '~')     # drowned chapel (optional side area)
rect(L0, 28, 27, 29, 27, '~')     # passage to hall D
rect(L0, 26, 28, 33, 32, '~')     # hall D (dark, flooded)
rect(L0, 34, 30, 42, 30)          # corridor to shortcut 2 stairs
rect(L0, 34, 32, 34, 32)          # hidden wall cell
rect(L0, 35, 32, 37, 34, ',')     # secret alcove
# Stairs (on the lower layer, rising onto the upper one)
L0[6][17] = '<'                   # Crypt Entrance -> ossuary (rises west)
L0[18][16] = '<'                  # shortcut 1 (rises west to the gate)
L0[18][43] = '>'                  # ossuary -> Warden's Hall (rises east)
L0[30][43] = '>'                  # shortcut 2 (rises east to the gate)

L0 = walls(L0)
L1 = walls(L1)
# Stairs cells on level 0 must not get walls on level 1 above them (they are void there).
for (z, x) in [(6, 17), (18, 16), (18, 43), (30, 43)]:
    L1[z][x] = ' '

legend = {
    "#": {"kind": "wall", "variants": ["wall", "wall", "wall", "wall_cracked", "wall_arched", "wall_window_closed"]},
    ".": {"kind": "floor", "piece": "floor_tile_large"},
    ",": {"kind": "floor", "piece": "floor_dirt_large"},
    ":": {"kind": "floor", "piece": "floor_tile_large_rocks"},
    "~": {"kind": "floor", "piece": "floor_dirt_large", "water": True, "dark": True},
    "<": {"kind": "stairs", "piece": "stairs", "rot": 90},
    ">": {"kind": "stairs", "piece": "stairs", "rot": 270},
    "^": {"kind": "stairs", "piece": "stairs", "rot": 0},
    "v": {"kind": "stairs", "piece": "stairs", "rot": 180},
}

zone = {
    "version": 1,
    "name": "zone",
    "cell_size": CELL,
    "model_scale": 0.5,
    "layer_height": LH,
    "music": "ambient",
    "legend": legend,
    "layers": [
        {"name": "ossuary", "level": 0, "origin": [0, 0], "rows": [''.join(r) for r in L0]},
        {"name": "upper", "level": 1, "origin": [0, 0], "rows": [''.join(r) for r in L1]},
    ],
    "player_spawn": {"pos": C(6, 31, 1), "rot": 180},
}

zone["shrines"] = [
    {"id": "shrine_entrance", "name": "Entrance Shrine", "pos": C(7, 6, 1), "rot": 180},
    {"id": "shrine_ossuary", "name": "Ossuary Shrine", "pos": C(21, 24, 0), "rot": 90},
    {"id": "shrine_warden", "name": "Warden's Shrine", "pos": C(49, 10, 1), "rot": 180},
]

zone["enemies"] = [
    # Crypt Entrance: a pair in room 1, one more at the way down.
    {"id": "e_m1", "kind": "minion", "pos": C(5, 17, 1), "rot": 0},
    {"id": "e_m2", "kind": "minion", "pos": C(10, 19, 1), "rot": 270, "patrol": [[7.0, 34.0], [21.0, 34.0]]},
    {"id": "e_m3", "kind": "minion", "pos": C(15, 6, 1), "rot": 270},
    # Flooded Ossuary, hall A
    {"id": "e_m4", "kind": "minion", "pos": C(22, 5, 0), "rot": 90},
    {"id": "e_m5", "kind": "minion", "pos": C(24, 9, 0), "rot": 90},
    {"id": "e_r1", "kind": "rogue", "pos": C(29, 4, 0), "rot": 270},
    {"id": "e_g1", "kind": "mage", "pos": C(30, 10, 0), "rot": 270},
    {"id": "e_w1", "kind": "warrior", "pos": C(35, 5, 0), "rot": 270},
    # hall B
    {"id": "e_r2", "kind": "rogue", "pos": C(26, 19, 0), "rot": 0, "patrol": [[52.0, 38.0], [62.0, 38.0]]},
    {"id": "e_r3", "kind": "rogue", "pos": C(30, 24, 0), "rot": 270},
    {"id": "e_g2", "kind": "mage", "pos": C(32, 17, 0), "rot": 270},
    {"id": "e_g3", "kind": "mage", "pos": C(32, 26, 0), "rot": 270},
    {"id": "e_m6", "kind": "minion", "pos": C(38, 18, 0), "rot": 270},
    {"id": "e_m7", "kind": "minion", "pos": C(40, 18, 0), "rot": 270},
    # drowned chapel (optional)
    {"id": "e_g4", "kind": "mage", "pos": C(19, 31, 0), "rot": 0},
    {"id": "e_g5", "kind": "mage", "pos": C(22, 32, 0), "rot": 0},
    {"id": "e_m8", "kind": "minion", "pos": C(21, 29, 0), "rot": 0},
    # hall D (dark)
    {"id": "e_m9", "kind": "minion", "pos": C(27, 29, 0), "rot": 90},
    {"id": "e_m10", "kind": "minion", "pos": C(30, 31, 0), "rot": 90},
    {"id": "e_m11", "kind": "minion", "pos": C(32, 29, 0), "rot": 180},
    {"id": "e_r4", "kind": "rogue", "pos": C(38, 30, 0), "rot": 270},
    # Warden's Hall gauntlet
    {"id": "e_w2", "kind": "warrior", "pos": C(48, 16, 1), "rot": 270},
    {"id": "e_w3", "kind": "warrior", "pos": C(52, 19, 1), "rot": 270},
    {"id": "e_w4", "kind": "warrior", "pos": C(48, 22, 1), "rot": 0},
    {"id": "e_m12", "kind": "minion", "pos": C(54, 15, 1), "rot": 270},
    {"id": "e_m13", "kind": "minion", "pos": C(54, 23, 1), "rot": 270},
    # The Twin Blades
    {"id": "twin_a", "kind": "twin_blade", "pos": C(47, 32, 1), "rot": 0},
    {"id": "twin_b", "kind": "twin_blade", "pos": C(52, 32, 1), "rot": 0},
    # The Hollow Warden waits in the arena behind the fog gate.
    {"id": "warden", "kind": "hollow_warden", "pos": [99.0, 2.0, 5.0], "rot": 0, "dormant": True},
]

zone["braziers"] = [
    {"id": "b_hallA_w", "pos": C(19, 3, 0)},
    {"id": "b_hallA_e", "pos": C(30, 2, 0)},
    {"id": "b_hallB_n", "pos": C(19, 17, 0)},
    {"id": "b_hallB_s", "pos": C(32, 21, 0)},
    {"id": "b_chapel", "pos": C(23, 28, 0)},
    {"id": "b_hallD", "pos": C(32, 31, 0), "reveals": "secret_wall"},
    {"id": "b_gauntlet", "pos": C(55, 24, 1)},
    {"id": "b_arena_1", "pos": C(45, 1, 1), "arena": True},
    {"id": "b_arena_2", "pos": C(54, 1, 1), "arena": True},
    {"id": "b_arena_3", "pos": C(45, 6, 1), "arena": True},
    {"id": "b_arena_4", "pos": C(54, 6, 1), "arena": True},
]

zone["gates"] = [
    {"id": "gate_shortcut_entrance", "kind": "shortcut", "pos": C(13, 18, 1), "rot": 90, "open_side": 90},
    {"id": "gate_shortcut_warden", "kind": "shortcut", "pos": C(44, 30, 1), "rot": 90, "open_side": 90},
    {"id": "gate_sealed", "kind": "locked", "pos": C(49, 13, 1), "rot": 0, "unlock_boss": "twin_blades"},
    {"id": "secret_wall", "kind": "hidden", "pos": C(34, 32, 0), "rot": 90},
]
zone["fog_gate"] = {"id": "fog_gate", "pos": C(49, 8, 1), "rot": 0}

zone["items"] = [
    {"id": "greataxe", "kind": "weapon", "model": "kaykit_adventures/items/axe_2handed.gltf", "pos": C(37, 4, 0), "rot": 30,
     "label": "Take the greataxe"},
    {"id": "shard_chapel", "kind": "flask_shard", "model": "quaternius_props/Potion_1.gltf", "pos": C(20, 33, 0),
     "label": "Pick up the Flask Shard"},
    {"id": "shard_secret", "kind": "flask_shard", "model": "quaternius_props/Potion_1.gltf", "pos": C(36, 33, 0),
     "label": "Pick up the Flask Shard"},
    {"id": "embers_gauntlet", "kind": "embers", "model": "kaykit_dungeon/coin_stack_large.gltf.glb", "pos": C(55, 14, 1),
     "label": "Take the embers"},
]

zone["hints"] = [
    {"pos": C(6, 31, 1), "text": "Your lantern is your life. Guard its flame."},
    {"pos": C(6, 27, 1), "text": "Tap {Roll} to roll through danger. Hold it to sprint."},
    {"pos": C(6, 23, 1), "text": "{LockOn} locks on.  {Attack} light attack.  {Heavy} heavy (hold to charge)."},
    {"pos": C(7, 15, 1), "text": "Hold {Block} to raise your shield. Tap it just before a hit to parry."},
    {"pos": C(7, 9, 1), "text": "Rest at lantern shrines with {Interact}. The dead rise again."},
    {"pos": C(12, 18, 1), "text": "A gate. Perhaps it opens from the other side."},
    {"pos": C(19, 6, 0), "text": "The dark hides more than bones. Light the braziers."},
    {"pos": C(22, 21, 0), "text": "{Flask} drinks an oil flask and feeds your flame."},
    {"pos": C(46, 18, 1), "text": "Heavy blows break shields. Or strike from the side."},
]

zone["triggers"] = [
    {"id": "area_entrance", "name": "Crypt Entrance", "pos": [13.0, 3.0, 63.0], "size": [12, 4, 12], "music": "ambient"},
    {"id": "area_ossuary", "name": "Flooded Ossuary", "pos": [40.0, 1.0, 13.0], "size": [6, 3, 18], "music": "ambient"},
    {"id": "area_warden", "name": "Warden's Hall", "pos": [92.0, 3.0, 38.0], "size": [8, 4, 8], "music": "ambient"},
    {"id": "area_arena", "name": "", "pos": [99.0, 3.0, 8.0], "size": [24, 4, 16]},
]

def torch(x, z, level, wall):
    """Torch on the wall at the given side of a cell: wall in 'n','s','e','w'."""
    cx, y, cz = C(x, z, level)
    off = {"n": (0, -0.75), "s": (0, 0.75), "e": (0.75, 0), "w": (-0.75, 0)}[wall]
    return {"pos": [cx + off[0], y + 1.5, cz + off[1]], "preset": "torch_light"}

lights = [
    torch(4, 31, 1, "w"), torch(8, 29, 1, "e"), torch(5, 25, 1, "w"), torch(2, 18, 1, "w"), torch(12, 16, 1, "e"),
    torch(2, 8, 1, "w"), torch(12, 9, 1, "e"), torch(16, 6, 1, "n"),
    torch(45, 15, 1, "w"), torch(55, 17, 1, "e"), torch(45, 22, 1, "w"), torch(45, 28, 1, "w"), torch(54, 28, 1, "e"),
    torch(46, 10, 1, "w"), torch(53, 10, 1, "e"),
]
for i, l in enumerate(lights):
    l["id"] = "torch_%d" % i
zone["lights"] = lights

def prop(model, x, z, level, rot=0, scale=1.0, collide=True, dx=0.0, dz=0.0):
    p = C(x, z, level)
    p[0] += dx
    p[2] += dz
    d = {"model": model, "pos": p, "rot": rot, "collide": collide}
    if scale != 1.0:
        d["scale"] = scale
    return d

K = "kaykit_dungeon/"
Q = "quaternius_props/"
zone["props"] = [
    prop(K + "barrel_large.gltf.glb", 4, 33, 1, 10), prop(K + "crates_stacked.gltf.glb", 8, 33, 1, -15),
    prop(K + "pillar.gltf.glb", 4, 17, 1), prop(K + "pillar.gltf.glb", 10, 17, 1), prop(K + "pillar.gltf.glb", 4, 21, 1),
    prop(K + "pillar.gltf.glb", 10, 21, 1), prop(K + "table_long_broken.gltf.glb", 11, 21, 1, 90),
    prop(K + "sword_shield_broken.gltf.glb", 3, 15, 1, 30, collide=False),
    prop(K + "bed_decorated.gltf.glb", 11, 4, 1, 180), prop(K + "shelf_small_candles.gltf.glb", 2, 4, 1, 90),
    prop(K + "barrel_small_stack.gltf.glb", 12, 11, 1, 0),
    prop(K + "pillar_decorated.gltf.glb", 21, 4, 0), prop(K + "pillar_decorated.gltf.glb", 28, 4, 0),
    prop(K + "pillar_decorated.gltf.glb", 21, 9, 0), prop(K + "pillar_decorated.gltf.glb", 28, 9, 0),
    prop(K + "rubble_half.gltf.glb", 31, 7, 0, 90, collide=True), prop(K + "coin_stack_small.gltf.glb", 26, 3, 0, 0, collide=False),
    prop(K + "chest_gold.glb", 38, 7, 0, 270), prop(Q + "WeaponStand.gltf", 37, 3, 0, 0, 1.4, False),
    prop(K + "column.gltf.glb", 22, 18, 0), prop(K + "column.gltf.glb", 29, 18, 0), prop(K + "column.gltf.glb", 26, 23, 0),
    prop(K + "rubble_large.gltf.glb", 28, 25, 0, 20), prop(K + "barrier_column.gltf.glb", 19, 21, 0, 0),
    prop(Q + "CandleStick_Stand.gltf", 23, 22, 0, 0, 1.4, False), prop(Q + "Chandelier.gltf", 21, 30, 0, 0, 1.3, False),
    prop(K + "trunk_large_A.gltf.glb", 35, 34, 0, 0), prop(Q + "Cage_Small.gltf", 27, 32, 0, 0, 1.6),
    prop(K + "pillar.gltf.glb", 47, 17, 1), prop(K + "pillar.gltf.glb", 53, 17, 1), prop(K + "pillar.gltf.glb", 47, 21, 1),
    prop(K + "pillar.gltf.glb", 53, 21, 1), prop(K + "banner_patternA_red.gltf.glb", 50, 14, 1, 0, collide=False, dz=-0.7),
    prop(K + "pillar_decorated.gltf.glb", 46, 29, 1), prop(K + "pillar_decorated.gltf.glb", 53, 29, 1),
    prop(K + "sword_shield_gold.gltf.glb", 50, 34, 1, 0, collide=False),
    prop(K + "pillar_decorated.gltf.glb", 46, 2, 1), prop(K + "pillar_decorated.gltf.glb", 53, 2, 1),
    prop(K + "pillar_decorated.gltf.glb", 46, 5, 1), prop(K + "pillar_decorated.gltf.glb", 53, 5, 1),
]

# ------------------------------------------------------------------ dressing
# Deterministic clutter: low props along walls (no collision) and bigger ones in room corners
# (with collision). Picks come from a fixed hash of the cell, so re-running gives the same zone.
# Cells near anything the player interacts with, stairs and the boss arena stay clear.
def cell_of(pos):
    return int(pos[0] // CELL), int(pos[2] // CELL), int(round(pos[1] / LH))

keep_clear = set()
def clear_around(pos, r=1):
    cx, cz, lv = cell_of(pos)
    for dz in range(-r, r + 1):
        for dx in range(-r, r + 1):
            keep_clear.add((cx + dx, cz + dz, lv))

for group in ("shrines", "braziers", "gates", "items"):
    for m in zone[group]:
        clear_around(m["pos"])
clear_around(zone["fog_gate"]["pos"], 2)
clear_around(zone["player_spawn"]["pos"], 1)
for h in zone["hints"]:
    clear_around(h["pos"], 0)
for e in zone["enemies"]:
    clear_around(e["pos"], 0)
for pr in zone["props"]:
    clear_around(pr["pos"], 0)
for (z, x) in [(6, 17), (18, 16), (18, 43), (30, 43)]:   # stairs and their landings
    for lv in (0, 1):
        for dz in (-1, 0, 1):
            for dx in (-2, -1, 0, 1, 2):
                keep_clear.add((x + dx, z + dz, lv))
for z in range(0, 9):                                       # boss arena: keep the floor open
    for x in range(43, 57):
        keep_clear.add((x, z, 1))

def hval(x, z, lv, salt=0):
    return ((x * 73856093) ^ (z * 19349663) ^ (lv * 83492791) ^ (salt * 2654435761)) % 1000

WALL_PROPS = [(K + "candle_triple.gltf.glb", 1.0), (K + "candle_melted.gltf.glb", 1.0), (K + "bottle_A_brown.gltf.glb", 1.0),
              (K + "coin_stack_small.gltf.glb", 1.0), (K + "sword_shield_broken.gltf.glb", 1.0), (K + "plate_stack.gltf.glb", 1.0),
              (K + "box_small.gltf.glb", 1.0), (Q + "Book_Stack_1.gltf", 1.4), (Q + "Bucket_Wooden_1.gltf", 1.4),
              (Q + "Chain_Coil.gltf", 1.4), (Q + "Vase_2.gltf", 1.4), (Q + "Vase_Rubble_Medium.gltf", 1.4),
              (Q + "CandleStick_Triple.gltf", 1.4), (Q + "Scroll_2.gltf", 1.4)]
CORNER_PROPS = [(K + "barrel_large.gltf.glb", 1.0), (K + "barrel_small_stack.gltf.glb", 1.0), (K + "crates_stacked.gltf.glb", 1.0),
                (K + "box_stacked.gltf.glb", 1.0), (K + "keg_decorated.gltf.glb", 1.0), (K + "trunk_medium_A.gltf.glb", 1.0),
                (K + "rubble_half.gltf.glb", 1.0), (K + "shelf_small_candles.gltf.glb", 1.0)]

def is_floor(g, x, z):
    return 0 <= z < D and 0 <= x < W and g[z][x] in ".,:"

def is_wall(g, x, z):
    return not (0 <= z < D and 0 <= x < W) or g[z][x] == "#"

dressing = []
for lv, g in ((0, L0), (1, L1)):
    for z in range(D):
        for x in range(W):
            if not is_floor(g, x, z) or (x, z, lv) in keep_clear:
                continue
            n, s_, e, w = is_wall(g, x, z - 1), is_wall(g, x, z + 1), is_wall(g, x + 1, z), is_wall(g, x - 1, z)
            walls_here = [d for d, b in (("n", n), ("s", s_), ("e", e), ("w", w)) if b]
            h = hval(x, z, lv)
            vec = {"n": (0, -1), "s": (0, 1), "e": (1, 0), "w": (-1, 0)}
            if len(walls_here) == 2 and walls_here not in (["n", "s"], ["e", "w"]):
                ox, oz = vec[walls_here[0]][0] + vec[walls_here[1]][0], vec[walls_here[0]][1] + vec[walls_here[1]][1]
                # A room corner (not a corridor bend): the cell diagonally inward is open floor too.
                if is_floor(g, x - ox, z - oz) and h < 650:
                    model, sc = CORNER_PROPS[hval(x, z, lv, 1) % len(CORNER_PROPS)]
                    dressing.append(prop(model, x, z, lv, hval(x, z, lv, 2) % 360, sc, True, ox * 0.5, oz * 0.5))
            elif len(walls_here) == 1:
                d = walls_here[0]
                ox, oz = vec[d]
                side_a, side_b = (x + oz, z + ox), (x - oz, z - ox)
                if is_floor(g, x - ox, z - oz) and is_floor(g, *side_a) and is_floor(g, *side_b) and h < 220:
                    model, sc = WALL_PROPS[hval(x, z, lv, 3) % len(WALL_PROPS)]
                    shift = (hval(x, z, lv, 4) % 100 - 50) / 100.0
                    dressing.append(prop(model, x, z, lv, hval(x, z, lv, 5) % 360, sc, False,
                                         ox * 0.62 + oz * shift, oz * 0.62 + ox * shift))
zone["props"] += dressing
print("dressing:", len(dressing), "props")

out = sys.argv[1] if len(sys.argv) > 1 else "data/levels/zone.json"
with open(out, "w") as f:
    json.dump(zone, f, indent=1)
print("wrote", out)
for name, g in (("level 0", L0), ("level 1", L1)):
    print(name)
    for r in g:
        print("  " + ''.join(r))
