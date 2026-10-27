/*
 * fontchooser-fake.c - GTK's font dialogue on the fake RISC OS: when
 * another font is chosen, the preview (a GtkEntry drawn with the font as
 * a Pango attribute) shows it on the screen.
 *   fontchooser-fake APPDIR OUTDIR SERIF.ttf
 * APPDIR's fonts (DejaVu Sans) plus SERIF.ttf (DejaVu Serif), in
 * OUTDIR's own fonts.conf. The dialogue opens on DejaVu Sans 28; then:
 *   1. DejaVu Serif Book is chosen in the list (its row made the
 *      cursor, as a click or the arrow keys do);
 *   2. gtk_font_chooser_set_font: DejaVu Sans 28 again.
 * Then AbiWord's way of drawing its font dialogue's preview: a drawing
 * area painted with gdk_cairo_create outside GTK's paint cycle, when a
 * setting changes; it must reach the screen too.
 * Each time the preview's pixels on the fake screen are compared with
 * the entry drawn by GTK itself (gtk_widget_draw), and with the pixels
 * before. FC_SHOTS=1 saves OUTDIR/shot-*.ppm. Prints TAP. MIT licence.
 */
#include <gtk/gtk.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int n_tests, n_failed;
static const char *outdir;
static GtkWidget *dialog, *preview;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
  fflush (stdout);
}

static void
shot (const char *name)
{
  char path[512];
  FILE *f;
  int x, y;

  if (!getenv ("FC_SHOTS"))
    return;
  g_snprintf (path, sizeof path, "%s/shot-%s.ppm", outdir, name);
  if (!(f = fopen (path, "wb")))
    return;
  fprintf (f, "P6\n%d %d\n255\n", 1280, 1024);
  for (y = 0; y < 1024; y++)
    for (x = 0; x < 1280; x++)
      {
        unsigned int p = fake_bank_pixel (1, x, y);
        fputc (p & 0xFF, f);
        fputc ((p >> 8) & 0xFF, f);
        fputc ((p >> 16) & 0xFF, f);
      }
  fclose (f);
}

static void
find_widgets (GtkWidget *w, gpointer data)
{
  GtkWidget **tv = data;

  if (GTK_IS_ENTRY (w) && !GTK_IS_SEARCH_ENTRY (w) && !GTK_IS_SPIN_BUTTON (w) && gtk_widget_get_mapped (w) && !preview)
    preview = w;
  if (GTK_IS_TREE_VIEW (w) && !*tv)
    *tv = w;
  if (GTK_IS_CONTAINER (w))
    gtk_container_forall (GTK_CONTAINER (w), find_widgets, data);
}

/* the screen's pixels in the entry's text area: how many differ from GTK
   drawing the entry now; *ink: the dark ones; *sum: a checksum */
static int
measure (int *ink, guint32 *sum)
{
  GtkAllocation a;
  cairo_surface_t *s;
  cairo_t *cr;
  int ox, oy, x, y, diff = 0, wx, wy, stride;
  guint32 *px;

  gtk_widget_get_allocation (preview, &a);
  s = cairo_image_surface_create (CAIRO_FORMAT_RGB24, a.width, a.height);
  cr = cairo_create (s);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);
  gtk_widget_draw (preview, cr);
  cairo_destroy (cr);
  cairo_surface_flush (s);
  px = (guint32 *) cairo_image_surface_get_data (s);
  stride = cairo_image_surface_get_stride (s) / 4;
  gdk_window_get_origin (gtk_widget_get_window (dialog), &ox, &oy);
  gtk_widget_translate_coordinates (preview, dialog, 0, 0, &wx, &wy);
  *ink = 0;
  *sum = 0;
  for (y = 4; y < a.height - 4; y++)
    for (x = 4; x < a.width - 4; x++)
      {
        unsigned int sp = fake_bank_pixel (1, ox + wx + x, oy + wy + y);
        guint32 gp = px[y * stride + x];
        int dr = abs ((int) (sp & 0xFF) - (int) ((gp >> 16) & 0xFF));
        int dg = abs ((int) ((sp >> 8) & 0xFF) - (int) ((gp >> 8) & 0xFF));
        int db = abs ((int) ((sp >> 16) & 0xFF) - (int) (gp & 0xFF));
        if (dr > 48 || dg > 48 || db > 48)
          diff++;
        if ((sp & 0xFF) < 100)
          (*ink)++;
        *sum = *sum * 31 + (sp & 0xFFFFFF);
      }
  cairo_surface_destroy (s);
  return diff;
}

static int diff0 = -1, diff1 = -1, diff2 = -1, ink0, ink1, ink2;
static guint32 sum0, sum1, sum2;
static char *font1, *font2;
static GtkWidget *tree;

static GtkWidget *area_win, *area;
static int outside_before = -1, outside_after = -1, updates_before;

/* the area's middle pixel on the screen: 1 if dark */
static int
area_dark (void)
{
  int ox, oy, wx, wy;
  gdk_window_get_origin (gtk_widget_get_window (area_win), &ox, &oy);
  gtk_widget_translate_coordinates (area, area_win, 0, 0, &wx, &wy);
  return (fake_bank_pixel (1, ox + wx + 50, oy + wy + 30) & 0xFF) < 100;
}

static gboolean
step5 (gpointer data)
{
  (void) data;
  outside_after = area_dark ();
  shot ("outside");
  printf ("# drawn outside the paint cycle: the area is %s on the screen (%d Wimp_UpdateWindow calls since)\n",
          outside_after ? "dark" : "still light", fake_update_calls - updates_before);
  gtk_widget_destroy (area_win);
  gtk_main_quit ();
  return FALSE;
}

static gboolean
step4 (gpointer data)
{
  cairo_t *cr;

  (void) data;
  outside_before = area_dark ();
  updates_before = fake_update_calls;
  /* as AbiWord's GR_UnixCairoGraphics::_beginPaint does outside a draw
     handler */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  cr = gdk_cairo_create (gtk_widget_get_window (area));
  G_GNUC_END_IGNORE_DEPRECATIONS
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_paint (cr);
  cairo_surface_flush (cairo_get_target (cr));
  cairo_destroy (cr);
  g_timeout_add (500, step5, NULL);
  return FALSE;
}

static gboolean
step3 (gpointer data)
{
  (void) data;
  diff2 = measure (&ink2, &sum2);
  font2 = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (dialog));
  shot ("set");
  printf ("# set_font: %s; %d pixels differ, %d inked\n", font2, diff2, ink2);
  gtk_widget_destroy (dialog);
  /* a window with a drawing area (white) */
  area_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  area = gtk_drawing_area_new ();
  gtk_widget_set_size_request (area, 200, 100);
  gtk_container_add (GTK_CONTAINER (area_win), area);
  gtk_widget_show_all (area_win);
  g_timeout_add (800, step4, NULL);
  return FALSE;
}

static gboolean
step2 (gpointer data)
{
  (void) data;
  diff1 = measure (&ink1, &sum1);
  font1 = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (dialog));
  shot ("chosen");
  printf ("# chosen in the list: %s; %d pixels differ, %d inked\n", font1, diff1, ink1);
  gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), "DejaVu Sans 28");
  g_timeout_add (800, step3, NULL);
  return FALSE;
}

static gboolean
step1 (gpointer data)
{
  GtkTreeModel *m;
  GtkTreeIter it;
  gboolean ok;

  (void) data;
  find_widgets (dialog, &tree);
  if (!preview || !tree)
    {
      printf ("# no preview or list\n");
      gtk_main_quit ();
      return FALSE;
    }
  diff0 = measure (&ink0, &sum0);
  shot ("open");
  printf ("# open: %d pixels differ, %d inked\n", diff0, ink0);
  /* DejaVu Serif Book's row */
  m = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
  for (ok = gtk_tree_model_get_iter_first (m, &it); ok; ok = gtk_tree_model_iter_next (m, &it))
    {
      char *title = NULL;
      gtk_tree_model_get (m, &it, 3 /* PREVIEW_TITLE_COLUMN */, &title, -1);
      if (title && strcmp (title, "DejaVu Serif Book") == 0)
        {
          GtkTreePath *p = gtk_tree_model_get_path (m, &it);
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree), p, NULL, FALSE);
          gtk_tree_path_free (p);
          g_free (title);
          break;
        }
      g_free (title);
    }
  g_timeout_add (800, step2, NULL);
  return FALSE;
}

int
main (int argc, char **argv)
{
  const char *appdir;
  char *fonts, *conf, *text;
  int i;

  if (argc < 4)
    return 2;
  appdir = argv[1];
  outdir = argv[2];
  fonts = g_build_filename (outdir, "fonts", NULL);
  g_mkdir_with_parents (fonts, 0755);
  for (i = 3; i < argc; i++)
    {
      char *data, *dst, *base;
      gsize len;
      if (!g_file_get_contents (argv[i], &data, &len, NULL))
        continue;
      base = g_path_get_basename (argv[i]);
      dst = g_build_filename (fonts, base, NULL);
      g_file_set_contents (dst, data, len, NULL);
      g_free (dst); g_free (base); g_free (data);
    }
  text = g_strdup_printf ("<?xml version=\"1.0\"?>\n<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n"
                          "<fontconfig><dir>%s/fonts</dir><dir>%s</dir><cachedir>%s/cache</cachedir></fontconfig>\n",
                          appdir, fonts, outdir);
  conf = g_build_filename (outdir, "fonts.conf", NULL);
  g_file_set_contents (conf, text, -1, NULL);
  g_setenv ("FONTCONFIG_FILE", conf, TRUE);
  g_setenv ("XDG_DATA_DIRS", g_build_filename (appdir, "share", NULL), TRUE);
  g_unsetenv ("UnixFC$Dir");
  g_setenv ("RISCOS_GTK_RISCOS_FONTS", "0", TRUE);

  fake_set_screen (1280, 1024, 0, 5);
  fake_wimp_nulls = 1;
  /* time for the steps: null events 20 ms apart */
  for (i = 0; i < 400; i++)
    {
      fake_wimp_script[fake_wimp_script_len][0] = FAKE_SLEEP;
      fake_wimp_script[fake_wimp_script_len][1] = 20;
      fake_wimp_script_len++;
    }
  gtk_init (&argc, &argv);
  dialog = gtk_font_chooser_dialog_new ("Font", NULL);
  gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), "DejaVu Sans 28");
  gtk_widget_show (dialog);
  g_timeout_add (1200, step1, NULL);
  gtk_main ();

  check (diff0 >= 0 && diff0 < 20 && ink0 > 50, "the font dialogue's preview is on the screen as GTK drew it");
  check (font1 && strstr (font1, "DejaVu Serif") && diff1 >= 0 && diff1 < 20 && sum1 != sum0,
         "DejaVu Serif chosen in the list: the preview on the screen changes to it");
  check (font2 && strstr (font2, "DejaVu Sans") && diff2 >= 0 && diff2 < 20 && sum2 != sum1,
         "gtk_font_chooser_set_font (DejaVu Sans): the preview on the screen changes back");
  check (outside_before == 0 && outside_after == 1,
         "drawing on a window outside GTK's paint cycle (gdk_cairo_create, as AbiWord's previews do) reaches the screen");
  printf ("1..%d\n", n_tests);
  return n_failed;
}
