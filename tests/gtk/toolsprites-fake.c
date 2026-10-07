/*
 * toolsprites-fake.c - riscos-gtk: reading a theme's window border (tool)
 * sprites the way the Raspberry theme stores them, on the fake Wimp.
 *
 * The RISC OS 5 Raspberry theme's tool sprites are 8bpp without a palette.
 * The Wimp colours them through translation tables kept in the same Tools
 * file as sprites named "table_<Wimp colour>" (the Wimp's Doc/ToolTables:
 * 256 words, &00BBGGRR; table_2 when there's none for a colour). On a Pi
 * Wimp_ReadSysInfo 9 gives a table of pointers to the sprites the Wimp
 * knows by name, which the tables aren't in; they lie beside them.
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <gdk/gdk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>

static int n_tests, failures;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    failures++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
}

static int area[16384];
static int ptrs[16];

/* an 8bpp sprite, no palette, no mask, 16 x 8 pixels all of value v */
static int *
add8 (int *at, const char *name, int v)
{
  int words = 4, rows = 8;
  at[0] = 44 + words * rows * 4;
  memset (&at[1], 0, 12);
  strncpy ((char *) &at[1], name, 12);
  at[4] = words - 1;
  at[5] = rows - 1;
  at[6] = 0;
  at[7] = 31;
  at[8] = at[9] = 44;
  at[10] = (int) ((4u << 27) | (90u << 14) | (90u << 1) | 1u);
  memset (&at[11], v, words * rows * 4);
  return at + at[0] / 4;
}

/* a 16 x 16 32bpp table sprite: entry i grey, except the ones given */
static int *
add_table (int *at, const char *name, int i1, unsigned c1, int i2, unsigned c2)
{
  int i;
  at[0] = 44 + 256 * 4;
  memset (&at[1], 0, 12);
  strncpy ((char *) &at[1], name, 12);
  at[4] = 15;
  at[5] = 15;
  at[6] = 0;
  at[7] = 31;
  at[8] = at[9] = 44;
  at[10] = (int) ((6u << 27) | (90u << 14) | (90u << 1) | 1u);
  for (i = 0; i < 256; i++)
    at[11 + i] = i | i << 8 | i << 16;
  at[11 + i1] = (int) c1;
  at[11 + i2] = (int) c2;
  return at + at[0] / 4;
}

static unsigned
pixel (GdkPixbuf *pb)
{
  const guchar *p;
  if (pb == NULL)
    return 0xFFFFFFFF;
  p = gdk_pixbuf_get_pixels (pb);
  return (unsigned) p[0] << 16 | p[1] << 8 | p[2];
}

int
main (void)
{
  int *p = area + 1024, n = 0;
  GdkPixbuf *up, *bar, *grip;
  unsigned u, b, g;

  /* the Pi's layout: a 64-byte block, the count, two words that aren't
     offsets, then +16/+20 a pointer to a table of sprite pointers */
  ptrs[n++] = (int) (long) p; p = add8 (p, "uicon", 5);
  ptrs[n++] = (int) (long) p; p = add8 (p, "dicon", 5);
  ptrs[n++] = (int) (long) p; p = add8 (p, "vwellt", 5);
  ptrs[n++] = (int) (long) p; p = add8 (p, "vbarmid", 7);
  ptrs[n++] = (int) (long) p; p = add8 (p, "sicon", 9);
  /* the tables: in the file, not in the Wimp's table */
  p = add_table (p, "table_2", 5, 0x00336699, 9, 0x00ABCDEF);
  p = add_table (p, "table_1", 7, 0x00445566, 5, 0x00000000);
  area[0] = 64;
  area[1] = n;
  area[2] = 0x0042F3A4;
  area[3] = 0x00438124;
  area[4] = area[5] = (int) (long) ptrs;
  area[8] = (int) 0xFFFFFFF0;     /* not a pointer: OS_ValidateAddress would wrap */
  area[9] = 0x10;                 /* nor this: the bottom page */
  fake_tool_area_override = area;

  up = gdk_riscos_wimp_tool_sprite ("uicon");
  bar = gdk_riscos_wimp_tool_sprite ("vbarmid");
  grip = gdk_riscos_wimp_tool_sprite ("sicon");
  u = pixel (up);
  b = pixel (bar);
  g = pixel (grip);
  check (u == 0x996633, "an arrow (scroll bar outer, colour 3, no table_3) is coloured by table_2");
  check (b == 0x665544, "the slider (scroll bar inner, colour 1) is coloured by table_1");
  check (g == 0xEFCDAB, "the size icon (a window gadget, colour 2) is coloured by table_2");
  printf ("# uicon &%06X, vbarmid &%06X, sicon &%06X\n", u, b, g);
  printf ("1..%d\n", n_tests);
  return failures;
}
