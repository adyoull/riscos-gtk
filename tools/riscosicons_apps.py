"""riscosicons_apps.py - riscos-gtk: programs' own icons in the RISC OS
style, for tools/mkriscosicons.py --pack NAME.

A program's own icon names (Gnumeric's gnumeric-*, Geany's geany-* and
classviewer-*) drawn as riscos-gtk's RISCOS theme draws the standard
ones; build/build-app.sh (ICON_PACK=NAME) puts them into that program's
RISCOS theme, which GTK searches before the program's own (hicolor or
resource) icons. Original drawings, on the 24-unit grid. Fill patterns
(Gnumeric's gnumeric-pattern-*) aren't icons and stay as they are.

MIT licence. Copyright (C) 2026 Andrew Youll.
"""
import math

from riscosdraw import (K, K2, WHITE, PAPER, PAPER2, CREAM, FLAP, FLAP2, GREY, GREY2,  # noqa: F401
                        GREY3, RED, RED2, GREEN, GREEN2, BLUE, BLUE2, LBLUE, YELLOW, YELLOW2,
                        ORANGE, ORANGE2, BROWN, BROWN2, GOLD, GOLD2, PINK, PINK2,
                        envelope, sheet, arrow, pencil, padlock, clock, tick, cross, badge,
                        gear, folder, paperclip)

PACKS = {}
_font = {}

BLUEA = (BLUE, BLUE2)
GREENA = (GREEN, GREEN2)
REDA = (RED, RED2)
HILITE = ((255, 236, 150, 255), (250, 215, 90, 255))     # selected cells
HEAD = ((200, 220, 250, 255), (160, 190, 235, 255))      # header cells


def setup(fontfile_fn, helpers):
    """fontfile_fn(name) -> path; helpers: mkriscosicons' shapes"""
    _font["f"] = fontfile_fn
    globals().update(helpers)


def font(name):
    return _font["f"](name)


def pack(name, context="actions"):
    lst = PACKS.setdefault(name, [])

    def reg(icon_name):
        def deco(f):
            lst.append((icon_name, context, f))
            return f
        return deco
    return reg


# ---- shapes for spreadsheets ----

def grid(c, x0, y0, x1, y1, cols=3, rows=3, fill=(WHITE, PAPER2), line=GREY2, outline=K2):
    c.rect(x0, y0, x1, y1, fill, None)
    for i in range(1, cols):
        x = x0 + (x1 - x0) * i / cols
        c.line([(x, y0), (x, y1)], line, 1)
    for i in range(1, rows):
        y = y0 + (y1 - y0) * i / rows
        c.line([(x0, y), (x1, y)], line, 1)
    if outline:
        c.rect(x0, y0, x1, y1, None, outline, 1)


def cell_box(c, x0, y0, x1, y1, i, j, cols=3, rows=3, fill=HILITE):
    w, h = (x1 - x0) / cols, (y1 - y0) / rows
    c.rect(x0 + i * w, y0 + j * h, x0 + (i + 1) * w, y0 + (j + 1) * h, fill, None)


G = (3.5, 3.5, 20.5, 20.5)      # the usual grid


def border_icon(edges, thick=False, double=False, dashed=False):
    """a 3 x 3 grid with the borders given drawn black: edges from top,
    bottom, left, right, inh (inside horizontal), inv (inside vertical),
    diag, rdiag"""
    def f(c):
        x0, y0, x1, y1 = G
        grid(c, x0, y0, x1, y1, outline=GREY2 if not dashed else None)
        w = 2.2 if thick else 1.4
        L = {"top": [(x0, y0), (x1, y0)], "bottom": [(x0, y1), (x1, y1)],
             "left": [(x0, y0), (x0, y1)], "right": [(x1, y0), (x1, y1)],
             "diag": [(x0, y0), (x1, y1)], "rdiag": [(x0, y1), (x1, y0)]}
        for e in edges:
            if e == "inh":
                for i in (1, 2):
                    y = y0 + (y1 - y0) * i / 3
                    c.line([(x0, y), (x1, y)], K, 1.4)
            elif e == "inv":
                for i in (1, 2):
                    x = x0 + (x1 - x0) * i / 3
                    c.line([(x, y0), (x, y1)], K, 1.4)
            elif e == "bottom" and double:
                c.line([(x0, y1 - 1.5), (x1, y1 - 1.5)], K, 1)
                c.line([(x0, y1 + 0.8), (x1, y1 + 0.8)], K, 1)
            else:
                c.line(L[e], K, w if e in ("bottom", "top", "left", "right") else 1.4)
        if dashed:
            for (a, b) in ((0, 0), (1, 1)):
                pass
    return f


def align_icon(h=None, v=None):
    """text lines in a cell, aligned"""
    def f(c):
        c.rect(3.5, 3.5, 20.5, 20.5, (WHITE, PAPER2), K2)
        if v:
            ys = {"top": [7, 10], "center": [10.5, 13.5], "bottom": [14, 17],
                  "justify": [7, 12, 17], "distributed": [7, 10.5, 14, 17]}[v]
            for y in ys:
                c.line([(6.5, y), (17.5, y)], K, 1.6)
            return
        for i, y in enumerate((7.5, 10.5, 13.5, 16.5)):
            if h == "fill":
                c.line([(6, y), (18, y)], K, 1.4)
            elif h == "distributed":
                for x in (6, 10.5, 15):
                    c.line([(x, y), (x + 3, y)], K, 1.4)
            else:                       # general: text left, numbers right
                if i % 2 == 0:
                    c.line([(6, y), (12, y)], K, 1.4)
                else:
                    c.line([(12, y), (18, y)], BLUE2, 1.4)
    return f


def column_icon(mark, row=False):
    def f(c):
        x0, y0, x1, y1 = 2.5, 3.5, 18.5, 20.5
        grid(c, x0, y0, x1, y1)
        if row:
            c.rect(x0, y0 + (y1 - y0) / 3, x1, y0 + 2 * (y1 - y0) / 3, HILITE, K2, 1)
        else:
            c.rect(x0 + (x1 - x0) / 3, y0, x0 + 2 * (x1 - x0) / 3, y1, HILITE, K2, 1)
        mark(c)
    return f


def mk_plus(c):
    plus(c, 18.5, 18.5, 4.5)


def mk_minus(c):
    minus(c, 18.5, 18.5, 4.5)


def mk_hide(c):
    eye(c, 18, 18.5, 5, closed=True)


def mk_unhide(c):
    eye(c, 18, 18.5, 5)


def eye(c, cx, cy, r, closed=False):
    pts = [(cx + r * math.cos(math.radians(a)), cy - r * 0.6 * math.sin(math.radians(a))) for a in range(0, 181, 15)]
    pts += [(cx + r * math.cos(math.radians(a)), cy + r * 0.6 * math.sin(math.radians(a))) for a in range(180, 361, 15)]
    c.poly(pts, WHITE)
    if closed:
        c.line([(cx - r, cy), (cx + r, cy)], K, 1.2)
        c.line([(cx - r * 0.9, cy + r * 0.7), (cx + r * 0.9, cy - r * 0.7)], RED, 1.6)
    else:
        c.ellipse(cx - r * 0.45, cy - r * 0.45, cx + r * 0.45, cy + r * 0.45, BLUEA, None)
        c.ellipse(cx - r * 0.18, cy - r * 0.18, cx + r * 0.18, cy + r * 0.18, K, None)


def mk_size(row):
    def f(c):
        if row:
            arrow(c, 15.5, 11.5, 22.5, 16.5, "up", BLUEA)
            arrow(c, 15.5, 16.5, 22.5, 22.5, "down", BLUEA)
        else:
            arrow(c, 12.5, 15.5, 17.5, 22.5, "left", BLUEA)
            arrow(c, 17.5, 15.5, 22.5, 22.5, "right", BLUEA)
    return f


def note(c, x0, y0, x1, y1):
    """a comment: a yellow note with its corner turned"""
    c.poly([(x0, y0), (x1, y0), (x1, y1 - 4), (x1 - 4, y1), (x0, y1)], ((255, 250, 170, 255), (240, 220, 100, 255)))
    c.poly([(x1, y1 - 4), (x1 - 4, y1 - 4), (x1 - 4, y1)], (230, 200, 80, 255))
    for y in (y0 + 3, y0 + 5.5, y0 + 8):
        if y < y1 - 4:
            c.line([(x0 + 2, y), (x1 - 3, y)], GREY3, 1)


def funnel(c, x0, y0, x1, y1, fill=BLUEA):
    mx = (x0 + x1) / 2
    c.poly([(x0, y0), (x1, y0), (mx + 2, y0 + (y1 - y0) * 0.55), (mx + 2, y1), (mx - 2, y1 - 2),
            (mx - 2, y0 + (y1 - y0) * 0.55)], fill)


def chain_small(c, x0, y0, x1, y1):
    chain(c, x0, y0, x1, y1)


def globe(c, cx, cy, r):
    c.ellipse(cx - r, cy - r, cx + r, cy + r, ((120, 180, 255, 255), BLUE2))
    c.line([(cx, cy - r), (cx, cy + r)], WHITE, 0.8)
    c.line([(cx - r, cy), (cx + r, cy)], WHITE, 0.8)
    c.ellipse(cx - r * 0.45, cy - r, cx + r * 0.45, cy + r, None, WHITE, 0.8)


def bracket(c, x, y0, y1, sign):
    c.line([(x + 3, y0), (x, y0), (x, y1), (x + 3, y1)], K, 1.4)
    my = (y0 + y1) / 2
    c.rect(x - 3.5, my - 3.5, x + 3.5, my + 3.5, WHITE, K, 1)
    c.line([(x - 2, my), (x + 2, my)], K, 1.2)
    if sign == "+":
        c.line([(x, my - 2), (x, my + 2)], K, 1.2)


def wimp_button(c, x0, y0, x1, y1, fill=(GREY, GREY2)):
    """a RISC OS action button: slabbed out"""
    c.rect(x0, y0, x1, y1, fill, K)
    c.line([(x0 + 1, y1 - 1), (x0 + 1, y0 + 1), (x1 - 1, y0 + 1)], WHITE, 1)
    c.line([(x0 + 1, y1 - 1), (x1 - 1, y1 - 1), (x1 - 1, y0 + 1)], GREY3, 1)


def txt(c, x, y, s, size, f="DejaVuSans-Bold.ttf", col=K):
    c.text(x, y, s, size, font(f), col)


# ---- Gnumeric ----

gn = pack("gnumeric")

gn("gnumeric-autofilter")(lambda c: (grid(c, *G), funnel(c, 9.5, 8.5, 21.5, 21.5)))
gn("gnumeric-autofilter-delete")(lambda c: (grid(c, *G), funnel(c, 6.5, 5.5, 18.5, 18.5), cross(c, 14, 14, 21, 21)))
gn("gnumeric-autosum")(lambda c: txt(c, 4.5, 0.5, "Σ", 21, "DejaVuSerif-Bold.ttf"))
gn("gnumeric-equal")(lambda c: (c.line([(5, 9), (19, 9)], K, 2.4), c.line([(5, 15), (19, 15)], K, 2.4)))
gn("gnumeric-exprentry")(lambda c: (c.rect(2.5, 6.5, 21.5, 17.5, WHITE, K), txt(c, 4.5, 4.8, "=", 12),
                                     c.line([(13, 9), (13, 15)], K, 1)))
gn("gnumeric-brush")(lambda c: (c.line([(19.5, 3.5), (11, 12)], K, 3.4), c.line([(19.5, 3.5), (11, 12)], BROWN, 1.6),
                                 c.poly([(9, 10), (13.5, 14.5), (10, 19), (4, 21), (6, 15)], (YELLOW, ORANGE))))
gn("gnumeric-bucket")(lambda c: (c.poly([(4, 10), (12, 3), (20, 11), (12, 19)], (WHITE, GREY2)),
                                  c.poly([(4, 10), (12, 18.5), (8, 21)], BLUEA, BLUE2, 1),
                                  c.ellipse(17.5, 15, 21.5, 21.5, BLUEA, None)))
gn("gnumeric-cells-merge")(lambda c: (grid(c, *G), c.rect(3.5, 9.2, 20.5, 14.8, HILITE, K, 1),
                                       arrow(c, 4.5, 9.5, 10.5, 14.5, "right", BLUEA),
                                       arrow(c, 13.5, 9.5, 19.5, 14.5, "left", BLUEA)))
gn("gnumeric-cells-split")(lambda c: (grid(c, *G), c.rect(3.5, 9.2, 20.5, 14.8, HILITE, K, 1),
                                       c.line([(12, 9.2), (12, 14.8)], K, 1.2),
                                       arrow(c, 3.5, 9.5, 9.5, 14.5, "left", BLUEA),
                                       arrow(c, 14.5, 9.5, 20.5, 14.5, "right", BLUEA)))
gn("gnumeric-center-across-selection")(lambda c: (grid(c, 2.5, 5.5, 21.5, 18.5, 3, 1),
                                                   c.rect(2.5, 5.5, 21.5, 18.5, HILITE, K, 1),
                                                   c.line([(8, 12), (16, 12)], K, 1.6),
                                                   c.poly([(3.5, 12), (6.5, 9.5), (6.5, 14.5)], BLUEA, None),
                                                   c.poly([(20.5, 12), (17.5, 9.5), (17.5, 14.5)], BLUEA, None)))
for kind, mark in (("add", mk_plus), ("delete", mk_minus), ("hide", mk_hide), ("unhide", mk_unhide)):
    gn("gnumeric-column-" + kind)(column_icon(mark))
    gn("gnumeric-row-" + kind)(column_icon(mark, True))
gn("gnumeric-column-size")(column_icon(mk_size(False)))
gn("gnumeric-row-size")(column_icon(mk_size(True), True))
gn("gnumeric-comment-add")(lambda c: (note(c, 3.5, 3.5, 19.5, 19.5), plus(c, 18, 18, 4.5)))
gn("gnumeric-comment-delete")(lambda c: (note(c, 3.5, 3.5, 19.5, 19.5), cross(c, 14, 14, 21, 21)))
gn("gnumeric-comment-edit")(lambda c: (note(c, 3.5, 3.5, 19.5, 19.5), pencil(c, 11, 20, 22, 9)))
gn("gnumeric-component-insert")(lambda c: (grid(c, *G), c.rect(8.5, 8.5, 18.5, 18.5, BLUEA, K)))
gn("gnumeric-component-insert-shaped")(lambda c: (grid(c, *G), c.ellipse(8, 8, 19, 19, BLUEA)))
gn("gnumeric-data-slicer")(lambda c: (grid(c, *G), funnel(c, 3.5, 3.5, 12.5, 12.5, GREENA), c.rect(12, 12, 20.5, 20.5, HILITE, K, 1)))
gn("gnumeric-pivottable")(lambda c: (grid(c, *G, cols=4, rows=4), c.rect(3.5, 3.5, 20.5, 7.75, HEAD, K, 1),
                                      c.rect(3.5, 3.5, 7.75, 20.5, HEAD, K, 1)))
gn("gnumeric-detail-hide")(lambda c: (grid(c, 8.5, 3.5, 21.5, 20.5), bracket(c, 4.5, 4.5, 19.5, "-")))
gn("gnumeric-detail-show")(lambda c: (grid(c, 8.5, 3.5, 21.5, 20.5), bracket(c, 4.5, 4.5, 19.5, "+")))
gn("gnumeric-group")(lambda c: (grid(c, 8.5, 3.5, 21.5, 20.5, 2, 4), bracket(c, 4.5, 4.5, 19.5, "-"),
                                 arrow(c, 14.5, 8.5, 21.5, 15.5, "left", GREENA)))
gn("gnumeric-ungroup")(lambda c: (grid(c, 8.5, 3.5, 21.5, 20.5, 2, 4), bracket(c, 4.5, 4.5, 19.5, "+"),
                                   arrow(c, 14.5, 8.5, 21.5, 15.5, "right", REDA)))
gn("gnumeric-font")(lambda c: txt(c, 3.5, 0, "A", 21, "DejaVuSerif.ttf"))
gn("gnumeric-format-accounting")(lambda c: ([c.ellipse(4.5, y, 15.5, y + 4.5, (GOLD, GOLD2)) for y in (16.5, 13, 9.5)],
                                             c.rect(10.5, 4.5, 21.5, 12.5, ((150, 220, 150, 255), GREEN2)),
                                             c.ellipse(14, 6.5, 18, 10.5, None, WHITE, 0.9)))
gn("gnumeric-format-percentage")(lambda c: txt(c, 1, 1, "%", 19))
gn("gnumeric-format-thousand-separator")(lambda c: txt(c, 0.5, 4.5, "000", 11.5))
gn("gnumeric-format-precision-increase")(lambda c: (txt(c, 1, 0.5, ".0", 10), txt(c, 6, 10.5, ".00", 10),
                                                     arrow(c, 16, 2.5, 22.5, 10.5, "down", GREENA)))
gn("gnumeric-format-precision-decrease")(lambda c: (txt(c, 1, 0.5, ".00", 10), txt(c, 6, 10.5, ".0", 10),
                                                     arrow(c, 16, 2.5, 22.5, 10.5, "down", REDA)))
gn("gnumeric-formulaguru")(lambda c: (txt(c, 1.5, 3, "f(x)", 11, "DejaVuSerif-Italic.ttf"),
                                       c.line([(15, 19), (21.5, 12.5)], K, 2.4), c.ellipse(19.5, 9.5, 23, 13, (YELLOW, ORANGE), None)))
gn("gnumeric-graphguru")(lambda c: (c.rect(2.5, 3.5, 21.5, 20.5, (WHITE, PAPER2)),
                                     c.rect(5.5, 12.5, 8.5, 18.5, REDA, K, 1), c.rect(10.5, 7.5, 13.5, 18.5, BLUEA, K, 1),
                                     c.rect(15.5, 10.5, 18.5, 18.5, GREENA, K, 1)))
gn("gnumeric-format-halign-fill")(align_icon(h="fill"))
gn("gnumeric-format-halign-general")(align_icon(h="general"))
gn("gnumeric-format-halign-distributed")(align_icon(h="distributed"))
for va in ("top", "center", "bottom", "justify", "distributed"):
    gn("gnumeric-format-valign-" + va)(align_icon(v=va))
BORDERS = {
    "all": (["top", "bottom", "left", "right", "inh", "inv"],), "bottom": (["bottom"],),
    "diag": (["diag"],), "rev-diag": (["rdiag"],), "double-bottom": (["bottom"], False, True),
    "inside-horiz": (["inh"],), "inside-vert": (["inv"],), "inside": (["inh", "inv"],),
    "left": (["left"],), "no": ([],), "none": ([],), "outline": (["top", "bottom", "left", "right"],),
    "outside": (["top", "bottom", "left", "right"],), "right": (["right"],), "top": (["top"],),
    "thick-bottom": (["bottom"], True), "thick-outside": (["top", "bottom", "left", "right"], True),
    "top-n-bottom": (["top", "bottom"],), "top-n-double-bottom": (["top", "bottom"], False, True),
    "top-n-thick-bottom": (["top", "bottom"], True),
}
for b, args in BORDERS.items():
    gn("gnumeric-format-border-" + b)(border_icon(*args))
LINKS = {
    "add": lambda c: plus(c, 18.5, 18.5, 4.5), "delete": lambda c: cross(c, 15, 15, 22, 22),
    "edit": lambda c: pencil(c, 12, 21, 22, 11), "email": lambda c: envelope(c, 11.5, 13.5, 22.5, 21.5),
    "external": lambda c: globe(c, 17.5, 17, 5), "url": lambda c: globe(c, 17.5, 17, 5),
    "internal": lambda c: grid(c, 12.5, 12.5, 22.5, 21.5),
}
for l, mark in LINKS.items():
    gn("gnumeric-link-" + l)(lambda c, mark=mark: (chain(c, 1.5, 1.5, 15.5, 15.5), mark(c)))
gn("gnumeric-object-arrow")(lambda c: (c.line([(4, 20), (17, 7)], K, 1.8), c.poly([(20.5, 3.5), (12.5, 6), (18, 11.5)], K, None)))
gn("gnumeric-object-line")(lambda c: c.line([(4, 20), (20, 4)], K, 1.8))
gn("gnumeric-object-ellipse")(lambda c: c.ellipse(2.5, 5.5, 21.5, 18.5, (LBLUE, (120, 170, 230, 255))))
gn("gnumeric-object-rectangle")(lambda c: c.rect(3.5, 5.5, 20.5, 18.5, (LBLUE, (120, 170, 230, 255))))
gn("gnumeric-object-frame")(lambda c: (c.rect(2.5, 5.5, 21.5, 19.5, None, K2, 1), c.rect(4, 3, 12, 8, (240, 240, 240, 255), None),
                                        txt(c, 4, 1.5, "abc", 6, "DejaVuSans.ttf")))
gn("gnumeric-object-label")(lambda c: txt(c, 1.5, 5, "abc", 11, "DejaVuSans.ttf"))
gn("gnumeric-object-button")(lambda c: (wimp_button(c, 2.5, 6.5, 21.5, 17.5), txt(c, 7, 7.5, "OK", 8)))
gn("gnumeric-object-checkbox")(lambda c: (c.rect(5.5, 5.5, 18.5, 18.5, WHITE, K), tick(c, 7.5, 8, 17, 16)))
gn("gnumeric-object-radiobutton")(lambda c: (c.ellipse(5, 5, 19, 19, WHITE), c.ellipse(9, 9, 15, 15, K, None)))
gn("gnumeric-object-combo")(lambda c: (c.rect(2.5, 7.5, 21.5, 16.5, WHITE, K), c.line([(5, 12), (12, 12)], K2, 1.2),
                                        wimp_button(c, 15.5, 7.5, 21.5, 16.5),
                                        c.poly([(16.8, 10.5), (20.2, 10.5), (18.5, 13.5)], K, None)))
gn("gnumeric-object-list")(lambda c: (c.rect(2.5, 3.5, 17.5, 20.5, WHITE, K), c.rect(3, 8.5, 17, 12.5, LBLUE, None),
                                       [c.line([(4.5, y), (14.5, y)], K, 1.2) for y in (6, 10.5, 15, 18.5)],
                                       c.rect(17.5, 3.5, 21.5, 20.5, (GREY, GREY2), K, 1)))
gn("gnumeric-object-scrollbar")(lambda c: (c.rect(8.5, 1.5, 15.5, 22.5, (GREY2, GREY2), K),
                                            wimp_button(c, 8.5, 1.5, 15.5, 6.5), wimp_button(c, 8.5, 17.5, 15.5, 22.5),
                                            wimp_button(c, 9.5, 9, 14.5, 14, ((220, 220, 220, 255), GREY)),
                                            c.poly([(10.5, 5), (13.5, 5), (12, 3)], K, None),
                                            c.poly([(10.5, 19), (13.5, 19), (12, 21)], K, None)))
gn("gnumeric-object-slider")(lambda c: (c.rect(2.5, 10.5, 21.5, 13.5, (GREY2, GREY), K), wimp_button(c, 9.5, 6.5, 14.5, 17.5)))
gn("gnumeric-object-spinbutton")(lambda c: (c.rect(2.5, 7.5, 15.5, 16.5, WHITE, K), txt(c, 4, 7.6, "12", 7, "DejaVuSans.ttf"),
                                             wimp_button(c, 15.5, 4.5, 21.5, 12), wimp_button(c, 15.5, 12, 21.5, 19.5),
                                             c.poly([(17, 9.5), (20, 9.5), (18.5, 7)], K, None),
                                             c.poly([(17, 14.5), (20, 14.5), (18.5, 17)], K, None)))
HF = {"cell": lambda c: grid(c, 7.5, 7.5, 16.5, 16.5, 2, 2), "date": lambda c: txt(c, 6, 7, "31", 8),
      "page": lambda c: txt(c, 8, 7, "#", 8), "pages": lambda c: txt(c, 6.5, 7, "#/#", 6),
      "sheet": lambda c: grid(c, 7.5, 8.5, 16.5, 15.5, 3, 2), "time": lambda c: clock(c, 12, 12, 4.5)}
for k, mark in HF.items():
    gn("gnumeric-pagesetup-hf-" + k)(lambda c, mark=mark: (sheet(c, 4.5, 2.5, 19.5, 21.5, False),
                                                           c.line([(6.5, 5.5), (15.5, 5.5)], GREY2, 1),
                                                           c.line([(6.5, 19.5), (17.5, 19.5)], GREY2, 1), mark(c)))
gn("gnumeric-protection-yes")(lambda c: padlock(c, 5.5, 2.5, 18.5, 21.5))
gn("gnumeric-protection-yes-dialog")(lambda c: padlock(c, 5.5, 2.5, 18.5, 21.5))
gn("gnumeric-protection-no")(lambda c: padlock(c, 5.5, 2.5, 18.5, 21.5, False))
gn("gnumeric-subscript")(lambda c: (txt(c, 2, 0.5, "x", 17, "DejaVuSerif-Italic.ttf"), txt(c, 13, 9.5, "2", 10, "DejaVuSans.ttf", BLUE2)))
gn("gnumeric-superscript")(lambda c: (txt(c, 2, 2.5, "x", 17, "DejaVuSerif-Italic.ttf"), txt(c, 13, -0.5, "2", 10, "DejaVuSans.ttf", BLUE2)))
gn("gnumeric-visible")(lambda c: eye(c, 12, 12, 9.5))


# ---- Geany ----

ge = pack("geany")


def symbol_badge(c, fill, letter, sq=False):
    if sq:
        c.rect(2.5, 2.5, 21.5, 21.5, fill)
    else:
        c.ellipse(2, 2, 22, 22, fill)
    c.text(12, 12, letter, 14, font("DejaVuSans-Bold.ttf"), WHITE, anchor="mm")


ge("classviewer-class")(lambda c: symbol_badge(c, (ORANGE, ORANGE2), "C", True))
ge("classviewer-struct")(lambda c: symbol_badge(c, ((180, 120, 220, 255), (110, 50, 160, 255)), "S", True))
ge("classviewer-namespace")(lambda c: symbol_badge(c, (GREY2, GREY3), "N", True))
ge("classviewer-method")(lambda c: symbol_badge(c, BLUEA, "f"))
ge("classviewer-member")(lambda c: symbol_badge(c, GREENA, "m"))
ge("classviewer-var")(lambda c: symbol_badge(c, ((60, 190, 190, 255), (0, 120, 120, 255)), "v"))
ge("classviewer-macro")(lambda c: symbol_badge(c, REDA, "#"))
ge("classviewer-other")(lambda c: symbol_badge(c, ((170, 170, 170, 255), GREY3), "?"))
ge("geany-build")(lambda c: (gear(c, 10, 10, 7.5), gear(c, 17, 17, 5, (CREAM, FLAP2))))
ge("geany-save-all")(lambda c: (floppy(c, 7.5, 2.5, 21.5, 16.5), floppy(c, 2.5, 7.5, 16.5, 21.5)))
ge("geany-close-all")(lambda c: (c.rect(8.5, 2.5, 21.5, 15.5, (WHITE, GREY)), c.rect(2.5, 8.5, 15.5, 21.5, (WHITE, GREY)),
                                  cross(c, 6, 12, 12, 18, GREY3)))
