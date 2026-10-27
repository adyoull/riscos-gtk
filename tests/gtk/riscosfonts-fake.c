/*
 * riscosfonts-fake.c - the desktop's own fonts in GTK programs
 * (gdk/riscos/gdkriscosfonts.c, gdkriscosotf.c), on the fake RISC OS with
 * a stand-in Font Manager:
 *   - Fake.Medium and Fake.Bold.Oblique, outline fonts with 'A' (a square
 *     with a square hole, both contours drawn the same way round, as an
 *     even-odd font may; every other letter too, Latin 1's as well), 'a'
 *     (the same glyph as
 *     A, by its internal code), the Euro (a triangle) and the space;
 *   - Bitmap.Medium, whose characters can't be had as outlines;
 *   - WIMPSymbol (the Wimp's symbols, left out);
 *   - Corpus.Medium (the same glyphs), for the generic families.
 * GTK starts with the program's own fontconfig set-up and
 * Choices$Write = OUTDIR/choices; then:
 *   - fontconfig has the family Fake, its Medium face of weight regular,
 *     upright, and its Bold Oblique bold and slanted; Bitmap and
 *     WIMPSymbol aren't there;
 *   - the files are in Choices.GTK.FontCache, with its Index;
 *   - without FontConfig's fonts, Monospace is the desktop's Corpus (Sans
 *     stays the program's DejaVu Sans);
 *   - pango draws 'A' in Fake with its hole (the nonzero rule after the
 *     contours are turned), 'a' as 'A', and the Euro;
 *   - a second start uses the cache: nothing painted again;
 *   - RISCOS_GTK_RISCOS_FONTS=0 turns it off;
 *   - names: Sassoon.Primary.Bold.Italic is the family "Sassoon Primary",
 *     style "Bold Italic"; Homerton.Medium is regular; Selwyn is Regular.
 *   riscosfonts-fake APPDIR OUTDIR
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <fontconfig/fontconfig.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>

/* (libgdk's, gdkriscosfonts.h) */
int _gdk_riscos_fonts_add (FcConfig *config, GString *log);
void _gdk_riscos_font_names (const char *id, char **family, char **style, char **ps_name,
                             int *weight, int *width, gboolean *italic);

static int n_tests, n_failed;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
  fflush (stdout);
}

/* ---- the stand-in Font Manager ---- */

static const char *const fonts[] = { "Bitmap.Medium", "Corpus.Medium", "Fake.Bold.Oblique", "Fake.Medium", "WIMPSymbol" };
static int paints, bitmap_tries, *out_start, *out_at;

static _kernel_oserror err = { 0, "" };

static _kernel_oserror *
fail (const char *m)
{
  err.errnum = 1;
  snprintf (err.errmess, sizeof err.errmess, "%s", m);
  return &err;
}

static int
font_of (const char *name)
{
  guint i;
  for (i = 0; i < G_N_ELEMENTS (fonts); i++)
    {
      char want[64];
      snprintf (want, sizeof want, "\\F%s\\EUTF8", fonts[i]);
      if (strcmp (name, want) == 0)
        return i + 1;
    }
  return 0;
}

/* a path object: a closed polygon or two */
static void
put_path (const int *pts, int n, const int *pts2, int n2)
{
  int *p = out_at, *q = p + 10, i;

  for (i = 0; i < n; i++)
    {
      *q++ = i == 0 ? 2 : 8;
      *q++ = pts[2 * i];
      *q++ = pts[2 * i + 1];
    }
  *q++ = 5;
  for (i = 0; i < n2; i++)
    {
      *q++ = i == 0 ? 2 : 8;
      *q++ = pts2[2 * i];
      *q++ = pts2[2 * i + 1];
    }
  if (n2)
    *q++ = 5;
  *q++ = 0;
  p[0] = 2;
  p[1] = (q - p) * 4;
  p[2] = p[3] = 0; p[4] = p[5] = 40000;
  p[6] = 0; p[7] = -1; p[8] = 0; p[9] = 64;
  out_at = q;
}

static int
font_hook (int no, _kernel_swi_regs *r, _kernel_oserror **e)
{
  static const int outer[] = { 0, 0, 40000, 0, 40000, 40000, 0, 40000 };
  static const int inner[] = { 10000, 10000, 30000, 10000, 30000, 30000, 10000, 30000 };   /* the same way round */
  static const int tri[] = { 0, 0, 20000, 40000, 40000, 0 };                                /* clockwise */

  switch (no & ~0x20000)
    {
    case 0x40091:       /* Font_ListFonts: identifiers */
      if ((r->r[2] & 0xFFFF) >= (int) G_N_ELEMENTS (fonts))
        r->r[2] = -1;
      else
        {
          strcpy ((char *) (long) r->r[1], fonts[r->r[2] & 0xFFFF]);
          r->r[2] = (r->r[2] & 0xFFFF) + 1;
        }
      return 1;
    case 0x40081:       /* Font_FindFont */
      r->r[0] = font_of ((const char *) (long) r->r[1]);
      if (!r->r[0])
        *e = fail ("Font not found");
      return 1;
    case 0x40082:       /* Font_LoseFont */
    case 0x4074F:       /* ColourTrans_SetFontColours */
      return 1;
    case 0x40084:       /* Font_ReadInfo: OS units at 100 points (250 to the em) */
      r->r[1] = -10; r->r[2] = -50; r->r[3] = 250; r->r[4] = 240;
      return 1;
    case 0x4009D:       /* Font_ReadFontPrefix */
      *e = fail ("not faked");
      return 1;
    case 0x400A9:       /* Font_EnumerateCharacters: the space, A-Z, a-z, Latin 1's letters
                           (English, as fontconfig has it; a's glyph is A's, by
                           its internal code), the Euro */
      {
        int c = r->r[1];
        gboolean letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0xC0 && c <= 0xFF);
        r->r[2] = c == 0x20 ? 1 : c == 'a' ? 'A' : letter ? c : c == 0x20AC ? 0x80 : -1;
        r->r[1] = c < 0x20 ? 0x20 : c == 0x20 ? 'A' : c == 'Z' ? 'a' : c == 'z' ? 0xC0 : c == 0xFF ? 0x20AC :
                  letter ? c + 1 : -1;
        return 1;
      }
    case 0x4009E:       /* Font_SwitchOutputToBuffer */
      if (r->r[1] > 8)
        {
          out_start = out_at = (int *) (long) r->r[1];
          r->r[1] = 0;
        }
      else if (r->r[1] == 0)
        {
          if (out_at)
            *out_at = 0;
          r->r[1] = (int) (long) out_at;
          out_start = out_at = NULL;
        }
      return 1;
    case 0x40086:       /* Font_Paint: one character */
      {
        const char *s = (const char *) (long) r->r[1];
        if (r->r[0] == 1)
          {
            bitmap_tries++;
            *e = fail ("Font cache full");      /* (bitmaps only) */
            return 1;
          }
        paints++;
        if (!out_at)
          return 1;
        if (s[0] != ' ' && (guchar) s[0] != 0xE2)
          put_path (outer, 4, inner, 4);
        else if ((guchar) s[0] == 0xE2)         /* the Euro */
          put_path (tri, 3, NULL, 0);
        return 1;
      }
    case 0x400A1:       /* Font_ScanString: the advance, millipoints */
      {
        const char *s = (const char *) (long) r->r[1];
        r->r[3] = s[0] == ' ' ? 25000 : 70000;
        r->r[4] = 0;
        return 1;
      }
    }
  return 0;
}

/* ---- checks ---- */

static FcPattern *
find_face (const char *family, const char *style)
{
  FcPattern *pat = FcPatternBuild (NULL, FC_FAMILY, FcTypeString, family, FC_STYLE, FcTypeString, style, NULL);
  FcObjectSet *os = FcObjectSetBuild (FC_FAMILY, FC_STYLE, FC_WEIGHT, FC_SLANT, FC_FILE, FC_CHARSET, NULL);
  FcFontSet *fs = FcFontList (NULL, pat, os);
  FcPattern *got = fs && fs->nfont ? FcPatternDuplicate (fs->fonts[0]) : NULL;

  if (fs)
    FcFontSetDestroy (fs);
  FcObjectSetDestroy (os);
  FcPatternDestroy (pat);
  return got;
}

static char *
matched_family (const char *family)
{
  FcPattern *pat = FcNameParse ((const FcChar8 *) family), *m;
  FcResult res;
  FcChar8 *f = NULL;
  char *out;

  FcConfigSubstitute (NULL, pat, FcMatchPattern);
  FcDefaultSubstitute (pat);
  if (getenv ("RF_DEBUG"))
    FcPatternPrint (pat);
  m = FcFontMatch (NULL, pat, &res);
  if (m)
    FcPatternGetString (m, FC_FAMILY, 0, &f);
  out = g_strdup (f ? (const char *) f : "(none)");
  if (m)
    FcPatternDestroy (m);
  FcPatternDestroy (pat);
  return out;
}

static gboolean
has_family (const char *family)
{
  FcPattern *pat = FcPatternBuild (NULL, FC_FAMILY, FcTypeString, family, NULL);
  FcObjectSet *os = FcObjectSetBuild (FC_FAMILY, NULL);
  FcFontSet *fs = FcFontList (NULL, pat, os);
  gboolean has = fs && fs->nfont > 0;

  if (fs)
    FcFontSetDestroy (fs);
  FcObjectSetDestroy (os);
  FcPatternDestroy (pat);
  return has;
}

/* draws text in the font; ink: the ink rectangle; returns the image */
static cairo_surface_t *
draw (const char *font, const char *text, PangoRectangle *ink)
{
  cairo_surface_t *s = cairo_image_surface_create (CAIRO_FORMAT_RGB24, 200, 120);
  cairo_t *cr = cairo_create (s);
  PangoLayout *l = pango_cairo_create_layout (cr);
  PangoFontDescription *d = pango_font_description_from_string (font);

  pango_font_description_set_absolute_size (d, 64 * PANGO_SCALE);     /* 64 pixels to the em */

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);
  cairo_set_source_rgb (cr, 0, 0, 0);
  pango_layout_set_font_description (l, d);
  pango_layout_set_text (l, text, -1);
  cairo_move_to (cr, 10, 10);
  pango_cairo_show_layout (cr, l);
  pango_layout_get_pixel_extents (l, ink, NULL);
  ink->x += 10;
  ink->y += 10;
  pango_font_description_free (d);
  g_object_unref (l);
  cairo_destroy (cr);
  cairo_surface_flush (s);
  return s;
}

static gboolean
dark (cairo_surface_t *s, int x, int y)
{
  guint32 *px = (guint32 *) (cairo_image_surface_get_data (s) + y * cairo_image_surface_get_stride (s));
  return (px[x] & 0xFF) < 100;
}

int
main (int argc, char **argv)
{
  const char *appdir, *outdir;
  char *choices, *cache, *file;
  FcPattern *med, *bold;
  int weight, width;
  gboolean italic;
  char *fam, *sty, *ps;

  if (argc < 3)
    return 2;
  appdir = argv[1];
  outdir = argv[2];
  choices = g_build_filename (outdir, "choices", NULL);
  cache = g_build_filename (choices, "GTK", "FontCache", NULL);
  g_mkdir_with_parents (choices, 0755);
  g_setenv ("Choices$Write", choices, TRUE);
  g_setenv ("FONTCONFIG_FILE", g_build_filename (appdir, "fonts", "fonts.conf", NULL), TRUE);
  g_setenv ("XDG_DATA_DIRS", g_build_filename (appdir, "share", NULL), TRUE);
  g_unsetenv ("UnixFC$Dir");
  fake_swi_hook = font_hook;

  fake_set_screen (1280, 720, 0, 5);
  fake_wimp_nulls = 1;
  gtk_init (&argc, &argv);

  med = find_face ("Fake", "Medium");
  bold = find_face ("Fake", "Bold Oblique");
  {
    int mw = -1, ms = -1, bw = -1, bs = -1;
    if (med) { FcPatternGetInteger (med, FC_WEIGHT, 0, &mw); FcPatternGetInteger (med, FC_SLANT, 0, &ms); }
    if (bold) { FcPatternGetInteger (bold, FC_WEIGHT, 0, &bw); FcPatternGetInteger (bold, FC_SLANT, 0, &bs); }
    printf ("# Fake Medium: weight %d slant %d; Fake Bold Oblique: weight %d slant %d; %d paints\n",
            mw, ms, bw, bs, paints);
    check (med && mw == FC_WEIGHT_REGULAR && ms == FC_SLANT_ROMAN,
           "the RISC OS font Fake.Medium is in fontconfig: family Fake, regular weight, upright");
    check (bold && bw == FC_WEIGHT_BOLD && bs != FC_SLANT_ROMAN,
           "and Fake.Bold.Oblique is its bold, slanted face");
  }
  check (!has_family ("Bitmap") && !has_family ("WIMPSymbol"),
         "a font without outlines and WIMPSymbol are left out");
  {
    FcCharSet *cs = NULL;
    if (med)
      FcPatternGetCharSet (med, FC_CHARSET, 0, &cs);
    check (cs && FcCharSetHasChar (cs, 'A') && FcCharSetHasChar (cs, 'a') && FcCharSetHasChar (cs, 0x20AC)
           && FcCharSetHasChar (cs, 0xE9) && !FcCharSetHasChar (cs, 0x100),
           "its characters are the Font Manager's (A-Z, a-z, Latin 1's letters, the Euro)");
  }
  {
    char *mono = matched_family ("Monospace"), *sans = matched_family ("Sans");
    printf ("# Monospace %s, Sans %s\n", mono, sans);
    check (strcmp (mono, "Corpus") == 0 && strcmp (sans, "DejaVu Sans") == 0,
           "without FontConfig's fonts, Monospace is the desktop's Corpus; Sans stays DejaVu Sans");
    g_free (mono);
    g_free (sans);
  }
  file = g_build_filename (cache, "Fake-Medium.otf", NULL);
  {
    char *index = g_build_filename (cache, "Index", NULL), *text = NULL;
    g_file_get_contents (index, &text, NULL, NULL);
    check (g_file_test (file, G_FILE_TEST_EXISTS) && text && strstr (text, "Fake-Bold-Oblique.otf\t"),
           "the files are kept in Choices.GTK.FontCache, with its Index");
    g_free (text);
    g_free (index);
  }
  {
    PangoRectangle ink, ink2;
    cairo_surface_t *s = draw ("Fake", "A", &ink);
    cairo_surface_t *s2 = draw ("Fake", "a", &ink2);
    int cx = ink.x + ink.width / 2, cy = ink.y + ink.height / 2;
    gboolean hole = !dark (s, cx, cy);
    gboolean ring = dark (s, ink.x + 3, cy) && dark (s, cx, ink.y + 3);

    printf ("# 'A' ink %dx%d at %d,%d: centre %s, edge %s\n", ink.width, ink.height, ink.x, ink.y,
            hole ? "clear" : "inked", ring ? "inked" : "clear");
    check (ink.width >= 38 && ink.width <= 42 && hole && ring,
           "pango draws Fake's A: a square with its hole (contours turned for the nonzero rule)");
    check (ink2.width == ink.width && ink2.height == ink.height && !dark (s2, cx, cy),
           "and 'a', which the font maps to the same glyph, as A");
    cairo_surface_write_to_png (s, g_build_filename (outdir, "A.png", NULL));
    cairo_surface_destroy (s);
    cairo_surface_destroy (s2);
    s = draw ("Fake", "\xE2\x82\xAC", &ink);
    check (ink.width >= 38 && dark (s, ink.x + ink.width / 2, ink.y + ink.height - 3),
           "and the Euro (the triangle the Font Manager gives)");
    cairo_surface_destroy (s);
  }
  {
    FcConfig *config = FcConfigCreate ();
    GString *log = g_string_new (NULL);
    int before = paints, before_bitmap = bitmap_tries, n;

    n = _gdk_riscos_fonts_add (config, log);
    printf ("# again: %s\n", log->str);
    check (n == 3 && paints == before && bitmap_tries == before_bitmap && strstr (log->str, "0 made now"),
           "the next start uses the cache: nothing is painted again (nor the bitmap font tried)");
    g_string_truncate (log, 0);
    g_setenv ("RISCOS_GTK_RISCOS_FONTS", "0", TRUE);
    n = _gdk_riscos_fonts_add (config, log);
    check (n == 0 && strstr (log->str, "off"), "RISCOS_GTK_RISCOS_FONTS=0 turns it off");
    g_unsetenv ("RISCOS_GTK_RISCOS_FONTS");
    g_string_free (log, TRUE);
    FcConfigDestroy (config);
  }
  _gdk_riscos_font_names ("Sassoon.Primary.Bold.Italic", &fam, &sty, &ps, &weight, &width, &italic);
  printf ("# Sassoon.Primary.Bold.Italic: %s | %s | %s | %d %d %d\n", fam, sty, ps, weight, width, italic);
  check (strcmp (fam, "Sassoon Primary") == 0 && strcmp (sty, "Bold Italic") == 0 && weight == 700 && italic &&
         strcmp (ps, "SassoonPrimary-BoldItalic") == 0,
         "names: Sassoon.Primary.Bold.Italic is Sassoon Primary, Bold Italic");
  g_free (fam); g_free (sty); g_free (ps);
  _gdk_riscos_font_names ("Homerton.Medium", &fam, &sty, &ps, &weight, &width, &italic);
  check (strcmp (fam, "Homerton") == 0 && strcmp (sty, "Medium") == 0 && weight == 400 && !italic,
         "Homerton.Medium is Homerton, Medium, the regular weight");
  g_free (fam); g_free (sty); g_free (ps);
  _gdk_riscos_font_names ("Selwyn", &fam, &sty, &ps, &weight, &width, &italic);
  check (strcmp (fam, "Selwyn") == 0 && strcmp (sty, "Regular") == 0, "Selwyn is Selwyn, Regular");
  g_free (fam); g_free (sty); g_free (ps);

  printf ("1..%d\n", n_tests);
  return n_failed;
}
