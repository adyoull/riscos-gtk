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
    mkriscosicons.py --pack NAME FONTDIR THEMEDIR [SHEET.png]
      a program's own icons (tools/riscosicons_apps.py: gnumeric, geany)
      into its RISCOS theme

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



# ---- fallbacks for the desktop's sprites (folders, file types, discs) ----
# The desktop's own sprites (directory, file_fff, harddisc...) come first
# (gtk/riscos/gtkriscostheme.c); these are for a desktop without them.

def harddisc(c, x0, y0, x1, y1):
    c.rect(x0, y0, x1, y1, (GREY, GREY3))
    c.rect(x0 + 2, y0 + 2, x1 - 2, y0 + (y1 - y0) * 0.5, (WHITE, GREY), K2)
    c.line([(x0 + 3, y1 - 3), (x1 - 8, y1 - 3)], K2, 1)
    c.ellipse(x1 - 5.5, y1 - 5, x1 - 2.5, y1 - 2, GREENA, None)


def file_icon(c, mark=None):
    """a RISC OS file icon: a sheet with its corner folded; mark draws on it"""
    sheet(c, 4.5, 2.5, 19.5, 21.5, mark is None)
    if mark:
        mark(c)


icon("folder", "places")(lambda c: folder(c, 1.5, 4.5, 22.5, 20.5))
icon("folder-open", "places")(lambda c: folder(c, 1.5, 4.5, 22.5, 20.5, True))
icon("drive-harddisk", "devices")(lambda c: harddisc(c, 1.5, 6.5, 22.5, 17.5))
icon("drive-removable-media", "devices")(lambda c: floppy(c, 3.5, 3.5, 20.5, 20.5, (GREY2, GREY3)))
icon("media-floppy", "devices")(lambda c: floppy(c, 3.5, 3.5, 20.5, 20.5, (GREY2, GREY3)))
icon("drive-optical", "devices")(lambda c: (harddisc(c, 1.5, 11.5, 22.5, 21.5), disc_cd(c, 12, 8, 6.5)))
icon("text-x-generic", "mimetypes")(lambda c: file_icon(c))
icon("text-html", "mimetypes")(lambda c: file_icon(c, lambda c: (
    c.ellipse(6.5, 7.5, 17.5, 18.5, ((120, 180, 255, 255), BLUE2)),
    c.line([(12, 7.5), (12, 18.5)], WHITE, 0.8), c.line([(6.5, 13), (17.5, 13)], WHITE, 0.8),
    c.ellipse(9.5, 7.5, 14.5, 18.5, None, WHITE, 0.8))))
icon("image-x-generic", "mimetypes")(lambda c: file_icon(c, lambda c: picture(c, 6.5, 9.5, 17.5, 18.5)))
icon("audio-x-generic", "mimetypes")(lambda c: file_icon(c, lambda c: (
    c.ellipse(7, 14.5, 11, 18.5, K, None), c.ellipse(12.5, 13, 16.5, 17, K, None),
    c.line([(10.5, 16.5), (10.5, 8.5), (16, 7), (16, 15)], K, 1.4))))
icon("video-x-generic", "mimetypes")(lambda c: file_icon(c, lambda c: (
    c.rect(6.5, 8.5, 17.5, 18.5, K2, K),
    [c.rect(7.2, y, 8.4, y + 1, WHITE, None) for y in (9.5, 12, 14.5, 17)],
    [c.rect(15.6, y, 16.8, y + 1, WHITE, None) for y in (9.5, 12, 14.5, 17)],
    c.poly([(10.5, 11), (14, 13.5), (10.5, 16)], WHITE, None))))
icon("package-x-generic", "mimetypes")(lambda c: (
    c.poly([(2.5, 8.5), (12, 4.5), (21.5, 8.5), (12, 12.5)], (CREAM, FLAP)),
    c.poly([(2.5, 8.5), (12, 12.5), (12, 21.5), (2.5, 17.5)], (BROWN, BROWN2)),
    c.poly([(21.5, 8.5), (12, 12.5), (12, 21.5), (21.5, 17.5)], (FLAP2, BROWN)),
    c.line([(7, 6.5), (16.5, 10.5), (16.5, 15)], GOLD2, 1.4)))
icon("application-x-executable", "mimetypes")(lambda c: file_icon(c, lambda c: gear(c, 12, 13.5, 5.5, GREENA)))
icon("font-x-generic", "mimetypes")(lambda c: file_icon(c, lambda c: c.text(7, 6.5, "A", 12, font("DejaVuSerif-Bold.ttf"))))


# ---- symbolic icons: one colour (GTK colours them), for GTK's own widgets ----
# Drawn black on a 16-unit grid at 16 and 24 pixels as NAME.symbolic.png
# (GTK's form: black where the foreground colour goes, alpha the shape).

SYMBOLIC = []          # (name, context, draw)
SYMBOLIC_SIZES = [16, 24]


def sym(name, context="actions"):
    def reg(f):
        SYMBOLIC.append((name, context, f))
        return f
    return reg


def fill(c, pts):
    c.poly(pts, K, None)


def ln(c, pts, w=1.6):
    c.line(pts, K, w)


def box(c, x0, y0, x1, y1, w=1.4):
    c.rect(x0, y0, x1, y1, None, K, w)


def tri(c, d, cx=8, cy=8, r=4.5):
    """a RISC OS scroll arrow: a solid triangle pointing d"""
    pts = {"down": [(cx - r, cy - r * 0.55), (cx + r, cy - r * 0.55), (cx, cy + r * 0.65)],
           "up": [(cx - r, cy + r * 0.55), (cx + r, cy + r * 0.55), (cx, cy - r * 0.65)],
           "left": [(cx + r * 0.55, cy - r), (cx + r * 0.55, cy + r), (cx - r * 0.65, cy)],
           "right": [(cx - r * 0.55, cy - r), (cx - r * 0.55, cy + r), (cx + r * 0.65, cy)]}[d]
    fill(c, pts)


def sym_arrow(c, d):
    """a block arrow (go-*)"""
    pts = [(5.5, 14), (10.5, 14), (10.5, 8.5), (13.5, 8.5), (8, 2), (2.5, 8.5), (5.5, 8.5)]
    rot = {"up": lambda x, y: (x, y), "down": lambda x, y: (x, 16 - y),
           "left": lambda x, y: (y, x), "right": lambda x, y: (16 - y, x)}[d]
    fill(c, [rot(x, y) for x, y in pts])


def sym_folder(c, cut=None):
    fill(c, [(1, 3), (6, 3), (7.5, 4.5), (15, 4.5), (15, 14), (1, 14)])
    c.erase([(2.5, 7), (13.5, 7), (13.5, 12.5), (2.5, 12.5)])
    if cut:
        cut(c)


def sym_sheet(c, x0=3, y0=1, x1=13, y1=15):
    fill(c, [(x0, y0), (x1 - 3.5, y0), (x1, y0 + 3.5), (x1, y1), (x0, y1)])
    c.erase([(x0 + 1.5, y0 + 1.5), (x1 - 4.2, y0 + 1.5), (x1 - 4.2, y0 + 4.7), (x1 - 1.5, y0 + 4.7),
             (x1 - 1.5, y1 - 1.5), (x0 + 1.5, y1 - 1.5)])


def sym_ring(c, cx, cy, r, w=1.6):
    c.ellipse(cx - r, cy - r, cx + r, cy + r, None, K, w)


def sym_curve(c, left, cx=8, cy=9, r=4.5):
    """a curling arrow (undo: left, redo: right)"""
    s = 1 if left else -1
    pts = [(cx + s * r * math.cos(math.radians(a)), cy - r * math.sin(math.radians(a))) for a in range(-60, 181, 20)]
    ln(c, pts, 1.8)
    ex, ey = pts[-1]
    fill(c, [(ex - 3, ey - 0.5), (ex + 3, ey - 0.5), (ex, ey + 3.5)])


def sym_gear(c, cx=8, cy=8, r=6.5):
    pts = []
    for i in range(32):
        a = 2 * math.pi * i / 32
        rr = r if (i % 4) in (0, 1) else r * 0.72
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    fill(c, pts)
    c.erase(ellipse=(cx - r * 0.3, cy - r * 0.3, cx + r * 0.3, cy + r * 0.3))


def sym_magnifier(c, cx=6.5, cy=6.5, r=4.5, inside=None):
    sym_ring(c, cx, cy, r, 1.8)
    ln(c, [(cx + r * 0.75, cy + r * 0.75), (14.5, 14.5)], 2.6)
    if inside in ("+", "-"):
        ln(c, [(cx - 2.3, cy), (cx + 2.3, cy)], 1.4)
    if inside == "+":
        ln(c, [(cx, cy - 2.3), (cx, cy + 2.3)], 1.4)


def sym_padlock(c, closed=True):
    fill(c, [(3, 7.5), (13, 7.5), (13, 15), (3, 15)])
    sx = 8 if closed else 11
    pts = [(sx - 3.2, 7.5 if closed else 6)] + [(sx + 3.2 * math.cos(math.radians(a)), 4.5 + 3.2 * math.sin(math.radians(a)))
                                                for a in range(180, 361, 15)] + [(sx + 3.2, 7.5)]
    ln(c, pts, 1.6)
    c.erase(ellipse=(7, 9.5, 9, 11.5))


def sym_speaker(c, waves):
    fill(c, [(1.5, 6), (4.5, 6), (8.5, 2), (8.5, 14), (4.5, 10), (1.5, 10)])
    for i in range(waves):
        r = 2.5 + 2.2 * i
        c.arc(8.5, 8, r, -50, 50, K, 1.4)


def sym_monitor(c):
    fill(c, [(1, 2), (15, 2), (15, 11.5), (1, 11.5)])
    c.erase([(2.5, 3.5), (13.5, 3.5), (13.5, 10), (2.5, 10)])
    fill(c, [(6, 11.5), (10, 11.5), (11, 14.5), (5, 14.5)])


def sym_bin(c, full=False):
    fill(c, [(3, 4), (13, 4), (12, 15), (4, 15)])
    fill(c, [(2, 2), (14, 2), (14, 3.4), (2, 3.4)])
    for x in (6, 8, 10):
        c.erase(line=[(x, 6), (x, 13)], w=0.9)
    if full:
        fill(c, [(4, 0.5), (8, 0.5), (8, 1.5), (4, 1.5)])


# the widgets' own: arrows, the field clear button, add and remove, window controls
sym("pan-down-symbolic")(lambda c: tri(c, "down"))
sym("pan-up-symbolic")(lambda c: tri(c, "up"))
sym("pan-start-symbolic")(lambda c: tri(c, "left"))
sym("pan-end-symbolic")(lambda c: tri(c, "right"))
sym("go-up-symbolic")(lambda c: sym_arrow(c, "up"))
sym("go-down-symbolic")(lambda c: sym_arrow(c, "down"))
sym("go-previous-symbolic")(lambda c: sym_arrow(c, "left"))
sym("go-next-symbolic")(lambda c: sym_arrow(c, "right"))
sym("edit-clear-symbolic")(lambda c: (c.ellipse(1, 1, 15, 15, K, None),
                                      c.erase(line=[(5, 5), (11, 11)], w=1.8), c.erase(line=[(11, 5), (5, 11)], w=1.8)))
sym("list-add-symbolic")(lambda c: (ln(c, [(2, 8), (14, 8)], 2.4), ln(c, [(8, 2), (8, 14)], 2.4)))
sym("list-remove-symbolic")(lambda c: ln(c, [(2, 8), (14, 8)], 2.4))
sym("window-close-symbolic")(lambda c: (ln(c, [(3.5, 3.5), (12.5, 12.5)], 2.2), ln(c, [(12.5, 3.5), (3.5, 12.5)], 2.2)))
sym("window-minimize-symbolic")(lambda c: fill(c, [(3, 11), (13, 11), (13, 13.5), (3, 13.5)]))
sym("window-maximize-symbolic")(lambda c: (box(c, 2.5, 2.5, 13.5, 13.5, 1.4), fill(c, [(2.5, 2.5), (13.5, 2.5), (13.5, 5), (2.5, 5)])))
sym("window-restore-symbolic")(lambda c: (box(c, 5.5, 2.5, 13.5, 10.5, 1.2), box(c, 2.5, 5.5, 10.5, 13.5, 1.4),
                                          fill(c, [(2.5, 5.5), (10.5, 5.5), (10.5, 7.5), (2.5, 7.5)])))
sym("open-menu-symbolic")(lambda c: [ln(c, [(2.5, y), (13.5, y)], 1.8) for y in (4, 8, 12)])
sym("view-more-symbolic")(lambda c: [c.ellipse(x - 1.6, 6.4, x + 1.6, 9.6, K, None) for x in (3, 8, 13)])
sym("object-select-symbolic")(lambda c: ln(c, [(2, 8.5), (6, 12.5), (14, 3.5)], 2.4))
sym("process-working-symbolic")(lambda c: [c.ellipse(8 + 5.5 * math.cos(math.radians(a)) - r, 8 + 5.5 * math.sin(math.radians(a)) - r,
                                                     8 + 5.5 * math.cos(math.radians(a)) + r, 8 + 5.5 * math.sin(math.radians(a)) + r,
                                                     (0, 0, 0, int(60 + 195 * i / 7)), None)
                                           for i, a in enumerate(range(0, 360, 45)) for r in (0.8 + 0.12 * i,)])
sym("caps-lock-symbolic")(lambda c: (fill(c, [(8, 1.5), (14, 8), (10.5, 8), (10.5, 10.5), (5.5, 10.5), (5.5, 8), (2, 8)]),
                                     fill(c, [(5.5, 12), (10.5, 12), (10.5, 14), (5.5, 14)])))
sym("switch-on-symbolic")(lambda c: ln(c, [(8, 3), (8, 13)], 2))
sym("switch-off-symbolic")(lambda c: sym_ring(c, 8, 8, 5, 2))
sym("starred-symbolic")(lambda c: fill(c, [(8 + (7 if i % 2 == 0 else 3) * math.cos(math.radians(-90 + 36 * i)),
                                              8.5 + (7 if i % 2 == 0 else 3) * math.sin(math.radians(-90 + 36 * i))) for i in range(10)]))
sym("bookmark-new-symbolic")(lambda c: fill(c, [(3.5, 1), (12.5, 1), (12.5, 15), (8, 11), (3.5, 15)]))
sym("changes-prevent-symbolic")(lambda c: sym_padlock(c, True))
sym("changes-allow-symbolic")(lambda c: sym_padlock(c, False))
sym("dialog-password-symbolic", "status")(lambda c: sym_padlock(c, True))
sym("dialog-warning-symbolic", "status")(lambda c: (fill(c, [(8, 1), (15.5, 14.5), (0.5, 14.5)]),
                                                    c.erase(line=[(8, 5.5), (8, 9.5)], w=1.8),
                                                    c.erase(ellipse=(7, 11, 9, 13))))
sym("audio-volume-muted-symbolic", "status")(lambda c: (sym_speaker(c, 0), ln(c, [(10.5, 6), (14.5, 10)], 1.4),
                                                        ln(c, [(14.5, 6), (10.5, 10)], 1.4)))
sym("audio-volume-low-symbolic", "status")(lambda c: sym_speaker(c, 1))
sym("audio-volume-medium-symbolic", "status")(lambda c: sym_speaker(c, 2))
sym("audio-volume-high-symbolic", "status")(lambda c: sym_speaker(c, 3))
sym("emblem-system-symbolic", "emblems")(lambda c: sym_gear(c))
sym("system-run-symbolic")(lambda c: sym_gear(c))
sym("emblem-synchronizing-symbolic", "emblems")(lambda c: (c.arc(8, 8, 5.5, 200, 340, K, 1.8), c.arc(8, 8, 5.5, 20, 160, K, 1.8),
                                                           fill(c, [(11.5, 1.5), (15, 5.5), (10, 6)]),
                                                           fill(c, [(4.5, 14.5), (1, 10.5), (6, 10)])))
sym("view-refresh-symbolic")(lambda c: (c.arc(8, 8, 5.5, 200, 340, K, 1.8), c.arc(8, 8, 5.5, 20, 160, K, 1.8),
                                        fill(c, [(11.5, 1.5), (15, 5.5), (10, 6)]), fill(c, [(4.5, 14.5), (1, 10.5), (6, 10)])))
sym("document-open-recent-symbolic")(lambda c: (sym_ring(c, 8, 8, 6.5, 1.6), ln(c, [(8, 4), (8, 8), (11, 9.5)], 1.6)))
sym("find-location-symbolic")(lambda c: (sym_ring(c, 8, 8, 5, 1.6), ln(c, [(8, 0.5), (8, 4)]), ln(c, [(8, 12), (8, 15.5)]),
                                         ln(c, [(0.5, 8), (4, 8)]), ln(c, [(12, 8), (15.5, 8)]), c.ellipse(6.5, 6.5, 9.5, 9.5, K, None)))
sym("face-smile-symbolic")(lambda c: (sym_ring(c, 8, 8, 6.5), c.ellipse(5, 5, 7, 7.5, K, None), c.ellipse(9, 5, 11, 7.5, K, None),
                                      c.arc(8, 8, 3.8, 20, 160, K, 1.4)))

# documents and editing
sym("document-new-symbolic")(lambda c: (sym_sheet(c), ln(c, [(8, 7), (8, 13)], 1.4), ln(c, [(5, 10), (11, 10)], 1.4)))
sym("document-open-symbolic")(lambda c: (sym_folder(c),))
sym("document-save-symbolic")(lambda c: (fill(c, [(1.5, 1.5), (12.5, 1.5), (14.5, 3.5), (14.5, 14.5), (1.5, 14.5)]),
                                         c.erase([(4.5, 1.5), (11, 1.5), (11, 6), (4.5, 6)]),
                                         c.erase([(4, 9), (12, 9), (12, 13), (4, 13)])))
sym("text-x-generic-symbolic", "mimetypes")(lambda c: (sym_sheet(c), [ln(c, [(5, y), (11, y)], 1) for y in (8, 10, 12)]))
sym("application-x-executable-symbolic", "mimetypes")(lambda c: sym_gear(c))
sym("edit-copy-symbolic")(lambda c: (box(c, 1.5, 1.5, 10, 10, 1.4), fill(c, [(5, 5), (14.5, 5), (14.5, 14.5), (5, 14.5)]),
                                     c.erase([(6.5, 6.5), (13, 6.5), (13, 13), (6.5, 13)])))
sym("edit-cut-symbolic")(lambda c: (ln(c, [(4.5, 1), (10, 10)], 1.4), ln(c, [(11.5, 1), (6, 10)], 1.4),
                                    sym_ring(c, 4.5, 12, 2.4, 1.5), sym_ring(c, 11.5, 12, 2.4, 1.5)))
sym("edit-paste-symbolic")(lambda c: (fill(c, [(2, 2.5), (14, 2.5), (14, 15), (2, 15)]),
                                      c.erase([(3.5, 4.5), (12.5, 4.5), (12.5, 13.5), (3.5, 13.5)]),
                                      fill(c, [(5.5, 1), (10.5, 1), (10.5, 4), (5.5, 4)]),
                                      [ln(c, [(5, y), (11, y)], 1) for y in (7.5, 10)]))
sym("edit-undo-symbolic")(lambda c: sym_curve(c, True))
sym("edit-redo-symbolic")(lambda c: sym_curve(c, False))
sym("edit-delete-symbolic")(lambda c: (ln(c, [(3, 3), (13, 13)], 2.4), ln(c, [(13, 3), (3, 13)], 2.4)))
sym("edit-find-symbolic")(lambda c: sym_magnifier(c))
sym("edit-select-all-symbolic")(lambda c: ([ln(c, [(x, 1.5), (x + 1.5, 1.5)], 1) for x in (1.5, 4.5, 7.5, 10.5, 13)],
                                           [ln(c, [(x, 14.5), (x + 1.5, 14.5)], 1) for x in (1.5, 4.5, 7.5, 10.5, 13)],
                                           [ln(c, [(1.5, y), (1.5, y + 1.5)], 1) for y in (4.5, 7.5, 10.5)],
                                           [ln(c, [(14.5, y), (14.5, y + 1.5)], 1) for y in (4.5, 7.5, 10.5)],
                                           fill(c, [(4.5, 4.5), (11.5, 4.5), (11.5, 11.5), (4.5, 11.5)])))
sym("zoom-in-symbolic")(lambda c: sym_magnifier(c, inside="+"))
sym("zoom-out-symbolic")(lambda c: sym_magnifier(c, inside="-"))

# places (the file chooser's sidebar), devices and the network
sym("folder-symbolic", "places")(lambda c: sym_folder(c))
sym("folder-documents-symbolic", "places")(lambda c: sym_folder(c, lambda c: [ln(c, [(4, y), (12, y)], 1) for y in (8.8, 10.8)]))
sym("folder-download-symbolic", "places")(lambda c: sym_folder(c, lambda c: fill(c, [(7, 7.5), (9, 7.5), (9, 9.5), (10.5, 9.5),
                                                                                     (8, 12), (5.5, 9.5), (7, 9.5)])))
sym("folder-music-symbolic", "places")(lambda c: sym_folder(c, lambda c: (c.ellipse(5.5, 9.8, 8, 12, K, None),
                                                                          ln(c, [(7.8, 11), (7.8, 7.8), (10.5, 7.5)], 1))))
sym("folder-pictures-symbolic", "places")(lambda c: sym_folder(c, lambda c: fill(c, [(4, 12), (7, 8.5), (9, 10.5), (10.5, 9),
                                                                                     (12.5, 12)])))
sym("folder-videos-symbolic", "places")(lambda c: sym_folder(c, lambda c: fill(c, [(6.5, 8), (10.5, 9.8), (6.5, 11.6)])))
sym("folder-publicshare-symbolic", "places")(lambda c: sym_folder(c, lambda c: (c.ellipse(7, 7.8, 9, 9.8, K, None),
                                                                                fill(c, [(5.5, 12), (6, 10.3), (10, 10.3), (10.5, 12)]))))
sym("folder-remote-symbolic", "places")(lambda c: sym_folder(c, lambda c: (sym_ring(c, 8, 9.8, 2.2, 1), ln(c, [(5.5, 9.8), (10.5, 9.8)], 0.8))))
sym("folder-saved-search-symbolic", "places")(lambda c: sym_folder(c, lambda c: (sym_ring(c, 7.5, 9.5, 1.8, 1),
                                                                                 ln(c, [(8.8, 10.8), (10.5, 12.3)], 1.2))))
sym("folder-templates-symbolic", "places")(lambda c: sym_folder(c, lambda c: box(c, 5, 8, 11, 11.5, 1)))
sym("user-home-symbolic", "places")(lambda c: (fill(c, [(8, 1), (15.5, 7.5), (13, 7.5), (13, 15), (3, 15), (3, 7.5), (0.5, 7.5)]),
                                               c.erase([(6.5, 10), (9.5, 10), (9.5, 15), (6.5, 15)])))
sym("user-desktop-symbolic", "places")(lambda c: sym_monitor(c))
sym("user-trash-symbolic", "places")(lambda c: sym_bin(c))
sym("user-trash-full-symbolic", "places")(lambda c: sym_bin(c, True))
sym("drive-harddisk-symbolic", "devices")(lambda c: (fill(c, [(1, 4), (15, 4), (15, 12), (1, 12)]),
                                                     c.erase([(2.5, 5.5), (13.5, 5.5), (13.5, 8), (2.5, 8)]),
                                                     c.erase(ellipse=(11, 9, 13, 11))))
sym("media-eject-symbolic", "devices")(lambda c: (fill(c, [(8, 2), (14.5, 9), (1.5, 9)]), fill(c, [(1.5, 11), (14.5, 11), (14.5, 14), (1.5, 14)])))
sym("network-server-symbolic", "devices")(lambda c: ([fill(c, [(2, y), (14, y), (14, y + 3.5), (2, y + 3.5)]) for y in (1.5, 6.25, 11)],
                                                     [c.erase(ellipse=(11, y + 1, 12.6, y + 2.6)) for y in (1.5, 6.25, 11)]))
sym("network-workgroup-symbolic", "places")(lambda c: (fill(c, [(1, 1), (8, 1), (8, 6.5), (1, 6.5)]),
                                                       fill(c, [(8, 9), (15, 9), (15, 14.5), (8, 14.5)]),
                                                       ln(c, [(4.5, 6.5), (4.5, 11.5), (8, 11.5)], 1.2)))


def render(f, size):
    k = size / 24
    c = Icon(24, 24, k, minline=True)
    f(c)
    return c.image()


def render_symbolic(f, size):
    c = Icon(16, 16, size / 16)
    f(c)
    im = c.image()
    # GTK's symbolic form: black, the shape in alpha
    black = im.copy()
    black.paste((0, 0, 0, 255), (0, 0) + im.size)
    black.putalpha(im.getchannel("A"))
    return black


def draw_theme(fontdir, themedir, sizes=SIZES):
    FONTS["dir"] = fontdir
    dirs = set()
    for name, ctx, f in ICONS:
        for size in sizes:
            rel = "%dx%d/%s" % (size, size, ctx)
            os.makedirs(os.path.join(themedir, rel), exist_ok=True)
            render(f, size).save(os.path.join(themedir, rel, name + ".png"))
            dirs.add((rel, size, ctx))
    for name, ctx, f in SYMBOLIC:
        for size in SYMBOLIC_SIZES:
            rel = "%dx%d/%s" % (size, size, ctx)
            os.makedirs(os.path.join(themedir, rel), exist_ok=True)
            render_symbolic(f, size).save(os.path.join(themedir, rel, name + ".symbolic.png"))
            dirs.add((rel, size, ctx))
    return dirs


def draw_pack(name, fontdir, themedir, sizes=SIZES):
    """a program's own icons (tools/riscosicons_apps.py) into its RISCOS
    theme (made by build/mkicons.py): listed in index.theme and README"""
    import riscosicons_apps as apps
    FONTS["dir"] = fontdir
    apps.setup(font, {"plus": plus, "minus": minus, "chain": chain, "floppy": floppy})
    icons = apps.PACKS[name]
    dirs = set()
    for n, ctx, f in icons:
        for size in sizes:
            rel = "%dx%d/%s" % (size, size, ctx)
            os.makedirs(os.path.join(themedir, rel), exist_ok=True)
            render(f, size).save(os.path.join(themedir, rel, n + ".png"))
            dirs.add((rel, size, ctx))
    index = os.path.join(themedir, "index.theme")
    if os.path.exists(index):
        text = open(index).read()
        listed = text.split("Directories=", 1)[1].split("\n", 1)[0].split(",")
        new = sorted((d for d in dirs if d[0] not in listed), key=lambda d: (d[1], d[2]))
        if new:
            text = text.replace("Directories=" + ",".join(listed),
                                "Directories=" + ",".join(listed + [d[0] for d in new]), 1)
            text += "".join("[%s]\nSize=%d\nContext=%s\nType=Fixed\n\n" % (r, sz, c.capitalize())
                            for r, sz, c in new)
            open(index, "w").write(text)
    with open(os.path.join(themedir, "README"), "a") as f:
        f.write("And the program's own, drawn by tools/mkriscosicons.py --pack %s (MIT):\n  %s\n"
                % (name, " ".join(sorted(n for n, _c, _f in icons))))
    return icons


def main():
    if sys.argv[1] == "--pack":
        name, fontdir, themedir = sys.argv[2:5]
        icons = draw_pack(name, fontdir, themedir)
        if len(sys.argv) > 5:
            from PIL import Image
            cols, z = 10, 3
            cell = 24 * z + 16 * z + 12
            rows = (len(icons) + cols - 1) // cols
            im = Image.new("RGBA", (cols * cell, rows * (24 * z + 8)), (221, 221, 221, 255))
            for i, (_n, _c, f) in enumerate(icons):
                x, y = (i % cols) * cell + 4, (i // cols) * (24 * z + 8) + 4
                im.alpha_composite(render(f, 24).resize((24 * z, 24 * z), Image.NEAREST), (x, y))
                im.alpha_composite(render(f, 16).resize((16 * z, 16 * z), Image.NEAREST), (x + 24 * z + 4, y))
            im.save(sys.argv[5])
        print("mkriscosicons: %s's %d icons" % (name, len(icons)))
        return
    fontdir, themedir = sys.argv[1], sys.argv[2]
    dirs = draw_theme(fontdir, themedir)
    if len(sys.argv) > 3:
        from PIL import Image
        entries = [(lambda f=f: render(f, 24), lambda f=f: render(f, 16)) for _n, _c, f in ICONS]
        entries += [(lambda f=f: render_symbolic(f, 24), lambda f=f: render_symbolic(f, 16)) for _n, _c, f in SYMBOLIC]
        cols, z = 10, 3
        cell = 24 * z + 16 * z + 12
        rows = (len(entries) + cols - 1) // cols
        im = Image.new("RGBA", (cols * cell, rows * (24 * z + 8)), (221, 221, 221, 255))
        for i, (big, small) in enumerate(entries):
            x, y = (i % cols) * cell + 4, (i // cols) * (24 * z + 8) + 4
            im.alpha_composite(big().resize((24 * z, 24 * z), Image.NEAREST), (x, y))
            im.alpha_composite(small().resize((16 * z, 16 * z), Image.NEAREST), (x + 24 * z + 4, y))
        im.save(sys.argv[3])
    print("mkriscosicons: %d icons at %s pixels, %d symbolic at %s" %
          (len(ICONS), ", ".join(map(str, SIZES)), len(SYMBOLIC), ", ".join(map(str, SYMBOLIC_SIZES))))
    print("dirs:")
    for rel, _s, _c in sorted(dirs):
        print(rel)


if __name__ == "__main__":
    main()
