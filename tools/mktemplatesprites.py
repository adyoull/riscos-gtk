#!/usr/bin/env python3
"""mktemplatesprites.py OUTDIR - the riscos-gtk devkit template's
application icon: a small window with a speech bubble saying hello, at
90 dpi (34 and 17 pixels) and 180 dpi (68 and 34). Writes OUTDIR/!hello.png,
sm!hello.png, !hello11.png and sm!hello11.png for png2sprite.py. MIT licence.
"""
import os
import sys
from PIL import Image, ImageDraw

S = 8   # supersampling


def icon(size):
    n = size * S
    im = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    u = n / 34.0
    def r(x0, y0, x1, y1, fill, outline=None, w=1):
        d.rectangle([x0 * u, y0 * u, x1 * u, y1 * u], fill=fill, outline=outline, width=max(1, int(w * u)))
    # the window: cream work area, Wimp-blue title bar, black outline
    r(2, 5, 31, 31, (238, 238, 221, 255), (0, 0, 0, 255), 1.2)
    r(2, 5, 31, 11, (153, 187, 238, 255), (0, 0, 0, 255), 1.2)
    r(4, 7, 7, 9.5, (221, 221, 221, 255), (0, 0, 0, 255), 0.8)
    r(26, 7, 29, 9.5, (221, 221, 221, 255), (0, 0, 0, 255), 0.8)
    # a speech bubble
    d.ellipse([7 * u, 14 * u, 27 * u, 26 * u], fill=(255, 255, 255, 255), outline=(0, 0, 0, 255), width=max(1, int(1.1 * u)))
    d.polygon([(11 * u, 24 * u), (9 * u, 29.5 * u), (15 * u, 25.3 * u)], fill=(255, 255, 255, 255))
    d.line([(11 * u, 24.4 * u), (9 * u, 29.5 * u), (15 * u, 25.6 * u)], fill=(0, 0, 0, 255), width=max(1, int(1.1 * u)))
    # "hi" in green
    g = (0, 150, 60, 255)
    w = max(1, int(1.8 * u))
    d.line([(13 * u, 16.5 * u), (13 * u, 23.5 * u)], fill=g, width=w)
    d.line([(13 * u, 20 * u), (16.5 * u, 19 * u), (17 * u, 23.5 * u)], fill=g, width=w)
    d.line([(20.5 * u, 19 * u), (20.5 * u, 23.5 * u)], fill=g, width=w)
    d.ellipse([19.6 * u, 16 * u, 21.4 * u, 17.8 * u], fill=g)
    return im.resize((size, size), Image.LANCZOS)


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for name, size in (("!hello", 34), ("sm!hello", 17), ("!hello11", 68), ("sm!hello11", 34)):
        icon(size).save(os.path.join(out, name + ".png"))


if __name__ == "__main__":
    main()
