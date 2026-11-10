/*
 * simdtest.c - riscos-gtk: the NEON and ARMv6 SIMD code in pixman,
 * libpng and libjpeg-turbo gives what their C code gives.
 *
 * Draws with cairo (pixman: fills, blends, scaling, masks, 16 bpp),
 * decodes PNGs with libpng (its NEON row filters) with NEON on and off in
 * the same run, and saves and loads a JPEG with libjpeg-turbo. Prints a
 * hash of each result and how long it took. Run it twice, once with
 *   PIXMAN_DISABLE="arm-neon arm-simd" JSIMD_FORCENONE=1
 * and the hashes must match (build/build-linuxarm.sh does). In-process
 * checks (libpng on and off) print TAP. MIT licence.
 */
#include <cairo.h>
#include <gdk/gdk.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* libjpeg-turbo's (internal, but exported): 1 if NEON is used */
extern int jsimd_can_idct_islow (void);

static int n_tests, failures;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    failures++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
}

static unsigned
hash (const unsigned char *p, size_t n, unsigned h)
{
  while (n--)
    h = (h ^ *p++) * 16777619u;
  return h;
}

static unsigned
surface_hash (cairo_surface_t *s, unsigned v)
{
  int y, h = cairo_image_surface_get_height (s), stride = cairo_image_surface_get_stride (s);
  int row = cairo_image_surface_get_width (s) *
            (cairo_image_surface_get_format (s) == CAIRO_FORMAT_RGB16_565 ? 2 : 4);
  const unsigned char *d;

  cairo_surface_flush (s);
  d = cairo_image_surface_get_data (s);
  for (y = 0; y < h; y++)
    v = hash (d + (size_t) y * stride, row, v);
  return v;
}

static double
now (void)
{
  struct timespec t;
  clock_gettime (CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}

/* a picture with some of everything: gradients, alpha, odd widths */
static cairo_surface_t *
picture (int w, int h, cairo_format_t f)
{
  cairo_surface_t *s = cairo_image_surface_create (f, w, h);
  cairo_t *cr = cairo_create (s);
  cairo_pattern_t *g;
  int i;

  cairo_set_source_rgb (cr, 0.9, 0.85, 0.7);
  cairo_paint (cr);
  g = cairo_pattern_create_linear (0, 0, w, h);
  cairo_pattern_add_color_stop_rgba (g, 0, 1, 0, 0, 0.8);
  cairo_pattern_add_color_stop_rgba (g, 0.5, 0, 0.6, 1, 0.4);
  cairo_pattern_add_color_stop_rgba (g, 1, 0.1, 0.9, 0.2, 1);
  cairo_set_source (cr, g);
  cairo_rectangle (cr, 3, 5, w - 11, h - 7);
  cairo_fill (cr);
  cairo_pattern_destroy (g);
  for (i = 0; i < 40; i++)
    {
      cairo_set_source_rgba (cr, (i * 37 % 255) / 255.0, (i * 91 % 255) / 255.0,
                             (i * 53 % 255) / 255.0, 0.3 + (i % 7) / 10.0);
      cairo_arc (cr, (i * 41) % w, (i * 29) % h, 5 + i % 23, 0, 6.283);
      cairo_fill (cr);
    }
  cairo_destroy (cr);
  return s;
}

static unsigned
draw_test (double *secs)
{
  cairo_surface_t *src = picture (301, 203, CAIRO_FORMAT_ARGB32);
  cairo_surface_t *dst = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 517, 389);
  cairo_surface_t *d16 = cairo_image_surface_create (CAIRO_FORMAT_RGB16_565, 333, 211);
  cairo_surface_t *mask = cairo_image_surface_create (CAIRO_FORMAT_A8, 517, 389);
  cairo_t *cr;
  double t = now ();
  unsigned v;
  int i;

  cr = cairo_create (mask);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.6);
  cairo_arc (cr, 250, 190, 170, 0, 6.283);
  cairo_fill (cr);
  cairo_destroy (cr);

  for (i = 0; i < 20; i++)
    {
      cr = cairo_create (dst);
      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_paint (cr);
      /* copy, over, scaled (nearest and bilinear), through a mask */
      cairo_set_source_surface (cr, src, 7, 9);
      cairo_paint (cr);
      cairo_save (cr);
      cairo_scale (cr, 1.37, 1.21);
      cairo_set_source_surface (cr, src, 20, 15);
      cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_BILINEAR);
      cairo_paint_with_alpha (cr, 0.7);
      cairo_restore (cr);
      cairo_save (cr);
      cairo_scale (cr, 0.5, 0.5);
      cairo_set_source_surface (cr, src, 300, 300);
      cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_NEAREST);
      cairo_paint (cr);
      cairo_restore (cr);
      cairo_set_source_rgba (cr, 0.2, 0.3, 0.9, 0.5);
      cairo_mask_surface (cr, mask, 0, 0);
      cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
      cairo_set_source_rgba (cr, 0.1, 0.05, 0.0, 0.2);
      cairo_rectangle (cr, 13, 17, 401, 211);
      cairo_fill (cr);
      cairo_destroy (cr);

      cr = cairo_create (d16);
      cairo_set_source_surface (cr, src, -3, 1);
      cairo_paint (cr);
      cairo_set_source_rgba (cr, 1, 0.5, 0, 0.5);
      cairo_rectangle (cr, 50, 40, 201, 99);
      cairo_fill (cr);
      cairo_destroy (cr);
    }
  *secs = now () - t;
  v = surface_hash (dst, surface_hash (d16, 2166136261u));
  cairo_surface_destroy (src);
  cairo_surface_destroy (dst);
  cairo_surface_destroy (d16);
  cairo_surface_destroy (mask);
  return v;
}

/* ---- PNG: written by cairo (libpng picks a filter per row), read with
   libpng with its NEON row filters on and off ---- */
typedef struct { unsigned char *data; size_t len, pos; } Buf;

static cairo_status_t
write_buf (void *closure, const unsigned char *data, unsigned int length)
{
  Buf *b = closure;
  b->data = realloc (b->data, b->len + length);
  memcpy (b->data + b->len, data, length);
  b->len += length;
  return CAIRO_STATUS_SUCCESS;
}

static void
read_fn (png_structp p, png_bytep out, png_size_t n)
{
  Buf *b = png_get_io_ptr (p);
  if (b->pos + n > b->len)
    png_error (p, "short");
  memcpy (out, b->data + b->pos, n);
  b->pos += n;
}

static unsigned
png_decode (Buf *b, int neon, int strip_alpha, double *secs)
{
  png_structp p = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop info = png_create_info_struct (p);
  png_bytep *rows;
  unsigned v = 2166136261u;
  int y, h, rowbytes;
  double t = now ();

  b->pos = 0;
  if (setjmp (png_jmpbuf (p)))
    return 0;
#ifdef PNG_ARM_NEON
  png_set_option (p, PNG_ARM_NEON, neon ? PNG_OPTION_ON : PNG_OPTION_OFF);
#endif
  png_set_read_fn (p, b, read_fn);
  png_read_info (p, info);
  if (strip_alpha)
    png_set_strip_alpha (p);
  png_read_update_info (p, info);
  h = png_get_image_height (p, info);
  rowbytes = png_get_rowbytes (p, info);
  rows = malloc (h * sizeof *rows);
  for (y = 0; y < h; y++)
    rows[y] = malloc (rowbytes);
  png_read_image (p, rows);
  for (y = 0; y < h; y++)
    {
      v = hash (rows[y], rowbytes, v);
      free (rows[y]);
    }
  free (rows);
  png_destroy_read_struct (&p, &info, NULL);
  *secs = now () - t;
  return v;
}

static void
png_test (void)
{
  cairo_surface_t *s;
  Buf b4 = { 0 }, b3 = { 0 };
  double t1, t2;
  unsigned on, off;

  s = picture (641, 479, CAIRO_FORMAT_ARGB32);   /* 4 bytes a pixel */
  cairo_surface_write_to_png_stream (s, write_buf, &b4);
  cairo_surface_destroy (s);
  s = picture (639, 481, CAIRO_FORMAT_RGB24);    /* 3 bytes a pixel (RGB) */
  cairo_surface_write_to_png_stream (s, write_buf, &b3);
  cairo_surface_destroy (s);

  on = png_decode (&b4, 1, 0, &t1);
  off = png_decode (&b4, 0, 0, &t2);
  printf ("# png RGBA: %08x (NEON on, %.3f s), %08x (off, %.3f s)\n", on, t1, off, t2);
  check (on != 0 && on == off, "PNG, 4 bytes a pixel: the same with libpng's NEON on and off");
  on = png_decode (&b3, 1, 0, &t1);
  off = png_decode (&b3, 0, 0, &t2);
  printf ("# png RGB: %08x (NEON on, %.3f s), %08x (off, %.3f s)\n", on, t1, off, t2);
  check (on != 0 && on == off, "PNG, 3 bytes a pixel: the same with libpng's NEON on and off");
  free (b4.data);
  free (b3.data);
}

/* ---- JPEG: saved and loaded by gdk-pixbuf (libjpeg-turbo) ---- */
static unsigned
jpeg_test (double *secs)
{
  cairo_surface_t *s = picture (643, 487, CAIRO_FORMAT_RGB24);
  GdkPixbuf *pb = gdk_pixbuf_get_from_surface (s, 0, 0, 643, 487), *back;
  GdkPixbufLoader *l;
  gchar *buf = NULL;
  gsize len = 0;
  unsigned v = 0;
  double t = now ();
  int i;

  for (i = 0; i < 5; i++)
    {
      g_free (buf);
      gdk_pixbuf_save_to_buffer (pb, &buf, &len, "jpeg", NULL, "quality", "85", NULL);
    }
  v = hash ((const unsigned char *) buf, len, 2166136261u);
  for (i = 0; i < 5; i++)
    {
      l = gdk_pixbuf_loader_new_with_type ("jpeg", NULL);
      gdk_pixbuf_loader_write (l, (const guchar *) buf, len, NULL);
      gdk_pixbuf_loader_close (l, NULL);
      back = gdk_pixbuf_loader_get_pixbuf (l);
      if (i == 0 && back)
        v = hash (gdk_pixbuf_get_pixels (back),
                  (size_t) gdk_pixbuf_get_rowstride (back) * (gdk_pixbuf_get_height (back) - 1) +
                  gdk_pixbuf_get_width (back) * gdk_pixbuf_get_n_channels (back), v);
      g_object_unref (l);
    }
  *secs = now () - t;
  g_free (buf);
  g_object_unref (pb);
  cairo_surface_destroy (s);
  return v;
}

int
main (int argc, char **argv)
{
  double t;
  unsigned v;

  setvbuf (stdout, NULL, _IONBF, 0);
  printf ("# libjpeg-turbo NEON: %s\n", jsimd_can_idct_islow () ? "yes" : "no");
  v = draw_test (&t);
  printf ("draw %08x\n# cairo/pixman drawing: %.3f s\n", v, t);
  v = jpeg_test (&t);
  printf ("jpeg %08x\n# JPEG save and load: %.3f s\n", v, t);
  png_test ();
  printf ("1..%d\n", n_tests);
  return failures;
}
