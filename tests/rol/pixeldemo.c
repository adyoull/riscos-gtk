/*
 * pixeldemo.c - on RISC OS: a window whose lower part is shown from the
 * program's own pixels (gdk_riscos_window_set_pixel_area), as a browser
 * shows its page. A gradient in the screen's order or the other one
 * (PIXELDEMO_SWAP=1), rows padded to 64 bytes, in memory of its own; after
 * 2 s a red square is drawn into it and announced as changed.
 * Writes <Wimp$ScrapDir>.PixelDemoLog. MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <stdio.h>
#include <stdlib.h>

#define AW 320
#define AH 200

static GtkWidget *area;
static guint32 *pixels;
static int stride, swap;

static guint32
colour (int r, int g, int b)
{
  int trgb = gdk_riscos_screen_is_trgb () ^ swap;
  return trgb ? (0xFF000000u | (r << 16) | (g << 8) | b) : (0xFF000000u | (b << 16) | (g << 8) | r);
}

static gboolean
square (gpointer d)
{
  cairo_rectangle_int_t r = { 20, 20, 60, 60 };
  cairo_region_t *damage = cairo_region_create_rectangle (&r);
  int x, y;

  for (y = 20; y < 80; y++)
    for (x = 20; x < 80; x++)
      pixels[y * (stride / 4) + x] = colour (255, 0, 0);
  gdk_riscos_window_pixel_area_changed (gtk_widget_get_window (area), damage);
  cairo_region_destroy (damage);
  printf ("pixeldemo: red square drawn\n");
  fflush (stdout);
  return G_SOURCE_REMOVE;
}

static void
on_map (GtkWidget *w, gpointer d)
{
  cairo_rectangle_int_t a = { 0, 0, AW, AH };
  int x, y;

  for (y = 0; y < AH; y++)
    for (x = 0; x < AW; x++)
      pixels[y * (stride / 4) + x] = colour (x * 255 / AW, y * 255 / AH, 128);
  gdk_riscos_window_set_pixel_area (gtk_widget_get_window (area), &a, pixels, stride, AW, AH,
                                    gdk_riscos_screen_is_trgb () ^ swap);
  printf ("pixeldemo: pixel area set (screen %s, pixels %s, stride %d, at %p)\n",
          gdk_riscos_screen_is_trgb () ? "TRGB" : "TBGR",
          (gdk_riscos_screen_is_trgb () ^ swap) ? "TRGB" : "TBGR", stride, (void *) pixels);
  fflush (stdout);
  g_timeout_add (2000, square, NULL);
}

static gboolean
on_draw (GtkWidget *w, cairo_t *cr, gpointer d)
{
  cairo_set_source_rgb (cr, 0, 0.6, 0);      /* GTK's: green (shouldn't show) */
  cairo_paint (cr);
  return TRUE;
}

int
main (int argc, char **argv)
{
  GtkWidget *win, *box;

  freopen ("<Wimp$ScrapDir>.PixelDemoLog", "w", stdout);
  dup2 (fileno (stdout), 2);
  swap = getenv ("PIXELDEMO_SWAP") && atoi (getenv ("PIXELDEMO_SWAP"));
  stride = (AW * 4 + 63) & ~63;
  stride += 64;                              /* padded beyond the width */
  pixels = malloc (stride * AH);
  gtk_init (&argc, &argv);
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Pixel area");
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Above: GTK. Below: the program's pixels."), FALSE, FALSE, 4);
  area = gtk_drawing_area_new ();
  gtk_widget_set_size_request (area, AW, AH);
  g_signal_connect (area, "draw", G_CALLBACK (on_draw), NULL);
  g_signal_connect_after (area, "map", G_CALLBACK (on_map), NULL);
  gtk_box_pack_start (GTK_BOX (box), area, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_button_new_with_label ("A GTK button below"), FALSE, FALSE, 4);
  gtk_container_add (GTK_CONTAINER (win), box);
  g_signal_connect (win, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (win);
  gtk_main ();
  return 0;
}
