/*
 * gdkwimptheme-riscos.c - the desktop's Wimp colours and Wimp sprites, for
 * riscos-gtk's theme (gtk/riscos/gtkriscostheme.c).
 *
 *   gdk_riscos_wimp_palette ()  the 16 Wimp colours now (Wimp_ReadPalette)
 *   gdk_riscos_wimp_sprite ()   a sprite from the Wimp's sprite pool as a
 *                               GdkPixbuf, at screen pixel size
 *
 * Sprites are read, not plotted: Wimp_SpriteOp 24 finds the sprite by name
 * (its address is used at once, before any Wimp_Poll, as the PRMs ask),
 * and its pixels are decoded here: 1, 2, 4, 8 bpp with or without a
 * palette (without one: the Wimp colours for 1/2/4 bpp, as the Wimp plots
 * them, and the standard 256-colour palette for 8 bpp), 16 and 32 bpp;
 * old-style masks (same depth as the image), new-format 1bpp masks and
 * RISC OS 5 8bpp alpha masks. Each sprite pixel becomes 2^(sprite eig -
 * screen eig) screen pixels, so a 90 dpi sprite is 1:1 at EIG 1 and a
 * 45 dpi one (old mode 12) is doubled vertically.
 *
 * SWIs as in the RISC OS PRMs: Wimp_SpriteOp (24: select, R2 -> sprite),
 * Wimp_ReadPalette, OS_ReadModeVariable (4, 5, 9 for numbered modes).
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as the GTK code it
 * plugs into.
 */
#include "config.h"

#include "gdkriscoswimp.h"
#include "gdkprivate-riscos.h"
#include "gdkdisplay-riscos.h"

#include <string.h>
#include <kernel.h>
#include <swis.h>

#ifndef Wimp_SpriteOp
#define Wimp_SpriteOp 0x400E9
#endif
#ifndef Wimp_ReadPalette
#define Wimp_ReadPalette 0x400E5
#endif
#ifndef OS_ReadModeVariable
#define OS_ReadModeVariable 0x35
#endif
#ifndef OS_ReadVduVariables
#define OS_ReadVduVariables 0x31
#endif

/* Wimp_ReadPalette: 16 Wimp colours, then border and pointer colours,
   each &BBGGRRxx */
static gboolean
read_palette (unsigned int pal[20])
{
  _kernel_swi_regs r;

  memset (pal, 0, 20 * sizeof (unsigned int));
  r.r[1] = (int) pal;
  return _kernel_swi (Wimp_ReadPalette, &r, &r) == NULL;
}

static guint32
rgb_of (unsigned int bbggrrxx)
{
  return ((bbggrrxx >> 8) & 0xFF) << 16 | ((bbggrrxx >> 16) & 0xFF) << 8 | ((bbggrrxx >> 24) & 0xFF);
}

/**
 * gdk_riscos_wimp_palette:
 * @rgb: (out): the 16 Wimp colours as 0xRRGGBB
 *
 * Returns: %TRUE if the Wimp gave them.
 */
gboolean
gdk_riscos_wimp_palette (guint32 rgb[16])
{
  unsigned int pal[20];
  int i;

  if (!read_palette (pal))
    return FALSE;
  for (i = 0; i < 16; i++)
    rgb[i] = rgb_of (pal[i]);
  return TRUE;
}

/* The standard 256-colour palette (an 8bpp sprite without its own):
   bits 0-1 tint; red 2 and 4, green 5 and 6, blue 3 and 7 */
static guint32
default256 (int c)
{
  int tint = c & 3;
  int r = ((c >> 2) & 1) << 2 | ((c >> 4) & 1) << 3 | tint;
  int g = ((c >> 5) & 1) << 2 | ((c >> 6) & 1) << 3 | tint;
  int b = ((c >> 3) & 1) << 2 | ((c >> 7) & 1) << 3 | tint;
  return (r * 0x11) << 16 | (g * 0x11) << 8 | (b * 0x11);
}

static int
screen_eig (int var)
{
  int vars[2] = { var, -1 }, val = 1;
  _kernel_swi_regs r;

  r.r[0] = (int) vars;
  r.r[1] = (int) &val;
  if (_kernel_swi (OS_ReadVduVariables, &r, &r) != NULL)
    return 1;
  return val;
}

static int
mode_var (int mode, int var, int fallback)
{
  _kernel_swi_regs r;

  r.r[0] = mode;
  r.r[1] = var;
  if (_kernel_swi (OS_ReadModeVariable, &r, &r) != NULL)
    return fallback;
  return r.r[2];
}

static int
eig_for_dpi (int dpi)
{
  return dpi >= 180 ? 0 : dpi >= 90 ? 1 : dpi >= 45 ? 2 : 3;
}

/* Decode the sprite at spr into a new pixbuf (RGBA, sprite pixels) */
static GdkPixbuf *
decode (const int *spr, int *xeig, int *yeig)
{
  int words = spr[4] + 1, rows = spr[5] + 1, first = spr[6], last = spr[7];
  int image = spr[8], mask = spr[9];
  unsigned int mode = (unsigned int) spr[10];
  int log2bpp, bpp, width, x, y, newfmt = 0, alpha_mask = 0, n_pal;
  guint32 pal[256];
  unsigned int wimp[20];
  const unsigned char *img = (const unsigned char *) spr + image;
  const unsigned char *msk = mask != image ? (const unsigned char *) spr + mask : NULL;
  GdkPixbuf *pb;
  guchar *px;
  int stride;

  if (mode < 256)
    {
      log2bpp = mode_var (mode, 9, 2);
      *xeig = mode_var (mode, 4, 1);
      *yeig = mode_var (mode, 5, 1);
    }
  else if ((mode & 1) && ((mode >> 27) & 15) == 15)
    {
      /* RISC OS 5 mode word: type in bits 20-26, eigs in bits 4-7 */
      static const int types[] = { -1, 0, 1, 2, 3, 4, 5, -1, -1, -1, 4 };
      int type = (mode >> 20) & 127;
      log2bpp = type < (int) G_N_ELEMENTS (types) ? types[type] : -1;
      *xeig = (mode >> 4) & 3;
      *yeig = (mode >> 6) & 3;
      newfmt = 1;
      alpha_mask = (mode >> 31) & 1;
    }
  else if (mode & 1)
    {
      static const int types[] = { -1, 0, 1, 2, 3, 4, 5, -1, -1, -1, 4 };
      int type = (mode >> 27) & 15;
      log2bpp = type < (int) G_N_ELEMENTS (types) ? types[type] : -1;
      *xeig = eig_for_dpi ((mode >> 1) & 0x1FFF);
      *yeig = eig_for_dpi ((mode >> 14) & 0x1FFF);
      newfmt = 1;
      alpha_mask = (mode >> 31) & 1;
    }
  else
    return NULL;                   /* a mode selector: not in sprite files */
  if (log2bpp < 0 || log2bpp > 5)
    return NULL;
  bpp = 1 << log2bpp;
  width = (words * 32 - first - (31 - last)) / bpp;
  if (width <= 0 || width > 512 || rows <= 0 || rows > 512)
    return NULL;

  /* the palette: the sprite's own (between header and image), else a default */
  n_pal = (MIN (image, mask) - 44) / 8;
  if (bpp <= 8)
    {
      int i, colours = 1 << bpp;
      if (n_pal >= colours || (n_pal > 0 && n_pal >= 16))
        for (i = 0; i < colours; i++)
          pal[i] = i < n_pal ? rgb_of ((unsigned int) spr[11 + 2 * i]) : 0;
      else if (bpp == 8)
        for (i = 0; i < 256; i++)
          pal[i] = default256 (i);
      else
        {
          /* the Wimp's colours, as Wimp_ReadPixTrans maps them */
          static const int map1[2] = { 0, 7 }, map2[4] = { 0, 2, 4, 7 };
          read_palette (wimp);
          for (i = 0; i < colours; i++)
            pal[i] = rgb_of (wimp[bpp == 1 ? map1[i] : bpp == 2 ? map2[i] : i]);
        }
    }

  pb = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, rows);
  if (pb == NULL)
    return NULL;
  px = gdk_pixbuf_get_pixels (pb);
  stride = gdk_pixbuf_get_rowstride (pb);
  for (y = 0; y < rows; y++)
    {
      const unsigned char *row = img + (size_t) y * words * 4;
      for (x = 0; x < width; x++)
        {
          int bit = first + x * bpp;
          guint32 c, v;
          int a = 255;
          guchar *p = px + y * stride + x * 4;

          if (bpp <= 8)
            {
              v = (row[bit >> 3] >> (bit & 7)) & ((1 << bpp) - 1);
              c = pal[v];
            }
          else if (bpp == 16)
            {
              v = row[bit >> 3] | row[(bit >> 3) + 1] << 8;     /* 1:5:5:5 &BGR */
              c = ((v & 31) * 255 / 31) << 16 | (((v >> 5) & 31) * 255 / 31) << 8 |
                  (((v >> 10) & 31) * 255 / 31);
            }
          else
            {
              v = row[bit >> 3] | row[(bit >> 3) + 1] << 8 | row[(bit >> 3) + 2] << 16;
              c = (v & 0xFF) << 16 | (v & 0xFF00) | (v >> 16 & 0xFF);   /* &BBGGRR */
            }
          if (msk)
            {
              if (!newfmt)
                {
                  const unsigned char *mrow = msk + (size_t) y * words * 4;
                  int m = bpp <= 8 ? (mrow[bit >> 3] >> (bit & 7)) & ((1 << bpp) - 1)
                                   : mrow[bit >> 3] | mrow[(bit >> 3) + 1];
                  a = m ? 255 : 0;
                }
              else if (alpha_mask)
                {
                  int mwords = (width * 8 + 31) / 32;
                  a = msk[(size_t) y * mwords * 4 + x];
                }
              else
                {
                  int mwords = (width + 31) / 32;
                  a = (msk[(size_t) y * mwords * 4 + (x >> 3)] >> (x & 7)) & 1 ? 255 : 0;
                }
            }
          p[0] = c >> 16;
          p[1] = c >> 8;
          p[2] = c;
          p[3] = a;
        }
    }
  return pb;
}

/**
 * gdk_riscos_wimp_sprite:
 * @name: a sprite in the Wimp's sprite pool, e.g. "optoff"
 *
 * Returns: (transfer full) (nullable): the sprite at screen pixel size,
 * or %NULL if the Wimp hasn't got it or it can't be read.
 */
GdkPixbuf *
gdk_riscos_wimp_sprite (const char *name)
{
  _kernel_swi_regs r;
  GdkPixbuf *pb, *scaled;
  int xeig = 1, yeig = 1, sx, sy, w, h;

  memset (&r, 0, sizeof r);
  r.r[0] = 24;                     /* select sprite: R2 -> it */
  r.r[2] = (int) name;
  if (_kernel_swi (Wimp_SpriteOp, &r, &r) != NULL || r.r[2] == 0)
    return NULL;
  pb = decode ((const int *) r.r[2], &xeig, &yeig);
  if (pb == NULL)
    return NULL;
  sx = xeig - screen_eig (4);
  sy = yeig - screen_eig (5);
  if (sx == 0 && sy == 0)
    return pb;
  w = gdk_pixbuf_get_width (pb);
  h = gdk_pixbuf_get_height (pb);
  w = sx >= 0 ? w << sx : MAX (1, w >> -sx);
  h = sy >= 0 ? h << sy : MAX (1, h >> -sy);
  scaled = gdk_pixbuf_scale_simple (pb, w, h, GDK_INTERP_NEAREST);
  g_object_unref (pb);
  return scaled;
}
