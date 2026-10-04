# Generates the PNG assets for the RISC OS GTK3 theme, at 1x and 2x.
# Square edges are drawn pixel-exact at each scale (RISC OS sprites are crisp);
# only curves and the tick are anti-aliased.
from PIL import Image, ImageDraw
import os, sys
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gtk-3.0", "assets")
os.makedirs(OUT, exist_ok=True)
SS = 8  # supersampling for curved shapes

def well_square(px, k, fill):
    """16-unit square well, slab in: dark top/left, white bottom/right, two rings."""
    im = Image.new("RGBA", (px, px), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    def ring(i, tl, br):
        a, b = i * k, px - 1 - i * k
        d.rectangle([a, a, b, b], fill=br)             # bottom/right colour
        d.rectangle([a, a, b - k, a + k - 1], fill=tl)  # top edge
        d.rectangle([a, a, a + k - 1, b - k], fill=tl)  # left edge
    ring(0, "#555555", "#ffffff")
    ring(1, "#999999", "#dddddd")
    d.rectangle([2*k, 2*k, px - 1 - 2*k, px - 1 - 2*k], fill=fill)
    return im

def overlay(base, draw_fn):
    """Draw anti-aliased marks at SS x size, downsample, composite over base."""
    px = base.size[0]
    big = Image.new("RGBA", (px*SS, px*SS), (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(big), px*SS / 16)
    base.alpha_composite(big.resize((px, px), Image.LANCZOS))
    return base

def tick(colour):
    return lambda d, u: d.line([(3.5*u, 8.5*u), (6.5*u, 11.5*u), (12.5*u, 4.5*u)],
                               fill=colour, width=int(2.2*u), joint="curve")

def dash(colour):
    return lambda d, u: d.rectangle([4*u, 7*u, 12*u, 9*u], fill=colour)

def well_round(px, fill):
    """Round well. Rings are sized in whole pixels so they stay solid at 1x."""
    k = px / 16
    S = px * SS; c = S / 2
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    w1 = max(1.4 * k, 1.4) * SS   # outer ring width
    w2 = max(1.2 * k, 1.2) * SS   # inner ring width
    def disc(r, a0, a1, colour):
        d.pieslice([c - r, c - r, c + r, c + r], a0, a1, fill=colour)
    r0 = S / 2
    # Slab in, lit from the top left as everything on the desktop is: the
    # top-left half dark, the bottom-right half light. (PIL angles run
    # clockwise from 3 o'clock: 135-315 is the top-left half.)
    disc(r0, 315, 495, "#ffffff"); disc(r0, 135, 315, "#444444")
    disc(r0 - w1, 315, 495, "#dddddd"); disc(r0 - w1, 135, 315, "#888888")
    r2 = r0 - w1 - w2
    d.ellipse([c - r2, c - r2, c + r2, c + r2], fill=fill)
    return im.resize((px, px), Image.LANCZOS)

def dot(colour):
    return lambda d, u: d.ellipse([5*u, 5*u, 11*u, 11*u], fill=colour)

def arrow(px, direction, colour):
    S = px * SS
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(im).polygon([(S*0.2, S*0.7), (S*0.8, S*0.7), (S*0.5, S*0.28)], fill=colour)
    rot = {"up": 0, "right": 270, "down": 180, "left": 90}[direction]
    return im.rotate(rot).resize((px, px), Image.LANCZOS)

def blank(px):
    return Image.new("RGBA", (px, px), (0, 0, 0, 0))

def save(im, name):
    im.save(os.path.join(OUT, name))

for k, sfx in ((1, ""), (2, "@2")):
    px = 16 * k
    for st, fill, mark in (("", "#ffffff", "#000000"), ("-insensitive", "#dddddd", "#888888")):
        save(well_square(px, k, fill), f"check{st}{sfx}.png")
        save(overlay(well_square(px, k, fill), tick(mark)), f"check-checked{st}{sfx}.png")
        save(overlay(well_square(px, k, fill), dash(mark)), f"check-mixed{st}{sfx}.png")
        save(well_round(px, fill), f"radio{st}{sfx}.png")
        save(overlay(well_round(px, fill), dot(mark)), f"radio-checked{st}{sfx}.png")
        save(overlay(well_round(px, fill), dash(mark)), f"radio-mixed{st}{sfx}.png")
    # Wimp menu ticks: a bare tick, no box (black, white when highlighted, grey when shaded)
    for st, c in (("", "#000000"), ("-selected", "#ffffff"), ("-insensitive", "#999999")):
        save(overlay(blank(px), tick(c)), f"menu-tick{st}{sfx}.png")
        save(overlay(blank(px), dot(c)), f"menu-dot{st}{sfx}.png")
    for dirn in ("up", "down", "left", "right"):
        for st, c in (("", "#000000"), ("-insensitive", "#999999"), ("-selected", "#ffffff")):
            save(arrow(12 * k, dirn, c), f"arrow-{dirn}{st}{sfx}.png")
print(len(os.listdir(OUT)), "assets in", OUT)
