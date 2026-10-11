#!/usr/bin/env python3
"""mkpng.py OUT.png - a 64 x 48 red PNG for the tests (no libraries). MIT."""
import struct
import sys
import zlib


def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)


raw = b"".join(b"\0" + b"\xc8\x28\x28" * 64 for _ in range(48))
with open(sys.argv[1], "wb") as f:
    f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 64, 48, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
