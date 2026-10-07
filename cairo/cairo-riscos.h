/* cairo-riscos.h - riscos-gtk: printing a page recorded by cairo through
 * RISC OS's printer drivers as vectors (the Draw module) where it can be,
 * as images where it can't.
 *
 * Added to cairo 1.18.0 by riscos-gtk (patches/cairo-1.18.0-riscos.diff;
 * the code is in riscos-gtk's cairo/). LGPL-2.1-or-later or MPL-1.1, as
 * cairo. Copyright (C) 2026 Andrew Youll. */
#ifndef CAIRO_RISCOS_H
#define CAIRO_RISCOS_H

#include "cairo.h"

CAIRO_BEGIN_DECLS

/* A page's vector operations, in the recording's units (points for a
   printed page; y down), as paths: move_to/line_to/curve_to/close_path,
   then fill or stroke to draw what's been given since the last one. */
typedef struct {
  void (*move_to)    (void *closure, double x, double y);
  void (*line_to)    (void *closure, double x, double y);
  void (*curve_to)   (void *closure, double x1, double y1, double x2, double y2,
                      double x3, double y3);
  void (*close_path) (void *closure);
  /* fill: red, green, blue 0..1; even_odd or non-zero winding */
  void (*fill)       (void *closure, double r, double g, double b, int even_odd);
  /* stroke: width 0 = the thinnest line; cap 0 butt, 1 round, 2 square;
     join 0 mitre, 1 round, 2 bevel; dashes (n_dashes of them, from
     dash_offset) in the same units, or none */
  void (*stroke)     (void *closure, double r, double g, double b, double width,
                      int cap, int join, double miter_limit,
                      const double *dashes, int n_dashes, double dash_offset);
} cairo_riscos_page_funcs_t;

/* Gives the page's operations that can be drawn as vectors (solid colour
   fills, strokes and text, unclipped or clipped to one rectangle) to
   funcs, in order, and returns the region (in the recording's units) that
   must be printed as an image instead: cairo's own analysis (as for PDF
   and PostScript) - an operation it can't draw as a vector, and anything
   under it, is part of that region; the image of that region (all the
   page's operations drawn there) goes on top of the vectors. Returns NULL
   if the page can't be read. The region is the caller's. */
cairo_public cairo_region_t *
cairo_riscos_page_vectors (cairo_surface_t *recording,
                           const cairo_riscos_page_funcs_t *funcs,
                           void *closure);

CAIRO_END_DECLS

#endif
