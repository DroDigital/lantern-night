#!/usr/bin/env python3
"""Draw the Lantern Knight app icon (a warm lantern on a cold dark tile) and export
packaging/{windows/icon.ico, macos/icon.icns, linux/lantern-knight.png}.

Usage: python3 -I tools/make_icon.py <repo_root>
"""
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


def draw(size=1024):
    s = size
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # rounded dark blue-gray tile
    d.rounded_rectangle([s * 0.04, s * 0.04, s * 0.96, s * 0.96], radius=s * 0.18, fill=(22, 27, 38, 255))
    # warm glow
    glow = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for i in range(40, 0, -1):
        r = s * 0.42 * i / 40
        a = int(150 * (1 - i / 40) ** 1.6)
        gd.ellipse([s / 2 - r, s * 0.56 - r, s / 2 + r, s * 0.56 + r], fill=(255, 150, 60, a))
    glow = glow.filter(ImageFilter.GaussianBlur(s * 0.02))
    img = Image.alpha_composite(img, glow)
    d = ImageDraw.Draw(img)
    metal = (58, 52, 60, 255)
    # handle ring
    d.arc([s * 0.42, s * 0.14, s * 0.58, s * 0.32], 180, 360, fill=metal, width=int(s * 0.03))
    # cap
    d.polygon([(s * 0.36, s * 0.34), (s * 0.64, s * 0.34), (s * 0.58, s * 0.25), (s * 0.42, s * 0.25)], fill=metal)
    d.rectangle([s * 0.33, s * 0.34, s * 0.67, s * 0.38], fill=metal)
    # glass body
    d.rounded_rectangle([s * 0.36, s * 0.38, s * 0.64, s * 0.74], radius=s * 0.04, fill=(255, 196, 110, 255))
    # flame
    cx, cy = s * 0.5, s * 0.6
    pts = []
    for k in range(64):
        t = k / 64 * 2 * math.pi
        rr = s * 0.07 * (1 - 0.55 * math.sin(t))
        pts.append((cx + rr * math.cos(t) * 0.8, cy + rr * math.sin(t) * 1.3 + s * 0.02))
    d.polygon(pts, fill=(255, 245, 210, 255))
    # bars
    for x in (0.45, 0.55):
        d.rectangle([s * x - s * 0.012, s * 0.38, s * x + s * 0.012, s * 0.74], fill=metal)
    # base
    d.rectangle([s * 0.33, s * 0.74, s * 0.67, s * 0.79], fill=metal)
    d.rectangle([s * 0.38, s * 0.79, s * 0.62, s * 0.82], fill=metal)
    return img


def main():
    root = Path(sys.argv[1])
    img = draw(1024)
    (root / "packaging/linux").mkdir(parents=True, exist_ok=True)
    img.resize((256, 256), Image.LANCZOS).save(root / "packaging/linux/lantern-knight.png")
    img.save(root / "packaging/windows/icon.ico", sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    img.save(root / "packaging/macos/icon.icns")
    img.resize((512, 512), Image.LANCZOS).save(root / "assets/icon.png")


if __name__ == "__main__":
    main()
