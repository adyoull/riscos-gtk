/* cairo-riscos-print.c - riscos-gtk: a page recorded by cairo, as vectors
 * for RISC OS's printer drivers (cairo-riscos.h).
 *
 * As cairo's PDF and PostScript surfaces do (cairo-paginated-surface.c's
 * _paint_page): cairo's analysis surface replays the recording against a
 * surface of ours that says which operations it can draw; those (the
 * NATIVE region) are replayed again and handed to the caller as paths;
 * what it can't draw comes back as a region to print as an image.
 *
 * Drawn as vectors: fills, strokes (a ctm without shear or uneven scale)
 * and text (glyph outlines, unhinted), in an opaque solid colour with
 * OVER or SOURCE, unclipped or inside a one-rectangle clip; a paint in an
 * opaque solid colour (a background) as a filled rectangle. The rest (images,
 * gradients, transparency, clip paths, colour glyphs) is the image region.
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later or MPL-1.1, as cairo.
 */
#include "cairoint.h"

#include "cairo-riscos.h"
#include "cairo-analysis-surface-private.h"
#include "cairo-clip-inline.h"
#include "cairo-error-private.h"
#include "cairo-pattern-private.h"
#include "cairo-recording-surface-private.h"
#include "cairo-recording-surface-inline.h"
#include "cairo-scaled-font-private.h"
#include "cairo-surface-backend-private.h"
#include "cairo-default-context-private.h"

typedef struct {
  cairo_surface_t base;
  cairo_rectangle_int_t extents;
  cairo_bool_t analysing;
  const cairo_riscos_page_funcs_t *funcs;
  void *closure;
  double dx, dy;                      /* offset of the path being given (glyphs) */
} walker_t;

static const cairo_surface_backend_t walker_backend;

/* ---- what can be drawn ---------------------------------------------------- */

static cairo_bool_t
solid_opaque (cairo_operator_t op, const cairo_pattern_t *source, const cairo_color_t **colour)
{
  const cairo_solid_pattern_t *solid;

  if (op != CAIRO_OPERATOR_OVER && op != CAIRO_OPERATOR_SOURCE)
    return FALSE;
  if (source->type != CAIRO_PATTERN_TYPE_SOLID)
    return FALSE;
  solid = (const cairo_solid_pattern_t *) source;
  if (! CAIRO_COLOR_IS_OPAQUE (&solid->color))
    return FALSE;
  *colour = &solid->color;
  return TRUE;
}

/* no clip, or one rectangle the operation is inside */
static cairo_bool_t
clip_ok (const cairo_clip_t *clip, const cairo_rectangle_int_t *extents)
{
  if (clip == NULL)
    return TRUE;
  if (clip->path != NULL || clip->num_boxes > 1)
    return FALSE;
  return _cairo_rectangle_contains_rectangle (&clip->extents, extents);
}

/* ---- the paths ------------------------------------------------------------ */

static cairo_status_t
w_move_to (void *closure, const cairo_point_t *p)
{
  walker_t *w = closure;
  w->funcs->move_to (w->closure, _cairo_fixed_to_double (p->x) + w->dx,
                     _cairo_fixed_to_double (p->y) + w->dy);
  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
w_line_to (void *closure, const cairo_point_t *p)
{
  walker_t *w = closure;
  w->funcs->line_to (w->closure, _cairo_fixed_to_double (p->x) + w->dx,
                     _cairo_fixed_to_double (p->y) + w->dy);
  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
w_curve_to (void *closure, const cairo_point_t *p0, const cairo_point_t *p1, const cairo_point_t *p2)
{
  walker_t *w = closure;
  w->funcs->curve_to (w->closure,
                      _cairo_fixed_to_double (p0->x) + w->dx, _cairo_fixed_to_double (p0->y) + w->dy,
                      _cairo_fixed_to_double (p1->x) + w->dx, _cairo_fixed_to_double (p1->y) + w->dy,
                      _cairo_fixed_to_double (p2->x) + w->dx, _cairo_fixed_to_double (p2->y) + w->dy);
  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
w_close_path (void *closure)
{
  walker_t *w = closure;
  w->funcs->close_path (w->closure);
  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
give_path (walker_t *w, const cairo_path_fixed_t *path, double dx, double dy)
{
  w->dx = dx;
  w->dy = dy;
  return _cairo_path_fixed_interpret (path, w_move_to, w_line_to, w_curve_to, w_close_path, w);
}

/* ---- the operations --------------------------------------------------------- */

static cairo_bool_t
walker_get_extents (void *abstract_surface, cairo_rectangle_int_t *rectangle)
{
  walker_t *w = abstract_surface;
  *rectangle = w->extents;
  return TRUE;
}

static cairo_int_status_t
walker_paint (void *abstract_surface, cairo_operator_t op, const cairo_pattern_t *source,
              const cairo_clip_t *clip)
{
  walker_t *w = abstract_surface;
  const cairo_color_t *c;
  cairo_rectangle_int_t r = clip ? clip->extents : w->extents;

  if (_cairo_clip_is_all_clipped (clip))
    return CAIRO_INT_STATUS_NOTHING_TO_DO;
  if (! solid_opaque (op, source, &c) || ! clip_ok (clip, &r))
    return CAIRO_INT_STATUS_UNSUPPORTED;
  if (w->analysing)
    return CAIRO_INT_STATUS_SUCCESS;
  w->funcs->move_to (w->closure, r.x, r.y);
  w->funcs->line_to (w->closure, r.x + r.width, r.y);
  w->funcs->line_to (w->closure, r.x + r.width, r.y + r.height);
  w->funcs->line_to (w->closure, r.x, r.y + r.height);
  w->funcs->close_path (w->closure);
  w->funcs->fill (w->closure, c->red, c->green, c->blue, FALSE);
  return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
walker_mask (void *abstract_surface, cairo_operator_t op, const cairo_pattern_t *source,
             const cairo_pattern_t *mask, const cairo_clip_t *clip)
{
  if (_cairo_clip_is_all_clipped (clip))
    return CAIRO_INT_STATUS_NOTHING_TO_DO;
  return CAIRO_INT_STATUS_UNSUPPORTED;
}

static cairo_int_status_t
walker_fill (void *abstract_surface, cairo_operator_t op, const cairo_pattern_t *source,
             const cairo_path_fixed_t *path, cairo_fill_rule_t fill_rule, double tolerance,
             cairo_antialias_t antialias, const cairo_clip_t *clip)
{
  walker_t *w = abstract_surface;
  const cairo_color_t *c;
  cairo_rectangle_int_t ext;
  cairo_status_t status;

  if (_cairo_clip_is_all_clipped (clip))
    return CAIRO_INT_STATUS_NOTHING_TO_DO;
  _cairo_path_fixed_approximate_fill_extents (path, &ext);
  if (! solid_opaque (op, source, &c) || ! clip_ok (clip, &ext))
    return CAIRO_INT_STATUS_UNSUPPORTED;
  if (w->analysing)
    return CAIRO_INT_STATUS_SUCCESS;
  status = give_path (w, path, 0, 0);
  if (unlikely (status))
    return status;
  w->funcs->fill (w->closure, c->red, c->green, c->blue, fill_rule == CAIRO_FILL_RULE_EVEN_ODD);
  return CAIRO_INT_STATUS_SUCCESS;
}

static cairo_int_status_t
walker_stroke (void *abstract_surface, cairo_operator_t op, const cairo_pattern_t *source,
               const cairo_path_fixed_t *path, const cairo_stroke_style_t *style,
               const cairo_matrix_t *ctm, const cairo_matrix_t *ctm_inverse, double tolerance,
               cairo_antialias_t antialias, const cairo_clip_t *clip)
{
  walker_t *w = abstract_surface;
  const cairo_color_t *c;
  cairo_rectangle_int_t ext;
  cairo_status_t status;
  double sx = hypot (ctm->xx, ctm->yx), sy = hypot (ctm->xy, ctm->yy);
  double dashes[32];
  unsigned int i;
  int cap, join;

  if (_cairo_clip_is_all_clipped (clip))
    return CAIRO_INT_STATUS_NOTHING_TO_DO;
  _cairo_path_fixed_approximate_stroke_extents (path, style, ctm, TRUE, &ext);
  if (! solid_opaque (op, source, &c) || ! clip_ok (clip, &ext))
    return CAIRO_INT_STATUS_UNSUPPORTED;
  /* a uniform scale (and rotation) only: a line width is then a length */
  if (sx <= 0 || fabs (sx - sy) > sx * 1e-4 ||
      fabs (ctm->xx * ctm->xy + ctm->yx * ctm->yy) > sx * sx * 1e-4 ||
      style->num_dashes > ARRAY_LENGTH (dashes))
    return CAIRO_INT_STATUS_UNSUPPORTED;
  if (w->analysing)
    return CAIRO_INT_STATUS_SUCCESS;
  for (i = 0; i < style->num_dashes; i++)
    dashes[i] = style->dash[i] * sx;
  cap = style->line_cap == CAIRO_LINE_CAP_ROUND ? 1 : style->line_cap == CAIRO_LINE_CAP_SQUARE ? 2 : 0;
  join = style->line_join == CAIRO_LINE_JOIN_ROUND ? 1 : style->line_join == CAIRO_LINE_JOIN_BEVEL ? 2 : 0;
  status = give_path (w, path, 0, 0);
  if (unlikely (status))
    return status;
  w->funcs->stroke (w->closure, c->red, c->green, c->blue,
                    style->is_hairline ? 0 : style->line_width * sx, cap, join, style->miter_limit,
                    dashes, style->num_dashes, style->dash_offset * sx);
  return CAIRO_INT_STATUS_SUCCESS;
}

/* the font without hinting: outlines as designed, not fitted to the
   recording's grid (a point) */
static cairo_scaled_font_t *
unhinted (cairo_scaled_font_t *scaled_font)
{
  cairo_font_options_t *options = cairo_font_options_create ();
  cairo_matrix_t font_matrix, ctm;
  cairo_scaled_font_t *font;

  cairo_scaled_font_get_font_options (scaled_font, options);
  cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
  cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
  cairo_scaled_font_get_font_matrix (scaled_font, &font_matrix);
  cairo_scaled_font_get_ctm (scaled_font, &ctm);
  font = cairo_scaled_font_create (cairo_scaled_font_get_font_face (scaled_font), &font_matrix, &ctm,
                                   options);
  cairo_font_options_destroy (options);
  return font;
}

static cairo_int_status_t
walker_show_glyphs (void *abstract_surface, cairo_operator_t op, const cairo_pattern_t *source,
                    cairo_glyph_t *glyphs, int num_glyphs, cairo_scaled_font_t *scaled_font,
                    const cairo_clip_t *clip)
{
  walker_t *w = abstract_surface;
  const cairo_color_t *c;
  cairo_rectangle_int_t ext;
  cairo_scaled_font_t *font;
  cairo_int_status_t status = CAIRO_INT_STATUS_SUCCESS;
  int i;

  if (_cairo_clip_is_all_clipped (clip))
    return CAIRO_INT_STATUS_NOTHING_TO_DO;
  if (! solid_opaque (op, source, &c))
    return CAIRO_INT_STATUS_UNSUPPORTED;
  if (! _cairo_scaled_font_glyph_approximate_extents (scaled_font, glyphs, num_glyphs, &ext))
    return CAIRO_INT_STATUS_UNSUPPORTED;
  if (! clip_ok (clip, &ext))
    return CAIRO_INT_STATUS_UNSUPPORTED;
  font = unhinted (scaled_font);
  if (font->status)
    {
      cairo_scaled_font_destroy (font);
      return CAIRO_INT_STATUS_UNSUPPORTED;
    }
  _cairo_scaled_font_freeze_cache (font);
  for (i = 0; i < num_glyphs; i++)
    {
      cairo_scaled_glyph_t *glyph;
      status = _cairo_scaled_glyph_lookup (font, glyphs[i].index, CAIRO_SCALED_GLYPH_INFO_PATH,
                                           NULL, &glyph);
      if (status || glyph->path == NULL)
        {
          /* no outline (a bitmap or colour glyph): an image */
          status = CAIRO_INT_STATUS_UNSUPPORTED;
          break;
        }
      if (! w->analysing)
        {
          status = give_path (w, glyph->path, glyphs[i].x, glyphs[i].y);
          if (status)
            break;
        }
    }
  _cairo_scaled_font_thaw_cache (font);
  cairo_scaled_font_destroy (font);
  if (status)
    return status;
  if (! w->analysing)
    w->funcs->fill (w->closure, c->red, c->green, c->blue, FALSE);
  return CAIRO_INT_STATUS_SUCCESS;
}

static const cairo_surface_backend_t walker_backend = {
  .type = CAIRO_INTERNAL_SURFACE_TYPE_NULL,
  .create_context = _cairo_default_context_create,
  .get_extents = walker_get_extents,
  .paint = walker_paint,
  .mask = walker_mask,
  .stroke = walker_stroke,
  .fill = walker_fill,
  .show_glyphs = walker_show_glyphs,
};

cairo_region_t *
cairo_riscos_page_vectors (cairo_surface_t *recording, const cairo_riscos_page_funcs_t *funcs,
                           void *closure)
{
  walker_t *w;
  cairo_surface_t *analysis;
  cairo_region_t *fallback = NULL;
  cairo_rectangle_t ext;
  unsigned int regions_id = 0;
  cairo_status_t status;

  if (recording == NULL || recording->status || ! _cairo_surface_is_recording (recording) ||
      ! cairo_recording_surface_get_extents (recording, &ext))
    return NULL;
  w = _cairo_malloc (sizeof (walker_t));
  if (w == NULL)
    return NULL;
  _cairo_surface_init (&w->base, &walker_backend, NULL, CAIRO_CONTENT_COLOR_ALPHA, TRUE);
  w->extents.x = floor (ext.x);
  w->extents.y = floor (ext.y);
  w->extents.width = ceil (ext.x + ext.width) - w->extents.x;
  w->extents.height = ceil (ext.y + ext.height) - w->extents.y;
  w->funcs = funcs;
  w->closure = closure;
  w->analysing = TRUE;

  analysis = _cairo_analysis_surface_create (&w->base, TRUE);
  if (analysis->status)
    goto out;
  status = _cairo_recording_surface_region_array_attach (recording, &regions_id);
  if (status)
    goto out;
  status = _cairo_recording_surface_replay_and_create_regions (recording, regions_id, NULL,
                                                               analysis, FALSE, FALSE);
  if (status)
    goto out;
  if (_cairo_analysis_surface_has_supported (analysis))
    {
      w->analysing = FALSE;
      status = _cairo_recording_surface_replay_region (recording, regions_id, NULL, &w->base,
                                                       CAIRO_RECORDING_REGION_NATIVE);
      if (status)
        goto out;
    }
  fallback = cairo_region_copy (_cairo_analysis_surface_get_unsupported (analysis));

out:
  if (regions_id)
    _cairo_recording_surface_region_array_remove (recording, regions_id);
  cairo_surface_destroy (analysis);
  cairo_surface_finish (&w->base);
  cairo_surface_destroy (&w->base);
  return fallback;
}
