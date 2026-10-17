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
  SPRITEDIR  !clawsmail.png (34 x 34) and sm!clawsmail.png (17 x 17) for
             !Sprites, and !clawsmail11.png (68 x 68) and sm!clawsmail11.png
             (34 x 34) for !Sprites11 (180 dpi), for tools/png2sprite.py.
  Every theme icon is also drawn twice the size as NAME@2.png, for 180 dpi
  (EX0 EY0) modes.
  SHEET.png  optional: every icon enlarged, to look at.

Needs Pillow. MIT licence. Copyright (C) 2026 Andrew Youll.
"""
import os
import sys
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from riscosdraw import *  # noqa: E402,F401,F403  (the palette, Icon and the shapes)

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


def emblem_image(name, k=1):
    c = Icon(9, 9, k)
    dict(EMBLEMS)[name](c)
    return c.image()


def folder_image(name, k=1):
    open_, ems, trash = FOLDERS[name]
    c = Icon(16, 16, k)
    if trash:
        wastebasket(c, 2.5, 1.5, 13.5, 14.5)
    else:
        folder(c, 0.5, 3.5, 14.5, 13.5, open_)
    im = c.image()
    im = place_emblems(im, ems, k)
    if "noselect" in name:
        im = Image.blend(im, Image.new("RGBA", im.size, (220, 220, 220, 0)), 0.45)
    return im


def place_emblems(im, ems, k=1):
    """the kind's emblem bottom right; hrm and mark top left / bottom left"""
    pos = {"inbox": (7, 7), "outbox": (7, 7), "queue": (7, 7), "drafts": (7, 7), "subs": (7, 7),
           "hrm": (0, 7), "mark": (7, 0)}
    for e in ems:
        im.alpha_composite(emblem_image(e, k), (pos[e][0] * k, pos[e][1] * k))
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


def app_icon(size, k=1):
    """a letter in its envelope, scored by Claws' claw: three green
    scratches across its front (k: k times bigger, for the logos)"""
    if size == 34:
        c = Icon(34, 34, k)
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


def render(f, w, h, k=1):
    c = Icon(w, h, k)
    f(c)
    return c.image()


def main():
    theme, sprites = sys.argv[1], sys.argv[2]
    os.makedirs(theme, exist_ok=True)
    os.makedirs(sprites, exist_ok=True)
    out = []
    # each icon also drawn twice the size, NAME@2.png, for 180 dpi (EX0
    # EY0) modes, where GTK draws at GDK scale 2 (riscos-claws.c)
    for name, f in TOOLBAR:
        im = render(f, 24, 24)
        im.save(os.path.join(theme, name + ".png"))
        render(f, 24, 24, 2).save(os.path.join(theme, name + "@2.png"))
        out.append((name, im))
    for name, w, h, f in MARKS:
        im = render(f, w, h)
        im.save(os.path.join(theme, name + ".png"))
        render(f, w, h, 2).save(os.path.join(theme, name + "@2.png"))
        out.append((name, im))
    for name, _ in EMBLEMS:
        im = emblem_image(name)
        im.save(os.path.join(theme, "emblem_" + name + ".png"))
        emblem_image(name, 2).save(os.path.join(theme, "emblem_" + name + "@2.png"))
        out.append(("emblem_" + name, im))
    for name in sorted(FOLDERS):
        im = folder_image(name)
        im.save(os.path.join(theme, name + ".png"))
        folder_image(name, 2).save(os.path.join(theme, name + "@2.png"))
        out.append((name, im))
    a = app_icon(34)
    a.save(os.path.join(sprites, "!clawsmail.png"))
    s = app_icon(17)
    s.save(os.path.join(sprites, "sm!clawsmail.png"))
    out += [("!clawsmail", a), ("sm!clawsmail", s)]
    # !Sprites11 (180 dpi): 68 x 68, and the 34 x 34 drawing as the small
    # icon (PRMs, Icon Sets)
    app_icon(34, 2).save(os.path.join(sprites, "!clawsmail11.png"))
    a.save(os.path.join(sprites, "sm!clawsmail11.png"))
    # the About box's and set-up wizard's logo, and the window icons
    # (riscos_claws_priv_pixbuf): the application icon, drawn big
    big = app_icon(34, 4)
    big.save(os.path.join(theme, "claws_mail_logo.png"))
    app_icon(34, 8).save(os.path.join(theme, "claws_mail_logo@2.png"))
    app_icon(34, 2).save(os.path.join(theme, "claws_mail_logo_small.png"))
    big.save(os.path.join(theme, "claws_mail_logo_small@2.png"))
    big.resize((64, 64), Image.LANCZOS).save(os.path.join(theme, "claws_mail_icon_64.png"))
    big.resize((48, 48), Image.LANCZOS).save(os.path.join(theme, "claws_mail_icon.png"))
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
