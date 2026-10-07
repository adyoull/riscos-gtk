/*
 * printvec-fake.c - riscos-gtk: cairo_riscos_page_vectors (cairo/,
 * riscos-gtk's addition to cairo), which splits a printed page into what
 * the printer driver gets as Draw paths and what it gets as images.
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <cairo.h>
#include <cairo-riscos.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static int n_tests, failures;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    failures++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
}

static int moves, lines, curves, closes, fills, strokes, even_odd;
static double fill_rgb[8][3], stroke_width, dash0, dash_off;
static int n_dashes, stroke_cap, stroke_join;
static double first_x, first_y;

static void
mv (void *c, double x, double y)
{
  if (moves++ == 0)
    {
      first_x = x;
      first_y = y;
    }
}
static void ln (void *c, double x, double y) { lines++; }
static void cv (void *c, double a, double b, double d, double e, double f, double g) { curves++; }
static void cl (void *c) { closes++; }

static void
fl (void *c, double r, double g, double b, int eo)
{
  if (fills < 8)
    {
      fill_rgb[fills][0] = r;
      fill_rgb[fills][1] = g;
      fill_rgb[fills][2] = b;
    }
  fills++;
  even_odd += eo;
}

static void
st (void *c, double r, double g, double b, double width, int cap, int join, double miter,
    const double *dashes, int n, double off)
{
  strokes++;
  stroke_width = width;
  stroke_cap = cap;
  stroke_join = join;
  n_dashes = n;
  dash0 = n ? dashes[0] : 0;
  dash_off = off;
}

static const cairo_riscos_page_funcs_t funcs = { mv, ln, cv, cl, fl, st };

static int
region_has (cairo_region_t *r, int x, int y, int w, int h)
{
  cairo_rectangle_int_t rect = { x, y, w, h };
  return cairo_region_contains_rectangle (r, &rect) == CAIRO_REGION_OVERLAP_IN;
}

int
main (void)
{
  cairo_rectangle_t paper = { 0, 0, 595, 842 };
  cairo_surface_t *page = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA, &paper);
  cairo_t *cr = cairo_create (page);
  cairo_pattern_t *grad;
  cairo_region_t *images;
  double dash = 3;

  /* 1: a solid red rectangle, even-odd */
  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
  cairo_rectangle (cr, 10, 20, 100, 50);
  cairo_fill (cr);
  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_WINDING);

  /* 2: a dashed blue curve, scaled by 2: width 2 -> 4, dash 3 -> 6 */
  cairo_save (cr);
  cairo_scale (cr, 2, 2);
  cairo_set_source_rgb (cr, 0, 0, 1);
  cairo_set_line_width (cr, 2);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_BEVEL);
  cairo_set_dash (cr, &dash, 1, 1);
  cairo_move_to (cr, 20, 100);
  cairo_curve_to (cr, 40, 80, 60, 120, 80, 100);
  cairo_stroke (cr);
  cairo_restore (cr);

  /* 3: a gradient: an image */
  grad = cairo_pattern_create_linear (300, 300, 400, 300);
  cairo_pattern_add_color_stop_rgb (grad, 0, 0, 1, 0);
  cairo_pattern_add_color_stop_rgb (grad, 1, 0, 0, 1);
  cairo_set_source (cr, grad);
  cairo_rectangle (cr, 300, 300, 100, 100);
  cairo_fill (cr);
  cairo_pattern_destroy (grad);

  /* 4: solid, but over the gradient: in the image, not a vector */
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_rectangle (cr, 320, 320, 20, 20);
  cairo_fill (cr);

  /* 5: half transparent: an image */
  cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
  cairo_rectangle (cr, 100, 600, 50, 50);
  cairo_fill (cr);

  /* 6: clipped to a circle: an image */
  cairo_save (cr);
  cairo_arc (cr, 450, 700, 30, 0, 2 * M_PI);
  cairo_clip (cr);
  cairo_set_source_rgb (cr, 0, 1, 0);
  cairo_paint (cr);
  cairo_restore (cr);

  cairo_destroy (cr);

  images = cairo_riscos_page_vectors (page, &funcs, NULL);
  check (images != NULL, "the page is read");
  check (fills == 1 && fill_rgb[0][0] == 1 && fill_rgb[0][1] == 0 && even_odd == 1 &&
         fabs (first_x - 10) < 0.01 && fabs (first_y - 20) < 0.01,
         "the solid rectangle is a vector fill: red, even-odd, from 10,20 (points, y down)");
  check (strokes == 1 && curves >= 1 && fabs (stroke_width - 4) < 0.01 && n_dashes == 1 &&
         fabs (dash0 - 6) < 0.01 && fabs (dash_off - 2) < 0.01 && stroke_cap == 1 && stroke_join == 2,
         "the dashed curve is a vector stroke: width and dashes scaled (4, 6 from 2), round caps, bevel joins");
  check (images && region_has (images, 300, 300, 100, 100),
         "the gradient is in the image region");
  check (images && region_has (images, 100, 600, 50, 50), "the half-transparent fill is in it");
  check (images && region_has (images, 420, 670, 60, 60), "the circle-clipped paint is in it");
  check (images && !region_has (images, 10, 20, 100, 50) && fills == 1,
         "the rectangle isn't, and the solid square over the gradient isn't a vector (the image covers it)");
  printf ("# %d fills, %d strokes (width %.2f, dash %.2f), %d moves, %d lines, %d curves, %d closes;"
          " image rectangles %d\n", fills, strokes, stroke_width, dash0, moves, lines, curves, closes,
          images ? cairo_region_num_rectangles (images) : -1);
  if (images)
    cairo_region_destroy (images);
  cairo_surface_destroy (page);
  printf ("1..%d\n", n_tests);
  return failures;
}
