#!/usr/bin/env python3
"""mkmtpaintsprites.py - riscos-gtk: !mtPaint's icons from mtPaint's own.

    mkmtpaintsprites.py MTPAINT_SRC OUTDIR

doc/mtpaint.png (48 x 48; GPL 3, as mtPaint) reduced to the RISC OS icon
sizes (PRMs, Icon Sets): 34 x 34 and 17 x 17 at 90 dpi (!Sprites: !mtpaint,
sm!mtpaint), and 68 x 68 (enlarged: the only picture is 48 pixels) and
34 x 34 at 180 dpi (!Sprites11), as PNGs for tools/png2sprite.py.
Needs Pillow. MIT licence. Copyright (C) 2026 Andrew Youll.
"""
import os
import sys
from PIL import Image


def main():
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    im = Image.open(os.path.join(src, "doc", "mtpaint.png")).convert("RGBA")
    for name, size in (("!mtpaint", 34), ("sm!mtpaint", 17), ("!mtpaint11", 68), ("sm!mtpaint11", 34)):
        im.resize((size, size), Image.LANCZOS).save(os.path.join(out, name + ".png"))


if __name__ == "__main__":
    main()
