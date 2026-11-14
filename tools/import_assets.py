#!/usr/bin/env python3
"""Copy the CC0 asset packs from a download folder into assets/.

Usage: python3 -I tools/import_assets.py <download_dir> <repo_assets_dir>

<download_dir> must contain:
  KayKit-Character-Pack-Adventures-1.0/   (git clone)
  KayKit-Character-Pack-Skeletons-1.0/    (git clone)
  KayKit-Dungeon-Remastered-1.0/          (git clone)
  megakit/                                (unzipped Quaternius Fantasy Props MegaKit, standard)
  x/ken_fonts, x/kenney_input-prompts_1, x/RPGsounds_Kenney,
  x/sfx_breaking_and_falling, x/sword_-_starninjas_1,
  x/sword_clash_-_starninjas_0, x/80-CC0-creature-SFX_0   (unzipped OpenGameArt packs)
  dark_cavern_ambient_001.ogg, basilisk_boss_battle_in_game.ogg

Only glTF/GLB models (with .bin) and textures are copied; Godot .import/.tscn/.tres are ignored.
"""
import shutil
import sys
from pathlib import Path

from PIL import Image


def copy_glob(src: Path, dst: Path, patterns):
    dst.mkdir(parents=True, exist_ok=True)
    n = 0
    for pat in patterns:
        for f in sorted(src.glob(pat)):
            if f.is_file():
                shutil.copy2(f, dst / f.name)
                n += 1
    return n


def main():
    dl = Path(sys.argv[1]).resolve()
    out = Path(sys.argv[2]).resolve()
    out.mkdir(parents=True, exist_ok=True)

    adv = dl / "KayKit-Character-Pack-Adventures-1.0/addons/kaykit_character_pack_adventures"
    skel = dl / "KayKit-Character-Pack-Skeletons-1.0/addons/kaykit_character_pack_skeletons"
    dun = dl / "KayKit-Dungeon-Remastered-1.0/addons/kaykit_dungeon_remastered"

    # Characters: only the knight (player) and the barbarian (greataxe source) are needed.
    n = copy_glob(adv / "Characters/gltf", out / "kaykit_adventures",
                  ["Knight.glb", "Barbarian.glb", "knight_texture.png", "barbarian_texture.png"])
    n += copy_glob(adv / "Assets/gltf", out / "kaykit_adventures/items", ["*.gltf", "*.bin", "*.png"])
    shutil.copy2(adv / "LICENSE.txt", out / "kaykit_adventures/LICENSE.txt")
    print("kaykit_adventures:", n)

    n = copy_glob(skel / "Characters/gltf", out / "kaykit_skeletons", ["*.glb", "*.png"])
    n += copy_glob(skel / "Assets/gltf", out / "kaykit_skeletons/items", ["*.gltf", "*.bin", "*.png"])
    shutil.copy2(skel / "LICENSE.txt", out / "kaykit_skeletons/LICENSE.txt")
    print("kaykit_skeletons:", n)

    n = copy_glob(dun / "Assets/gltf", out / "kaykit_dungeon", ["*.gltf", "*.glb", "*.bin", "*.png"])
    if (dun / "LICENSE.txt").exists():
        shutil.copy2(dun / "LICENSE.txt", out / "kaykit_dungeon/LICENSE.txt")
    print("kaykit_dungeon:", n)

    # Quaternius props: models + textures downscaled to 512 (the lit shader only samples base color).
    mk = dl / "megakit"
    qd = out / "quaternius_props"
    n = copy_glob(mk / "Exports/glTF", qd, ["*.gltf", "*.bin"])
    for tex in sorted((mk / "Textures").glob("T_Trim_*.png")):
        img = Image.open(tex)
        img = img.resize((512, 512), Image.LANCZOS)
        img.save(qd / tex.name, optimize=True)
        n += 1
    shutil.copy2(mk / "License_Standard.txt", qd / "License_Standard.txt")
    print("quaternius_props:", n)

    x = dl / "x"
    # Kenney font + input prompts (Xbox, PlayStation, Switch, keyboard & mouse, default size).
    n = copy_glob(x / "ken_fonts", out / "fonts", ["kenvector_future.ttf", "kenvector_future_thin.ttf",
                                                   "kenpixel.ttf", "license.txt"])
    print("fonts:", n)
    ip = x / "kenney_input-prompts_1"
    for fam, sub in [("xbox", "Xbox Series/Default"), ("playstation", "PlayStation Series/Default"),
                     ("switch", "Nintendo Switch/Default"), ("keyboard", "Keyboard & Mouse/Default")]:
        src = ip / sub
        if src.exists():
            k = copy_glob(src, out / "prompts" / fam, ["*.png"])
            print("prompts", fam, k)
        else:
            print("prompts MISSING", fam, src)
    shutil.copy2(ip / "License.txt", out / "prompts/License.txt")

    # Audio
    a = out / "audio"
    n = copy_glob(x / "RPGsounds_Kenney/OGG", a / "kenney_rpg", ["*.ogg"])
    n += copy_glob(x / "sfx_breaking_and_falling", a / "impacts", ["**/*.ogg"])
    n += copy_glob(x / "sword_-_starninjas_1", a / "sword", ["**/*.ogg"])
    n += copy_glob(x / "sword_clash_-_starninjas_0", a / "sword", ["**/*.ogg"])
    n += copy_glob(x / "80-CC0-creature-SFX_0", a / "creature", ["**/*.ogg"])
    (a / "music").mkdir(parents=True, exist_ok=True)
    shutil.copy2(dl / "dark_cavern_ambient_001.ogg", a / "music/ambient.ogg")
    shutil.copy2(dl / "basilisk_boss_battle_in_game.ogg", a / "music/boss.ogg")
    print("audio:", n + 2)


if __name__ == "__main__":
    main()
