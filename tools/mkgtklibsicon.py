#!/usr/bin/env python3
"""mkgtklibsicon.py - riscos-gtk: the icon of !GTKLibs (the shared GTK
libraries for mixed-linked programs), in the RISC OS style.

    mkgtklibsicon.py OUTDIR

OUTDIR/!Sprites,ff9 (90 dpi) and OUTDIR/!Sprites11,ff9 (180 dpi), each
with "!gtklibs" (34 x 34) and "sm!gtklibs" (17 x 17): a shelf of three
library volumes, one leaning on the others. An original drawing
(tools/riscosdraw.py's style). Needs Pillow. MIT licence.
Copyright (C) 2026 Andrew Youll.
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from riscosdraw import (K, WHITE, BLUE, BLUE2, GREEN, GREEN2, ORANGE, ORANGE2,  # noqa: E402
                        BROWN, BROWN2, Icon)


def band(c, x0, x1, y, h, w=1):
    c.rect(x0, y, x1, y + h, WHITE, None)


def book(c, x0, y0, x1, y1, cols, bands=True):
    top, bot = cols
    c.rect(x0, y0, x1, y1, (top, bot), K, 1)
    if bands:
        h = (y1 - y0)
        band(c, x0 + 1, x1 - 1, y0 + h * 0.18, max(1.0, h * 0.06))
        band(c, x0 + 1, x1 - 1, y0 + h * 0.72, max(1.0, h * 0.06))


def large(k):
    c = Icon(34, 34, k)
    # the shelf
    c.rect(2.5, 27.5, 31.5, 31.5, (BROWN, BROWN2), K, 1)
    # two upright volumes
    book(c, 4.5, 6.5, 11.5, 27.5, (BLUE, BLUE2))
    book(c, 11.5, 9.5, 18.5, 27.5, (GREEN, GREEN2))
    # the third leans against them
    c.poly([(19.5, 27.5), (26.0, 27.5), (31.0, 9.5), (24.5, 8.0)], (ORANGE, ORANGE2), K, 1)
    c.line([(21.6, 23.0), (27.6, 23.6)], WHITE, 1.2)
    c.line([(24.1, 13.0), (29.6, 14.0)], WHITE, 1.2)
    return c.image()


def small(k):
    c = Icon(17, 17, k)
    c.rect(0.5, 13.5, 16.5, 15.5, (BROWN, BROWN2), K, 1)
    book(c, 1.5, 3.5, 5.5, 13.5, (BLUE, BLUE2), bands=False)
    book(c, 5.5, 5.5, 9.5, 13.5, (GREEN, GREEN2), bands=False)
    c.poly([(10.0, 13.5), (13.5, 13.5), (16.0, 4.5), (12.5, 4.0)], (ORANGE, ORANGE2), K, 1)
    return c.image()


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    with tempfile.TemporaryDirectory() as t:
        for k, name in ((1, "!Sprites,ff9"), (2, "!Sprites11,ff9")):
            lp, sp = os.path.join(t, "l%d.png" % k), os.path.join(t, "s%d.png" % k)
            large(k).save(lp)
            small(k).save(sp)
            args = [sys.executable, os.path.join(HERE, "png2sprite.py")]
            if k == 2:
                args += ["--dpi", "180"]
            subprocess.check_call(args + [os.path.join(out, name), "!gtklibs=" + lp, "sm!gtklibs=" + sp])
            if len(sys.argv) > 2:          # PREVIEWDIR: the PNGs too
                large(k).save(os.path.join(sys.argv[2], "gtklibs%d.png" % k))
                small(k).save(os.path.join(sys.argv[2], "smgtklibs%d.png" % k))


if __name__ == "__main__":
    main()
