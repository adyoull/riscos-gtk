#!/usr/bin/env python3
"""mkl3afpadsprites.py - riscos-gtk: !L3afpad's 180 dpi icons (!Sprites11).

    mkl3afpadsprites.py L3AFPAD_SRC OUTDIR

From L3afpad's own drawing, data/l3afpad_original.svg (GPL 2, as
L3afpad): the sheet's small copy of the leaf is left out, the leaf drawn
large with rsvg-convert, cropped to it, centred in a square and reduced
to 68 x 68 (!l3afpad11.png) and 34 x 34 (sm!l3afpad11.png), the 180 dpi
icon sizes (PRMs, Icon Sets), for tools/png2sprite.py --dpi 180.
Needs rsvg-convert and Pillow. MIT licence. Copyright (C) 2026 Andrew Youll.
"""
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from PIL import Image

SVG = "{http://www.w3.org/2000/svg}"


def main():
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    tree = ET.parse(os.path.join(src, "data", "l3afpad_original.svg"))
    # the layer holds two groups: the leaf, and a small copy of it (the
    # one scaled by about 0.2)
    for layer in tree.getroot().iter(SVG + "g"):
        for g in list(layer):
            if g.tag == SVG + "g" and any("matrix(0.19" in (c.get("transform") or "") for c in g.iter()):
                layer.remove(g)
    with tempfile.TemporaryDirectory() as tmp:
        svg = os.path.join(tmp, "leaf.svg")
        png = os.path.join(tmp, "leaf.png")
        tree.write(svg)
        subprocess.run(["rsvg-convert", "-w", "1200", "-h", "800", svg, "-o", png], check=True)
        im = Image.open(png).convert("RGBA")
    im = im.crop(im.getbbox())
    side = max(im.size) * 106 // 100
    sq = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    sq.alpha_composite(im, ((side - im.width) // 2, (side - im.height) // 2))
    sq.resize((68, 68), Image.LANCZOS).save(os.path.join(out, "!l3afpad11.png"))
    sq.resize((34, 34), Image.LANCZOS).save(os.path.join(out, "sm!l3afpad11.png"))


if __name__ == "__main__":
    main()
