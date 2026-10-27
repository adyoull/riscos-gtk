/*
 * gdkriscosotf.h - riscos-gtk: a small OpenType (CFF outlines) writer, for
 * the RISC OS fonts GDK makes available to fontconfig (gdkriscosfonts.c).
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as GTK.
 */
#ifndef __GDK_RISCOS_OTF_H__
#define __GDK_RISCOS_OTF_H__

#include <glib.h>

typedef struct _RiscosOtf RiscosOtf;

/* family "Homerton", style "Bold Oblique", PostScript name
   "Homerton-BoldOblique"; weight 100-900 (OS/2 usWeightClass), width
   1-9 (usWidthClass); italic: slanted. Units per em: upm. */
RiscosOtf *riscos_otf_new (int upm, const char *family, const char *style, const char *ps_name,
                           int weight, int width, gboolean italic);
void riscos_otf_free (RiscosOtf *f);

/* the font's ascent and descent (descent negative), font units */
void riscos_otf_set_extent (RiscosOtf *f, int ascent, int descent);

/* a glyph: begin (its advance), its contours (each begun with move_to
   and closed with close, implicitly by the next move_to or end), end;
   returns its glyph index. Coordinates are font units, y up; any winding
   (the contours are turned to the nonzero rule's directions, from how
   they nest) */
int riscos_otf_glyph_begin (RiscosOtf *f, int advance);
void riscos_otf_move_to (RiscosOtf *f, double x, double y);
void riscos_otf_line_to (RiscosOtf *f, double x, double y);
void riscos_otf_curve_to (RiscosOtf *f, double x1, double y1, double x2, double y2, double x3, double y3);
void riscos_otf_close (RiscosOtf *f);
int riscos_otf_glyph_end (RiscosOtf *f);

/* character (Unicode) -> glyph */
void riscos_otf_map (RiscosOtf *f, guint32 ch, int glyph);

int riscos_otf_n_glyphs (RiscosOtf *f);

/* the font file */
GBytes *riscos_otf_build (RiscosOtf *f);

#endif
