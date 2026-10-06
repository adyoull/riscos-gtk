#!/usr/bin/env python3
"""png2sprite.py - PNGs into a RISC OS sprite file (riscos-gtk).

    png2sprite.py OUT,ff9 NAME=FILE.png [NAME=FILE.png ...]

Each sprite is 32bpp (&BBGGRR words) at 90 x 90 dpi (a new-format mode
word, RISC OS 3.5+; 32bpp sprites need RISC OS 3.5 or later) with a
1bpp mask from the PNG's alpha (opaque where alpha > 127). Names are
lower-cased, at most 12 characters. MIT licence.
"""
import struct
import sys
from PIL import Image


def sprite(name, path):
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    mwords = (w + 31) // 32
    img = bytearray()
    mask = bytearray()
    px = im.load()
    for y in range(h):
        mrow = bytearray(mwords * 4)
        for x in range(w):
            r, g, b, a = px[x, y]
            img += struct.pack("<I", r | (g << 8) | (b << 16))
            if a > 127:
                mrow[x >> 3] |= 1 << (x & 7)
        mask += mrow
    name = name.lower()[:12].encode("latin-1")
    hdr_size = 44
    size = hdr_size + len(img) + len(mask)
    mode = (6 << 27) | (90 << 14) | (90 << 1) | 1
    hdr = struct.pack("<i12siiiiiii", size, name.ljust(12, b"\0"), w - 1, h - 1, 0, 31,
                      hdr_size, hdr_size + len(img), mode)
    return hdr + bytes(img) + bytes(mask)


def main():
    out = sys.argv[1]
    sprites = [sprite(*a.split("=", 1)) for a in sys.argv[2:]]
    body = b"".join(sprites)
    # a sprite file is a sprite area without its first word
    area = struct.pack("<iii", len(sprites), 16, 16 + len(body))
    with open(out, "wb") as f:
        f.write(area + body)


if __name__ == "__main__":
    main()
