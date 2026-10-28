/* svgtest.c - riscos-gtk: SVG pictures (build/build-svg.sh).
 * Built twice: linked with riscos-gtk-svg (librsvg and its GdkPixbuf
 * loader, built in), and with -DNO_SVG, without it, where gdk-pixbuf
 * must have no SVG loader and say so cleanly.
 *   svgtest DIR [FONTCONFIG_FILE]
 * MIT licence. */
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>
#include <stdio.h>
#include <string.h>
#ifndef NO_SVG
#include <librsvg/rsvg.h>
#endif

static int n_tests, n_failed;

static void
check (gboolean ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
  fflush (stdout);
}

/* 100 x 50: the left half red, a blue circle in the right half, a
   gradient strip and a black H along the bottom */
static const char svg[] =
  "<?xml version=\"1.0\"?>\n"
  "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"50\" viewBox=\"0 0 100 50\">\n"
  " <defs><linearGradient id=\"g\" x1=\"0\" x2=\"1\">"
  "<stop offset=\"0\" stop-color=\"#000\"/><stop offset=\"1\" stop-color=\"#fff\"/></linearGradient></defs>\n"
  " <rect x=\"0\" y=\"0\" width=\"50\" height=\"40\" fill=\"#ff0000\"/>\n"
  " <circle cx=\"75\" cy=\"20\" r=\"15\" style=\"fill:#0000ff\"/>\n"
  " <rect x=\"0\" y=\"40\" width=\"60\" height=\"10\" fill=\"url(#g)\"/>\n"
  " <text x=\"64\" y=\"50\" font-family=\"DejaVu Sans\" font-size=\"14\" font-weight=\"bold\">H</text>\n"
  "</svg>\n";

static const guchar *
at (GdkPixbuf *pb, int x, int y)
{
  return gdk_pixbuf_read_pixels (pb) + y * gdk_pixbuf_get_rowstride (pb) + x * gdk_pixbuf_get_n_channels (pb);
}

static gboolean
near (const guchar *p, int r, int g, int b)
{
  return ABS (p[0] - r) < 24 && ABS (p[1] - g) < 24 && ABS (p[2] - b) < 24;
}

int
main (int argc, char **argv)
{
  const char *dir = argc > 1 ? argv[1] : g_get_tmp_dir ();
  if (argc > 2)
    g_setenv ("FONTCONFIG_FILE", argv[2], TRUE);
  char *file = g_build_filename (dir, "svgtest.svg", NULL);
  check (g_file_set_contents (file, svg, -1, NULL), "write the SVG file");

  gboolean have = FALSE;
  GSList *formats = gdk_pixbuf_get_formats ();
  for (GSList *l = formats; l; l = l->next)
    if (strcmp (gdk_pixbuf_format_get_name (l->data), "svg") == 0)
      have = TRUE;
  g_slist_free (formats);

  GError *err = NULL;
  GdkPixbuf *pb = gdk_pixbuf_new_from_file (file, &err);
#ifdef NO_SVG
  check (!have, "without riscos-gtk-svg: gdk-pixbuf has no SVG loader");
  check (pb == NULL && err != NULL && err->code == GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
         "without riscos-gtk-svg: an SVG file is an unknown type, not a crash");
  if (err)
    printf ("# %s\n", err->message);
#else
  check (have, "gdk-pixbuf lists the SVG loader");
  check (pb && gdk_pixbuf_get_width (pb) == 100 && gdk_pixbuf_get_height (pb) == 50,
         "gdk_pixbuf_new_from_file: an SVG at its own size, 100x50");
  if (err)
    printf ("# %s\n", err->message);
  g_clear_error (&err);
  if (pb)
    {
      check (near (at (pb, 10, 10), 255, 0, 0) && near (at (pb, 75, 20), 0, 0, 255)
             && at (pb, 95, 2)[3] == 0,
             "its pixels: red, blue, and transparent outside the shapes");
      check (at (pb, 2, 45)[0] < 40 && at (pb, 57, 45)[0] > 200,
             "the gradient goes from black to white");
      int dark = 0;
      for (int y = 38; y < 50; y++)
        for (int x = 63; x < 76; x++)
          if (at (pb, x, y)[3] > 200 && at (pb, x, y)[0] < 80)
            dark++;
      printf ("# text: %d dark pixels\n", dark);
      check (dark > 15, "the text is drawn (pango, from fontconfig)");
      g_object_unref (pb);
    }

  /* a size of its own: what icon themes do with scalable icons */
  pb = gdk_pixbuf_new_from_file_at_size (file, 48, 48, &err);
  check (pb && gdk_pixbuf_get_width (pb) == 48 && gdk_pixbuf_get_height (pb) == 24
         && near (at (pb, 5, 5), 255, 0, 0) && near (at (pb, 36, 10), 0, 0, 255),
         "gdk_pixbuf_new_from_file_at_size: drawn at 48x24, not scaled up from a bitmap");
  g_clear_error (&err);
  g_clear_object (&pb);

  /* by stream, as GtkImage and the loaders' users read files */
  GdkPixbufLoader *ld = gdk_pixbuf_loader_new_with_type ("svg", &err);
  gboolean ok = ld && gdk_pixbuf_loader_write (ld, (const guchar *) svg, sizeof svg - 1, &err)
                && gdk_pixbuf_loader_close (ld, &err);
  pb = ok ? gdk_pixbuf_loader_get_pixbuf (ld) : NULL;
  check (pb && gdk_pixbuf_get_width (pb) == 100, "GdkPixbufLoader of type svg");
  g_clear_error (&err);
  g_clear_object (&ld);

  /* librsvg itself, as GOffice and AbiWord use it */
  RsvgHandle *h = rsvg_handle_new_from_data ((const guint8 *) svg, sizeof svg - 1, &err);
  RsvgDimensionData d = { 0 };
  if (h)
    rsvg_handle_get_dimensions (h, &d);
  check (h && d.width == 100 && d.height == 50, "rsvg_handle_new_from_data, its size");
  if (h)
    {
      cairo_surface_t *s = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 200, 100);
      cairo_t *cr = cairo_create (s);
      cairo_scale (cr, 2, 2);
      check (rsvg_handle_render_cairo (h, cr), "rsvg_handle_render_cairo");
      cairo_destroy (cr);
      cairo_surface_flush (s);
      guint32 *px = (guint32 *) cairo_image_surface_get_data (s);
      int stride = cairo_image_surface_get_stride (s) / 4;
      check (px[20 * stride + 20] == 0xffff0000 && px[40 * stride + 150] == 0xff0000ff,
             "drawn on cairo, scaled 2x: red and blue where they should be");
      cairo_surface_destroy (s);
      g_object_unref (h);
    }
  g_clear_error (&err);

  /* a broken file: an error, not a crash */
  h = rsvg_handle_new_from_data ((const guint8 *) "<svg", 4, &err);
  check (h == NULL && err != NULL, "a broken SVG gives an error");
  g_clear_error (&err);
  g_clear_object (&h);
#endif
  g_free (file);
  printf ("1..%d\n# svgtest%s: %d checks, %d failed\n", n_tests,
#ifdef NO_SVG
          "-nosvg",
#else
          "",
#endif
          n_tests, n_failed);
  return n_failed;
}
