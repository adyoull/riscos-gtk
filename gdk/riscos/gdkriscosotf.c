/*
 * gdkriscosotf.c - riscos-gtk: a small OpenType font writer, with CFF
 * (PostScript Type 2) outlines: enough for FreeType and fontconfig to use
 * the fonts gdkriscosfonts.c makes from the RISC OS Font Manager's
 * outlines (cubic curves, as CFF's are).
 *
 * Tables: CFF, OS/2 (version 4), cmap (formats 4 and 12), head, hhea,
 * hmtx, maxp (0.5), name, post (3.0). No hints, no kerning.
 *
 * Contours may come in either direction (RISC OS fonts are filled with
 * the even-odd rule); each is turned so CFF's nonzero rule fills the same
 * shape: counter-clockwise when an even number of the glyph's other
 * contours surround it, clockwise when an odd number do.
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as GTK.
 */
#include "config.h"

#include "gdkriscosotf.h"

#include <math.h>
#include <string.h>

typedef struct
{
  char op;                      /* 'M', 'L', 'C' */
  double x[3], y[3];            /* the end point is [0] for M and L, [2] for C */
} Seg;

typedef struct
{
  int advance;
  GByteArray *cs;               /* the Type 2 charstring */
  int xmin, ymin, xmax, ymax;
  gboolean empty;
} Glyph;

typedef struct
{
  guint32 ch;
  int glyph;
} Map;

struct _RiscosOtf
{
  int upm, weight, width, ascent, descent;
  gboolean italic;
  char *family, *style, *ps_name;
  GArray *glyphs;               /* Glyph */
  GArray *map;                  /* Map */
  GPtrArray *contours;          /* the glyph being made: GArray of Seg each */
  int advance;
};

/* ---- big-endian output ---- */

static void
put8 (GByteArray *a, int v)
{
  guint8 b = v;
  g_byte_array_append (a, &b, 1);
}

static void
put16 (GByteArray *a, int v)
{
  guint8 b[2] = { (v >> 8) & 0xff, v & 0xff };
  g_byte_array_append (a, b, 2);
}

static void
put32 (GByteArray *a, guint32 v)
{
  guint8 b[4] = { v >> 24, (v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff };
  g_byte_array_append (a, b, 4);
}

static void
set32 (GByteArray *a, guint at, guint32 v)
{
  a->data[at] = v >> 24;
  a->data[at + 1] = (v >> 16) & 0xff;
  a->data[at + 2] = (v >> 8) & 0xff;
  a->data[at + 3] = v & 0xff;
}

/* ---- the font ---- */

RiscosOtf *
riscos_otf_new (int upm, const char *family, const char *style, const char *ps_name,
                int weight, int width, gboolean italic)
{
  RiscosOtf *f = g_new0 (RiscosOtf, 1);
  int b = upm / 10, h = upm * 7 / 10;

  f->upm = upm;
  f->family = g_strdup (family);
  f->style = g_strdup (style);
  f->ps_name = g_strdup (ps_name);
  f->weight = weight;
  f->width = width;
  f->italic = italic;
  f->ascent = upm * 8 / 10;
  f->descent = -upm / 5;
  f->glyphs = g_array_new (FALSE, TRUE, sizeof (Glyph));
  f->map = g_array_new (FALSE, FALSE, sizeof (Map));
  /* .notdef: a box */
  riscos_otf_glyph_begin (f, upm / 2);
  riscos_otf_move_to (f, b, 0);
  riscos_otf_line_to (f, upm / 2 - b, 0);
  riscos_otf_line_to (f, upm / 2 - b, h);
  riscos_otf_line_to (f, b, h);
  riscos_otf_close (f);
  riscos_otf_move_to (f, b + upm / 20, upm / 20);
  riscos_otf_line_to (f, b + upm / 20, h - upm / 20);
  riscos_otf_line_to (f, upm / 2 - b - upm / 20, h - upm / 20);
  riscos_otf_line_to (f, upm / 2 - b - upm / 20, upm / 20);
  riscos_otf_close (f);
  riscos_otf_glyph_end (f);
  return f;
}

void
riscos_otf_free (RiscosOtf *f)
{
  guint i;

  if (!f)
    return;
  for (i = 0; i < f->glyphs->len; i++)
    g_byte_array_unref (g_array_index (f->glyphs, Glyph, i).cs);
  g_array_free (f->glyphs, TRUE);
  g_array_free (f->map, TRUE);
  if (f->contours)
    g_ptr_array_free (f->contours, TRUE);
  g_free (f->family);
  g_free (f->style);
  g_free (f->ps_name);
  g_free (f);
}

void
riscos_otf_set_extent (RiscosOtf *f, int ascent, int descent)
{
  f->ascent = ascent;
  f->descent = descent;
}

int
riscos_otf_n_glyphs (RiscosOtf *f)
{
  return f->glyphs->len;
}

void
riscos_otf_map (RiscosOtf *f, guint32 ch, int glyph)
{
  Map m = { ch, glyph };

  g_array_append_val (f->map, m);
}

/* ---- glyphs ---- */

int
riscos_otf_glyph_begin (RiscosOtf *f, int advance)
{
  if (f->contours)
    g_ptr_array_free (f->contours, TRUE);
  f->contours = g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);
  f->advance = advance;
  return f->glyphs->len;
}

static GArray *
current (RiscosOtf *f)
{
  return f->contours->len ? g_ptr_array_index (f->contours, f->contours->len - 1) : NULL;
}

void
riscos_otf_move_to (RiscosOtf *f, double x, double y)
{
  GArray *c = g_array_new (FALSE, FALSE, sizeof (Seg));
  Seg s = { 'M', { x }, { y } };

  g_array_append_val (c, s);
  g_ptr_array_add (f->contours, c);
}

void
riscos_otf_line_to (RiscosOtf *f, double x, double y)
{
  GArray *c = current (f);
  Seg s = { 'L', { x }, { y } };

  if (c)
    g_array_append_val (c, s);
}

void
riscos_otf_curve_to (RiscosOtf *f, double x1, double y1, double x2, double y2, double x3, double y3)
{
  GArray *c = current (f);
  Seg s = { 'C', { x1, x2, x3 }, { y1, y2, y3 } };

  if (c)
    g_array_append_val (c, s);
}

void
riscos_otf_close (RiscosOtf *f)
{
  /* the next move_to starts another contour; nothing to do */
  (void) f;
}

static void
seg_end (const Seg *s, double *x, double *y)
{
  int k = s->op == 'C' ? 2 : 0;

  *x = s->x[k];
  *y = s->y[k];
}

/* the contour's outline as a polygon (curves in 8 steps) */
static GArray *
flatten (GArray *c)
{
  GArray *p = g_array_new (FALSE, FALSE, sizeof (double) * 2);
  double px = 0, py = 0;
  guint i;
  int t;

  for (i = 0; i < c->len; i++)
    {
      Seg *s = &g_array_index (c, Seg, i);
      double pt[2];

      if (s->op == 'C')
        for (t = 1; t <= 8; t++)
          {
            double u = t / 8.0, v = 1 - u;

            pt[0] = v * v * v * px + 3 * v * v * u * s->x[0] + 3 * v * u * u * s->x[1] + u * u * u * s->x[2];
            pt[1] = v * v * v * py + 3 * v * v * u * s->y[0] + 3 * v * u * u * s->y[1] + u * u * u * s->y[2];
            g_array_append_val (p, pt);
          }
      else
        {
          pt[0] = s->x[0];
          pt[1] = s->y[0];
          g_array_append_val (p, pt);
        }
      seg_end (s, &px, &py);
    }
  return p;
}

static double
area (GArray *p)
{
  double a = 0;
  guint i;

  for (i = 0; i < p->len; i++)
    {
      double *u = &g_array_index (p, double, 2 * i);
      double *v = &g_array_index (p, double, 2 * ((i + 1) % p->len));

      a += u[0] * v[1] - v[0] * u[1];
    }
  return a / 2;
}

static gboolean
inside (GArray *p, double x, double y)
{
  gboolean in = FALSE;
  guint i, j;

  for (i = 0, j = p->len - 1; i < p->len; j = i++)
    {
      double *a = &g_array_index (p, double, 2 * i);
      double *b = &g_array_index (p, double, 2 * j);

      if ((a[1] > y) != (b[1] > y) && x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0])
        in = !in;
    }
  return in;
}

/* the contour the other way round */
static GArray *
reverse (GArray *c)
{
  GArray *r = g_array_new (FALSE, FALSE, sizeof (Seg));
  int n = c->len, i;
  double *nx = g_new (double, n + 1), *ny = g_new (double, n + 1);
  Seg *segs = g_new (Seg, n + 1), s;

  /* the nodes, closed explicitly (the last node is the first) */
  for (i = 0; i < n; i++)
    {
      segs[i] = g_array_index (c, Seg, i);
      seg_end (&segs[i], &nx[i], &ny[i]);
    }
  if (nx[n - 1] != nx[0] || ny[n - 1] != ny[0])
    {
      memset (&segs[n], 0, sizeof (Seg));
      segs[n].op = 'L';
      segs[n].x[0] = nx[n] = nx[0];
      segs[n].y[0] = ny[n] = ny[0];
      n++;
    }
  memset (&s, 0, sizeof s);
  s.op = 'M';
  s.x[0] = nx[0];
  s.y[0] = ny[0];
  g_array_append_val (r, s);
  for (i = n - 1; i >= 1; i--)
    {
      Seg *o = &segs[i];

      memset (&s, 0, sizeof s);
      if (o->op == 'C')
        {
          s.op = 'C';
          s.x[0] = o->x[1]; s.y[0] = o->y[1];
          s.x[1] = o->x[0]; s.y[1] = o->y[0];
          s.x[2] = nx[i - 1]; s.y[2] = ny[i - 1];
        }
      else
        {
          s.op = 'L';
          s.x[0] = nx[i - 1]; s.y[0] = ny[i - 1];
        }
      g_array_append_val (r, s);
    }
  /* the closing line back to the start is implicit */
  if (r->len > 2 && g_array_index (r, Seg, r->len - 1).op == 'L')
    g_array_set_size (r, r->len - 1);
  g_free (nx);
  g_free (ny);
  g_free (segs);
  return r;
}

/* Type 2 charstring numbers */
static void
cs_num (GByteArray *a, int v)
{
  if (v >= -107 && v <= 107)
    put8 (a, v + 139);
  else if (v >= 108 && v <= 1131)
    {
      v -= 108;
      put8 (a, (v >> 8) + 247);
      put8 (a, v & 0xff);
    }
  else if (v >= -1131 && v <= -108)
    {
      v = -v - 108;
      put8 (a, (v >> 8) + 251);
      put8 (a, v & 0xff);
    }
  else
    {
      v = CLAMP (v, -32768, 32767);
      put8 (a, 28);
      put16 (a, v);
    }
}

static void
grow (Glyph *g, int x, int y)
{
  if (g->empty)
    {
      g->xmin = g->xmax = x;
      g->ymin = g->ymax = y;
      g->empty = FALSE;
      return;
    }
  g->xmin = MIN (g->xmin, x);
  g->xmax = MAX (g->xmax, x);
  g->ymin = MIN (g->ymin, y);
  g->ymax = MAX (g->ymax, y);
}

int
riscos_otf_glyph_end (RiscosOtf *f)
{
  Glyph g = { 0 };
  GPtrArray *polys = g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);
  GPtrArray *cs = f->contours;
  int cx = 0, cy = 0;
  guint i, j, k;

  g.advance = f->advance;
  g.cs = g_byte_array_new ();
  g.empty = TRUE;
  /* tidy: no contours of a point only; no closing line back to the
     start (closing is implicit) */
  for (i = 0; i < cs->len; )
    {
      GArray *c = g_ptr_array_index (cs, i);
      Seg *first = &g_array_index (c, Seg, 0);
      Seg *last = &g_array_index (c, Seg, c->len - 1);

      if (c->len > 2 && last->op == 'L' && last->x[0] == first->x[0] && last->y[0] == first->y[0])
        g_array_set_size (c, c->len - 1);
      if (c->len < 2)
        {
          g_ptr_array_remove_index (cs, i);
          continue;
        }
      g_ptr_array_add (polys, flatten (c));
      i++;
    }
  /* directions for the nonzero rule */
  for (i = 0; i < cs->len; i++)
    {
      GArray *p = g_ptr_array_index (polys, i);
      double *pt = &g_array_index (p, double, 0);
      double a = area (p);
      int depth = 0;

      for (j = 0; j < cs->len; j++)
        if (j != i && inside (g_ptr_array_index (polys, j), pt[0], pt[1]))
          depth++;
      if (a != 0 && (a > 0) != (depth % 2 == 0))
        {
          GArray *old = g_ptr_array_index (cs, i);

          cs->pdata[i] = reverse (old);
          g_array_unref (old);
        }
    }
  /* the charstring: width, then the contours */
  if (g.advance != 0)
    cs_num (g.cs, g.advance);
  for (i = 0; i < cs->len; i++)
    {
      GArray *c = g_ptr_array_index (cs, i);

      for (k = 0; k < c->len; k++)
        {
          Seg *s = &g_array_index (c, Seg, k);
          int n = s->op == 'C' ? 3 : 1, m;

          for (m = 0; m < n; m++)
            {
              int x = (int) lrint (s->x[m]), y = (int) lrint (s->y[m]);

              cs_num (g.cs, x - cx);
              cs_num (g.cs, y - cy);
              cx = x;
              cy = y;
              grow (&g, x, y);
            }
          put8 (g.cs, s->op == 'M' ? 21 : s->op == 'L' ? 5 : 8);
        }
    }
  put8 (g.cs, 14);      /* endchar */
  g_array_append_val (f->glyphs, g);
  g_ptr_array_free (polys, TRUE);
  g_ptr_array_free (f->contours, TRUE);
  f->contours = NULL;
  return f->glyphs->len - 1;
}

/* ---- CFF ---- */

static void
dict_int (GByteArray *a, int v)
{
  if (v >= -107 && v <= 107)
    put8 (a, v + 139);
  else if (v >= 108 && v <= 1131)
    {
      v -= 108;
      put8 (a, (v >> 8) + 247);
      put8 (a, v & 0xff);
    }
  else if (v >= -1131 && v <= -108)
    {
      v = -v - 108;
      put8 (a, (v >> 8) + 251);
      put8 (a, v & 0xff);
    }
  else
    {
      put8 (a, 29);
      put32 (a, (guint32) v);
    }
}

static void
dict_int5 (GByteArray *a, int v)
{
  put8 (a, 29);
  put32 (a, (guint32) v);
}

/* an INDEX of the items (GByteArray each) */
static void
put_index (GByteArray *out, GPtrArray *items)
{
  guint i, total = 1;
  int off_size;

  put16 (out, items->len);
  if (items->len == 0)
    return;
  for (i = 0; i < items->len; i++)
    total += ((GByteArray *) g_ptr_array_index (items, i))->len;
  off_size = total < 0x100 ? 1 : total < 0x10000 ? 2 : total < 0x1000000 ? 3 : 4;
  put8 (out, off_size);
  total = 1;
  for (i = 0; i <= items->len; i++)
    {
      int b;

      for (b = off_size - 1; b >= 0; b--)
        put8 (out, (total >> (8 * b)) & 0xff);
      if (i < items->len)
        total += ((GByteArray *) g_ptr_array_index (items, i))->len;
    }
  for (i = 0; i < items->len; i++)
    {
      GByteArray *it = g_ptr_array_index (items, i);
      g_byte_array_append (out, it->data, it->len);
    }
}

static GByteArray *
bytes_of (const char *s)
{
  GByteArray *a = g_byte_array_new ();
  g_byte_array_append (a, (const guint8 *) s, strlen (s));
  return a;
}

/* each glyph's name: uniXXXX (uXXXXX) for its first character, gN if none */
static char *
glyph_name (RiscosOtf *f, int glyph)
{
  guint i;
  guint32 best = G_MAXUINT32;

  for (i = 0; i < f->map->len; i++)
    {
      Map *m = &g_array_index (f->map, Map, i);
      if (m->glyph == glyph && m->ch < best)
        best = m->ch;
    }
  if (best == G_MAXUINT32)
    return g_strdup_printf ("g%d", glyph);
  return best > 0xFFFF ? g_strdup_printf ("u%05X", best) : g_strdup_printf ("uni%04X", best);
}

static GByteArray *
build_cff (RiscosOtf *f, int bbox[4])
{
  GByteArray *out = g_byte_array_new ();
  GPtrArray *names = g_ptr_array_new_with_free_func ((GDestroyNotify) g_byte_array_unref);
  GPtrArray *strings = g_ptr_array_new_with_free_func ((GDestroyNotify) g_byte_array_unref);
  GPtrArray *empty = g_ptr_array_new ();
  GPtrArray *tops = g_ptr_array_new_with_free_func ((GDestroyNotify) g_byte_array_unref);
  GPtrArray *charstrings = g_ptr_array_new ();
  GByteArray *top = g_byte_array_new ();
  GByteArray *charset = g_byte_array_new ();
  GByteArray *priv = g_byte_array_new ();
  guint i, n = f->glyphs->len, before_top, top_len;
  int charset_off, cs_off, priv_off;
  GByteArray *tail = g_byte_array_new ();

  g_ptr_array_add (names, bytes_of (f->ps_name));
  for (i = 1; i < n; i++)
    {
      char *name = glyph_name (f, i);
      g_ptr_array_add (strings, bytes_of (name));
      g_free (name);
    }
  /* charset (format 0): the glyphs' names' string ids (391 standard ones
     come first) */
  put8 (charset, 0);
  for (i = 1; i < n; i++)
    put16 (charset, 391 + i - 1);
  for (i = 0; i < n; i++)
    g_ptr_array_add (charstrings, g_array_index (f->glyphs, Glyph, i).cs);
  /* Private DICT: widths from 0 */
  dict_int (priv, 0); put8 (priv, 20);    /* defaultWidthX */
  dict_int (priv, 0); put8 (priv, 21);    /* nominalWidthX */

  /* the header and the Name INDEX, then the Top DICT (its size known:
     its offsets are written 5 bytes long) */
  put8 (out, 1); put8 (out, 0); put8 (out, 4); put8 (out, 4);
  put_index (out, names);
  before_top = out->len;
  /* the Top DICT's length: FontBBox, charset, CharStrings, Private */
  {
    GByteArray *probe = g_byte_array_new ();
    for (i = 0; i < 4; i++)
      dict_int (probe, bbox[i]);
    put8 (probe, 5);
    dict_int5 (probe, 0); put8 (probe, 15);
    dict_int5 (probe, 0); put8 (probe, 17);
    dict_int5 (probe, 0); dict_int5 (probe, 0); put8 (probe, 18);
    top_len = probe->len;
    g_byte_array_unref (probe);
  }
  /* after the Top DICT INDEX (count 2, offSize 1, 2 offsets: top_len < 255) */
  put_index (tail, strings);
  put_index (tail, empty);          /* Global Subr INDEX */
  charset_off = before_top + 2 + 1 + 2 + top_len + tail->len;
  g_byte_array_append (tail, charset->data, charset->len);
  cs_off = before_top + 2 + 1 + 2 + top_len + tail->len;
  put_index (tail, charstrings);
  priv_off = before_top + 2 + 1 + 2 + top_len + tail->len;
  g_byte_array_append (tail, priv->data, priv->len);

  for (i = 0; i < 4; i++)
    dict_int (top, bbox[i]);
  put8 (top, 5);
  dict_int5 (top, charset_off); put8 (top, 15);
  dict_int5 (top, cs_off); put8 (top, 17);
  dict_int5 (top, priv->len); dict_int5 (top, priv_off); put8 (top, 18);
  g_assert (top->len == top_len && top_len < 255);
  g_ptr_array_add (tops, top);
  put_index (out, tops);
  g_assert (out->len == before_top + 2 + 1 + 2 + top_len);
  g_byte_array_append (out, tail->data, tail->len);

  g_ptr_array_free (names, TRUE);
  g_ptr_array_free (strings, TRUE);
  g_ptr_array_free (empty, TRUE);
  g_ptr_array_free (tops, TRUE);
  g_ptr_array_free (charstrings, TRUE);
  g_byte_array_unref (charset);
  g_byte_array_unref (priv);
  g_byte_array_unref (tail);
  return out;
}

/* ---- the other tables ---- */

static int
cmp_map (gconstpointer a, gconstpointer b)
{
  const Map *x = a, *y = b;
  return x->ch < y->ch ? -1 : x->ch > y->ch;
}

static GByteArray *
build_cmap (RiscosOtf *f)
{
  GByteArray *t = g_byte_array_new ();
  GByteArray *f4 = g_byte_array_new (), *f12 = g_byte_array_new ();
  GArray *m = g_array_copy (f->map);
  GArray *start = g_array_new (FALSE, FALSE, sizeof (int));
  GArray *end = g_array_new (FALSE, FALSE, sizeof (int));
  GArray *base = g_array_new (FALSE, FALSE, sizeof (int));
  GArray *gids = g_array_new (FALSE, FALSE, sizeof (int));
  guint i, j, n_groups = 0;
  int seg_count, sr, es, len4;
  gboolean have4;

  g_array_sort (m, cmp_map);
  /* no duplicate characters */
  for (i = 1; i < m->len; )
    if (g_array_index (m, Map, i).ch == g_array_index (m, Map, i - 1).ch)
      g_array_remove_index (m, i);
    else
      i++;

  /* format 4: runs of consecutive characters, through glyphIdArray */
  for (i = 0; i < m->len; i++)
    {
      Map *e = &g_array_index (m, Map, i);
      int ch = e->ch;

      if (e->ch >= 0xFFFF)
        break;
      if (start->len && g_array_index (end, int, end->len - 1) == ch - 1)
        g_array_index (end, int, end->len - 1) = ch;
      else
        {
          int b = gids->len;
          g_array_append_val (start, ch);
          g_array_append_val (end, ch);
          g_array_append_val (base, b);
        }
      g_array_append_val (gids, e->glyph);
    }
  seg_count = start->len + 1;
  for (sr = 1, es = 0; sr * 2 <= seg_count; sr *= 2, es++)
    ;
  sr *= 2;
  len4 = 16 + 8 * seg_count + 2 * gids->len;
  have4 = len4 <= 0xFFFF;
  if (have4)
    {
      put16 (f4, 4); put16 (f4, len4); put16 (f4, 0);
      put16 (f4, seg_count * 2); put16 (f4, sr); put16 (f4, es); put16 (f4, seg_count * 2 - sr);
      for (i = 0; i < start->len; i++)
        put16 (f4, g_array_index (end, int, i));
      put16 (f4, 0xFFFF);
      put16 (f4, 0);
      for (i = 0; i < start->len; i++)
        put16 (f4, g_array_index (start, int, i));
      put16 (f4, 0xFFFF);
      for (i = 0; i < start->len; i++)
        put16 (f4, 0);                                  /* idDelta */
      put16 (f4, 1);
      for (i = 0; i < start->len; i++)                  /* idRangeOffset */
        put16 (f4, 2 * (seg_count - i + g_array_index (base, int, i)));
      put16 (f4, 0);
      for (i = 0; i < gids->len; i++)
        put16 (f4, g_array_index (gids, int, i));
    }
  /* format 12: groups of consecutive characters and glyphs */
  {
    GByteArray *groups = g_byte_array_new ();
    for (i = 0; i < m->len; i = j)
      {
        Map *e = &g_array_index (m, Map, i);
        for (j = i + 1; j < m->len; j++)
          {
            Map *x = &g_array_index (m, Map, j);
            if (x->ch != e->ch + (j - i) || x->glyph != e->glyph + (int) (j - i))
              break;
          }
        put32 (groups, e->ch);
        put32 (groups, g_array_index (m, Map, j - 1).ch);
        put32 (groups, e->glyph);
        n_groups++;
      }
    put16 (f12, 12); put16 (f12, 0); put32 (f12, 16 + groups->len); put32 (f12, 0);
    put32 (f12, n_groups);
    g_byte_array_append (f12, groups->data, groups->len);
    g_byte_array_unref (groups);
  }
  put16 (t, 0);
  put16 (t, have4 ? 2 : 1);
  if (have4)
    {
      put16 (t, 3); put16 (t, 1); put32 (t, 4 + 8 * 2);
      put16 (t, 3); put16 (t, 10); put32 (t, 4 + 8 * 2 + f4->len);
      g_byte_array_append (t, f4->data, f4->len);
    }
  else
    {
      put16 (t, 3); put16 (t, 10); put32 (t, 4 + 8);
    }
  g_byte_array_append (t, f12->data, f12->len);

  g_byte_array_unref (f4);
  g_byte_array_unref (f12);
  g_array_free (m, TRUE);
  g_array_free (start, TRUE);
  g_array_free (end, TRUE);
  g_array_free (base, TRUE);
  g_array_free (gids, TRUE);
  return t;
}

static void
name_utf16 (GByteArray *a, const char *s)
{
  gunichar2 *u = g_utf8_to_utf16 (s, -1, NULL, NULL, NULL);
  int i;

  for (i = 0; u && u[i]; i++)
    put16 (a, u[i]);
  g_free (u);
}

static GByteArray *
build_name (RiscosOtf *f)
{
  GByteArray *t = g_byte_array_new ();
  GByteArray *strs = g_byte_array_new ();
  char *full = g_strdup_printf ("%s %s", f->family, f->style);
  const char *subfamily = f->weight >= 600 ? (f->italic ? "Bold Italic" : "Bold")
                                           : (f->italic ? "Italic" : "Regular");
  struct { int id; const char *s; } recs[] = {
    { 0, "Made from a RISC OS font by riscos-gtk, for this computer's use only" },
    { 1, f->family }, { 2, subfamily }, { 3, full }, { 4, full }, { 5, "Version 1.0" },
    { 6, f->ps_name }, { 16, f->family }, { 17, f->style },
  };
  guint i, n = G_N_ELEMENTS (recs);

  put16 (t, 0); put16 (t, n); put16 (t, 6 + 12 * n);
  for (i = 0; i < n; i++)
    {
      guint at = strs->len;
      name_utf16 (strs, recs[i].s);
      put16 (t, 3); put16 (t, 1); put16 (t, 0x409); put16 (t, recs[i].id);
      put16 (t, strs->len - at); put16 (t, at);
    }
  g_byte_array_append (t, strs->data, strs->len);
  g_byte_array_unref (strs);
  g_free (full);
  return t;
}

static int
glyph_for (RiscosOtf *f, guint32 ch)
{
  guint i;
  for (i = 0; i < f->map->len; i++)
    if (g_array_index (f->map, Map, i).ch == ch)
      return g_array_index (f->map, Map, i).glyph;
  return -1;
}

static guint32
checksum (const guint8 *d, guint len)
{
  guint32 s = 0;
  guint i;

  for (i = 0; i < len; i += 4)
    s += ((guint32) d[i] << 24) | ((guint32) (i + 1 < len ? d[i + 1] : 0) << 16) |
         ((guint32) (i + 2 < len ? d[i + 2] : 0) << 8) | (i + 3 < len ? d[i + 3] : 0);
  return s;
}

GBytes *
riscos_otf_build (RiscosOtf *f)
{
  int n = f->glyphs->len, i, bbox[4] = { 0, 0, 0, 0 }, have_box = 0;
  int adv_max = 0, min_lsb = 0, min_rsb = 0, max_ext = 0, sum_adv = 0, n_adv = 0;
  int fixed_adv = -1;
  gboolean fixed = TRUE;
  guint32 first_ch = 0xFFFF, last_ch = 0;
  int x_height = 0, cap_height = 0, g;
  GByteArray *tables[9];
  static const char tags[9][5] = { "CFF ", "OS/2", "cmap", "head", "hhea", "hmtx", "maxp", "name", "post" };
  GByteArray *out = g_byte_array_new ();
  guint off, head_at = 0, t;

  for (i = 0; i < n; i++)
    {
      Glyph *gl = &g_array_index (f->glyphs, Glyph, i);

      adv_max = MAX (adv_max, gl->advance);
      if (gl->advance > 0 && i > 0)
        {
          sum_adv += gl->advance;
          n_adv++;
          if (fixed_adv < 0)
            fixed_adv = gl->advance;
          else if (fixed_adv != gl->advance)
            fixed = FALSE;
        }
      if (gl->empty)
        continue;
      if (!have_box)
        {
          bbox[0] = gl->xmin; bbox[1] = gl->ymin; bbox[2] = gl->xmax; bbox[3] = gl->ymax;
          have_box = 1;
          min_lsb = gl->xmin;
          min_rsb = gl->advance - gl->xmax;
          max_ext = gl->xmax;
        }
      bbox[0] = MIN (bbox[0], gl->xmin);
      bbox[1] = MIN (bbox[1], gl->ymin);
      bbox[2] = MAX (bbox[2], gl->xmax);
      bbox[3] = MAX (bbox[3], gl->ymax);
      min_lsb = MIN (min_lsb, gl->xmin);
      min_rsb = MIN (min_rsb, gl->advance - gl->xmax);
      max_ext = MAX (max_ext, gl->xmax);
    }
  if (fixed_adv < 0)
    fixed = FALSE;
  for (t = 0; t < f->map->len; t++)
    {
      guint32 ch = g_array_index (f->map, Map, t).ch;
      if (ch < first_ch) first_ch = ch;
      if (ch > last_ch) last_ch = ch;
    }
  if ((g = glyph_for (f, 'x')) >= 0)
    x_height = g_array_index (f->glyphs, Glyph, g).ymax;
  if ((g = glyph_for (f, 'H')) >= 0)
    cap_height = g_array_index (f->glyphs, Glyph, g).ymax;

  tables[0] = build_cff (f, bbox);

  /* OS/2, version 4 */
  {
    GByteArray *a = tables[1] = g_byte_array_new ();
    int upm = f->upm;
    int sel = (f->italic ? 1 : 0) | (f->weight >= 600 ? 32 : 0);

    if (!(sel & 33))
      sel |= 64;                                        /* REGULAR */
    put16 (a, 4);
    put16 (a, n_adv ? sum_adv / n_adv : upm / 2);
    put16 (a, f->weight);
    put16 (a, f->width);
    put16 (a, 0);                                       /* fsType: installable */
    put16 (a, upm * 65 / 100); put16 (a, upm * 60 / 100); put16 (a, 0); put16 (a, upm * 7 / 100);
    put16 (a, upm * 65 / 100); put16 (a, upm * 60 / 100); put16 (a, 0); put16 (a, upm * 48 / 100);
    put16 (a, upm * 5 / 100); put16 (a, upm * 26 / 100);
    put16 (a, 0);                                       /* sFamilyClass */
    put8 (a, fixed ? 2 : 0); put8 (a, 0); put8 (a, 0); put8 (a, fixed ? 9 : 0);   /* panose */
    for (i = 0; i < 6; i++) put8 (a, 0);
    put32 (a, (first_ch < 0x80 ? 1 : 0) | (last_ch >= 0xA0 ? 2 : 0));
    put32 (a, 0); put32 (a, 0); put32 (a, 0);
    put8 (a, 'R'); put8 (a, 'I'); put8 (a, 'S'); put8 (a, 'C');
    put16 (a, sel);
    put16 (a, MIN (first_ch, 0xFFFF));
    put16 (a, MIN (last_ch, 0xFFFF));
    put16 (a, f->ascent); put16 (a, f->descent); put16 (a, 0);
    put16 (a, MAX (f->ascent, bbox[3])); put16 (a, MAX (-f->descent, -bbox[1]));
    put32 (a, 1); put32 (a, 0);                         /* code pages: Latin 1 */
    put16 (a, x_height); put16 (a, cap_height);
    put16 (a, 0); put16 (a, 32); put16 (a, 0);
  }
  tables[2] = build_cmap (f);
  /* head */
  {
    GByteArray *a = tables[3] = g_byte_array_new ();
    put32 (a, 0x00010000); put32 (a, 0x00010000); put32 (a, 0); put32 (a, 0x5F0F3CF5);
    put16 (a, 3); put16 (a, f->upm);
    put32 (a, 0); put32 (a, 0); put32 (a, 0); put32 (a, 0);
    put16 (a, bbox[0]); put16 (a, bbox[1]); put16 (a, bbox[2]); put16 (a, bbox[3]);
    put16 (a, (f->weight >= 600 ? 1 : 0) | (f->italic ? 2 : 0));
    put16 (a, 8); put16 (a, 2); put16 (a, 0); put16 (a, 0);
  }
  /* hhea */
  {
    GByteArray *a = tables[4] = g_byte_array_new ();
    put32 (a, 0x00010000);
    put16 (a, f->ascent); put16 (a, f->descent); put16 (a, 0);
    put16 (a, adv_max); put16 (a, min_lsb); put16 (a, min_rsb); put16 (a, max_ext);
    put16 (a, 1); put16 (a, 0); put16 (a, 0);
    put16 (a, 0); put16 (a, 0); put16 (a, 0); put16 (a, 0);
    put16 (a, 0); put16 (a, n);
  }
  /* hmtx */
  {
    GByteArray *a = tables[5] = g_byte_array_new ();
    for (i = 0; i < n; i++)
      {
        Glyph *gl = &g_array_index (f->glyphs, Glyph, i);
        put16 (a, gl->advance);
        put16 (a, gl->empty ? 0 : gl->xmin);
      }
  }
  /* maxp 0.5 */
  {
    GByteArray *a = tables[6] = g_byte_array_new ();
    put32 (a, 0x00005000);
    put16 (a, n);
  }
  tables[7] = build_name (f);
  /* post 3.0 */
  {
    GByteArray *a = tables[8] = g_byte_array_new ();
    put32 (a, 0x00030000);
    put32 (a, f->italic ? 0xFFF40000u : 0);      /* -12 degrees */
    put16 (a, -f->upm / 10); put16 (a, f->upm / 20);
    put32 (a, fixed ? 1 : 0);
    put32 (a, 0); put32 (a, 0); put32 (a, 0); put32 (a, 0);
  }

  /* the table directory, then the tables (4-byte aligned) */
  {
    int nt = 9, sr = 1, es = 0;
    while (sr * 2 <= nt) { sr *= 2; es++; }
    put32 (out, 0x4F54544F);                            /* 'OTTO' */
    put16 (out, nt); put16 (out, sr * 16); put16 (out, es); put16 (out, nt * 16 - sr * 16);
    off = 12 + 16 * nt;
    for (t = 0; t < 9; t++)
      {
        put8 (out, tags[t][0]); put8 (out, tags[t][1]); put8 (out, tags[t][2]); put8 (out, tags[t][3]);
        put32 (out, checksum (tables[t]->data, tables[t]->len));
        put32 (out, off);
        put32 (out, tables[t]->len);
        if (t == 3)
          head_at = off;
        off += (tables[t]->len + 3) & ~3u;
      }
    for (t = 0; t < 9; t++)
      {
        static const guint8 zero[3] = { 0, 0, 0 };
        g_byte_array_append (out, tables[t]->data, tables[t]->len);
        g_byte_array_append (out, zero, ((tables[t]->len + 3) & ~3u) - tables[t]->len);
        g_byte_array_unref (tables[t]);
      }
    set32 (out, head_at + 8, 0xB1B0AFBA - checksum (out->data, out->len));
  }
  return g_byte_array_free_to_bytes (out);
}
