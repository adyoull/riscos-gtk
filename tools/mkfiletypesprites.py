#!/usr/bin/env python3
"""mkfiletypesprites.py - riscos-gtk: file type icons in the RISC OS style.

    mkfiletypesprites.py FONTDIR OUTDIR TYPE:KIND:LABEL:COLOUR ...

For each TYPE (three hex digits, e.g. a7f) the Filer's sprites file_TYPE
(34 x 34) and small_TYPE (17 x 17) as PNGs: OUTDIR/TYPE/file.png and
small.png at 90 dpi, file11.png and small11.png (68 and 34 pixels) at 180
dpi, for tools/png2sprite.py. build/build-app.sh makes them for the types
in a program's FileTypes that the desktop's themes have no icon for.

KIND: sheet (a spreadsheet's grid) or doc (lines of writing). LABEL: what
the band across the page says (XLSX, ODS...). COLOUR: the band's colour
(green, teal, blue, sky, grey, orange, red).

Original drawings (tools/riscosdraw.py's style). FONTDIR has DejaVu's TTFs.
Needs Pillow. MIT licence. Copyright (C) 2026 Andrew Youll.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from riscosdraw import K, WHITE, PAPER2, GREY, GREY2, GREY3, Icon  # noqa: E402

COLOURS = {
    "green": ((70, 180, 80, 255), (20, 120, 40, 255)),
    "teal": ((40, 170, 170, 255), (0, 110, 120, 255)),
    "blue": ((64, 120, 230, 255), (20, 60, 160, 255)),
    "sky": ((60, 160, 230, 255), (0, 100, 170, 255)),
    "grey": ((150, 150, 150, 255), (90, 90, 90, 255)),
    "orange": ((255, 153, 0, 255), (204, 85, 0, 255)),
    "red": ((230, 34, 34, 255), (170, 0, 0, 255)),
}


def tint(col, t=0.65):
    """col (a colour) mixed with white: t of white"""
    return tuple(int(col[i] + (255 - col[i]) * t) for i in range(3)) + (255,)


def page(c, x0, y0, x1, y1, fold):
    c.poly([(x0, y0), (x1 - fold, y0), (x1, y0 + fold), (x1, y1), (x0, y1)], (WHITE, PAPER2))
    c.poly([(x1 - fold, y0), (x1 - fold, y0 + fold), (x1, y0 + fold)], GREY, K)


def large(kind, label, colour, fontdir, k):
    c = Icon(34, 34, k)
    top, bot = COLOURS[colour]
    page(c, 6.5, 1.5, 27.5, 32.5, 6)
    if kind == "sheet":
        x0, y0, x1, y1, cols, rows = 9.5, 8.5, 24.5, 21.5, 4, 4
        c.rect(x0, y0, x1, y0 + (y1 - y0) / rows, tint(top), None)
        c.rect(x0, y0, x0 + (x1 - x0) / cols, y1, tint(top), None)
        for i in range(1, cols):
            x = x0 + (x1 - x0) * i / cols
            c.line([(x, y0), (x, y1)], GREY2, 1)
        for i in range(1, rows):
            y = y0 + (y1 - y0) * i / rows
            c.line([(x0, y), (x1, y)], GREY2, 1)
        c.rect(x0, y0, x1, y1, None, GREY3, 1)
    else:
        c.line([(9.5, 7.5), (17.5, 7.5)], bot, 1.6)
        for i, y in enumerate((10.5, 12.5, 14.5, 16.5, 18.5, 20.5)):
            c.line([(9.5, y), (24.5 - (5 if i == 5 else 0), y)], GREY2, 1)
    c.rect(3.5, 23.5, 30.5, 30.5, (top, bot), K, 1)
    c.text(17, 27.2, label, 6.2 if len(label) <= 4 else 5.2,
           os.path.join(fontdir, "DejaVuSansCondensed-Bold.ttf"), WHITE, anchor="mm")
    return c.image()


def small(kind, colour, k):
    c = Icon(17, 17, k)
    top, bot = COLOURS[colour]
    page(c, 3.5, 0.5, 13.5, 16.5, 3)
    if kind == "sheet":
        c.rect(5.5, 4.5, 11.5, 9.5, tint(top), None)
        c.line([(8.5, 4.5), (8.5, 9.5)], GREY2, 0.8)
        c.line([(5.5, 7), (11.5, 7)], GREY2, 0.8)
        c.rect(5.5, 4.5, 11.5, 9.5, None, GREY3, 0.8)
    else:
        for y in (4.5, 6.5, 8.5):
            c.line([(5.5, y), (11.5, y)], GREY2, 0.8)
    c.rect(1.5, 11.5, 15.5, 15.5, (top, bot), K, 1)
    return c.image()


def main():
    fontdir, out = sys.argv[1], sys.argv[2]
    for spec in sys.argv[3:]:
        typ, kind, label, colour = spec.split(":")
        d = os.path.join(out, typ.lower())
        os.makedirs(d, exist_ok=True)
        large(kind, label, colour, fontdir, 1).save(os.path.join(d, "file.png"))
        large(kind, label, colour, fontdir, 2).save(os.path.join(d, "file11.png"))
        small(kind, colour, 1).save(os.path.join(d, "small.png"))
        small(kind, colour, 2).save(os.path.join(d, "small11.png"))


if __name__ == "__main__":
    main()
