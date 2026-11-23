/*
 * pixelarea-fake.c - a part of a window shown from the program's own
 * pixels (gdk_riscos_window_set_pixel_area), as a browser shows its page:
 * plotted straight from them (or, in the other colour order, swapped into
 * a sprite of GDK's), clipped to the area, over what GTK draws, and
 * updated when the program says they changed.
 *   pixelarea-fake APPDIR match|swap
 * match: the pixels in the screen's order (TBGR here), swap: cairo's.
 * MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>

#define AW 120
#define AH 80
#define STRIDE (AW * 4 + 64)          /* rows padded, as a GPU's are */

static GtkWidget *win, *area;
static int stage, waits, drawn, failed, checks, swapmode;
static guint32 *pixels;
static guint32 mid_set = 1, edge_out = 1, mid_changed = 1, mid_after_gtk = 1, mid_cleared = 1, corner_changed = 1;
static int plots_direct;
static int overlay_calls, overlay_last_id, overlay_calls_after_unset;
static int wa_ok;

/* VideoOverlay_RedrawWindow, as GDK calls it */
static int
swi_hook (int no, _kernel_swi_regs *r, _kernel_oserror **e)
{
  if (no == 0x59CCA)
    {
      overlay_calls++;
      overlay_last_id = r->r[0];
      return 1;
    }
  return 0;
}

static void
check (int ok, const char *what)
{
  checks++;
  if (!ok)
    failed++;
  printf ("%s %d - %s\n", ok ? "ok" : "not ok", checks, what);
}

static void
add (int reason, int a, int b, int c)
{
  int *e = fake_wimp_script[fake_wimp_script_len++];
  e[0] = reason; e[1] = a; e[2] = b; e[3] = c;
}

static void
settle (void)
{
  int i;
  for (i = 0; i < 4; i++)
    {
      add (FAKE_SLEEP, 20, 0, 0);
      add (FAKE_NULL, 0, 0, 0);
    }
}

static gboolean
on_draw (GtkWidget *w, cairo_t *cr, gpointer d)
{
  drawn++;
  cairo_set_source_rgb (cr, 0, 1, 0);       /* GTK's drawing: green */
  cairo_paint (cr);
  return TRUE;
}

/* a colour in the buffer's order: cairo's (&00RRGGBB) or TBGR */
static guint32
px (int r, int g, int b)
{
  return swapmode ? (0xFF000000u | (r << 16) | (g << 8) | b) : (0xFF000000u | (b << 16) | (g << 8) | r);
}

static void
fill (int x0, int y0, int x1, int y1, guint32 c)
{
  int x, y;
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++)
      pixels[y * (STRIDE / 4) + x] = c;
}

/* a pixel of the area on the fake screen, &00BBGGRR (TBGR) */
static guint32
at (int ax, int ay)
{
  int i, best = -1, wx, wy, x, y;

  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle && (best < 0 || fake_windows[i].stack > fake_windows[best].stack))
      best = i;
  if (best < 0)
    return 1;
  wx = fake_windows[best].x0 >> 1;
  wy = 1024 - (fake_windows[best].y1 >> 1);
  gtk_widget_translate_coordinates (area, win, ax, ay, &x, &y);
  return fake_screen_pixel (wx + x, wy + y) & 0xFFFFFF;
}

static void
hook (int reason)
{
  cairo_rectangle_int_t a = { 0, 0, AW, AH };

  if (fake_wimp_script_pos () < fake_wimp_script_len - 1)
    return;
  switch (stage)
    {
    case 0:
      if (win && gtk_widget_get_mapped (win) && drawn > 0 && fake_plots > 0)
        {
          int before = fake_plots;
          fill (0, 0, AW, AH, px (0, 0, 255));                        /* blue */
          gdk_riscos_window_set_pixel_area (gtk_widget_get_window (area), &a, pixels,
                                            STRIDE, AW, AH, swapmode);
          plots_direct = fake_plots - before;
          mid_set = at (AW / 2, AH / 2);
          edge_out = at (AW + 2, AH / 2);
          /* new pixels in part of the same buffer */
          fill (0, 0, 20, 20, px (255, 0, 0));                        /* red corner */
          {
            cairo_rectangle_int_t d = { 0, 0, 20, 20 };
            cairo_region_t *dr = cairo_region_create_rectangle (&d);
            gdk_riscos_window_pixel_area_changed (gtk_widget_get_window (area), dr);
            cairo_region_destroy (dr);
          }
          corner_changed = at (5, 5);
          mid_changed = at (AW / 2, AH / 2);
          /* a video overlay attached to the window: RedrawWindow for each rectangle */
          gdk_riscos_window_set_overlay (gtk_widget_get_window (area), 1234);
          gdk_riscos_window_pixel_area_changed (gtk_widget_get_window (area), NULL);
          {
            int wx = 0, wy = 0, tx, ty;
            gtk_widget_translate_coordinates (area, win, 10, 5, &tx, &ty);
            wa_ok = gdk_riscos_window_to_work_area (gtk_widget_get_window (area), 10, 5, &wx, &wy) &&
                    wx == tx * 2 && wy == -ty * 2;
            printf ("# work area of (10,5) in the area: %d,%d (window %d,%d)\n", wx, wy, tx, ty);
          }
          stage = 1;
          gtk_widget_queue_draw (win);       /* GTK draws all of it again */
          settle ();
        }
      else if (++waits > 400)
        {
          stage = 99;
          gtk_main_quit ();
        }
      else
        add (FAKE_NULL, 0, 0, 0);
      return;
    case 1:
      mid_after_gtk = at (AW / 2, AH / 2);
      gdk_riscos_window_set_overlay (gtk_widget_get_window (area), 0);
      overlay_calls_after_unset = overlay_calls;
      gdk_riscos_window_pixel_area_changed (gtk_widget_get_window (area), NULL);
      gdk_riscos_window_set_pixel_area (gtk_widget_get_window (area), NULL, NULL, 0, 0, 0, FALSE);
      mid_cleared = at (AW / 2, AH / 2);
      stage = 2;
      gtk_main_quit ();
      add (FAKE_NULL, 0, 0, 0);
      return;
    default:
      add (FAKE_NULL, 0, 0, 0);
      return;
    }
}

int
main (int argc, char **argv)
{
  GtkWidget *box;

  if (argc < 3)
    return 2;
  swapmode = strcmp (argv[2], "swap") == 0;
  g_setenv ("FONTCONFIG_FILE", g_build_filename (argv[1], "fonts", "fonts.conf", NULL), TRUE);
  g_setenv ("XDG_DATA_DIRS", g_build_filename (argv[1], "share", NULL), TRUE);
  pixels = g_malloc0 (STRIDE * AH);
  fake_set_screen (1280, 1024, 0, 5);       /* TBGR */
  fake_wimp_nulls = 40;
  fake_wimp_hook = hook;
  fake_swi_hook = swi_hook;
  gtk_init (&argc, &argv);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Pixels");
  gtk_window_set_default_size (GTK_WINDOW (win), 300, 200);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Toolbar"), FALSE, FALSE, 0);
  area = gtk_drawing_area_new ();
  gtk_widget_set_size_request (area, AW, AH);
  gtk_widget_set_halign (area, GTK_ALIGN_START);
  gtk_widget_set_valign (area, GTK_ALIGN_START);
  g_signal_connect (area, "draw", G_CALLBACK (on_draw), NULL);
  gtk_box_pack_start (GTK_BOX (box), area, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (win), box);
  gtk_widget_show_all (win);
  gtk_main ();

  printf ("# %s (screen TBGR, &00BBGGRR): screen trgb %d; set: middle &%06X, beside &%06X; "
          "changed: corner &%06X, middle &%06X; after GTK redraws &%06X; cleared &%06X\n",
          argv[2], gdk_riscos_screen_is_trgb (), mid_set, edge_out, corner_changed, mid_changed,
          mid_after_gtk, mid_cleared);
  check (!gdk_riscos_screen_is_trgb (), "the fake screen is TBGR");
  check (mid_set == 0xFF0000 && plots_direct > 0, "the program's pixels are shown as soon as they're set (blue)");
  check (edge_out != 0xFF0000 && edge_out != 0, "beside the area: the window as GTK draws it (the pixels, rows padded, are clipped to the area)");
  check (corner_changed == 0x0000FF && mid_changed == 0xFF0000,
         "pixel_area_changed shows the changed part (red corner), the rest stays");
  check (mid_after_gtk == 0xFF0000, "GTK drawing the window again doesn't cover the area");
  check (mid_cleared == 0x00FF00, "with no pixels, GTK's drawing shows again");
  check (overlay_calls > 0 && overlay_last_id == 1234 && overlay_calls == overlay_calls_after_unset,
         "a set overlay gets VideoOverlay_RedrawWindow for the window's rectangles, and none once unset");
  check (wa_ok, "a point in the area in the top-level's work area (OS units, y up)");
  printf ("1..%d\n", checks);
  return failed;
}
