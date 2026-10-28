/*
 * gdkriscosfonts.c - riscos-gtk: the desktop's own fonts for every GTK
 * program. The RISC OS Font Manager's outline fonts (Homerton, Trinity,
 * Corpus, and every outline font on Font$Path) become OpenType files fontconfig, FreeType
 * and pango use like any other font: the same outlines the Font Manager
 * draws, made on this machine for its own use.
 *
 * - The fonts: Font_ListFonts, which searches every folder on Font$Path
 *   (!Fonts, and the folders font managers such as Fontastic add), and
 *   Font_ReadFontPrefix for the folder each font is in. Nothing here
 *   assumes !Fonts. WIMPSymbol (the Wimp's own symbols) is left out.
 * - Each face's characters: Font_EnumerateCharacters on the font in the
 *   UTF-8 encoding ("\F<name>\EUTF8"); characters the encoding maps to the
 *   same glyph share one.
 * - Each character's outline: painted at 100 points with the Font
 *   Manager's output switched to a buffer (Font_SwitchOutputToBuffer, no
 *   hints, an error for bitmap-only characters), which gives Draw path
 *   objects: moves, lines and cubic curves, in Draw units (1/640 point:
 *   64000 to the em at 100 points). Its advance: Font_ScanString
 *   (millipoints). The font's ascent and descent: Font_ReadInfo.
 * - The file: gdkriscosotf.c (CFF outlines), 1000 units to the em. The
 *   family is the name's first part(s) (Homerton), the style the rest
 *   (Bold.Oblique: "Bold Oblique", weight 700, slanted; Medium is the
 *   regular weight, 400).
 * - Kept in <Choices$Write>.GTK.FontCache (or <Wimp$ScrapDir>.GTK-
 *   FontCache without Choices), one file per face, made again when the
 *   font's files change (their names, sizes and dates are the key, in
 *   the cache's Index) or this converter does. Each is added to
 *   fontconfig as an application font (FcConfigAppFontAddFile).
 * - The Log says how many faces there are and how many were made.
 * - *Set RISCOS_GTK_RISCOS_FONTS 0 turns it off.
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as GTK.
 */
#include "config.h"

#include "gdkriscosotf.h"
#include "gdkriscosfonts.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <fontconfig/fontconfig.h>
#include <kernel.h>
#include <string.h>
#include <stdlib.h>

/* the converter's version: the cached files are made again when it changes */
#define CONVERTER "1"

#define Font_FindFont             0x40081
#define Font_LoseFont             0x40082
#define Font_ReadInfo             0x40084
#define Font_Paint                0x40086
#define Font_ListFonts            0x40091
#define Font_ReadFontPrefix       0x4009D
#define Font_SwitchOutputToBuffer 0x4009E
#define Font_ScanString           0x400A1
#define Font_EnumerateCharacters  0x400A9
#define ColourTrans_SetFontColours 0x4074F
#define OS_GBPB                   0x0C

#define DRAW_PER_UNIT 64        /* Draw units to a font unit: 64000 / 1000 */
#define BUF_BYTES (64 * 1024)

/* ---- names ---- */

static const struct { const char *word; int weight; } weights[] = {
  { "Thin", 100 }, { "Hairline", 100 }, { "ExtraLight", 200 }, { "UltraLight", 200 },
  { "Light", 300 }, { "Book", 400 }, { "Regular", 400 }, { "Roman", 400 }, { "Medium", 400 },
  { "Normal", 400 }, { "SemiBold", 600 }, { "DemiBold", 600 }, { "Demi", 600 },
  { "Bold", 700 }, { "ExtraBold", 800 }, { "UltraBold", 800 }, { "Heavy", 900 },
  { "Black", 900 }, { "Ultra", 900 },
};
static const struct { const char *word; int width; } widths[] = {
  { "UltraCondensed", 1 }, { "ExtraCondensed", 2 }, { "Condensed", 3 }, { "Narrow", 3 },
  { "SemiCondensed", 4 }, { "SemiExpanded", 6 }, { "Expanded", 7 }, { "Extended", 7 },
  { "Wide", 7 }, { "ExtraExpanded", 8 }, { "UltraExpanded", 9 },
};

static gboolean
style_word (const char *w, int *weight, int *width, gboolean *italic)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (weights); i++)
    if (g_ascii_strcasecmp (w, weights[i].word) == 0)
      {
        *weight = weights[i].weight;
        return TRUE;
      }
  for (i = 0; i < G_N_ELEMENTS (widths); i++)
    if (g_ascii_strcasecmp (w, widths[i].word) == 0)
      {
        *width = widths[i].width;
        return TRUE;
      }
  if (g_ascii_strcasecmp (w, "Italic") == 0 || g_ascii_strcasecmp (w, "Oblique") == 0 ||
      g_ascii_strcasecmp (w, "Slanted") == 0)
    {
      *italic = TRUE;
      return TRUE;
    }
  return FALSE;
}

/* "Homerton.Bold.Oblique" -> family "Homerton", style "Bold Oblique";
   the family is every part before the first style word */
void
_gdk_riscos_font_names (const char *id, char **family, char **style, char **ps_name,
                        int *weight, int *width, gboolean *italic)
{
  char **parts = g_strsplit (id, ".", -1);
  GString *fam = g_string_new (NULL), *sty = g_string_new (NULL), *ps = g_string_new (NULL);
  gboolean in_style = FALSE;
  int i;

  *weight = 400;
  *width = 5;
  *italic = FALSE;
  for (i = 0; parts[i]; i++)
    {
      if (!*parts[i])
        continue;
      if (i > 0 && (in_style || style_word (parts[i], weight, width, italic)))
        {
          if (in_style)
            style_word (parts[i], weight, width, italic);
          in_style = TRUE;
          if (sty->len)
            g_string_append_c (sty, ' ');
          g_string_append (sty, parts[i]);
        }
      else
        {
          if (fam->len)
            g_string_append_c (fam, ' ');
          g_string_append (fam, parts[i]);
        }
    }
  if (!sty->len)
    g_string_append (sty, "Regular");
  for (i = 0; fam->str[i]; i++)
    if (g_ascii_isalnum (fam->str[i]))
      g_string_append_c (ps, fam->str[i]);
  g_string_append_c (ps, '-');
  for (i = 0; sty->str[i]; i++)
    if (g_ascii_isalnum (sty->str[i]))
      g_string_append_c (ps, sty->str[i]);
  *family = g_string_free (fam, FALSE);
  *style = g_string_free (sty, FALSE);
  *ps_name = g_string_free (ps, FALSE);
  g_strfreev (parts);
}

/* ---- the Font Manager ---- */

static GPtrArray *
list_fonts (void)
{
  GPtrArray *ids = g_ptr_array_new_with_free_func (g_free);
  char id[256];
  int count = 0;

  for (;;)
    {
      _kernel_swi_regs r;

      r.r[1] = (int) (long) id;
      r.r[2] = count | (1 << 16);       /* the identifier, 0-terminated */
      r.r[3] = sizeof id;
      r.r[4] = 0;
      r.r[5] = 0;
      if (_kernel_swi (Font_ListFonts, &r, &r) != NULL || r.r[2] == -1)
        break;
      count = r.r[2];
      id[sizeof id - 1] = '\0';
      if (strcmp (id, "WIMPSymbol") != 0)
        g_ptr_array_add (ids, g_strdup (id));
    }
  return ids;
}

/* the key: the font's files (the folder the Font Manager reads it from):
   their names, sizes and dates */
static char *
font_key (int handle)
{
  char prefix[256];
  _kernel_swi_regs r;
  GString *key = g_string_new (CONVERTER);
  int entry = 0;
  char *dir;

  r.r[0] = handle;
  r.r[1] = (int) (long) prefix;
  r.r[2] = sizeof prefix;
  if (_kernel_swi (Font_ReadFontPrefix, &r, &r) != NULL)
    return g_string_free (key, FALSE);
  prefix[sizeof prefix - 1] = '\0';
  g_string_append_printf (key, " %s", prefix);
  dir = g_strdup (prefix);
  if (*dir && dir[strlen (dir) - 1] == '.')
    dir[strlen (dir) - 1] = '\0';
  /* OS_GBPB 10: names with load and exec (the date) and length */
  while (entry != -1)
    {
      guint32 buf[256];
      guint8 *p;
      int n, i;

      r.r[0] = 10;
      r.r[1] = (int) (long) dir;
      r.r[2] = (int) (long) buf;
      r.r[3] = 32;
      r.r[4] = entry;
      r.r[5] = sizeof buf;
      r.r[6] = 0;
      if (_kernel_swi (OS_GBPB, &r, &r) != NULL)
        break;
      n = r.r[3];
      entry = r.r[4];
      for (i = 0, p = (guint8 *) buf; i < n; i++)
        {
          guint32 w[3];
          const char *name = (const char *) (p + 20);

          memcpy (w, p, sizeof w);    /* load, exec (the date), length */
          g_string_append_printf (key, " %s:%x:%x:%x", name, w[0], w[1], w[2]);
          p += (20 + strlen (name) + 1 + 3) & ~3u;
        }
    }
  g_free (dir);
  return g_string_free (key, FALSE);
}

static int
utf8 (guint32 ch, char *s)
{
  int n = g_unichar_to_utf8 (ch, s);

  s[n] = '\0';
  return n;
}

/* one character into the font: painted into the buffer, its Draw path
   objects read back */
static gboolean
add_char (RiscosOtf *otf, int handle, guint32 ch, int *buf, int *glyph)
{
  char s[8];
  int n = utf8 (ch, s), *end, *p;
  _kernel_swi_regs r;
  _kernel_oserror *e;

  buf[0] = 0;
  buf[1] = BUF_BYTES - 8;
  r.r[0] = 1 << 4;                      /* no hints; bitmaps are an error */
  r.r[1] = (int) (long) buf;
  if (_kernel_swi (Font_SwitchOutputToBuffer, &r, &r) != NULL)
    return FALSE;
  r.r[0] = handle;
  r.r[1] = (int) (long) s;
  r.r[2] = (1 << 8) | (1 << 7);         /* this font; the length; millipoints */
  r.r[3] = 0;
  r.r[4] = 0;
  r.r[7] = n;
  e = _kernel_swi (Font_Paint, &r, &r);
  r.r[0] = 0;
  r.r[1] = 0;
  _kernel_swi (Font_SwitchOutputToBuffer, &r, &r);
  end = (int *) (long) r.r[1];
  if (e != NULL || end < buf || end > buf + BUF_BYTES / 4)
    return FALSE;
  /* the advance */
  r.r[0] = handle;
  r.r[1] = (int) (long) s;
  r.r[2] = (1 << 8) | (1 << 7);
  r.r[3] = 0x7FFFFFFF;
  r.r[4] = 0x7FFFFFFF;
  r.r[7] = n;
  if (_kernel_swi (Font_ScanString, &r, &r) != NULL)
    return FALSE;

  riscos_otf_glyph_begin (otf, (r.r[3] + 50) / 100);   /* millipoints at 100 points -> 1/1000 em */
  for (p = buf; p + 2 <= end; )
    {
      int type = p[0], size = p[1];

      if (size < 8 || p + size / 4 > end)
        break;
      if (type == 2)                    /* a path: its elements after 10 words */
        {
          int *q = p + 10, *qe = p + size / 4;

          while (q < qe && (*q & 0xFF) != 0)
            {
              switch (*q & 0xFF)
                {
                case 2:
                  riscos_otf_move_to (otf, q[1] / (double) DRAW_PER_UNIT, q[2] / (double) DRAW_PER_UNIT);
                  q += 3;
                  break;
                case 8:
                  riscos_otf_line_to (otf, q[1] / (double) DRAW_PER_UNIT, q[2] / (double) DRAW_PER_UNIT);
                  q += 3;
                  break;
                case 6:
                  riscos_otf_curve_to (otf, q[1] / (double) DRAW_PER_UNIT, q[2] / (double) DRAW_PER_UNIT,
                                       q[3] / (double) DRAW_PER_UNIT, q[4] / (double) DRAW_PER_UNIT,
                                       q[5] / (double) DRAW_PER_UNIT, q[6] / (double) DRAW_PER_UNIT);
                  q += 7;
                  break;
                case 5:
                  riscos_otf_close (otf);
                  q += 1;
                  break;
                default:
                  q = qe;               /* (not a tag fonts make) */
                }
            }
        }
      p += size / 4;
    }
  *glyph = riscos_otf_glyph_end (otf);
  return TRUE;
}

/* the face as an OpenType file; NULL if it can't be (no outlines) */
static GBytes *
convert (const char *id, int *n_chars)
{
  char *name = g_strdup_printf ("\\F%s\\EUTF8", id);
  char *family, *style, *ps;
  int weight, width, handle, ch = 0, *buf;
  gboolean italic;
  _kernel_swi_regs r;
  RiscosOtf *otf;
  GHashTable *by_code;
  GBytes *bytes = NULL;

  *n_chars = 0;
  r.r[1] = (int) (long) name;
  r.r[2] = r.r[3] = 100 * 16;           /* 100 points */
  r.r[4] = r.r[5] = 0;
  if (_kernel_swi (Font_FindFont, &r, &r) != NULL)
    {
      g_free (name);
      return NULL;
    }
  handle = r.r[0];
  g_free (name);
  r.r[0] = handle;
  r.r[1] = (int) 0xFFFFFF00;            /* (Font_Paint wants colours, even to a buffer) */
  r.r[2] = 0;
  r.r[3] = 14;
  _kernel_swi (ColourTrans_SetFontColours, &r, &r);

  _gdk_riscos_font_names (id, &family, &style, &ps, &weight, &width, &italic);
  otf = riscos_otf_new (1000, family, style, ps, weight, width, italic);
  buf = g_malloc (BUF_BYTES);
  by_code = g_hash_table_new (NULL, NULL);
  for (;;)
    {
      int next, code, glyph;

      r.r[0] = handle;
      r.r[1] = ch;
      if (_kernel_swi (Font_EnumerateCharacters, &r, &r) != NULL)
        break;
      next = r.r[1];
      code = r.r[2];
      if (ch > 0 && code != -1)
        {
          gpointer g;

          if (g_hash_table_lookup_extended (by_code, GINT_TO_POINTER (code), NULL, &g))
            riscos_otf_map (otf, ch, GPOINTER_TO_INT (g));
          else if (add_char (otf, handle, ch, buf, &glyph))
            {
              g_hash_table_insert (by_code, GINT_TO_POINTER (code), GINT_TO_POINTER (glyph));
              riscos_otf_map (otf, ch, glyph);
              (*n_chars)++;
            }
        }
      if (next == -1 || next <= ch)
        break;
      ch = next;
    }
  /* ascent and descent: the font's bounding box, OS units at 100 points
     (250 to the em) */
  r.r[0] = handle;
  if (_kernel_swi (Font_ReadInfo, &r, &r) == NULL)
    riscos_otf_set_extent (otf, r.r[4] * 4, r.r[2] * 4);
  if (*n_chars > 0)
    bytes = riscos_otf_build (otf);
  r.r[0] = handle;
  _kernel_swi (Font_LoseFont, &r, &r);
  g_hash_table_unref (by_code);
  g_free (buf);
  riscos_otf_free (otf);
  g_free (family);
  g_free (style);
  g_free (ps);
  return bytes;
}

static char *
key_of (const char *id)
{
  char *name = g_strdup_printf ("\\F%s\\EUTF8", id);
  _kernel_swi_regs r;
  char *key;

  r.r[1] = (int) (long) name;
  r.r[2] = r.r[3] = 12 * 16;
  r.r[4] = r.r[5] = 0;
  if (_kernel_swi (Font_FindFont, &r, &r) != NULL)
    {
      g_free (name);
      return NULL;
    }
  g_free (name);
  key = font_key (r.r[0]);
  _kernel_swi (Font_LoseFont, &r, &r);
  return key;
}

static char *
cache_dir (void)
{
#ifdef __riscos
  if (g_getenv ("Choices$Write"))
    {
      g_mkdir ("/<Choices$Write>/GTK", 0755);
      return g_strdup ("/<Choices$Write>/GTK/FontCache");
    }
  return g_strdup ("/<Wimp$ScrapDir>/GTK-FontCache");
#else
  /* the tests on Linux: the variable holds the folder itself */
  const char *choices = g_getenv ("Choices$Write");
  char *gtk;

  if (!choices)
    return g_build_filename (g_get_tmp_dir (), "GTK-FontCache", NULL);
  gtk = g_build_filename (choices, "GTK", NULL);
  g_mkdir (gtk, 0755);
  g_free (gtk);
  return g_build_filename (choices, "GTK", "FontCache", NULL);
#endif
}

/* the cache's Index: "<file>\t<key>" lines */
static GHashTable *
read_index (const char *dir)
{
  GHashTable *index = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  char *path = g_build_filename (dir, "Index", NULL);
  char *text = NULL, **lines, **l;

  if (g_file_get_contents (path, &text, NULL, NULL))
    {
      lines = g_strsplit (text, "\n", -1);
      for (l = lines; *l; l++)
        {
          char *tab = strchr (*l, '\t');
          if (tab)
            {
              *tab = '\0';
              g_hash_table_replace (index, g_strdup (*l), g_strdup (tab + 1));
            }
        }
      g_strfreev (lines);
    }
  g_free (text);
  g_free (path);
  return index;
}

static void
write_index (const char *dir, GHashTable *index)
{
  GString *s = g_string_new (NULL);
  GHashTableIter it;
  gpointer k, v;
  char *path = g_build_filename (dir, "Index", NULL);

  g_hash_table_iter_init (&it, index);
  while (g_hash_table_iter_next (&it, &k, &v))
    g_string_append_printf (s, "%s\t%s\n", (char *) k, (char *) v);
  g_file_set_contents (path, s->str, s->len, NULL);
  g_string_free (s, TRUE);
  g_free (path);
}

/* the file a face is kept in: "Homerton-Bold-Oblique.otf" */
static char *
file_name (const char *id)
{
  char *f = g_strdup_printf ("%s.otf", id), *p;

  for (p = f; p[4]; p++)
    if (*p == '.' || *p == '/' || *p == ' ')
      *p = '-';
  return f;
}

int
_gdk_riscos_fonts_add (FcConfig *config, GString *log)
{
  GPtrArray *ids;
  GHashTable *index;
  char *dir;
  gint64 t0 = g_get_monotonic_time ();
  int made = 0, added = 0, failed = 0, unwritten = 0;
  gboolean changed = FALSE;
  guint i;

  if (g_strcmp0 (g_getenv ("RISCOS_GTK_RISCOS_FONTS"), "0") == 0)
    {
      g_string_append (log, "RISC OS fonts off (RISCOS_GTK_RISCOS_FONTS is 0)");
      return 0;
    }
  ids = list_fonts ();
  if (ids->len == 0)
    {
      g_string_append (log, "no RISC OS fonts (the Font Manager lists none)");
      g_ptr_array_free (ids, TRUE);
      return 0;
    }
  dir = cache_dir ();
  g_mkdir (dir, 0755);
  index = read_index (dir);
  for (i = 0; i < ids->len; i++)
    {
      const char *id = g_ptr_array_index (ids, i);
      char *leaf = file_name (id);
      char *path = g_build_filename (dir, leaf, NULL);
      char *key = key_of (id);
      const char *had = g_hash_table_lookup (index, leaf);
      char *no_outlines = key ? g_strconcat ("-", key, NULL) : NULL;

      if (had && no_outlines && strcmp (had, no_outlines) == 0)
        failed++;                       /* (tried before: no outlines) */
      else if (key && (!had || strcmp (had, key) != 0 || !g_file_test (path, G_FILE_TEST_EXISTS)))
        {
          int n_chars;
          GBytes *otf = convert (id, &n_chars);

          if (otf)
            {
              char *tmp = g_strconcat (path, "-new", NULL);
              gsize len;
              const char *data = g_bytes_get_data (otf, &len);

              if (g_file_set_contents (tmp, data, len, NULL) && g_rename (tmp, path) == 0)
                {
                  g_hash_table_replace (index, g_strdup (leaf), g_strdup (key));
                  changed = TRUE;
                  made++;
                }
              else
                {
                  g_unlink (tmp);
                  unwritten++;
                }
              g_free (tmp);
              g_bytes_unref (otf);
            }
          else
            {
              /* no outlines (a bitmap font): remembered, not tried again
                 until its files change */
              g_hash_table_replace (index, g_strdup (leaf), g_strdup (no_outlines));
              changed = TRUE;
              failed++;
            }
        }
      if (g_file_test (path, G_FILE_TEST_EXISTS) &&
          FcConfigAppFontAddFile (config, (const FcChar8 *) path))
        added++;
      g_free (key);
      g_free (no_outlines);
      g_free (path);
      g_free (leaf);
    }
  if (changed)
    write_index (dir, index);
  g_string_append_printf (log, "%d RISC OS faces (%d made now in %.1f s", added, made,
                          (g_get_monotonic_time () - t0) / 1e6);
  if (failed)
    g_string_append_printf (log, "; %d without outlines left out", failed);
  if (unwritten)
    g_string_append_printf (log, "; %d couldn't be written", unwritten);
  g_string_append_printf (log, "), in %s", dir);
  g_hash_table_unref (index);
  g_free (dir);
  g_ptr_array_free (ids, TRUE);
  return added;
}
