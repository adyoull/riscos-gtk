#!/usr/bin/env python3
"""mkriscosicons.py - riscos-gtk: the RISCOS icon theme's own icons.

Original icons in the style of the RISC OS desktop (tools/riscosdraw.py,
the same drawing as ClawsMail's icons) for the freedesktop.org names
programs use for toolbars, menus and dialogue boxes: document-open,
edit-copy, go-next, zoom-in, dialog-warning... Each is drawn once on a
24-unit grid and made at every size GTK asks for.

build/mkicons.py puts them into every program's RISCOS theme, in place of
Adwaita's for the same names (Adwaita's stay for the rest, and the
desktop's own sprites still come first for folders, file types, discs and
the dialogue box symbols: gtk/riscos/gtkriscostheme.c).

    mkriscosicons.py FONTDIR THEMEDIR [SHEET.png]

FONTDIR has DejaVu's TTFs (for the letters on the text-format icons).
THEMEDIR gets SIZExSIZE/CONTEXT/NAME.png for SIZES. Prints the
"SIZExSIZE/CONTEXT" directories made, one per line, after a "dirs:" line.
SHEET.png: optional, every icon at 24 and 16 pixels, enlarged.

As a module: draw_theme(fontdir, themedir) -> set of (rel, size, context).
Needs Pillow. MIT licence. Copyright (C) 2026 Andrew Youll.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from riscosdraw import (Icon, K, K2, WHITE, PAPER, PAPER2, CREAM, FLAP, FLAP2, GREY, GREY2,  # noqa: E402
                        GREY3, RED, RED2, GREEN, GREEN2, BLUE, BLUE2, LBLUE, YELLOW, YELLOW2,
                        ORANGE, ORANGE2, BROWN, BROWN2, GOLD, GOLD2, PINK, PINK2, FOLDER,
                        FOLDER2, FOLDERIN, envelope, sheet, arrow, curved_arrow, pencil,
                        padlock, clock, tick, cross, badge, triangle_warn, wastebasket, gear,
                        folder, person, printer, paperclip, book, clipboard)

SIZES = [16, 22, 24, 32, 48]
FONTS = {}
ICONS = []          # (name, context, draw)

BLUEA = (BLUE, BLUE2)
GREENA = (GREEN, GREEN2)
REDA = (RED, RED2)


def icon(name, context="actions"):
    def reg(f):
        ICONS.append((name, context, f))
        return f
    return reg


def font(name):
    return os.path.join(FONTS["dir"], name)


# ---- more shapes (24-unit grid; outlines on n + 0.5) ----

def magnifier(c, cx, cy, r, inside=None):
    """a magnifying glass: lens centred (cx, cy), handle to bottom right"""
    a = math.radians(45)
    hx0, hy0 = cx + (r + 0.5) * math.cos(a), cy + (r + 0.5) * math.sin(a)
    hx1, hy1 = hx0 + r * 0.95, hy0 + r * 0.95
    c.line([(hx0, hy0), (hx1, hy1)], K, 4.2)
    c.line([(hx0 + 0.6, hy0 + 0.6), (hx1 - 0.4, hy1 - 0.4)], BROWN, 2.0)
    c.ellipse(cx - r, cy - r, cx + r, cy + r, ((235, 245, 255, 255), LBLUE))
    c.ellipse(cx - r + 1.5, cy - r + 1.5, cx + r - 1.5, cy + r - 1.5, None, WHITE, 0.8)
    if inside == "+":
        c.line([(cx - r * 0.55, cy), (cx + r * 0.55, cy)], K, 1.6)
        c.line([(cx, cy - r * 0.55), (cx, cy + r * 0.55)], K, 1.6)
    elif inside == "-":
        c.line([(cx - r * 0.55, cy), (cx + r * 0.55, cy)], K, 1.6)
    elif inside == "1":
        c.line([(cx - 0.5, cy - r * 0.5), (cx + 0.5, cy - r * 0.6), (cx + 0.5, cy + r * 0.55)], K, 1.4)
    elif inside == "fit":
        d = r * 0.5
        c.rect(cx - d, cy - d, cx + d, cy + d, None, K, 1)


def floppy(c, x0, y0, x1, y1, col=(BLUE, BLUE2)):
    """a 3.5in disc, as RISC OS's Save icons show"""
    w, h = x1 - x0, y1 - y0
    c.poly([(x0, y0), (x1 - 2.5, y0), (x1, y0 + 2.5), (x1, y1), (x0, y1)], col)
    c.rect(x0 + w * 0.22, y0, x1 - w * 0.22, y0 + h * 0.36, (GREY, GREY2), K)
    c.rect(x1 - w * 0.22 - w * 0.17, y0 + 1.5, x1 - w * 0.22 - w * 0.06, y0 + h * 0.36 - 1.5, K2, None)
    c.rect(x0 + w * 0.14, y0 + h * 0.52, x1 - w * 0.14, y1, (WHITE, PAPER2), K)
    for i in range(2):
        yy = y0 + h * 0.66 + i * h * 0.13
        c.line([(x0 + w * 0.24, yy), (x1 - w * 0.24, yy)], GREY2, 1)


def house(c, x0, y0, x1, y1):
    w, h = x1 - x0, y1 - y0
    mx = (x0 + x1) / 2
    c.rect(x0 + w * 0.12, y0 + h * 0.42, x1 - w * 0.12, y1, (CREAM, FLAP2))
    c.poly([(x0 - 0.5, y0 + h * 0.48), (mx, y0), (x1 + 0.5, y0 + h * 0.48)], REDA)
    c.rect(mx - w * 0.12, y1 - h * 0.34, mx + w * 0.12, y1, (BROWN, BROWN2))


def monitor(c, x0, y0, x1, y1):
    w, h = x1 - x0, y1 - y0
    c.rect(x0, y0, x1, y0 + h * 0.72, (GREY, GREY3))
    c.rect(x0 + 2, y0 + 2, x1 - 2, y0 + h * 0.72 - 2, ((90, 150, 230, 255), (20, 60, 150, 255)), K)
    c.poly([(x0 + w * 0.38, y0 + h * 0.72), (x1 - w * 0.38, y0 + h * 0.72), (x1 - w * 0.3, y1 - 1.5),
            (x0 + w * 0.3, y1 - 1.5)], (GREY, GREY3))
    c.rect(x0 + w * 0.2, y1 - 2, x1 - w * 0.2, y1, (GREY, GREY3))


def disc_cd(c, cx, cy, r):
    c.ellipse(cx - r, cy - r, cx + r, cy + r, ((230, 230, 240, 255), (170, 170, 190, 255)))
    c.arc(cx, cy, r * 0.72, 200, 260, (255, 255, 255, 255), 1.2)
    c.arc(cx, cy, r * 0.72, 20, 80, (140, 170, 230, 255), 1.2)
    c.ellipse(cx - r * 0.28, cy - r * 0.28, cx + r * 0.28, cy + r * 0.28, WHITE)


def scissors(c, x0, y0, x1, y1):
    w, h = x1 - x0, y1 - y0
    cx = (x0 + x1) / 2
    # blades crossing at the pivot, rings below
    py = y0 + h * 0.55
    c.poly([(cx - 0.8, py), (x0 + w * 0.68, y0), (x0 + w * 0.76, y0 + 0.8), (cx + 1.6, py + 0.5)], (WHITE, GREY2))
    c.poly([(cx + 0.8, py), (x1 - w * 0.68, y0), (x1 - w * 0.76, y0 + 0.8), (cx - 1.6, py + 0.5)], (WHITE, GREY2))
    r = w * 0.2
    for sx in (-1, 1):
        rx = cx + sx * w * 0.24
        ry = y1 - r
        c.line([(cx + sx * 0.6, py + 0.3), (rx - sx * r * 0.2, ry - r * 0.8)], K, 2.6)
        c.line([(cx + sx * 0.6, py + 0.3), (rx - sx * r * 0.2, ry - r * 0.8)], RED, 1.0)
        c.ellipse(rx - r, ry - r, rx + r, ry + r, None, K, 3.0)
        c.ellipse(rx - r, ry - r, rx + r, ry + r, None, RED, 1.2)
    c.ellipse(cx - 1, py - 0.5, cx + 1, py + 1.5, GREY3, K, 0.6)


def plus(c, cx, cy, r, fill=GREENA):
    t = r * 0.36
    c.poly([(cx - t, cy - r), (cx + t, cy - r), (cx + t, cy - t), (cx + r, cy - t), (cx + r, cy + t),
            (cx + t, cy + t), (cx + t, cy + r), (cx - t, cy + r), (cx - t, cy + t), (cx - r, cy + t),
            (cx - r, cy - t), (cx - t, cy - t)], fill)


def minus(c, cx, cy, r, fill=REDA):
    t = r * 0.36
    c.rect(cx - r, cy - t, cx + r, cy + t, fill)


def text_lines(c, x0, y0, x1, y1, pattern, col=K, gap=3):
    """lines of text: pattern is a list of (indent, end) fractions"""
    y = y0
    for a, b in pattern:
        c.line([(x0 + (x1 - x0) * a, y), (x0 + (x1 - x0) * b, y)], col, 1.4)
        y += gap
        if y > y1:
            break


def star(c, cx, cy, r, fill=(YELLOW, ORANGE)):
    pts = []
    for i in range(10):
        a = math.radians(-90 + 36 * i)
        rr = r if i % 2 == 0 else r * 0.45
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    c.poly(pts, fill, K, 0.8)


def picture(c, x0, y0, x1, y1):
    """a framed picture: sky, a hill and the sun"""
    c.rect(x0, y0, x1, y1, ((150, 200, 255, 255), (220, 240, 255, 255)))
    w, h = x1 - x0, y1 - y0
    c.poly([(x0 + 0.5, y1 - 0.5), (x0 + w * 0.35, y0 + h * 0.45), (x0 + w * 0.6, y0 + h * 0.7),
            (x0 + w * 0.75, y0 + h * 0.55), (x1 - 0.5, y1 - 0.5)], GREENA, GREEN2, 0.8)
    c.ellipse(x1 - w * 0.32, y0 + h * 0.14, x1 - w * 0.12, y0 + h * 0.14 + w * 0.2, (YELLOW, ORANGE), ORANGE2, 0.8)
    c.rect(x0, y0, x1, y1, None, K)


def circular_arrows(c, cx, cy, r, fill=GREENA):
    """two arrows chasing round (refresh)"""
    for a0 in (200, 20):
        pts_o, pts_i = [], []
        for i in range(11):
            a = math.radians(a0 + 130 * i / 10)
            pts_o.append((cx + (r + 1.4) * math.cos(a), cy + (r + 1.4) * math.sin(a)))
            pts_i.append((cx + (r - 1.4) * math.cos(a), cy + (r - 1.4) * math.sin(a)))
        a = math.radians(a0 + 130)
        tx, ty = cx + r * math.cos(a), cy + r * math.sin(a)
        dx, dy = -math.sin(a), math.cos(a)        # the way round
        nx, ny = math.cos(a), math.sin(a)
        head = [(tx + nx * 3.2, ty + ny * 3.2), (tx + dx * 3.6, ty + dy * 3.6), (tx - nx * 3.2, ty - ny * 3.2)]
        c.poly(pts_o + [head[0], head[1], head[2]] + pts_i[::-1], fill)


def bar(c, x0, y0, x1, y1, fill=BLUEA):
    c.rect(x0, y0, x1, y1, fill)


def eraser(c, x0, y0, x1, y1):
    c.poly([(x0 + 4, y0), (x1, y0 + 6), (x1 - 4, y1), (x0, y1 - 6)], (PINK, PINK2))
    c.line([(x0 + 2, y0 + 5), (x1 - 2, y1 - 5)], K, 1)


def chain(c, x0, y0, x1, y1):
    for (ax, ay) in ((x0, y0 + (y1 - y0) * 0.45), (x0 + (x1 - x0) * 0.45, y0)):
        w = (x1 - x0) * 0.55
        h = (y1 - y0) * 0.55
        pts = []
        cx, cy = ax + w / 2, ay + h / 2
        for i in range(25):
            a = 2 * math.pi * i / 24
            pts.append((cx + w / 2 * math.cos(a) * 0.9 + (h / 2) * 0.0, cy + h / 2 * math.sin(a) * 0.55))
        # a rounded link, tilted 45 degrees
        rot = [((x - cx) * 0.707 - (y - cy) * 0.707 + cx, (x - cx) * 0.707 + (y - cy) * 0.707 + cy) for x, y in pts]
        c.line(rot, K, 3.0)
        c.line(rot, GREY, 1.2)


def media_frame(c):
    pass


# ---- dialogue boxes and status ----

icon("dialog-error", "status")(lambda c: badge(c, 12, 12, 9.5, REDA, "x"))
icon("dialog-information", "status")(lambda c: badge(c, 12, 12, 9.5, BLUEA, "i"))
icon("dialog-question", "status")(lambda c: badge(c, 12, 12, 9.5, BLUEA, "?"))
icon("dialog-warning", "status")(lambda c: triangle_warn(c, 12, 12.5, 9.5))
icon("dialog-password", "status")(lambda c: padlock(c, 5.5, 2.5, 18.5, 21.5))
icon("dialog-ok", "actions")(lambda c: tick(c, 3, 5, 21, 19))
icon("emblem-important", "emblems")(lambda c: badge(c, 12, 12, 9.5, REDA, "!"))
icon("security-high", "status")(lambda c: (padlock(c, 5.5, 2.5, 18.5, 21.5), tick(c, 13, 13, 22, 22)))
icon("printer-error", "status")(lambda c: (printer(c, 1.5, 2.5, 18.5, 18.5), badge(c, 17.5, 17.5, 5, REDA, "x")))
icon("printer-info", "status")(lambda c: (printer(c, 1.5, 2.5, 18.5, 18.5), badge(c, 17.5, 17.5, 5, BLUEA, "i")))
icon("printer-warning", "status")(lambda c: (printer(c, 1.5, 2.5, 18.5, 18.5), triangle_warn(c, 17.5, 17.5, 5)))
icon("printer-paused", "status")(lambda c: (printer(c, 1.5, 2.5, 18.5, 18.5),
                                             c.ellipse(12.5, 12.5, 22.5, 22.5, (WHITE, GREY)),
                                             bar(c, 15.5, 15.5, 16.5, 19.5, K), bar(c, 18.5, 15.5, 19.5, 19.5, K)))
icon("network-idle", "status")(lambda c: (monitor(c, 1.5, 1.5, 13.5, 12.5), monitor(c, 10.5, 10.5, 22.5, 21.5)))


# ---- documents ----

icon("document-new")(lambda c: (sheet(c, 4.5, 3.5, 17.5, 21.5), star(c, 17.5, 6.5, 5)))
icon("document-open")(lambda c: (sheet(c, 9.5, 1.5, 20.5, 14.5), folder(c, 2.5, 8.5, 20.5, 21.5, True)))
icon("document-save")(lambda c: floppy(c, 3.5, 3.5, 20.5, 20.5))
icon("document-save-as")(lambda c: (floppy(c, 2.5, 4.5, 17.5, 19.5), pencil(c, 11, 20, 22, 7)))
icon("document-revert")(lambda c: (sheet(c, 6.5, 2.5, 19.5, 21.5), curved_arrow(c, 9.5, 10.5, 5, True, BLUEA)))
icon("document-properties")(lambda c: (sheet(c, 3.5, 2.5, 17.5, 21.5), badge(c, 16, 15, 6, BLUEA, "i")))
icon("document-print")(lambda c: printer(c, 2.5, 3.5, 21.5, 21.5))
icon("document-print-preview")(lambda c: (printer(c, 1.5, 2.5, 17.5, 17.5), magnifier(c, 15.5, 14.5, 4.5)))
icon("document-send")(lambda c: (sheet(c, 2.5, 2.5, 14.5, 21.5), arrow(c, 11.5, 8.5, 22.5, 18.5, "right", BLUEA)))
icon("document-export")(lambda c: (sheet(c, 2.5, 2.5, 14.5, 21.5), arrow(c, 11.5, 8.5, 22.5, 18.5, "right", GREENA)))
icon("document-import")(lambda c: (sheet(c, 9.5, 2.5, 21.5, 21.5), arrow(c, 1.5, 8.5, 12.5, 18.5, "right", GREENA)))
icon("x-office-document", "mimetypes")(lambda c: (sheet(c, 4.5, 2.5, 19.5, 21.5), c.rect(6.5, 7.5, 11.5, 11.5, LBLUE, K2)))
icon("x-office-spreadsheet", "mimetypes")(lambda c: (
    sheet(c, 4.5, 2.5, 19.5, 21.5, False),
    [c.line([(6.5, y), (17.5, y)], GREY2, 1) for y in (8.5, 11.5, 14.5, 17.5)],
    [c.line([(x, 8.5), (x, 19.5)], GREY2, 1) for x in (10.5, 14.5)],
    c.rect(6.5, 8.5, 17.5, 11.5, (GREENA[0][:3] + (120,), GREENA[1][:3] + (120,)), None)))
icon("x-office-presentation", "mimetypes")(lambda c: (
    c.rect(2.5, 3.5, 21.5, 16.5, (WHITE, PAPER2)), c.line([(12, 16.5), (12, 21.5)], K, 1.4),
    c.line([(8, 21.5), (16, 21.5)], K, 1.4),
    c.poly([(5.5, 13.5), (5.5, 9.5), (8.5, 9.5), (8.5, 13.5)], REDA, K, 0.8),
    c.poly([(10.5, 13.5), (10.5, 6.5), (13.5, 6.5), (13.5, 13.5)], BLUEA, K, 0.8),
    c.poly([(15.5, 13.5), (15.5, 8.5), (18.5, 8.5), (18.5, 13.5)], GREENA, K, 0.8)))


# ---- editing ----

icon("edit-copy")(lambda c: (sheet(c, 2.5, 2.5, 14.5, 17.5), sheet(c, 9.5, 6.5, 21.5, 21.5)))
icon("edit-cut")(lambda c: scissors(c, 3.5, 1.5, 20.5, 22.5))
icon("edit-paste")(lambda c: (clipboard(c, 3.5, 1.5, 17.5, 21.5), sheet(c, 11.5, 8.5, 21.5, 22.5)))
icon("edit-delete")(lambda c: cross(c, 5, 5, 19, 19))
icon("edit-clear")(lambda c: (sheet(c, 2.5, 2.5, 15.5, 21.5), eraser(c, 10.5, 9.5, 22.5, 21.5)))
icon("edit-undo")(lambda c: curved_arrow(c, 13, 12.5, 6.5, True, BLUEA, 4))
icon("edit-redo")(lambda c: curved_arrow(c, 11, 12.5, 6.5, False, BLUEA, 4))
icon("edit-find")(lambda c: magnifier(c, 9.5, 9.5, 6.5))
icon("system-search")(lambda c: magnifier(c, 9.5, 9.5, 6.5))
icon("edit-find-replace")(lambda c: (magnifier(c, 8.5, 8.5, 5.5), pencil(c, 12, 21, 22, 10)))
icon("edit-select-all")(lambda c: (
    text_lines(c, 6.5, 7.5, 17.5, 17.5, [(0, 1), (0, 0.8), (0, 1), (0, 0.6)], K2),
    [c.line([(x, 2.5), (x + 2, 2.5)], K, 1) for x in (2.5, 6.5, 10.5, 14.5, 18.5)],
    [c.line([(x, 21.5), (x + 2, 21.5)], K, 1) for x in (2.5, 6.5, 10.5, 14.5, 18.5)],
    [c.line([(2.5, y), (2.5, y + 2)], K, 1) for y in (4.5, 8.5, 12.5, 16.5)],
    [c.line([(21.5, y), (21.5, y + 2)], K, 1) for y in (4.5, 8.5, 12.5, 16.5)]))
icon("tools-check-spelling")(lambda c: (c.rect(2.5, 4.5, 21.5, 13.5, WHITE, K2),
                                         c.text(4, 4.6, "abc", 8.5, font("DejaVuSans.ttf")),
                                         tick(c, 8, 12, 19, 21)))
icon("insert-text")(lambda c: (c.rect(2.5, 4.5, 21.5, 19.5, WHITE, K2),
                                c.text(6, 4, "A", 14, font("DejaVuSerif-Bold.ttf")),
                                c.line([(17.5, 7.5), (17.5, 16.5)], K, 1)))
icon("insert-image")(lambda c: picture(c, 2.5, 4.5, 21.5, 19.5))
icon("insert-link")(lambda c: chain(c, 2.5, 2.5, 21.5, 21.5))
icon("insert-object")(lambda c: (c.poly([(12, 2.5), (21.5, 7.5), (12, 12.5), (2.5, 7.5)], (WHITE, GREY)),
                                  c.poly([(2.5, 7.5), (12, 12.5), (12, 21.5), (2.5, 16.5)], (GREY, GREY2)),
                                  c.poly([(21.5, 7.5), (12, 12.5), (12, 21.5), (21.5, 16.5)], (GREY2, GREY3))))
icon("list-add")(lambda c: plus(c, 12, 12, 8.5))
icon("list-remove")(lambda c: minus(c, 12, 12, 8.5))
icon("mark-location")(lambda c: (c.poly([(12, 21.5)] + [(12 + 6.5 * math.cos(math.radians(a)),
                                                           9 + 6.5 * math.sin(math.radians(a)))
                                                          for a in range(140, 401, 20)], REDA),
                                  c.ellipse(9.5, 6.5, 14.5, 11.5, WHITE)))
icon("bookmark-new")(lambda c: (c.poly([(5.5, 2.5), (16.5, 2.5), (16.5, 21.5), (11, 16.5), (5.5, 21.5)], REDA),
                                 star(c, 17.5, 16.5, 5)))
icon("appointment-new")(lambda c: (clock(c, 11, 12, 9), star(c, 18, 18, 4.5)))


# ---- text formatting ----

def fmt_letter(s, fontfile, underline=False, strike=False):
    def f(c):
        c.text(6.5, 0.5, s, 18, font(fontfile))
        if underline:
            c.line([(5.5, 21.5), (18.5, 21.5)], K, 2)
        if strike:
            c.line([(4.5, 12.5), (19.5, 12.5)], RED, 2)
    return f


icon("format-text-bold")(fmt_letter("B", "DejaVuSans-Bold.ttf"))
icon("format-text-italic")(fmt_letter("I", "DejaVuSerif-Italic.ttf"))
icon("format-text-underline")(fmt_letter("U", "DejaVuSans.ttf", underline=True))
icon("format-text-strikethrough")(fmt_letter("S", "DejaVuSans.ttf", strike=True))
icon("format-justify-left")(lambda c: text_lines(c, 3.5, 4.5, 20.5, 20.5,
                                                 [(0, 1), (0, 0.6), (0, 1), (0, 0.6), (0, 1)], gap=4))
icon("format-justify-right")(lambda c: text_lines(c, 3.5, 4.5, 20.5, 20.5,
                                                  [(0, 1), (0.4, 1), (0, 1), (0.4, 1), (0, 1)], gap=4))
icon("format-justify-center")(lambda c: text_lines(c, 3.5, 4.5, 20.5, 20.5,
                                                   [(0, 1), (0.2, 0.8), (0, 1), (0.2, 0.8), (0, 1)], gap=4))
icon("format-justify-fill")(lambda c: text_lines(c, 3.5, 4.5, 20.5, 20.5, [(0, 1)] * 5, gap=4))
icon("format-indent-more")(lambda c: (text_lines(c, 3.5, 4.5, 20.5, 20.5,
                                                  [(0, 1), (0.45, 1), (0.45, 1), (0.45, 1), (0, 1)], gap=4),
                                       arrow(c, 2.5, 8.5, 8.5, 15.5, "right", BLUEA)))
icon("format-indent-less")(lambda c: (text_lines(c, 3.5, 4.5, 20.5, 20.5,
                                                  [(0, 1), (0.45, 1), (0.45, 1), (0.45, 1), (0, 1)], gap=4),
                                       arrow(c, 2.5, 8.5, 8.5, 15.5, "left", BLUEA)))


# ---- going places ----

icon("go-up")(lambda c: arrow(c, 4.5, 3.5, 19.5, 20.5, "up", BLUEA))
icon("go-down")(lambda c: arrow(c, 4.5, 3.5, 19.5, 20.5, "down", BLUEA))
icon("go-previous")(lambda c: arrow(c, 3.5, 4.5, 20.5, 19.5, "left", BLUEA))
icon("go-next")(lambda c: arrow(c, 3.5, 4.5, 20.5, 19.5, "right", BLUEA))
icon("go-top")(lambda c: (bar(c, 3.5, 2.5, 20.5, 5.5), arrow(c, 5.5, 7.5, 18.5, 21.5, "up", BLUEA)))
icon("go-bottom")(lambda c: (bar(c, 3.5, 18.5, 20.5, 21.5), arrow(c, 5.5, 2.5, 18.5, 16.5, "down", BLUEA)))
icon("go-first")(lambda c: (bar(c, 2.5, 3.5, 5.5, 20.5), arrow(c, 7.5, 5.5, 21.5, 18.5, "left", BLUEA)))
icon("go-last")(lambda c: (bar(c, 18.5, 3.5, 21.5, 20.5), arrow(c, 2.5, 5.5, 16.5, 18.5, "right", BLUEA)))
icon("go-jump")(lambda c: curved_arrow(c, 9, 13.5, 6, False, BLUEA, 4))
icon("go-home")(lambda c: house(c, 3.5, 3.5, 20.5, 21.5))
icon("user-home", "places")(lambda c: house(c, 3.5, 3.5, 20.5, 21.5))
icon("view-refresh")(lambda c: circular_arrows(c, 12, 12, 7))
icon("process-stop")(lambda c: (c.poly([(12 + 9.5 * math.cos(math.radians(22.5 + 45 * i)),
                                          12 + 9.5 * math.sin(math.radians(22.5 + 45 * i))) for i in range(8)], REDA),
                                 c.line([(8, 8), (16, 16)], WHITE, 2.2), c.line([(16, 8), (8, 16)], WHITE, 2.2)))
icon("application-exit")(lambda c: (c.rect(2.5, 2.5, 12.5, 21.5, (BROWN, BROWN2)),
                                     c.ellipse(9.5, 11, 11.5, 13, GOLD, None),
                                     arrow(c, 10.5, 7.5, 22.5, 16.5, "right", REDA)))
icon("window-close")(lambda c: (c.rect(3.5, 3.5, 20.5, 20.5, (WHITE, GREY)), cross(c, 8, 8, 16, 16, GREY3)))
icon("system-run")(lambda c: (gear(c, 10.5, 12.5, 8), arrow(c, 14.5, 12.5, 22.5, 20.5, "right", GREENA)))
icon("preferences-system")(lambda c: gear(c, 12, 12, 9.5))
icon("applications-system")(lambda c: (gear(c, 9, 9, 6.5), gear(c, 16.5, 16.5, 5.5, (CREAM, FLAP2))))
icon("preferences-desktop")(lambda c: (monitor(c, 1.5, 2.5, 18.5, 18.5), gear(c, 18, 18, 4.5)))
icon("help-about")(lambda c: badge(c, 12, 12, 9.5, BLUEA, "i"))
icon("help-contents")(lambda c: (book(c, 4.5, 2.5, 19.5, 21.5, BLUEA),
                                  badge(c, 12.5, 11, 5, (WHITE, LBLUE), "?") if False else
                                  c.text(8.5, 3.5, "?", 13, font("DejaVuSans-Bold.ttf"), WHITE)))
icon("image-missing", "status")(lambda c: (picture(c, 2.5, 4.5, 19.5, 18.5), badge(c, 17.5, 17.5, 5, REDA, "x")))
def corners(c, x0, y0, x1, y1, n, col=BLUE2):
    for (x, y, sx, sy) in ((x0, y0, 1, 1), (x1, y0, -1, 1), (x0, y1, 1, -1), (x1, y1, -1, -1)):
        c.line([(x, y + sy * n), (x, y), (x + sx * n, y)], col, 2)


icon("view-fullscreen")(lambda c: (c.rect(6.5, 7.5, 17.5, 16.5, (WHITE, GREY)), corners(c, 2.5, 2.5, 21.5, 21.5, 5)))
icon("view-restore")(lambda c: (c.rect(3.5, 3.5, 20.5, 20.5, (WHITE, GREY)),
                                 c.rect(7.5, 7.5, 16.5, 16.5, LBLUE, BLUE2)))
icon("view-sort-ascending")(lambda c: ([bar(c, 2.5, y, 2.5 + w, y + 2, K) for y, w in
                                        ((4.5, 5), (9.5, 8), (14.5, 11), (19.5, 14))],
                                       arrow(c, 16.5, 3.5, 22.5, 16.5, "down", BLUEA)))
icon("view-sort-descending")(lambda c: ([bar(c, 2.5, y, 2.5 + w, y + 2, K) for y, w in
                                         ((4.5, 14), (9.5, 11), (14.5, 8), (19.5, 5))],
                                        arrow(c, 16.5, 3.5, 22.5, 16.5, "down", BLUEA)))
icon("view-list")(lambda c: [(c.rect(2.5, y, 5.5, y + 3, LBLUE, K), c.line([(8, y + 1.5), (21.5, y + 1.5)], K, 1.4))
                             for y in (3.5, 10.5, 17.5)])
icon("view-grid")(lambda c: [c.rect(x, y, x + 7, y + 7, (WHITE, LBLUE)) for x in (3.5, 13.5) for y in (3.5, 13.5)])
icon("view-more")(lambda c: [c.ellipse(x - 2, 10, x + 2, 14, K, None) for x in (5, 12, 19)])
icon("zoom-in")(lambda c: magnifier(c, 9.5, 9.5, 7, "+"))
icon("zoom-out")(lambda c: magnifier(c, 9.5, 9.5, 7, "-"))
icon("zoom-original")(lambda c: magnifier(c, 9.5, 9.5, 7, "1"))
icon("zoom-fit-best")(lambda c: magnifier(c, 9.5, 9.5, 7, "fit"))
icon("object-flip-horizontal")(lambda c: (c.poly([(10.5, 4.5), (10.5, 19.5), (2.5, 19.5)], BLUEA),
                                           c.poly([(13.5, 4.5), (13.5, 19.5), (21.5, 19.5)], (WHITE, GREY)),
                                           c.line([(12, 2), (12, 22)], K2, 1)))
icon("object-flip-vertical")(lambda c: (c.poly([(4.5, 10.5), (19.5, 10.5), (19.5, 2.5)], BLUEA),
                                         c.poly([(4.5, 13.5), (19.5, 13.5), (19.5, 21.5)], (WHITE, GREY)),
                                         c.line([(2, 12), (22, 12)], K2, 1)))
icon("object-rotate-left")(lambda c: (c.rect(9.5, 9.5, 21.5, 21.5, (WHITE, GREY)),
                                       curved_arrow(c, 8.5, 9.5, 5, True, BLUEA)))
icon("object-rotate-right")(lambda c: (c.rect(2.5, 9.5, 14.5, 21.5, (WHITE, GREY)),
                                        curved_arrow(c, 15.5, 9.5, 5, False, BLUEA)))


# ---- media ----

def media_bg(c):
    c.ellipse(1.5, 1.5, 22.5, 22.5, ((250, 250, 250, 255), GREY))


icon("media-playback-start")(lambda c: (media_bg(c), c.poly([(9, 6.5), (18, 12), (9, 17.5)], GREENA)))
icon("media-playback-pause")(lambda c: (media_bg(c), bar(c, 7.5, 6.5, 10.5, 17.5, K), bar(c, 13.5, 6.5, 16.5, 17.5, K)))
icon("media-playback-stop")(lambda c: (media_bg(c), c.rect(7.5, 7.5, 16.5, 16.5, K)))
icon("media-record")(lambda c: (media_bg(c), c.ellipse(7, 7, 17, 17, REDA)))
icon("media-seek-forward")(lambda c: (media_bg(c), c.poly([(5.5, 7.5), (12, 12), (5.5, 16.5)], K),
                                       c.poly([(12, 7.5), (18.5, 12), (12, 16.5)], K)))
icon("media-seek-backward")(lambda c: (media_bg(c), c.poly([(18.5, 7.5), (12, 12), (18.5, 16.5)], K),
                                        c.poly([(12, 7.5), (5.5, 12), (12, 16.5)], K)))
icon("media-skip-forward")(lambda c: (media_bg(c), c.poly([(7, 7.5), (14.5, 12), (7, 16.5)], K),
                                       bar(c, 15.5, 7.5, 17.5, 16.5, K)))
icon("media-skip-backward")(lambda c: (media_bg(c), c.poly([(17, 7.5), (9.5, 12), (17, 16.5)], K),
                                        bar(c, 6.5, 7.5, 8.5, 16.5, K)))
icon("media-optical", "devices")(lambda c: disc_cd(c, 12, 12, 9.5))


# ---- places, devices and mail (the desktop's sprites come first) ----

icon("computer", "devices")(lambda c: monitor(c, 1.5, 2.5, 22.5, 21.5))
icon("user-desktop", "places")(lambda c: monitor(c, 1.5, 2.5, 22.5, 21.5))
icon("user-trash", "places")(lambda c: wastebasket(c, 4.5, 2.5, 19.5, 21.5))
icon("network-workgroup", "places")(lambda c: (monitor(c, 1.5, 1.5, 12.5, 11.5), monitor(c, 11.5, 11.5, 22.5, 21.5),
                                               c.line([(7, 13.5), (7, 17), (11, 17)], K, 1.2)))
icon("printer", "devices")(lambda c: printer(c, 2.5, 3.5, 21.5, 21.5))
icon("mail-send")(lambda c: (envelope(c, 2.5, 8.5, 19.5, 20.5), arrow(c, 11.5, 2.5, 22.5, 12.5, "right", BLUEA)))
icon("mail-attachment")(lambda c: paperclip(c, 7.5, 1.5, 16.5, 22.5))
icon("folder-documents", "places")(lambda c: (folder(c, 1.5, 4.5, 22.5, 20.5), sheet(c, 13.5, 11.5, 21.5, 22.5, True, 3)))
icon("folder-download", "places")(lambda c: (folder(c, 1.5, 4.5, 22.5, 20.5), arrow(c, 13.5, 9.5, 22.5, 22.5, "down", GREENA)))
icon("folder-pictures", "places")(lambda c: (folder(c, 1.5, 4.5, 22.5, 20.5), picture(c, 12.5, 12.5, 22.5, 21.5)))
icon("folder-music", "places")(lambda c: (folder(c, 1.5, 4.5, 22.5, 20.5),
                                          c.ellipse(13.5, 17.5, 17.5, 21.5, K, None),
                                          c.line([(17, 19.5), (17, 11.5), (21, 10.5)], K, 1.4)))
icon("folder-videos", "places")(lambda c: (folder(c, 1.5, 4.5, 22.5, 20.5),
                                           c.rect(12.5, 12.5, 22.5, 21.5, K2, K),
                                           c.poly([(16, 14.5), (20, 17), (16, 19.5)], WHITE, None)))


def render(f, size):
    k = size / 24
    c = Icon(24, 24, k, minline=True)
    f(c)
    return c.image()


def draw_theme(fontdir, themedir, sizes=SIZES):
    FONTS["dir"] = fontdir
    dirs = set()
    for name, ctx, f in ICONS:
        for size in sizes:
            rel = "%dx%d/%s" % (size, size, ctx)
            os.makedirs(os.path.join(themedir, rel), exist_ok=True)
            render(f, size).save(os.path.join(themedir, rel, name + ".png"))
            dirs.add((rel, size, ctx))
    return dirs


def main():
    fontdir, themedir = sys.argv[1], sys.argv[2]
    dirs = draw_theme(fontdir, themedir)
    if len(sys.argv) > 3:
        from PIL import Image
        cols, z = 10, 3
        cell = 24 * z + 16 * z + 12
        rows = (len(ICONS) + cols - 1) // cols
        im = Image.new("RGBA", (cols * cell, rows * (24 * z + 8)), (221, 221, 221, 255))
        for i, (name, _ctx, f) in enumerate(ICONS):
            x, y = (i % cols) * cell + 4, (i // cols) * (24 * z + 8) + 4
            a = render(f, 24).resize((24 * z, 24 * z), Image.NEAREST)
            b = render(f, 16).resize((16 * z, 16 * z), Image.NEAREST)
            im.alpha_composite(a, (x, y))
            im.alpha_composite(b, (x + 24 * z + 4, y))
        im.save(sys.argv[3])
    print("mkriscosicons: %d icons at %s pixels" % (len(ICONS), ", ".join(map(str, SIZES))))
    print("dirs:")
    for rel, _s, _c in sorted(dirs):
        print(rel)


if __name__ == "__main__":
    main()
