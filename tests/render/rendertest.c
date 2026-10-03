/*
 * rendertest.c - M1 test for riscos-gtk's drawing stack: fontconfig,
 * FreeType, harfbuzz, fribidi, pango, cairo, pixman and gdk-pixbuf, the
 * libraries GTK 3 draws every widget with.
 *
 * Built for RISC OS (!RenderTest) and for arm-linux under
 * qemu-arm-aligntrap, like glibtest. Prints TAP; exit status = failures.
 *
 *   rendertest DIR      writes DIR/rendertest.png (the text and shapes it
 *                       drew, to look at) and a JPEG it reads back
 *
 * Needs FONTCONFIG_FILE pointing at a fonts.conf with DejaVu Sans in it
 * (the RISC OS app and build-linuxarm.sh both set one up).
 *
 * MIT licence (see LICENSE).
 */
#include <pango/pangocairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gstdio.h>
#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <string.h>

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

/* ARGB32 pixel at x, y (cairo's native-endian premultiplied word). */
static guint32
pixel (cairo_surface_t *s, int x, int y)
{
  cairo_surface_flush (s);
  unsigned char *d = cairo_image_surface_get_data (s);
  int stride = cairo_image_surface_get_stride (s);
  guint32 v;
  memcpy (&v, d + y * stride + x * 4, 4);   /* aligned anyway; keeps the checker quiet */
  return v;
}

static int
count_dark (cairo_surface_t *s, int x0, int y0, int x1, int y1)
{
  int n = 0;
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++)
      if ((pixel (s, x, y) & 0xff) < 0x80)   /* blue channel dark */
        n++;
  return n;
}

static int
text_width (PangoContext *ctx, const char *text)
{
  PangoLayout *l = pango_layout_new (ctx);
  PangoFontDescription *fd = pango_font_description_from_string ("DejaVu Sans 20");
  pango_layout_set_font_description (l, fd);
  pango_layout_set_text (l, text, -1);
  int w, h;
  pango_layout_get_size (l, &w, &h);
  pango_font_description_free (fd);
  g_object_unref (l);
  return w;
}

static void
test_fontconfig (void)
{
  check (FcInit (), "fontconfig: FcInit with FONTCONFIG_FILE");
  FcPattern *p = FcNameParse ((const FcChar8 *) "sans-serif");
  FcConfigSubstitute (NULL, p, FcMatchPattern);
  FcDefaultSubstitute (p);
  FcResult r;
  FcPattern *m = FcFontMatch (NULL, p, &r);
  FcChar8 *family = NULL, *file = NULL;
  if (m)
    {
      FcPatternGetString (m, FC_FAMILY, 0, &family);
      FcPatternGetString (m, FC_FILE, 0, &file);
    }
  check (family && strcmp ((const char *) family, "DejaVu Sans") == 0,
         "fontconfig: sans-serif is DejaVu Sans");
  printf ("# font file: %s\n", file ? (const char *) file : "(none)");
  if (m)
    FcPatternDestroy (m);
  FcPatternDestroy (p);
}

static void
test_text (cairo_surface_t *s, PangoContext *ctx)
{
  cairo_t *cr = cairo_create (s);
  PangoLayout *l = pango_cairo_create_layout (cr);
  PangoFontDescription *fd = pango_font_description_from_string ("Sans 20");
  pango_layout_set_font_description (l, fd);
  /* Latin-1 and beyond: Ä ö ß, an em dash, the Euro sign */
  pango_layout_set_text (l, "GTK on RISC OS \xe2\x80\x94 \xc3\x84\xc3\xb6\xc3\x9f \xe2\x82\xac", -1);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_move_to (cr, 10, 10);
  pango_cairo_show_layout (cr, l);
  PangoRectangle ink, logical;
  pango_layout_get_pixel_extents (l, &ink, &logical);
  printf ("# text ink %dx%d at %d,%d; logical %dx%d\n", ink.width, ink.height,
          ink.x, ink.y, logical.width, logical.height);
  check (ink.width > 200 && ink.width < 380 && ink.height > 15 && ink.height < 40,
         "pango: text extents plausible for 20 pt DejaVu Sans");
  int dark = count_dark (s, 10, 10, 10 + logical.width, 10 + logical.height);
  check (dark > 500, "cairo+FreeType: glyphs drawn");
  printf ("# %d dark pixels in the text box\n", dark);
  check (count_dark (s, 0, 60, 400, 70) == 0, "cairo: nothing drawn below the text");
  /* missing glyphs show as hex boxes: pango reports them as unknown */
  check (pango_layout_get_unknown_glyphs_count (l) == 0, "pango: every character has a glyph");

  /* harfbuzz: DejaVu Sans kerns "AV" (GPOS) */
  int wa = text_width (ctx, "A"), wv = text_width (ctx, "V"), wav = text_width (ctx, "AV");
  check (wav < wa + wv, "harfbuzz: AV is kerned");
  printf ("# A=%d V=%d AV=%d (pango units)\n", wa, wv, wav);

  /* fribidi: Hebrew is right to left */
  check (pango_find_base_dir ("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d", -1) == PANGO_DIRECTION_RTL,
         "fribidi: Hebrew text is RTL");
  check (pango_find_base_dir ("RISC OS", -1) == PANGO_DIRECTION_LTR, "fribidi: Latin text is LTR");

  pango_font_description_free (fd);
  g_object_unref (l);
  cairo_destroy (cr);
}

static void
test_shapes (cairo_surface_t *s)
{
  cairo_t *cr = cairo_create (s);
  /* Wimp grey 4 (&BBBBBB) slab, as the theme draws buttons */
  cairo_set_source_rgb (cr, 0xbb / 255.0, 0xbb / 255.0, 0xbb / 255.0);
  cairo_rectangle (cr, 10, 70, 60, 30);
  cairo_fill (cr);
  check ((pixel (s, 40, 85) & 0xffffff) == 0xbbbbbb, "cairo: solid fill exact (Wimp grey 4)");
  /* antialiased circle edge: partially covered pixel is in between */
  cairo_set_source_rgb (cr, 0, 0, 1);
  cairo_arc (cr, 120, 85, 15.5, 0, 2 * G_PI);
  cairo_fill (cr);
  /* the circle's top is at y = 69.5, so pixel row 69 is half covered */
  guint32 c = pixel (s, 120, 85), e = pixel (s, 120, 69);
  check ((c & 0xffffff) == 0x0000ff, "cairo: circle centre blue");
  check ((e & 0xff0000) > 0 && (e & 0xff0000) < 0xff0000, "cairo/pixman: antialiased edge");
  /* a scaled pattern: pixman's bilinear path. 2x2 checker scaled to 40x40 */
  cairo_surface_t *chk = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 2, 2);
  cairo_t *c2 = cairo_create (chk);
  cairo_set_source_rgb (c2, 1, 1, 1);
  cairo_paint (c2);
  cairo_set_source_rgb (c2, 0, 0, 0);
  cairo_rectangle (c2, 0, 0, 1, 1);
  cairo_rectangle (c2, 1, 1, 1, 1);
  cairo_fill (c2);
  cairo_destroy (c2);
  cairo_save (cr);
  cairo_translate (cr, 160, 70);
  cairo_scale (cr, 20, 20);
  cairo_set_source_surface (cr, chk, 0, 0);
  cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_BILINEAR);
  cairo_paint (cr);
  cairo_restore (cr);
  /* source pixel centres land at 170,80 (black) and 190,80 (white); the
     middle, 180,90, is the bilinear mix of all four */
  guint32 tl = pixel (s, 170, 80), tr = pixel (s, 190, 80), mid = pixel (s, 180, 90);
  check ((tl & 0xff) < 0x20 && (tr & 0xff) > 0xe0, "pixman: scaled pattern at the source pixel centres");
  check ((mid & 0xff) > 0x40 && (mid & 0xff) < 0xc0, "pixman: bilinear blend in the middle");
  cairo_surface_destroy (chk);
  cairo_destroy (cr);
}

static void
test_pixbuf (cairo_surface_t *s, const char *dir)
{
  char *png = g_build_filename (dir, "rendertest.png", NULL);
  check (cairo_surface_write_to_png (s, png) == CAIRO_STATUS_SUCCESS, "cairo: write PNG");
  GError *err = NULL;
  GdkPixbuf *pb = gdk_pixbuf_new_from_file (png, &err);
  check (pb && gdk_pixbuf_get_width (pb) == 400 && gdk_pixbuf_get_height (pb) == 110,
         "gdk-pixbuf: PNG loads at 400x110");
  if (err)
    printf ("# %s\n", err->message);
  g_clear_error (&err);
  if (pb)
    {
      /* compare a few pixels with the surface (opaque, so no premultiply) */
      int n = gdk_pixbuf_get_n_channels (pb), rs = gdk_pixbuf_get_rowstride (pb);
      const guchar *px = gdk_pixbuf_read_pixels (pb);
      gboolean same = TRUE;
      int pts[][2] = { { 40, 85 }, { 120, 85 }, { 165, 75 }, { 15, 20 }, { 300, 100 } };
      for (unsigned i = 0; i < G_N_ELEMENTS (pts); i++)
        {
          guint32 sp = pixel (s, pts[i][0], pts[i][1]);
          const guchar *p = px + pts[i][1] * rs + pts[i][0] * n;
          if (p[0] != ((sp >> 16) & 0xff) || p[1] != ((sp >> 8) & 0xff) || p[2] != (sp & 0xff))
            same = FALSE;
        }
      check (same, "gdk-pixbuf: PNG pixels match what cairo drew");

      char *jpg = g_build_filename (dir, "rendertest.jpg", NULL);
      gboolean saved = gdk_pixbuf_save (pb, jpg, "jpeg", &err, "quality", "90", NULL);
      check (saved, "gdk-pixbuf: save JPEG");
      g_clear_error (&err);
      GdkPixbuf *pj = gdk_pixbuf_new_from_file (jpg, &err);
      check (pj && gdk_pixbuf_get_width (pj) == 400, "gdk-pixbuf: JPEG loads back");
      g_clear_error (&err);
      g_clear_object (&pj);
      g_remove (jpg);
      g_free (jpg);
      g_object_unref (pb);
    }
  printf ("# picture: %s\n", png);
  g_free (png);

  /* built-in loaders */
  GSList *formats = gdk_pixbuf_get_formats ();
  GString *names = g_string_new (NULL);
  static const char *const wanted[] = { "png", "jpeg", "gif", "bmp", "ico", "xpm" };
  unsigned found = 0;
  int n_formats = 0;
  for (GSList *f = formats; f; f = f->next)
    {
      char *name = gdk_pixbuf_format_get_name (f->data);
      g_string_append_printf (names, " %s", name);
      n_formats++;
      for (unsigned i = 0; i < G_N_ELEMENTS (wanted); i++)
        if (!strcmp (name, wanted[i]))
          found |= 1u << i;
      g_free (name);
    }
  g_slist_free (formats);
  check (found == (1u << G_N_ELEMENTS (wanted)) - 1, "gdk-pixbuf: png jpeg gif bmp ico xpm loaders built in");
  printf ("# %d formats registered\n", n_formats);
  printf ("# loaders:%s\n", names->str);
  g_string_free (names, TRUE);
}

int
main (int argc, char **argv)
{
  const char *dir = argc > 1 ? argv[1] : g_get_tmp_dir ();
  printf ("# rendertest: cairo %s, pango %s, gdk-pixbuf %s, FONTCONFIG_FILE=%s\n",
          cairo_version_string (), pango_version_string (), GDK_PIXBUF_VERSION,
          g_getenv ("FONTCONFIG_FILE") ? g_getenv ("FONTCONFIG_FILE") : "(unset)");
  test_fontconfig ();

  cairo_surface_t *s = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 400, 110);
  cairo_t *cr = cairo_create (s);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);
  cairo_destroy (cr);
  check (cairo_surface_status (s) == CAIRO_STATUS_SUCCESS, "cairo: ARGB32 image surface");

  PangoFontMap *fm = pango_cairo_font_map_get_default ();
  PangoContext *ctx = pango_font_map_create_context (fm);
  gint64 t0 = g_get_monotonic_time ();
  test_text (s, ctx);
  printf ("# first text layout and draw took %d ms (includes font loading)\n",
          (int) ((g_get_monotonic_time () - t0) / 1000));
  test_shapes (s);
  test_pixbuf (s, dir);
  g_object_unref (ctx);
  cairo_surface_destroy (s);
  printf ("1..%d\n# rendertest: %d checks, %d failed\n", n_tests, n_tests, n_failed);
  return n_failed;
}
