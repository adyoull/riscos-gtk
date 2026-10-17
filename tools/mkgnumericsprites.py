#!/usr/bin/env python3
"""mkgnumericsprites.py - riscos-gtk: !Gnumeric's icons from Gnumeric's own.

    mkgnumericsprites.py GNUMERIC_SRC OUTDIR

icons/*/apps/org.gnumeric.gnumeric.png (GPL, as Gnumeric) at the RISC OS
icon sizes (PRMs, Icon Sets): 34 x 34 and 17 x 17 at 90 dpi (!Sprites:
!gnumeric, sm!gnumeric) and 68 x 68 and 34 x 34 at 180 dpi (!Sprites11),
each from the nearest larger picture, as PNGs for tools/png2sprite.py.
Needs Pillow. MIT licence. Copyright (C) 2026 Andrew Youll.
"""
import os
import sys
from PIL import Image


def main():
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    pic = lambda n: Image.open(os.path.join(src, "icons", "%dx%d" % (n, n), "apps",
                                            "org.gnumeric.gnumeric.png")).convert("RGBA")
    for name, size, frm in (("!gnumeric", 34, 48), ("sm!gnumeric", 17, 24),
                            ("!gnumeric11", 68, 256), ("sm!gnumeric11", 34, 48)):
        pic(frm).resize((size, size), Image.LANCZOS).save(os.path.join(out, name + ".png"))


if __name__ == "__main__":
    main()
