#!/usr/bin/env python3
"""mkgeanysprites.py - riscos-gtk: !Geany's icons from Geany's own drawing.

    mkgeanysprites.py GEANY_SRC OUTDIR

icons/scalable/geany.svg (GPL 2, as Geany) drawn with rsvg-convert at
the RISC OS icon sizes (PRMs, Icon Sets): 34 x 34 and 17 x 17 at 90 dpi
(!Sprites: !geany, sm!geany), 68 x 68 and 34 x 34 at 180 dpi
(!Sprites11), written as PNGs for tools/png2sprite.py.
Needs rsvg-convert. MIT licence. Copyright (C) 2026 Andrew Youll.
"""
import os
import subprocess
import sys


def main():
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    svg = os.path.join(src, "icons", "scalable", "geany.svg")
    for name, size in (("!geany", 34), ("sm!geany", 17), ("!geany11", 68), ("sm!geany11", 34)):
        subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), svg,
                        "-o", os.path.join(out, name + ".png")], check=True)


if __name__ == "__main__":
    main()
