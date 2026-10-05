/*
 * gdkpointer-riscos.c - RISC OS pointer shapes for GDK cursors
 * (riscos-gtk's GDK backend).
 *
 * GTK asks for cursors by CSS name ("text", "pointer", "ew-resize"...);
 * gdkcursor-riscos.c maps them to RISCOS_POINTER_*. The server
 * (gdkriscos-server.c) shows the shape of the window under the pointer
 * and the normal pointer elsewhere, as the PRMs ask ("applications should
 * only change the mouse pointer when it is within the work area of one of
 * its windows").
 *
 * Each shape is a 2bpp sprite with a palette, made here at start-up (no
 * mask, so the active point given is used), set as pointer shape 2 with
 * OS_SpriteOp 36. Colour 0 is transparent, 1 black, 2 white. The normal
 * pointer comes back with Wimp_SpriteOp 36 "ptr_default" as shape 1, which
 * also restores the pointer palette (all shapes share it).
 *
 * Shapes are drawn as a white area; a black outline is added round it.
 * The sprites say 90 dpi, so they are 1:1 on the usual EIG 1 desktop.
 *
 * SWIs as in the RISC OS PRMs: OS_SpriteOp 36 (R3 bits 0-3 = shape;
 * R4, R5 = active point in pixels from the top left; R6 = R7 = 0: a 2bpp
 * sprite needs no translation table, palette entries 1-3 are used),
 * Wimp_SpriteOp 36.
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as the GTK code it
 * plugs into.
 */
#include "config.h"

#include "gdkprivate-riscos.h"

#include <string.h>
#include <kernel.h>
#include <swis.h>

#ifndef OS_SpriteOp
#define OS_SpriteOp 0x2E
#endif
#ifndef Wimp_SpriteOp
#define Wimp_SpriteOp 0x400E9
#endif

#define MAXW 32
#define MAXH 32

typedef struct
{
  const char *name;           /* sprite name */
  int w, h, hx, hy;           /* size and active point, pixels */
} Shape;

static int *area;             /* one sprite area with every shape */

/* ---- the drawings: is pixel (x, y) of shape s white? ---------------- */

static const char *hand[] = {
  "    oo",
  "    oo",
  "    oo",
  "    oo",
  "    oo oo",
  "    oo oo oo",
  "    oo oo oo o",
  "oo  oo oo oo o",
  "oo ooooooooooo",
  "ooooooooooooooo",
  " oooooooooooooo",
  "  ooooooooooooo",
  "  oooooooooooo",
  "   ooooooooooo",
  "   oooooooooo",
  "    ooooooooo",
  "    ooooooooo",
};

static gboolean
white (int s, int x, int y)
{
  if (x < 0 || y < 0)
    return FALSE;
  switch (s)
    {
    case RISCOS_POINTER_TEXT:             /* I-beam, 7 x 18 */
      return (x == 3 && y >= 1 && y <= 16) || ((y == 1 || y == 16) && x >= 1 && x <= 5);
    case RISCOS_POINTER_HAND:             /* 15 x 17 */
      return y < (int) G_N_ELEMENTS (hand) && x < (int) strlen (hand[y]) && hand[y][x] == 'o';
    case RISCOS_POINTER_H_RESIZE:         /* <-> 23 x 11 */
    case RISCOS_POINTER_V_RESIZE:
      {
        int a = s == RISCOS_POINTER_H_RESIZE ? x : y, b = s == RISCOS_POINTER_H_RESIZE ? y : x;
        int d = ABS (b - 5);
        return (d <= 1 && a >= 3 && a <= 19) ||
               (a >= 1 && a <= 5 && d <= a - 1) || (a >= 17 && a <= 21 && d <= 21 - a);
      }
    case RISCOS_POINTER_NWSE_RESIZE:      /* 17 x 17 */
    case RISCOS_POINTER_NESW_RESIZE:
      {
        int u = s == RISCOS_POINTER_NWSE_RESIZE ? x : 16 - x;
        return (ABS (u - y) <= 1 && u >= 2 && u <= 14) ||
               (u >= 1 && y >= 1 && u + y <= 7) || (u <= 15 && y <= 15 && u + y >= 25);
      }
    case RISCOS_POINTER_CROSS:            /* + 17 x 17 */
      return (x == 8 && y >= 1 && y <= 15) || (y == 8 && x >= 1 && x <= 15);
    case RISCOS_POINTER_MOVE:             /* four arrows, 19 x 19 */
      {
        int dx = ABS (x - 9), dy = ABS (y - 9);
        return (dy <= 1 && x >= 3 && x <= 15) || (dx <= 1 && y >= 3 && y <= 15) ||
               (x >= 1 && x <= 4 && dy <= x - 1) || (x >= 14 && x <= 17 && dy <= 17 - x) ||
               (y >= 1 && y <= 4 && dx <= y - 1) || (y >= 14 && y <= 17 && dx <= 17 - y);
      }
    default:
      return FALSE;
    }
}

static const Shape shapes[RISCOS_POINTER_COUNT] = {
  [RISCOS_POINTER_TEXT]        = { "gtk_text",  7, 18,  3,  9 },
  [RISCOS_POINTER_HAND]        = { "gtk_hand", 15, 17,  4,  0 },
  [RISCOS_POINTER_H_RESIZE]    = { "gtk_hsize", 23, 11, 11,  5 },
  [RISCOS_POINTER_V_RESIZE]    = { "gtk_vsize", 11, 23,  5, 11 },
  [RISCOS_POINTER_NWSE_RESIZE] = { "gtk_nwse", 17, 17,  8,  8 },
  [RISCOS_POINTER_NESW_RESIZE] = { "gtk_nesw", 17, 17,  8,  8 },
  [RISCOS_POINTER_CROSS]       = { "gtk_cross", 17, 17,  8,  8 },
  [RISCOS_POINTER_MOVE]        = { "gtk_move", 19, 19,  9,  9 },
};

/* Colour of pixel (x, y) of the sprite (a 1 pixel border round the
   drawing): 2 white, 1 black outline, 0 transparent */
static int
colour (int s, int x, int y)
{
  int dx, dy;

  if (white (s, x - 1, y - 1))
    return 2;
  for (dy = -1; dy <= 1; dy++)
    for (dx = -1; dx <= 1; dx++)
      if (white (s, x - 1 + dx, y - 1 + dy))
        return 1;
  return 0;
}

/* The sprite area: header, then per shape a 2bpp sprite with a palette */
static void
make_area (void)
{
  int s, size = 16, n = 0;
  char *p;

  for (s = 1; s < RISCOS_POINTER_COUNT; s++)
    {
      int w = MIN (shapes[s].w + 2, MAXW), h = MIN (shapes[s].h + 2, MAXH);
      size += 44 + 32 + ((w * 2 + 31) / 32) * 4 * h;
    }
  area = g_malloc0 (size);
  area[0] = size;
  area[2] = 16;                    /* first sprite */
  p = (char *) area + 16;
  for (s = 1; s < RISCOS_POINTER_COUNT; s++)
    {
      int w = MIN (shapes[s].w + 2, MAXW), h = MIN (shapes[s].h + 2, MAXH);
      int words = (w * 2 + 31) / 32, x, y;
      int *spr = (int *) p;
      unsigned int *pal, *img;

      spr[0] = 44 + 32 + words * 4 * h;
      strncpy ((char *) &spr[1], shapes[s].name, 12);
      spr[4] = words - 1;
      spr[5] = h - 1;
      spr[6] = 0;                  /* first bit used */
      spr[7] = (w * 2 - 1) % 32;   /* last bit used in the last word */
      spr[8] = 44 + 32;            /* image */
      spr[9] = 44 + 32;            /* mask = image: none */
      spr[10] = (2 << 27) | (90 << 14) | (90 << 1) | 1;   /* 2bpp, 90 x 90 dpi */
      pal = (unsigned int *) &spr[11];                    /* 4 entries, 2 words each */
      pal[0] = pal[1] = 0xFFFFFF00;                       /* (transparent) */
      pal[2] = pal[3] = 0x00000000;                       /* 1: black */
      pal[4] = pal[5] = 0xFFFFFF00;                       /* 2: white */
      pal[6] = pal[7] = 0x80808000;                       /* 3: grey (unused) */
      img = (unsigned int *) (p + 44 + 32);
      for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
          img[y * words + x / 16] |= (unsigned int) colour (s, x, y) << ((x % 16) * 2);
      p += spr[0];
      n++;
    }
  area[1] = n;
  area[3] = p - (char *) area;     /* first free */
}

/* Show shape (RISCOS_POINTER_*) as the pointer */
void
_gdk_riscos_pointer_show (int shape)
{
  _kernel_swi_regs r;

  memset (&r, 0, sizeof r);
  if (g_getenv ("RISCOS_GTK_DEBUG"))
    g_printerr ("riscos-gtk: pointer %s\n", shape > 0 && shape < RISCOS_POINTER_COUNT ?
                shapes[shape].name : "ptr_default");
  if (shape <= RISCOS_POINTER_DEFAULT || shape >= RISCOS_POINTER_COUNT)
    {
      r.r[0] = 36;
      r.r[2] = (int) "ptr_default";
      r.r[3] = 1;
      _kernel_swi (Wimp_SpriteOp, &r, &r);
      return;
    }
  if (area == NULL)
    make_area ();
  r.r[0] = 36 + 256;               /* user area, sprite by name */
  r.r[1] = (int) area;
  r.r[2] = (int) shapes[shape].name;
  r.r[3] = 2;
  r.r[4] = shapes[shape].hx + 1;   /* + the outline */
  r.r[5] = shapes[shape].hy + 1;
  if (_kernel_swi (OS_SpriteOp, &r, &r) != NULL)
    {
      r.r[0] = 36;                 /* couldn't: the normal pointer */
      r.r[2] = (int) "ptr_default";
      r.r[3] = 1;
      r.r[4] = r.r[5] = 0;
      r.r[1] = 0;
      _kernel_swi (Wimp_SpriteOp, &r, &r);
    }
}

/* For tests: the sprite area */
const int *
_gdk_riscos_pointer_area (void)
{
  if (area == NULL)
    make_area ();
  return area;
}
