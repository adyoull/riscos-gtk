/*
 * gdkriscosfonts.h - riscos-gtk: the RISC OS Font Manager's fonts for
 * fontconfig (gdkriscosfonts.c).
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as GTK.
 */
#ifndef __GDK_RISCOS_FONTS_H__
#define __GDK_RISCOS_FONTS_H__

#include <glib.h>
#include <fontconfig/fontconfig.h>

/* the Font Manager's outline fonts, made into OpenType files (kept in
   <Choices$Write>.GTK.FontCache) and added to the config as application
   fonts; what was done is appended to log; returns how many faces */
int _gdk_riscos_fonts_add (FcConfig *config, GString *log);

/* a RISC OS font name ("Homerton.Bold.Oblique") as a family, a style,
   a PostScript name, a weight (100-900), a width (1-9) and a slant */
void _gdk_riscos_font_names (const char *id, char **family, char **style, char **ps_name,
                             int *weight, int *width, gboolean *italic);

#endif
