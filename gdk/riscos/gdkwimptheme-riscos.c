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
 * gdk_riscos_wimp_tool_sprite () reads the window border sprites the same
 * way, from the Wimp's tool sprite area.
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
  r.r[2] = 0x45555254;             /* "TRUE": true colours (else the bottom
                                      4 bits of each component are 0) */
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

/* GDK's scale factor: 2 in an EX0 EY0 (180 dpi) mode, else 1;
   RISCOS_GTK_SCALE=1 or 2 overrides (gdkriscos-server.c's read_screen) */
int
_gdk_riscos_scale_factor (void)
{
  const char *e = g_getenv ("RISCOS_GTK_SCALE");

  if (e && (e[0] == '1' || e[0] == '2') && e[1] == 0)
    return e[0] - '0';
  return screen_eig (4) == 0 && screen_eig (5) == 0 ? 2 : 1;
}

/* OS units per GDK pixel, as an eigen factor: the screen's, plus one at
   GDK scale 2. Sprites are made this size for GTK, which draws in GDK
   pixels. */
/* set while making a sprite at GDK scale 2's own resolution (the
   screen's pixels): gdk_riscos_wimp_sprite_scaled */
static int hires;

static int
gdk_eig (int var)
{
  return screen_eig (var) + (_gdk_riscos_server_get_scale () == 2) - hires;
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

/* A tool translation table (RISC OS 5 Wimp, Doc/ToolTables) for the
   sprite being decoded: 256 colours (&RRGGBB) for an 8bpp sprite without a
   palette, or NULL */
static const guint32 *tool_table;

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
      if (n_pal >= colours)
        for (i = 0; i < colours; i++)
          pal[i] = rgb_of ((unsigned int) spr[11 + 2 * i]);
      else if (bpp == 8 && n_pal == 0 && tool_table)
        /* a theme's 8bpp tool sprite without a palette (the Raspberry
           theme's): its pixels index the Wimp's translation table */
        for (i = 0; i < 256; i++)
          pal[i] = tool_table[i];
      else if (bpp == 8 && n_pal >= 16)
        /* an old-style 256-colour palette: 16 entries for the low 4 bits
           of a colour, the high 4 bits adding red (bit 4), green (bits
           5, 6) and blue (bit 7) as the standard 256-colour palette does
           (VIDC1's 256-colour modes) */
        for (i = 0; i < 256; i++)
          {
            guint32 b = rgb_of ((unsigned int) spr[11 + 2 * (i & 15)]);
            int r = (b >> 16) & 0xFF, g = (b >> 8) & 0xFF, bl = b & 0xFF;
            r = MIN (255, r + ((i & 0x10) ? 0x88 : 0));
            g = MIN (255, g + ((i & 0x20) ? 0x44 : 0) + ((i & 0x40) ? 0x88 : 0));
            bl = MIN (255, bl + ((i & 0x80) ? 0x88 : 0));
            pal[i] = r << 16 | g << 8 | bl;
          }
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

/* A decoded sprite scaled to GDK pixels (screen pixels, or 90 dpi pixels
   at GDK scale 2) */
static GdkPixbuf *
to_screen (const int *spr)
{
  GdkPixbuf *pb, *scaled;
  int xeig = 1, yeig = 1, sx, sy, w, h;

  pb = decode (spr, &xeig, &yeig);
  if (pb == NULL)
    return NULL;
  sx = xeig - gdk_eig (4);
  sy = yeig - gdk_eig (5);
  if (sx == 0 && sy == 0)
    return pb;
  w = gdk_pixbuf_get_width (pb);
  h = gdk_pixbuf_get_height (pb);
  w = sx >= 0 ? w << sx : MAX (1, w >> -sx);
  h = sy >= 0 ? h << sy : MAX (1, h >> -sy);
  /* enlarged: pixel for pixel; reduced (a 180 dpi sprite at GDK scale 2):
     averaged */
  scaled = gdk_pixbuf_scale_simple (pb, w, h, sx < 0 || sy < 0 ? GDK_INTERP_BILINEAR : GDK_INTERP_NEAREST);
  g_object_unref (pb);
  return scaled;
}

static gboolean readable (const void *start, size_t len);
static gboolean header_ok (const int *area);
static const int *area_find (const int *area, const char *name);

#ifndef Wimp_BaseOfSprites
#define Wimp_BaseOfSprites 0x400EA
#endif

static gboolean
debugging (void)
{
  return g_getenv ("RISCOS_GTK_DEBUG") != NULL;
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
  const int *areas[2] = { NULL, NULL }, *spr = NULL;
  int i;

  /* the pool is two sprite areas (Wimp_BaseOfSprites: R0 the ROM part,
     R1 the RAM part, where sprites loaded later, a theme's, override);
     searched here, then Wimp_SpriteOp 24 if that finds nothing */
  memset (&r, 0, sizeof r);
  if (_kernel_swi (Wimp_BaseOfSprites, &r, &r) == NULL)
    {
      areas[0] = (const int *) r.r[1];
      areas[1] = (const int *) r.r[0];
    }
  for (i = 0; i < 2 && spr == NULL; i++)
    if (header_ok (areas[i]))
      spr = area_find (areas[i], name);
  if (spr == NULL)
    {
      _kernel_oserror *e;
      memset (&r, 0, sizeof r);
      r.r[0] = 24;                 /* select sprite: R2 -> it */
      r.r[2] = (int) name;
      e = _kernel_swi (Wimp_SpriteOp, &r, &r);
      if (e == NULL && r.r[2] != 0 && r.r[2] != (int) name)
        spr = (const int *) r.r[2];
      else if (debugging ())
        g_printerr ("riscos-gtk: Wimp sprite %s: not in the pool (RAM area &%X %s, ROM area &%X %s);"
                    " Wimp_SpriteOp 24: %s\n", name,
                    (unsigned int) (gsize) areas[0], header_ok (areas[0]) ? "ok" : "unreadable",
                    (unsigned int) (gsize) areas[1], header_ok (areas[1]) ? "ok" : "unreadable",
                    e ? e->errmess : "no sprite");
    }
  if (spr == NULL)
    return NULL;
  if (debugging ())
    g_printerr ("riscos-gtk: Wimp sprite %.12s: %d words x %d rows, bits %d-%d, mode &%X,"
                " palette %d, %s\n", (const char *) &spr[1], spr[4] + 1, spr[5] + 1, spr[6], spr[7],
                (unsigned int) spr[10], (MIN (spr[8], spr[9]) - 44) / 8,
                spr[8] == spr[9] ? "no mask" : "mask");
  {
    GdkPixbuf *pb = to_screen (spr);
    if (pb == NULL && debugging ())
      g_printerr ("riscos-gtk: Wimp sprite %s: couldn't decode it\n", name);
    return pb;
  }
}

/* ---- the tool sprites (window furniture) ---------------------------------

   The Wimp keeps the window border sprites (*ToolSprites; names in the
   PRMs' "Naming Convention": uicon, vbarmid, vwelltcap...) in their own
   sprite area, not the sprite pool, so Wimp_SpriteOp can't reach them.
   Wimp_ReadSysInfo 9 gives that area's address (the 2026 PRM text lists
   no reasons; the area is checked before it's used: readable, a sane
   sprite area header, and an "uicon" sprite in it). */

#ifndef Wimp_ReadSysInfo
#define Wimp_ReadSysInfo 0x400F2
#endif
#ifndef OS_ValidateAddress
#define OS_ValidateAddress 0x3A
#endif
#ifndef OS_Memory
#define OS_Memory 0x68
#endif

static gboolean
readable (const void *start, size_t len)
{
  _kernel_swi_regs r;
  int carry = 1;

  /* not a range that wraps past the top of memory (a word from the tool
     block taken as a pointer: OS_ValidateAddress faults on it on a Pi,
     seen on the Pi with 0.10), nor one in the bottom page */
  if ((gsize) start < 0x4000 || len <= 0 || len > (64 << 20) ||
      (gsize) start + len < (gsize) start || (gsize) start + len > 0xFFFFFFFFu)
    return FALSE;
  /* OS_Memory 24: readable in user mode (bit 0). OS_ValidateAddress only
     says memory is there: on a Pi the Wimp's block holds words that are
     addresses of privileged memory, which it passes and reading aborts
     (seen on the Pi with 0.10 and 0.11: EMT trap in block_find) */
  r.r[0] = 24;
  r.r[1] = (int) start;
  r.r[2] = (int) start + (int) len;
  if (_kernel_swi (OS_Memory, &r, &r) == NULL)
    return (r.r[1] & 1) != 0;
  /* older RISC OS: no OS_Memory 24 */
  r.r[0] = (int) start;
  r.r[1] = (int) start + (int) len;
  if (_kernel_swi_c (OS_ValidateAddress, &r, &r, &carry) != NULL)
    return FALSE;
  return carry == 0;
}

/* The sprite called name in the area (names are 12 characters, case
   ignored), or NULL; every step checked against the area's bounds */
static const int *
area_find (const int *area, const char *name)
{
  int count = area[1], off = area[2], free_off = area[3], i;

  for (i = 0; i < count && off >= 16 && off + 44 <= free_off; i++)
    {
      const int *spr = (const int *) ((const char *) area + off);
      if (spr[0] < 44 || off + spr[0] > free_off)
        return NULL;
      if (g_ascii_strncasecmp ((const char *) &spr[1], name, 12) == 0)
        return spr;
      off += spr[0];
    }
  return NULL;
}

/* A readable sprite area with a sane header */
static gboolean
header_ok (const int *area)
{
  if (area == NULL || ((unsigned int) (gsize) area & 3) || !readable (area, 16))
    return FALSE;
  if (area[0] < 16 || area[0] > (64 << 20) || area[1] < 0 || area[1] > 65536 ||
      area[2] < 16 || area[3] < area[2] || area[3] > area[0])
    return FALSE;
  return readable (area, area[3]);
}


/* For the log: what's at p (the first words, if readable) */
static void
describe (const char *what, const int *p)
{
  if (p && !((unsigned int) (gsize) p & 3) && readable (p, 32))
    g_printerr ("riscos-gtk: %s &%X: %08X %08X %08X %08X %08X; as a sprite, name \"%.12s\";"
                " readable to free: %s\n",
                what, (unsigned int) (gsize) p, (unsigned int) p[0], (unsigned int) p[1],
                (unsigned int) p[2], (unsigned int) p[3], (unsigned int) p[4], (const char *) &p[1],
                p[3] > 0 && p[3] < (64 << 20) && readable (p, p[3]) ? "yes" : "no");
  else
    g_printerr ("riscos-gtk: %s &%X: not readable\n", what, (unsigned int) (gsize) p);
}

/* Does spr look like a sprite (header values in range)? */
static gboolean
looks_like_sprite (const int *spr)
{
  return spr[0] >= 44 && spr[0] < (1 << 20) && spr[4] >= 0 && spr[4] < 256 &&
         spr[5] >= 0 && spr[5] < 1024 && spr[6] >= 0 && spr[6] < 32 && spr[7] >= 0 &&
         spr[7] < 32 && spr[8] >= 44 && spr[9] >= 44 && spr[8] < spr[0] && spr[9] <= spr[0];
}

/* The Wimp's tool sprites aren't always a plain sprite area: on a
   RISC OS 5 Pi, Wimp_ReadSysInfo 9's block starts like one (size, count
   90) but its first/free words aren't offsets into it. So a sprite is
   found four ways: a plain area; by name anywhere in the block (the
   header 4 bytes before the name); through a table of pointers to the
   sprites after the 16-byte header; or through a table of pointers to
   the sprites at the address in the block's fifth word (on the Pi its
   +16 and +20 both point to a table of addresses near each other). */
static const int *tool_block;
static int tool_how;                  /* 1 area, 2 scan, 3 pointer table */

static const int *
block_find (const int *block, int how, const char *name)
{
  int size = block[0], count = block[1], off, i;

  if (how == 1)
    return area_find (block, name);
  if (how == 2)
    {
      for (off = 16; off + 48 <= size; off += 4)
        {
          const int *spr = (const int *) ((const char *) block + off);
          if (g_ascii_strncasecmp ((const char *) &spr[1], name, 12) == 0 &&
              (strlen (name) == 12 || ((const char *) &spr[1])[strlen (name)] == 0) &&
              looks_like_sprite (spr) && off + spr[0] <= size)
            return spr;
        }
      return NULL;
    }
  if (how == 4)
    {
      /* +16 points to a table of pointers to the sprites (count of them) */
      const int *table = (const int *) block[4];
      if (table == NULL || ((gsize) table & 3) || count <= 0 || count > 4096 ||
          !readable (table, count * 4))
        return NULL;
      for (i = 0; i < count; i++)
        {
          const int *spr = (const int *) table[i];
          if (spr == NULL || ((gsize) spr & 3) || !readable (spr, 48))
            continue;
          if (g_ascii_strncasecmp ((const char *) &spr[1], name, 12) == 0 &&
              (strlen (name) == 12 || ((const char *) &spr[1])[strlen (name)] == 0) &&
              looks_like_sprite (spr) && readable (spr, spr[0]))
            return spr;
        }
      return NULL;
    }
  for (i = 0; i < count && 16 + 4 * i + 4 <= size; i++)
    {
      const int *spr = (const int *) block[4 + i];
      if (spr == NULL || ((gsize) spr & 3) || !readable (spr, 48))
        continue;
      if (g_ascii_strncasecmp ((const char *) &spr[1], name, 12) == 0 &&
          (strlen (name) == 12 || ((const char *) &spr[1])[strlen (name)] == 0) &&
          looks_like_sprite (spr) && readable (spr, spr[0]))
        return spr;
    }
  return NULL;
}

static gboolean
try_block (const int *block)
{
  int how;

  if (block == NULL || ((gsize) block & 3) || !readable (block, 16) ||
      block[0] < 16 || block[0] > (16 << 20) || !readable (block, block[0]))
    return FALSE;
  for (how = 1; how <= 4; how++)
    {
      if (how == 1 && !header_ok (block))
        continue;
      if (block_find (block, how, "uicon") || block_find (block, how, "uicon22"))
        {
          tool_block = block;
          tool_how = how;
          return TRUE;
        }
    }
  return FALSE;
}

static gboolean
tool_area (void)
{
  static gboolean looked;
  _kernel_swi_regs r;

  if (looked)
    return tool_block != NULL;
  looked = TRUE;
  memset (&r, 0, sizeof r);
  r.r[0] = 9;
  if (_kernel_swi (Wimp_ReadSysInfo, &r, &r) != NULL)
    return FALSE;
  if (!try_block ((const int *) r.r[0]))
    try_block ((const int *) r.r[1]);
  if (debugging ())
    {
      g_printerr ("riscos-gtk: tool sprites: Wimp_ReadSysInfo 9 = &%X, &%X: %s\n",
                  (unsigned int) r.r[0], (unsigned int) r.r[1],
                  tool_how == 1 ? "a sprite area" : tool_how == 2 ? "sprites found by name in the block" :
                  tool_how == 3 ? "a table of sprite pointers" :
                  tool_how == 4 ? "a table of sprite pointers at +16's address" : "not found");
      if (tool_block == NULL)
        {
          const int *p = (const int *) r.r[0];
          describe ("Wimp_ReadSysInfo 9 R0", p);
          describe ("Wimp_ReadSysInfo 9 R1", (const int *) r.r[1]);
          if (p && !((gsize) p & 3) && readable (p, 64))
            {
              int i;
              g_printerr ("riscos-gtk: R0 +16:");
              for (i = 4; i < 16; i++)
                g_printerr (" %08X", (unsigned int) p[i]);
              g_printerr ("\n");
              if (!((gsize) p[4] & 3) && readable ((const int *) p[4], 48))
                {
                  const int *t = (const int *) p[4];
                  describe ("R0+16 points to", t);
                  for (i = 0; i < 3; i++)
                    if (!((gsize) t[i] & 3) && readable ((const int *) t[i], 64))
                      {
                        char what[32];
                        g_snprintf (what, sizeof what, "table entry %d", i);
                        describe (what, (const int *) t[i]);
                      }
                }
            }
        }
    }
  return tool_block != NULL;
}

/* A sprite by name in the memory the tool sprites occupy: when the Wimp
   gives a table of pointers (to the sprites it knows by name), other
   sprites from the same Tools file (the translation tables) aren't in the
   table but lie among them, the file having been loaded as one area. */
static const int *
span_find (const char *name)
{
  const char *lo = NULL, *hi = NULL, *p;
  static const char *const known[] = { "uicon", "dicon", "vwellt", "vbarmid", "sicon", "bicon",
                                       "uicon22", "dicon22", "vwellt22", "vbarmid22", "sicon22" };
  size_t i, len = strlen (name);

  if (tool_how == 1 || tool_how == 2)
    return NULL;                  /* searched already: the whole area or block */
  for (i = 0; i < G_N_ELEMENTS (known); i++)
    {
      const int *spr = block_find (tool_block, tool_how, known[i]);
      if (spr == NULL)
        continue;
      if (lo == NULL || (const char *) spr < lo)
        lo = (const char *) spr;
      if (hi == NULL || (const char *) spr + spr[0] > hi)
        hi = (const char *) spr + spr[0];
    }
  if (lo == NULL)
    return NULL;
  /* the tables may come before or after the window sprites in the file */
  lo = (gsize) lo > 0x8000 + 64 * 1024 ? lo - 64 * 1024 : lo;
  hi = (gsize) hi < 0xFFFFFFFFu - 64 * 1024 ? hi + 64 * 1024 : hi;
  while (!readable (lo, 4096) && lo < hi)
    lo += 4096;
  for (p = (const char *) ((gsize) lo & ~(gsize) 3); p + 48 <= hi; p += 4)
    {
      const int *spr;
      if (((gsize) p & 4095) == 0 && !readable (p, 4096))
        break;
      spr = (const int *) p;
      if (g_ascii_strncasecmp (p + 4, name, 12) == 0 && (len == 12 || p[4 + len] == 0) &&
          looks_like_sprite (spr) && readable (spr, spr[0]))
        return spr;
    }
  return NULL;
}

/* RISC OS 5's tool translation tables (the Wimp's Doc/ToolTables): a
   theme's 8bpp tool sprites may have no palette; their pixels are then
   looked up in a table for the Wimp colour the part is drawn in, kept in
   the Tools file as a sprite "table_<colour>" whose image is 256 words
   (a 16 x 16 32bpp sprite: &00BBGGRR each). Without a table for that
   colour, table_2 is used. The parts' colours are a document window's
   (gdkriscos-server.c: title background 2, scroll bar outer 3, inner 1);
   the slider is the scroll bar's inner part, the well and arrows its outer. */
static gboolean
tool_translation (const char *name, guint32 table[256])
{
  int colour = 2, i;
  const int *t = NULL;
  char tn[16];

  if (strstr (name, "bar") != NULL)                  /* vbart, vbarmid, phbarl... */
    colour = 1;
  else if (strstr (name, "well") != NULL || g_str_has_suffix (name, "icon")) /* well, arrows */
    colour = 3;
  if (!strcmp (name, "sicon") || !strcmp (name, "bicon") || !strcmp (name, "cicon") ||
      !strcmp (name, "ticon") || !strcmp (name, "iicon"))
    colour = 2;                                      /* window gadgets: the title bar's */
  g_snprintf (tn, sizeof tn, "table_%d", colour);
  t = block_find (tool_block, tool_how, tn);
  if (t == NULL)
    t = span_find (tn);
  if (t == NULL && colour != 2)
    {
      t = block_find (tool_block, tool_how, "table_2");
      if (t == NULL)
        t = span_find ("table_2");
    }
  if (t == NULL || t[8] < 44 || (t[0] - t[8]) < 256 * 4)
    return FALSE;
  t = (const int *) ((const char *) t + t[8]);
  for (i = 0; i < 256; i++)
    {
      unsigned int w = (unsigned int) t[i];        /* &00BBGGRR */
      table[i] = (w & 0xFF) << 16 | (w & 0xFF00) | ((w >> 16) & 0xFF);
    }
  return TRUE;
}

/**
 * gdk_riscos_wimp_tool_sprite:
 * @name: a window border (tool) sprite, e.g. "uicon" or "vbarmid"
 *
 * Returns: (transfer full) (nullable): the sprite at screen pixel size,
 * or %NULL.
 */
GdkPixbuf *
gdk_riscos_wimp_tool_sprite (const char *name)
{
  const int *spr = NULL;
  char full[16];
  const char *suffix;

  if (!tool_area ())
    return NULL;
  /* A Tools file holds a set per resolution, named with the screen's
     sprite suffix: "uicon22" for 90 x 90 dpi (EIG 1 x 1), "uicon11" for
     180 dpi; the plain name is the 90 x 45 dpi set, or the only one. */
  suffix = gdk_eig (4) == 1 && gdk_eig (5) == 1 ? "22" :
           gdk_eig (4) == 0 && gdk_eig (5) == 0 ? "11" : "";
  if (*suffix)
    {
      g_snprintf (full, sizeof full, "%s%s", name, suffix);
      spr = block_find (tool_block, tool_how, full);
    }
  if (spr == NULL)
    spr = block_find (tool_block, tool_how, name);
  if (spr == NULL)
    return NULL;
  {
    guint32 table[256];
    GdkPixbuf *pb;
    gboolean tabled = tool_translation (name, table);

    if (g_getenv ("RISCOS_GTK_DEBUG"))
      g_printerr ("riscos-gtk: tool sprite %.12s: %d words x %d rows, bits %d-%d, mode &%X,"
                  " palette %d, %s%s\n", (const char *) &spr[1], spr[4] + 1, spr[5] + 1, spr[6], spr[7],
                  (unsigned int) spr[10], (MIN (spr[8], spr[9]) - 44) / 8,
                  spr[8] == spr[9] ? "no mask" : "mask", tabled ? ", translation table" : "");
    tool_table = tabled ? table : NULL;
    pb = to_screen (spr);
    tool_table = NULL;
    return pb;
  }
}

/**
 * gdk_riscos_scale_factor:
 *
 * GDK's scale factor on this desktop: 2 in an EX0 EY0 (180 dpi) mode,
 * where GTK lays out in 90 dpi pixels and draws at the screen's
 * resolution; otherwise 1.
 */
int
gdk_riscos_scale_factor (void)
{
  return _gdk_riscos_server_get_scale ();
}

/**
 * gdk_riscos_wimp_sprite_scaled:
 * @name: a sprite in the Wimp's pool
 * @scale: 1, or 2 for GDK scale 2's resolution
 *
 * As gdk_riscos_wimp_sprite(), but @scale times as many pixels each way
 * when @scale is GDK's scale factor: a 180 dpi sprite at its own size, a
 * 90 dpi one doubled pixel for pixel (as the Wimp shows it). For a CSS
 * -gtk-scaled() image's second picture.
 */
GdkPixbuf *
gdk_riscos_wimp_sprite_scaled (const char *name, int scale)
{
  GdkPixbuf *pb;
  int old = hires;

  hires = scale == 2 && _gdk_riscos_server_get_scale () == 2;
  pb = gdk_riscos_wimp_sprite (name);
  hires = old;
  return pb;
}

/**
 * gdk_riscos_wimp_tool_sprite_scaled:
 *
 * As gdk_riscos_wimp_sprite_scaled() for a window border (tool) sprite:
 * at scale 2 the Tools file's 180 dpi set ("uicon11") if it has one.
 */
GdkPixbuf *
gdk_riscos_wimp_tool_sprite_scaled (const char *name, int scale)
{
  GdkPixbuf *pb;
  int old = hires;

  hires = scale == 2 && _gdk_riscos_server_get_scale () == 2;
  pb = gdk_riscos_wimp_tool_sprite (name);
  hires = old;
  return pb;
}
