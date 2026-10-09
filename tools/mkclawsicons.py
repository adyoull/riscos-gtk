#!/usr/bin/env python3
"""mkclawsicons.py - riscos-gtk: ClawsMail's RISC OS icons, drawn here.

    mkclawsicons.py THEMEDIR SPRITEDIR [SHEET.png]

Original icons in the style of RISC OS 5's desktop (dark outlines, flat
colours lit from the top left, the Wimp's palette), drawn as vectors at 8x
and reduced, with outlines on pixel centres so they stay sharp:

  THEMEDIR   Claws Mail's icon theme (riscos-claws.c makes it the default):
             the toolbar icons (24 x 24), the message list's marks (12 x 12,
             15 x 12), the folder list's icons (16 x 16; on RISC OS these are
             made from the desktop's own small_dir / small_diro sprites at
             run time, with the emblem_*.png drawn here on top; the folder
             PNGs are used where the desktop has no such sprite).
  SPRITEDIR  !clawsmail.png (34 x 34) and sm!clawsmail.png (17 x 17), for
             tools/png2sprite.py.
  SHEET.png  optional: every icon enlarged, to look at.

Needs Pillow. MIT licence. Copyright (C) 2026 Andrew Youll.
"""
import math
import os
import sys
from PIL import Image, ImageDraw

S = 8  # supersampling

# the palette: RISC OS's Wimp colours and the tones of its 32bpp icons
K = (32, 32, 32, 255)          # outline
K2 = (96, 96, 96, 255)         # light outline / detail
WHITE = (255, 255, 255, 255)
PAPER = (255, 255, 250, 255)
PAPER2 = (226, 222, 204, 255)
CREAM = (255, 255, 204, 255)
FLAP = (236, 232, 212, 255)
FLAP2 = (204, 198, 172, 255)
GREY = (204, 204, 204, 255)
GREY2 = (153, 153, 153, 255)
GREY3 = (110, 110, 110, 255)
RED = (230, 34, 34, 255)
RED2 = (170, 0, 0, 255)
GREEN = (60, 200, 60, 255)
GREEN2 = (0, 136, 0, 255)
BLUE = (64, 136, 238, 255)
BLUE2 = (0, 68, 170, 255)
LBLUE = (170, 210, 255, 255)
YELLOW = (255, 221, 0, 255)
YELLOW2 = (204, 153, 0, 255)
ORANGE = (255, 153, 0, 255)
ORANGE2 = (204, 85, 0, 255)
BROWN = (180, 120, 60, 255)
BROWN2 = (120, 70, 20, 255)
GOLD = (255, 204, 51, 255)
GOLD2 = (190, 130, 0, 255)
PINK = (255, 150, 170, 255)
PINK2 = (200, 60, 90, 255)
FOLDER = (255, 214, 102, 255)
FOLDER2 = (214, 154, 30, 255)
FOLDERIN = (255, 236, 170, 255)


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(4))


class Icon:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.im = Image.new("RGBA", (w * S, h * S), (0, 0, 0, 0))

    def _p(self, pts):
        return [(x * S, y * S) for x, y in pts]

    def _layer(self, mask, fill):
        """fill (a colour or (top, bottom) gradient) through mask"""
        if isinstance(fill[0], tuple):
            top, bot = fill
            box = mask.getbbox()
            if not box:
                return
            y0, y1 = box[1], box[3]
            g = Image.new("RGBA", self.im.size)
            d = ImageDraw.Draw(g)
            for y in range(y0, y1):
                d.line([(0, y), (self.im.size[0], y)], fill=lerp(top, bot, (y - y0) / max(1, y1 - y0 - 1)))
        else:
            g = Image.new("RGBA", self.im.size, fill)
        a = Image.new("L", self.im.size, 0)
        a.paste(g.getchannel("A"), mask=mask)
        g.putalpha(a)
        self.im.alpha_composite(g)

    def poly(self, pts, fill=None, outline=K, ow=1.0, closed=True):
        if fill:
            m = Image.new("L", self.im.size, 0)
            ImageDraw.Draw(m).polygon(self._p(pts), fill=255)
            self._layer(m, fill)
        if outline:
            q = self._p(pts + ([pts[0]] if closed else []))
            l = Image.new("RGBA", self.im.size)
            ImageDraw.Draw(l).line(q, fill=outline, width=int(ow * S), joint="curve")
            for x, y in q:
                r = ow * S / 2
                ImageDraw.Draw(l).ellipse([x - r, y - r, x + r, y + r], fill=outline)
            self.im.alpha_composite(l)

    def line(self, pts, col=K, w=1.0):
        self.poly(pts, None, col, w, closed=False)

    def rect(self, x0, y0, x1, y1, fill=None, outline=K, ow=1.0):
        self.poly([(x0, y0), (x1, y0), (x1, y1), (x0, y1)], fill, outline, ow)

    def ellipse(self, x0, y0, x1, y1, fill=None, outline=K, ow=1.0):
        n = 48
        cx, cy, rx, ry = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) / 2, (y1 - y0) / 2
        pts = [(cx + rx * math.cos(2 * math.pi * i / n), cy + ry * math.sin(2 * math.pi * i / n)) for i in range(n)]
        self.poly(pts, fill, outline, ow)

    def arc(self, cx, cy, r, a0, a1, col=K, w=1.0, n=24):
        pts = [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
                cy + r * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]
        self.line(pts, col, w)

    def image(self):
        return self.im.reduce(S)


# ---- shapes, in pixel coordinates (outlines on pixel centres: n + 0.5) ----

def envelope(c, x0, y0, x1, y1, open_=False):
    """an envelope, back flap closed (a V) or open (a letter showing)"""
    mx = (x0 + x1) / 2
    if open_:
        c.poly([(x0, y0 + 4), (mx, y0 - 2), (x1, y0 + 4)], (FLAP, FLAP2))
        c.rect(x0 + 2, y0 - 1, x1 - 2, y1 - 3, PAPER, K2)
        for i in range(3):
            c.line([(x0 + 4, y0 + 1 + 2 * i), (x1 - 4, y0 + 1 + 2 * i)], GREY2, 1)
        c.poly([(x0, y0 + 4), (mx, y0 + (y1 - y0) * 0.65), (x1, y0 + 4), (x1, y1), (x0, y1)], (PAPER, PAPER2))
        c.line([(x0, y1), (mx - 1, y0 + (y1 - y0) * 0.55)], FLAP2, 1)
        c.line([(x1, y1), (mx + 1, y0 + (y1 - y0) * 0.55)], FLAP2, 1)
    else:
        c.rect(x0, y0, x1, y1, (PAPER, PAPER2))
        c.poly([(x0, y0), (mx, y0 + (y1 - y0) * 0.6), (x1, y0)], (FLAP, FLAP2))
        c.line([(x0, y1), (mx - 2, y0 + (y1 - y0) * 0.5)], FLAP2, 1)
        c.line([(x1, y1), (mx + 2, y0 + (y1 - y0) * 0.5)], FLAP2, 1)


def sheet(c, x0, y0, x1, y1, lines=True, fold=4):
    c.poly([(x0, y0), (x1 - fold, y0), (x1, y0 + fold), (x1, y1), (x0, y1)], (WHITE, PAPER2))
    c.poly([(x1 - fold, y0), (x1 - fold, y0 + fold), (x1, y0 + fold)], GREY, K)
    if lines:
        y = y0 + fold + 2
        while y < y1 - 1:
            c.line([(x0 + 2, y), (x1 - 2 - (3 if int(y) % 4 == 1 else 0), y)], GREY2, 1)
            y += 2


def arrow(c, x0, y0, x1, y1, direction, fill):
    """a fat block arrow in the box; direction 'up', 'down', 'left', 'right'"""
    w, h = x1 - x0, y1 - y0
    if direction in ("down", "up"):
        sh = w * 0.25
        hy = y0 + h * 0.5
        pts = [(x0 + sh, y0), (x1 - sh, y0), (x1 - sh, hy), (x1, hy), ((x0 + x1) / 2, y1), (x0, hy), (x0 + sh, hy)]
        if direction == "up":
            pts = [(x, y0 + y1 - y) for x, y in pts]
    else:
        sh = h * 0.25
        hx = x0 + w * 0.5
        pts = [(x0, y0 + sh), (hx, y0 + sh), (hx, y0), (x1, (y0 + y1) / 2), (hx, y1), (hx, y1 - sh), (x0, y1 - sh)]
        if direction == "left":
            pts = [(x0 + x1 - x, y) for x, y in pts]
    c.poly(pts, fill)


def curved_arrow(c, cx, cy, r, left, fill, thick=3.2):
    """a curling arrow (reply: points left, forward: right) round (cx, cy)"""
    n = 14
    s = -1 if left else 1
    outer, inner = [], []
    for i in range(n + 1):
        a = math.radians(180 + 180 * i / n) if not left else math.radians(-180 * i / n)
        outer.append((cx + (r + thick / 2) * math.cos(a), cy + (r + thick / 2) * math.sin(a)))
        inner.append((cx + (r - thick / 2) * math.cos(a), cy + (r - thick / 2) * math.sin(a)))
    # the shaft from one end round, then the head at the other end (pointing down)
    end = outer[-1], inner[-1]
    ex = (end[0][0] + end[1][0]) / 2
    ey = end[0][1]
    head = [(ex - s * 0 + 3.2, ey), (ex, ey + 3.6), (ex - 3.2, ey)]
    pts = outer + [head[0] if not left else head[2]] + [head[1]] + [head[2] if not left else head[0]] + inner[::-1]
    c.poly(pts, fill)


def pencil(c, x0, y0, x1, y1, body=(YELLOW, YELLOW2)):
    """a pencil from its tip (x0, y0) to the end (x1, y1), 3 px thick"""
    dx, dy = x1 - x0, y1 - y0
    L = math.hypot(dx, dy)
    ux, uy = dx / L, dy / L
    nx, ny = -uy * 1.7, ux * 1.7
    t = 4.0
    bx, by = x0 + ux * t, y0 + uy * t
    c.poly([(bx + nx, by + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (bx - nx, by - ny)], body)
    c.poly([(x0, y0), (bx + nx, by + ny), (bx - nx, by - ny)], (PAPER, FLAP2))
    c.poly([(x0, y0), (x0 + ux * 1.6 + nx * 0.4, y0 + uy * 1.6 + ny * 0.4),
            (x0 + ux * 1.6 - nx * 0.4, y0 + uy * 1.6 - ny * 0.4)], K, K, 0.6)
    ex, ey = x1 - ux * 2, y1 - uy * 2
    c.poly([(ex + nx, ey + ny), (x1 + nx, y1 + ny), (x1 - nx, y1 - ny), (ex - nx, ey - ny)], PINK, K)


def padlock(c, x0, y0, x1, y1, closed=True):
    w = x1 - x0
    by = y0 + (y1 - y0) * 0.45
    cx = (x0 + x1) / 2
    r = w * 0.28
    sx = cx if closed else cx + w * 0.42
    sy = by if closed else by - 2.5
    pts = [(sx - r, by)] + [(sx + r * math.cos(math.radians(a)), sy - r * 0.2 + r * math.sin(math.radians(a)))
                             for a in range(180, 361, 15)] + [(sx + r, by if closed else sy + 1)]
    pts[0] = (sx - r, by + 0.5)
    c.line(pts, K, 3.4)
    c.line(pts, (200, 200, 200, 255), 1.4)
    c.rect(x0, by, x1, y1, (GOLD, GOLD2))
    c.ellipse(cx - 1.3, by + 1.8, cx + 1.3, by + 4.4, K, None)
    c.line([(cx, by + 4), (cx, y1 - 1.6)], K, 1)


def clock(c, cx, cy, r):
    c.ellipse(cx - r, cy - r, cx + r, cy + r, (WHITE, GREY))
    c.line([(cx, cy), (cx, cy - r * 0.65)], K, 1)
    c.line([(cx, cy), (cx + r * 0.55, cy + r * 0.2)], K, 1)


def tick(c, x0, y0, x1, y1, col=GREEN):
    w, h = x1 - x0, y1 - y0
    pts = [(x0, y0 + h * 0.55), (x0 + w * 0.38, y1), (x1, y0)]
    c.line(pts, K, 3.4)
    c.line(pts, col, 1.6)


def cross(c, x0, y0, x1, y1, col=RED):
    for p in ([(x0, y0), (x1, y1)], [(x1, y0), (x0, y1)]):
        c.line(p, K, 3.4)
    for p in ([(x0, y0), (x1, y1)], [(x1, y0), (x0, y1)]):
        c.line(p, col, 1.6)


def badge(c, cx, cy, r, fill, symbol):
    """a round badge with a white symbol: '!' 'i' 'x' '?'"""
    c.ellipse(cx - r, cy - r, cx + r, cy + r, fill)
    if symbol == "!":
        c.line([(cx, cy - r * 0.55), (cx, cy + r * 0.15)], WHITE, 2)
        c.ellipse(cx - 1, cy + r * 0.35, cx + 1, cy + r * 0.35 + 2, WHITE, None)
    elif symbol == "i":
        c.ellipse(cx - 1, cy - r * 0.6, cx + 1, cy - r * 0.6 + 2, WHITE, None)
        c.line([(cx, cy - r * 0.15), (cx, cy + r * 0.55)], WHITE, 2)
    elif symbol == "x":
        d = r * 0.42
        c.line([(cx - d, cy - d), (cx + d, cy + d)], WHITE, 2)
        c.line([(cx + d, cy - d), (cx - d, cy + d)], WHITE, 2)
    elif symbol == "?":
        c.arc(cx, cy - r * 0.25, r * 0.32, 200, 400, WHITE, 1.8)
        c.line([(cx + r * 0.1, cy), (cx, cy + r * 0.25)], WHITE, 1.8)
        c.ellipse(cx - 1, cy + r * 0.45, cx + 1, cy + r * 0.45 + 2, WHITE, None)


def triangle_warn(c, cx, cy, r):
    c.poly([(cx, cy - r), (cx + r * 1.05, cy + r * 0.8), (cx - r * 1.05, cy + r * 0.8)], (YELLOW, ORANGE))
    c.line([(cx, cy - r * 0.4), (cx, cy + r * 0.2)], K, 2)
    c.ellipse(cx - 1, cy + r * 0.38, cx + 1, cy + r * 0.38 + 2, K, None)


def seal(c, cx, cy, r, fill=(RED, RED2)):
    n = 16
    pts = []
    for i in range(n * 2):
        a = math.pi * i / n
        rr = r if i % 2 == 0 else r * 0.82
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    c.poly([(cx - r * 0.5, cy + r * 0.5), (cx - r * 0.8, cy + r * 1.5), (cx - r * 0.2, cy + r * 1.2),
            (cx, cy + r * 1.6), (cx + r * 0.2, cy + r * 0.6)], (BLUE, BLUE2))
    c.poly(pts, fill)
    c.ellipse(cx - r * 0.5, cy - r * 0.5, cx + r * 0.5, cy + r * 0.5, None, (255, 220, 220, 255), 1)


def wastebasket(c, x0, y0, x1, y1):
    """RISC OS's wastebasket: a tapered grey bin with ribs"""
    w = x1 - x0
    c.poly([(x0, y0 + 2), (x1, y0 + 2), (x1 - w * 0.15, y1), (x0 + w * 0.15, y1)], (WHITE, GREY2))
    c.ellipse(x0, y0, x1, y0 + 4, (GREY, GREY2))
    for i in range(1, 4):
        x = x0 + w * i / 4
        c.line([(x, y0 + 4.5), (x0 + w * 0.15 + (w * 0.7) * i / 4, y1 - 1)], GREY3, 1)


def gear(c, cx, cy, r, fill=(GREY, GREY3)):
    n = 8
    pts = []
    for i in range(n * 4):
        a = 2 * math.pi * i / (n * 4)
        rr = r if (i % 4) in (0, 1) else r * 0.72
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    c.poly(pts, fill)
    c.ellipse(cx - r * 0.3, cy - r * 0.3, cx + r * 0.3, cy + r * 0.3, WHITE)


def folder(c, x0, y0, x1, y1, open_=False):
    """a desktop directory, as the Filer draws it (fallback for small_dir)"""
    tab = (x1 - x0) * 0.4
    c.poly([(x0, y0 + 1), (x0 + 1, y0), (x0 + tab, y0), (x0 + tab + 2, y0 + 2), (x1, y0 + 2), (x1, y1), (x0, y1)],
           (FOLDER, FOLDER2))
    if open_:
        c.poly([(x0 + 2, y0 + 5), (x1 + 1, y0 + 5), (x1 - 1, y1), (x0, y1)], (FOLDERIN, FOLDER))
    else:
        c.line([(x0 + 1, y0 + 4), (x1 - 1, y0 + 4)], FOLDERIN, 1)


def person(c, cx, cy, s):
    c.ellipse(cx - s * 0.35, cy - s, cx + s * 0.35, cy - s * 0.3, (CREAM, (240, 200, 150, 255)))
    c.poly([(cx - s * 0.65, cy + s * 0.5), (cx - s * 0.5, cy - s * 0.15), (cx + s * 0.5, cy - s * 0.15),
            (cx + s * 0.65, cy + s * 0.5)], (BLUE, BLUE2))


def printer(c, x0, y0, x1, y1):
    w = x1 - x0
    c.rect(x0 + w * 0.22, y0, x1 - w * 0.22, y0 + 6, WHITE)
    c.poly([(x0, y0 + 5), (x1, y0 + 5), (x1, y1 - 3), (x0, y1 - 3)], (GREY, GREY3))
    c.rect(x0 + w * 0.18, y1 - 6, x1 - w * 0.18, y1, (WHITE, PAPER2))
    c.line([(x0 + w * 0.3, y1 - 3.5), (x1 - w * 0.3, y1 - 3.5)], GREY2, 1)
    c.ellipse(x1 - 4, y0 + 7, x1 - 2, y0 + 9, GREEN, None)


def paperclip(c, x0, y0, x1, y1, col=GREY3):
    w = x1 - x0
    r = w / 2
    cx = x0 + r
    pts = []
    # a three-loop clip, drawn as one stroked path
    pts += [(cx + r * 0.55, y1 - r * 1.2), (cx + r * 0.55, y0 + r)]
    for i in range(13):
        a = math.radians(i * 15)
        pts.append((cx + r * math.cos(a), y0 + r - r * math.sin(a)))
    pts += [(cx - r, y1 - r)]
    for i in range(13):
        a = math.radians(180 + i * 15)
        pts.append((cx + r * 0.8 * math.cos(a), y1 - r - r * 0.8 * math.sin(a)))
    pts += [(cx + r * 0.8, y0 + r * 1.6)]
    c.line(pts, K, 2.6)
    c.line(pts, col, 1.0)


def book(c, x0, y0, x1, y1, fill=(RED, RED2)):
    c.rect(x0, y0, x1, y1, fill)
    c.rect(x0 + 2, y1 - 3, x1, y1, PAPER, K)
    c.line([(x0 + 2, y0), (x0 + 2, y1 - 3)], K, 1)


def thread_lines(c, x0, y0, x1, y1):
    """a thread: a message and two replies under it, joined by a tree line"""
    h = (y1 - y0) / 3
    for i, ind in enumerate((0, 4, 4)):
        yy = y0 + i * h
        c.rect(x0 + ind, yy, x1, yy + h - 2, (PAPER, PAPER2))
        c.line([(x0 + ind + 2, yy + (h - 2) / 2), (x1 - 2, yy + (h - 2) / 2)], GREY2, 1)
    c.line([(x0 + 2, y0 + h - 2), (x0 + 2, y0 + 2.5 * h - 1), (x0 + 4, y0 + 2.5 * h - 1)], K, 1)
    c.line([(x0 + 2, y0 + 1.5 * h - 1), (x0 + 4, y0 + 1.5 * h - 1)], K, 1)


def eye(c, cx, cy, r):
    c.poly([(cx - r, cy)] + [(cx + r * math.cos(math.radians(a)), cy - r * 0.6 * math.sin(math.radians(a)))
                              for a in range(180, -1, -15)] +
           [(cx + r * math.cos(math.radians(a)), cy + r * 0.6 * math.sin(math.radians(a))) for a in range(15, 180, 15)],
           WHITE)
    c.ellipse(cx - r * 0.45, cy - r * 0.45, cx + r * 0.45, cy + r * 0.45, (BLUE, BLUE2), None)
    c.ellipse(cx - r * 0.18, cy - r * 0.18, cx + r * 0.18, cy + r * 0.18, K, None)


def no_sign(c, cx, cy, r):
    c.ellipse(cx - r, cy - r, cx + r, cy + r, None, K, 3.2)
    c.ellipse(cx - r, cy - r, cx + r, cy + r, None, RED, 1.6)
    d = r * 0.7
    c.line([(cx - d, cy + d), (cx + d, cy - d)], K, 3.2)
    c.line([(cx - d, cy + d), (cx + d, cy - d)], RED, 1.6)


def clipboard(c, x0, y0, x1, y1):
    c.rect(x0, y0 + 1, x1, y1, (BROWN, BROWN2))
    c.rect(x0 + 2, y0 + 3, x1 - 2, y1 - 2, (WHITE, PAPER2), K2)
    w = x1 - x0
    c.rect(x0 + w * 0.3, y0, x1 - w * 0.3, y0 + 3, (GREY, GREY3))


# ---- the toolbar (24 x 24) ----

def tb(name, f):
    TOOLBAR.append((name, f))


TOOLBAR = []


def env24(c, open_=False):
    envelope(c, 2.5, 8.5, 19.5, 20.5, open_)


tb("mail_receive", lambda c: (env24(c), arrow(c, 12.5, 1.5, 22.5, 13.5, "down", (GREEN, GREEN2))))
tb("mail_receive_all", lambda c: (envelope(c, 5.5, 5.5, 20.5, 15.5), envelope(c, 1.5, 10.5, 16.5, 20.5),
                                   arrow(c, 13.5, 1.5, 22.5, 12.5, "down", (GREEN, GREEN2))))
tb("mail_send", lambda c: (env24(c), arrow(c, 11.5, 2.5, 22.5, 12.5, "right", (BLUE, BLUE2))))
tb("mail_send_queue", lambda c: (envelope(c, 4.5, 6.5, 19.5, 16.5), envelope(c, 1.5, 10.5, 16.5, 20.5),
                                  arrow(c, 12.5, 1.5, 22.5, 10.5, "right", (BLUE, BLUE2)), clock(c, 18, 18, 4.5)))
tb("mail_compose", lambda c: (sheet(c, 3.5, 2.5, 16.5, 21.5), pencil(c, 9, 18, 21, 4)))
tb("news_compose", lambda c: (sheet(c, 3.5, 2.5, 16.5, 21.5, False),
                               c.rect(5.5, 6.5, 10.5, 10.5, LBLUE, K2),
                               [c.line([(5.5, y), (14.5, y)], GREY2, 1) for y in (12.5, 14.5, 16.5, 18.5)],
                               pencil(c, 10, 18, 21, 5)))
tb("mail_reply", lambda c: (envelope(c, 4.5, 9.5, 21.5, 20.5), curved_arrow(c, 9.5, 9.5, 5, True, (ORANGE, ORANGE2))))
tb("mail_reply_to_all", lambda c: (envelope(c, 4.5, 9.5, 21.5, 20.5),
                                    curved_arrow(c, 12.5, 8.5, 5, True, (ORANGE, ORANGE2)),
                                    curved_arrow(c, 7.5, 9.5, 4.5, True, (ORANGE, ORANGE2))))
tb("mail_reply_to_author", lambda c: (envelope(c, 4.5, 11.5, 19.5, 21.5),
                                       curved_arrow(c, 8.5, 9.5, 4.5, True, (ORANGE, ORANGE2)),
                                       person(c, 18.5, 9.5, 6)))
tb("mail_reply_to_list", lambda c: (envelope(c, 4.5, 11.5, 19.5, 21.5),
                                     curved_arrow(c, 8.5, 9.5, 4.5, True, (ORANGE, ORANGE2)),
                                     c.rect(15.5, 1.5, 22.5, 10.5, WHITE),
                                     [c.line([(17, y), (21, y)], K2, 1) for y in (3.5, 5.5, 7.5)]))
tb("mail_forward", lambda c: (envelope(c, 2.5, 9.5, 19.5, 20.5), curved_arrow(c, 14.5, 9.5, 5, False, (GREEN, GREEN2))))
tb("open_mail", lambda c: envelope(c, 2.5, 9.5, 21.5, 21.5, True))
tb("mail_draft", lambda c: (sheet(c, 4.5, 2.5, 17.5, 21.5),
                             c.rect(6.5, 15.5, 15.5, 19.5, None, RED, 1), pencil(c, 11, 17, 21, 6)))
tb("mail_attach", lambda c: (sheet(c, 3.5, 3.5, 15.5, 21.5), paperclip(c, 13.5, 1.5, 20.5, 19.5)))
tb("insert_file", lambda c: (sheet(c, 6.5, 2.5, 19.5, 21.5), arrow(c, 1.5, 9.5, 12.5, 18.5, "right", (GREEN, GREEN2))))
tb("mail_sign", lambda c: (sheet(c, 3.5, 2.5, 17.5, 21.5, False),
                            c.line([(5.5, 17.5), (7.5, 14.5), (9.5, 17.5), (11.5, 14.5), (13.5, 17.5)], BLUE2, 1.2),
                            pencil(c, 13, 17, 22, 7, (BLUE, BLUE2))))
tb("mail_privacy_encrypted", lambda c: (env24(c), padlock(c, 13.5, 2.5, 21.5, 14.5)))
tb("mail_privacy_signed", lambda c: (env24(c), seal(c, 17.5, 7.5, 5)))
tb("trash_btn", lambda c: wastebasket(c, 4.5, 2.5, 19.5, 21.5))
tb("delete_btn", lambda c: (sheet(c, 4.5, 2.5, 17.5, 21.5), cross(c, 10, 9, 20, 19)))
tb("delete_dup_btn", lambda c: (sheet(c, 7.5, 1.5, 19.5, 16.5), sheet(c, 3.5, 5.5, 15.5, 21.5), cross(c, 11, 11, 20, 20)))
tb("spam_btn", lambda c: (env24(c), no_sign(c, 16.5, 7.5, 5.5)))
tb("ham_btn", lambda c: (env24(c), tick(c, 13, 2, 22, 11)))
tb("printer_btn", lambda c: printer(c, 2.5, 3.5, 21.5, 21.5))
tb("exec", lambda c: (gear(c, 10.5, 12.5, 8), arrow(c, 14.5, 12.5, 22.5, 20.5, "right", (GREEN, GREEN2))))
tb("go_folders", lambda c: (folder(c, 2.5, 8.5, 18.5, 20.5), arrow(c, 13.5, 1.5, 22.5, 11.5, "left", (BLUE, BLUE2))))
tb("address_book", lambda c: (book(c, 4.5, 2.5, 19.5, 21.5, (BLUE, BLUE2)), person(c, 12.5, 11.5, 6)))
tb("cancel", lambda c: badge(c, 12, 12, 9.5, (RED, RED2), "x"))
tb("close", lambda c: (c.rect(3.5, 3.5, 20.5, 20.5, (WHITE, GREY)), cross(c, 8, 8, 16, 16, GREY3)))
tb("preferences", lambda c: (c.rect(2.5, 3.5, 21.5, 20.5, (WHITE, GREY)),
                              [(c.line([(5.5, y), (18.5, y)], K, 1), c.rect(x - 1.5, y - 2.5, x + 1.5, y + 2.5, (BLUE, BLUE2)))
                               for x, y in ((14.5, 7.5), (8.5, 12.5), (12.5, 17.5))]))
tb("properties", lambda c: (sheet(c, 3.5, 2.5, 17.5, 21.5), badge(c, 16, 15, 6, (BLUE, BLUE2), "i")))
tb("check_spelling", lambda c: (c.rect(2.5, 4.5, 21.5, 13.5, WHITE, K2),
                                 [c.line([(4.5 + 4 * i, 8), (4.5 + 4 * i + 2, 10.5)], K, 1.4) for i in range(4)],
                                 tick(c, 8, 12, 19, 21)))
tb("edit_extern", lambda c: (sheet(c, 2.5, 2.5, 15.5, 21.5), pencil(c, 7, 17, 15, 8),
                              arrow(c, 13.5, 11.5, 22.5, 20.5, "right", (BLUE, BLUE2))))
tb("linewrap", lambda c: (c.rect(2.5, 2.5, 21.5, 21.5, WHITE, K2),
                           [c.line([(5.5, y), (18.5, y)], K, 1.4) for y in (6.5, 10.5)],
                           c.line([(5.5, 14.5), (11.5, 14.5)], K, 1.4),
                           c.line([(18.5, 6.5), (19.5, 6.5), (19.5, 17.5), (14.5, 17.5)], BLUE2, 1.6),
                           c.poly([(14.5, 14.5), (11.5, 17.5), (14.5, 20.5)], BLUE, BLUE2, 1)))
tb("linewrapcurrent", lambda c: (c.rect(2.5, 2.5, 21.5, 21.5, WHITE, K2),
                                  c.rect(3.5, 5.5, 20.5, 11.5, LBLUE, None),
                                  c.line([(5.5, 8.5), (18.5, 8.5)], K, 1.4),
                                  c.line([(5.5, 15.5), (11.5, 15.5)], GREY2, 1.4),
                                  c.line([(18.5, 8.5), (19.5, 8.5), (19.5, 18.5), (14.5, 18.5)], BLUE2, 1.6),
                                  c.poly([(14.5, 15.5), (11.5, 18.5), (14.5, 21.5)], BLUE, BLUE2, 1)))
tb("paste", lambda c: (clipboard(c, 3.5, 1.5, 17.5, 21.5), sheet(c, 11.5, 8.5, 21.5, 22.5)))
tb("up_arrow", lambda c: arrow(c, 4.5, 3.5, 19.5, 20.5, "up", (BLUE, BLUE2)))
tb("down_arrow", lambda c: arrow(c, 4.5, 3.5, 19.5, 20.5, "down", (BLUE, BLUE2)))
tb("mark_allread", lambda c: (envelope(c, 5.5, 4.5, 21.5, 14.5, True), envelope(c, 2.5, 9.5, 18.5, 20.5, True),
                               tick(c, 14, 13, 22, 21)))
tb("mark_allunread", lambda c: (envelope(c, 5.5, 4.5, 21.5, 14.5), envelope(c, 2.5, 9.5, 18.5, 20.5),
                                 c.ellipse(16.5, 14.5, 22.5, 20.5, (GREEN, GREEN2))))
tb("mark_read", lambda c: (envelope(c, 2.5, 8.5, 21.5, 20.5, True)))
tb("mark_unread", lambda c: (env24(c), c.ellipse(15.5, 2.5, 22.5, 9.5, (GREEN, GREEN2))))
tb("mark_mark", lambda c: (env24(c), tick(c, 12, 2, 22, 12, RED)))
tb("mark_unmark", lambda c: (env24(c), tick(c, 12, 2, 22, 12, (255, 255, 255, 255)), badge(c, 18.5, 17.5, 4.5, (RED, RED2), "x")))
tb("mark_locked", lambda c: (env24(c), padlock(c, 13.5, 1.5, 21.5, 13.5)))
tb("mark_unlocked", lambda c: (env24(c), padlock(c, 12.5, 2.5, 20.5, 14.5, False)))
tb("mark_ignorethread", lambda c: (thread_lines(c, 1.5, 2.5, 17.5, 22.5), no_sign(c, 17, 16, 5.5)))
tb("mark_watchthread", lambda c: (thread_lines(c, 1.5, 2.5, 17.5, 22.5), eye(c, 16.5, 17, 6.5)))
tb("notice_error", lambda c: badge(c, 12, 12, 9.5, (RED, RED2), "x"))
tb("notice_warn", lambda c: triangle_warn(c, 12, 12, 9))
tb("notice_note", lambda c: badge(c, 12, 12, 9.5, (BLUE, BLUE2), "i"))
tb("privacy_signed", lambda c: seal(c, 12, 10, 7))
tb("privacy_passed", lambda c: (seal(c, 10, 9, 6.5), tick(c, 13, 12, 22, 21)))
tb("privacy_failed", lambda c: (seal(c, 10, 9, 6.5), cross(c, 14, 13, 21, 20)))
tb("privacy_unknown", lambda c: (seal(c, 10, 9, 6.5, (GREY, GREY3)), badge(c, 17, 17, 5, (BLUE, BLUE2), "?")))
tb("privacy_warn", lambda c: (seal(c, 10, 9, 6.5), triangle_warn(c, 17, 17, 5)))
tb("privacy_expired", lambda c: (seal(c, 10, 9, 6.5, (GREY, GREY3)), clock(c, 17, 17, 5)))


# ---- the message list's marks ----

MARKS = []


def mk(name, w, h, f):
    MARKS.append((name, w, h, f))


def env15(c, open_=False, x0=0.5):
    envelope(c, x0 + 1, 2.5, x0 + 13, 10.5, open_)


def small_env(c, x0, y0, x1, y1, open_=False):
    mx = (x0 + x1) / 2
    if open_:
        c.poly([(x0, y0 + 2), (mx, y0 - 1), (x1, y0 + 2)], FLAP, K)
        c.rect(x0, y0 + 2, x1, y1, (PAPER, PAPER2))
        c.line([(x0, y0 + 2), (mx, y0 + (y1 - y0) * 0.65), (x1, y0 + 2)], K2, 1)
    else:
        c.rect(x0, y0, x1, y1, (PAPER, PAPER2))
        c.line([(x0, y0), (mx, y0 + (y1 - y0) * 0.6), (x1, y0)], K, 1)


mk("new", 15, 12, lambda c: (small_env(c, 1.5, 3.5, 12.5, 10.5), c.ellipse(9.5, 0.5, 14.5, 5.5, (RED, RED2))))
mk("unread", 15, 12, lambda c: (small_env(c, 1.5, 3.5, 12.5, 10.5), c.ellipse(9.5, 0.5, 14.5, 5.5, (GREEN, GREEN2))))
mk("read", 15, 12, lambda c: small_env(c, 1.5, 3.5, 12.5, 10.5, True))
mk("replied", 12, 12, lambda c: curved_arrow(c, 6.5, 6.5, 3, True, (ORANGE, ORANGE2), 2.4))
mk("forwarded", 12, 12, lambda c: curved_arrow(c, 5.5, 6.5, 3, False, (GREEN, GREEN2), 2.4))
mk("replied_and_forwarded", 12, 12, lambda c: (curved_arrow(c, 4.5, 5.5, 2.5, True, (ORANGE, ORANGE2), 2.0),
                                                curved_arrow(c, 7.5, 6.5, 2.5, False, (GREEN, GREEN2), 2.0)))
mk("mark", 12, 12, lambda c: tick(c, 1.5, 2, 10.5, 10, RED))
mk("deleted", 12, 12, lambda c: cross(c, 2.5, 2.5, 9.5, 9.5))
mk("moved", 12, 12, lambda c: arrow(c, 1.5, 2.5, 10.5, 9.5, "right", (BLUE, BLUE2)))
mk("copied", 12, 12, lambda c: (c.rect(1.5, 1.5, 7.5, 7.5, WHITE), c.rect(4.5, 4.5, 10.5, 10.5, (GREEN, GREEN2))))
mk("clip", 12, 12, lambda c: paperclip(c, 3.5, 0.5, 8.5, 11.5))
mk("locked", 12, 12, lambda c: padlock(c, 2.5, 1.5, 9.5, 10.5))
mk("ignorethread", 12, 12, lambda c: no_sign(c, 6, 6, 4))
mk("watchthread", 12, 12, lambda c: eye(c, 6, 6, 5))
mk("spam", 12, 12, lambda c: c.ellipse(1.5, 1.5, 10.5, 10.5, (PINK, PINK2)))
mk("key", 12, 12, lambda c: (c.ellipse(0.5, 3.5, 5.5, 8.5, (GOLD, GOLD2)),
                             c.line([(5.5, 6), (11, 6), (11, 8.5)], K, 2.4), c.line([(5.5, 6), (11, 6)], GOLD, 1),
                             c.line([(8.5, 6), (8.5, 8.5)], K, 1.4)))
mk("gpg_signed", 12, 12, lambda c: seal(c, 6, 4.5, 3.6))
mk("clip_gpg_signed", 12, 12, lambda c: (paperclip(c, 1.5, 0.5, 6.5, 11.5), seal(c, 8, 4, 3)))
mk("key_gpg_signed", 12, 12, lambda c: (c.ellipse(0.5, 5.5, 4.5, 9.5, (GOLD, GOLD2)),
                                        c.line([(4.5, 7.5), (8.5, 7.5)], K, 2.0), seal(c, 8.5, 3.5, 3)))
mk("clipkey", 12, 12, lambda c: (paperclip(c, 0.5, 0.5, 5.5, 11.5), c.ellipse(5.5, 4.5, 9.5, 8.5, (GOLD, GOLD2)),
                                 c.line([(9.5, 6.5), (11.5, 6.5)], K, 2.0)))
mk("quicksearch", 12, 12, lambda c: (c.ellipse(1.5, 1.5, 7.5, 7.5, LBLUE), c.line([(7, 7), (10.5, 10.5)], K, 2.2)))
mk("selection", 12, 12, lambda c: c.rect(1.5, 1.5, 10.5, 10.5, None, K, 1))


# ---- the folder list: emblems on the desktop's directory sprite ----

EMBLEMS = [  # name, (w, h) 9 x 9 drawn at the bottom right of the folder
    ("inbox", lambda c: arrow(c, 1.5, 0.5, 7.5, 8.5, "down", (GREEN, GREEN2))),
    ("outbox", lambda c: arrow(c, 0.5, 1.5, 8.5, 7.5, "right", (BLUE, BLUE2))),
    ("queue", lambda c: clock(c, 4.5, 4.5, 3.6)),
    ("drafts", lambda c: pencil(c, 1, 8, 8, 1)),
    ("mark", lambda c: tick(c, 1, 1.5, 8, 7.5, RED)),
    ("hrm", lambda c: (c.ellipse(0.5, 0.5, 8.5, 8.5, WHITE), c.line([(2.5, 4.5), (6.5, 4.5)], K, 1.4))),
    ("subs", lambda c: tick(c, 1, 1.5, 8, 7.5, GREEN)),
]

# folder icon name -> (open, emblems, trash)
FOLDERS = {}
for kind in ("dir", "inbox", "outbox", "queue", "trash", "drafts"):
    for state in ("close", "open"):
        for hrm in ("", "_hrm"):
            for mark in ("", "_mark"):
                if kind == "drafts" and hrm:
                    continue
                n = "%s_%s%s%s" % (kind, state, hrm, mark)
                em = ([kind] if kind not in ("dir", "trash") else []) + (["hrm"] if hrm else []) + \
                     (["mark"] if mark else [])
                FOLDERS[n] = (state == "open", em, kind == "trash")
FOLDERS["dir_noselect_close"] = (False, [], False)
FOLDERS["dir_noselect_open"] = (True, [], False)
FOLDERS["dir_noselect_close_mark"] = (False, ["mark"], False)
FOLDERS["dir_subs_close"] = (False, ["subs"], False)
FOLDERS["dir_subs_open"] = (True, ["subs"], False)
FOLDERS["dir_subs_close_mark"] = (False, ["subs", "mark"], False)


def emblem_image(name):
    c = Icon(9, 9)
    dict(EMBLEMS)[name](c)
    return c.image()


def folder_image(name):
    open_, ems, trash = FOLDERS[name]
    c = Icon(16, 16)
    if trash:
        wastebasket(c, 2.5, 1.5, 13.5, 14.5)
    else:
        folder(c, 0.5, 3.5, 14.5, 13.5, open_)
    im = c.image()
    im = place_emblems(im, ems)
    if "noselect" in name:
        im = Image.blend(im, Image.new("RGBA", im.size, (220, 220, 220, 0)), 0.45)
    return im


def place_emblems(im, ems):
    """the kind's emblem bottom right; hrm and mark top left / bottom left"""
    pos = {"inbox": (7, 7), "outbox": (7, 7), "queue": (7, 7), "drafts": (7, 7), "subs": (7, 7),
           "hrm": (0, 7), "mark": (7, 0)}
    for e in ems:
        im.alpha_composite(emblem_image(e), pos[e])
    return im


# ---- the application's icon ----

def taper(c, pts, wmax, fill, outline=K):
    """a stroke pointed at both ends, widest in the middle"""
    n = 24
    # resample the polyline
    segs = [math.hypot(pts[i + 1][0] - pts[i][0], pts[i + 1][1] - pts[i][1]) for i in range(len(pts) - 1)]
    L = sum(segs)
    samples = []
    for k in range(n + 1):
        d = L * k / n
        i = 0
        while i < len(segs) - 1 and d > segs[i]:
            d -= segs[i]
            i += 1
        t = d / segs[i]
        samples.append((pts[i][0] + (pts[i + 1][0] - pts[i][0]) * t, pts[i][1] + (pts[i + 1][1] - pts[i][1]) * t))
    left, right = [], []
    for k, (x, y) in enumerate(samples):
        a = samples[max(k - 1, 0)]
        b = samples[min(k + 1, n)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        l = math.hypot(dx, dy) or 1
        w = wmax / 2 * math.sin(math.pi * k / n) ** 0.7
        left.append((x - dy / l * w, y + dx / l * w))
        right.append((x + dy / l * w, y - dx / l * w))
    c.poly(left + right[::-1], fill, outline, 1)


def app_icon(size):
    """a letter in its envelope, scored by Claws' claw: three green
    scratches across its front"""
    if size == 34:
        c = Icon(34, 34)
        c.poly([(3.5, 14.5), (16.5, 4.5), (29.5, 14.5)], (FLAP, FLAP2))
        c.rect(7.5, 7.5, 25.5, 22.5, PAPER, K2)
        for y in (10.5, 12.5, 14.5, 16.5):
            c.line([(10, y), (23, y)], GREY2, 1)
        c.poly([(3.5, 14.5), (16.5, 24.5), (29.5, 14.5), (29.5, 30.5), (3.5, 30.5)], (PAPER, PAPER2))
        c.line([(3.5, 30.5), (13.5, 22.5)], FLAP2, 1)
        c.line([(29.5, 30.5), (19.5, 22.5)], FLAP2, 1)
        for i in range(3):
            x = 18 + i * 5
            taper(c, [(x + 3, 12 + i), (x - 6, 31 - i * 0.4)], 3.4, ((150, 235, 130, 255), GREEN2))
    else:
        c = Icon(17, 17)
        c.poly([(1.5, 7.5), (8.5, 2.5), (15.5, 7.5)], (FLAP, FLAP2))
        c.rect(3.5, 4.5, 13.5, 11.5, PAPER, K2)
        c.poly([(1.5, 7.5), (8.5, 12.5), (15.5, 7.5), (15.5, 15.5), (1.5, 15.5)], (PAPER, PAPER2))
        for i in range(3):
            x = 10 + i * 2.5
            c.line([(x + 1.5, 8 + i * 0.5), (x - 2.5, 14.5)], (0, 150, 0, 255), 1.3)
    return c.image()


def render(f, w, h):
    c = Icon(w, h)
    f(c)
    return c.image()


def main():
    theme, sprites = sys.argv[1], sys.argv[2]
    os.makedirs(theme, exist_ok=True)
    os.makedirs(sprites, exist_ok=True)
    out = []
    for name, f in TOOLBAR:
        im = render(f, 24, 24)
        im.save(os.path.join(theme, name + ".png"))
        out.append((name, im))
    for name, w, h, f in MARKS:
        im = render(f, w, h)
        im.save(os.path.join(theme, name + ".png"))
        out.append((name, im))
    for name, _ in EMBLEMS:
        im = emblem_image(name)
        im.save(os.path.join(theme, "emblem_" + name + ".png"))
        out.append(("emblem_" + name, im))
    for name in sorted(FOLDERS):
        im = folder_image(name)
        im.save(os.path.join(theme, name + ".png"))
        out.append((name, im))
    a = app_icon(34)
    a.save(os.path.join(sprites, "!clawsmail.png"))
    s = app_icon(17)
    s.save(os.path.join(sprites, "sm!clawsmail.png"))
    out += [("!clawsmail", a), ("sm!clawsmail", s)]
    if len(sys.argv) > 3:
        z, cols = 4, 10
        cell = 34 * z + 8
        rows = (len(out) + cols - 1) // cols
        sheet_im = Image.new("RGBA", (cols * cell, rows * cell), (221, 221, 221, 255))
        for i, (n, im) in enumerate(out):
            big = im.resize((im.width * z, im.height * z), Image.NEAREST)
            sheet_im.alpha_composite(big, ((i % cols) * cell + 4, (i // cols) * cell + 4))
        sheet_im.save(sys.argv[3])
    print("mkclawsicons: %d toolbar, %d marks, %d emblems, %d folders, app icon" %
          (len(TOOLBAR), len(MARKS), len(EMBLEMS), len(FOLDERS)))


if __name__ == "__main__":
    main()
