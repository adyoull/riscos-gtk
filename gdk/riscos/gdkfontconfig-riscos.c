/*
 * gdkfontconfig-riscos.c - riscos-gtk: which fontconfig set-up a program
 * uses. Called when the display opens, before pango first asks
 * fontconfig for a font.
 *
 * - With PackMan's FontConfig installed (UnixFC: it sets UnixFC$Dir and
 *   has fonts/fonts.conf), the program uses the system's set-up, as other
 *   fontconfig programs on the machine do: its fonts (UnixFont:), its
 *   font mapping and its font cache, built once and shared. riscos-gtk
 *   links fontconfig 2.14.1, the version PackMan has, so the config and
 *   the cache format are the same: every font PackMan installs in
 *   UnixFont: (and the aliases its fonts.conf gives: serif, monospace)
 *   is the program's to use. The program's own fonts (DejaVu Sans, in
 *   its fonts folder) are added on top as application fonts, so GTK's
 *   default face is always there.
 * - Without it, or if the system's set-up doesn't load, the program uses
 *   its own fonts/fonts.conf (FONTCONFIG_FILE, set by the program before
 *   gtk_init: its fonts only, its cache in <Wimp$ScrapDir>).
 * - *Set RISCOS_GTK_FONTS own makes a program use its own set-up even
 *   with UnixFC installed.
 * - The program's own fonts are added one file at a time
 *   (FcConfigAppFontAddFile), any its fonts.conf's folder didn't give:
 *   FileCore doesn't change a folder's date when files are added, so a
 *   font cache made before (an older copy of the program) would hide new
 *   ones.
 * - Fallback rules for the generic families, after the set-up's own:
 *   sans-serif, serif and monospace get DejaVu Sans, DejaVu Serif and
 *   DejaVu Sans Mono (then Liberation, FreeFont) appended right after
 *   them, so a font chooser's Serif and Monospace find those fonts when
 *   they're installed, even if the system's conf.d (fontconfig's
 *   60-latin.conf) didn't load or lacks them. The system's own
 *   preferences (which come first) still win.
 * - Families documents name that the machine is unlikely to have (Times
 *   New Roman, Arial, Courier New, Calibri; RISC OS's Trinity, Homerton,
 *   Corpus): their metric-compatible stand-ins (Liberation, Carlito,
 *   Caladea) if installed, else the generic family, so a document's
 *   fonts aren't all drawn alike (document_rules).
 * - Emoji: pango asks for the family "emoji"; the fallbacks append the
 *   colour emoji fonts (Noto Color Emoji...) and Symbola to it. Programs
 *   don't carry an emoji font: riscos-gtk's optional PackMan package
 *   TTF-Noto-Color-Emoji puts Noto Color Emoji in UnixFont (so it needs
 *   FontConfig, UnixFC); without one the Log line says so. Colour
 *   emoji fonts are bitmaps (109 pixels high in Noto's case), which
 *   fontconfig's 10-scale-bitmap-fonts.conf scales to the text's size;
 *   when the set-up's config files don't include it, its rules are added
 *   too, or an emoji is drawn ten times the size of the text around it.
 * - The desktop's own fonts (the Font Manager's: Homerton, Trinity,
 *   Corpus, and every font on Font$Path) are added as OpenType copies made on this
 *   machine (gdkriscosfonts.c), so they're in every font list and drawn
 *   as the desktop draws them; Trinity, Homerton and Corpus then match
 *   by name before their document_rules fallbacks, and they end the
 *   generic families' fallbacks: without FontConfig's fonts, Serif is
 *   Trinity and Monospace Corpus (Sans stays the program's DejaVu Sans).
 * - The Log gets one line saying what fontconfig has: the set-up, how
 *   many fonts and families (some named), the font folders, and what
 *   Sans, Serif and Monospace become.
 *
 * FONTCONFIG_PATH is where fontconfig finds files a config includes by a
 * relative name (conf.d), so it's set to UnixFC's folder too. Only this
 * program's environment is changed (setenv of a name without '$').
 * As riscos-warzone2100's riscos/riscos_fontconfig.c does.
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as GTK.
 */
#include "config.h"

#include "gdkprivate-riscos.h"
#include "gdkriscosfonts.h"

#include <glib.h>
#include <fontconfig/fontconfig.h>
#include <string.h>
#include <unistd.h>

static char *
unixfc_dir (void)
{
  /* getenv() of a name with a '$' reads the RISC OS variable */
  const char *dir = g_getenv ("UnixFC$Dir");

  if (dir == NULL || *dir == '\0')
    return NULL;
#ifdef __riscos
  /* a UnixLib path, expanded when a file is opened */
  return g_strdup ("/<UnixFC$Dir>");
#else
  /* the tests on Linux: the variable holds the folder itself */
  return g_strdup (dir);
#endif
}

/* the program's fonts folder's font files fontconfig doesn't have yet,
   added one by one; returns how many the program has there */
static int
add_own_fonts (FcConfig *config, const char *dir)
{
  FcFontSet *sets[2];
  GDir *d;
  const char *leaf;
  int n = 0;

  if (dir == NULL || (d = g_dir_open (dir, 0, NULL)) == NULL)
    return 0;
  sets[0] = FcConfigGetFonts (config, FcSetSystem);
  sets[1] = FcConfigGetFonts (config, FcSetApplication);
  while ((leaf = g_dir_read_name (d)) != NULL)
    {
      char *lower = g_ascii_strdown (leaf, -1);
      gboolean font = g_str_has_suffix (lower, ".ttf") || g_str_has_suffix (lower, ".otf") ||
                      g_str_has_suffix (lower, ".ttc") || g_str_has_suffix (lower, ".pfb");
      char *path;
      gboolean have = FALSE;
      int s, i;

      g_free (lower);
      if (!font)
        continue;
      n++;
      path = g_build_filename (dir, leaf, NULL);
      for (s = 0; s < 2 && !have; s++)
        for (i = 0; sets[s] && i < sets[s]->nfont && !have; i++)
          {
            FcChar8 *f;
            if (FcPatternGetString (sets[s]->fonts[i], FC_FILE, 0, &f) == FcResultMatch &&
                strcmp ((const char *) f, path) == 0)
              have = TRUE;
          }
      if (!have && !FcConfigAppFontAddFile (config, (const FcChar8 *) path))
        g_printerr ("riscos-gtk: fonts: couldn't add %s\n", path);
      g_free (path);
      if (!have)
        {
          sets[1] = FcConfigGetFonts (config, FcSetApplication);
        }
    }
  g_dir_close (d);
  return n;
}

static char *
match_family (FcConfig *config, const char *name)
{
  FcPattern *pat = FcNameParse ((const FcChar8 *) name), *m;
  FcResult res;
  FcChar8 *f;
  char *family = NULL;

  FcConfigSubstitute (config, pat, FcMatchPattern);
  FcDefaultSubstitute (pat);
  m = FcFontMatch (config, pat, &res);
  if (m && FcPatternGetString (m, FC_FAMILY, 0, &f) == FcResultMatch)
    family = g_strdup ((const char *) f);
  if (m)
    FcPatternDestroy (m);
  FcPatternDestroy (pat);
  return family ? family : g_strdup ("(none)");
}

/* Families documents name (Word's, the Mac's, RISC OS's own) that the
   machine is unlikely to have: their metric-compatible stand-ins
   (Liberation, Carlito, Caladea) when installed, else the generic family
   they belong to, so Times New Roman looks like a serif font and Courier
   New like a monospaced one, not all like DejaVu Sans (as fontconfig's
   30-metric-aliases and 45-latin, which a set-up may not have). Loaded
   before generic_rules, whose serif/sans-serif/monospace rules follow. */
static const char document_rules[] =
  "<?xml version=\"1.0\"?>\n"
  "<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n"
  "<fontconfig>\n"
  " <alias binding=\"same\"><family>Times New Roman</family><accept><family>Liberation Serif</family></accept><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Times</family><accept><family>Liberation Serif</family></accept><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Cambria</family><accept><family>Caladea</family></accept><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Georgia</family><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Garamond</family><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Book Antiqua</family><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Palatino Linotype</family><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Palatino</family><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Century Schoolbook</family><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Bookman Old Style</family><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Constantia</family><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>New York</family><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Trinity</family><default><family>serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Arial</family><accept><family>Liberation Sans</family></accept><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Helvetica</family><accept><family>Liberation Sans</family></accept><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Arial Narrow</family><accept><family>Liberation Sans Narrow</family></accept><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Calibri</family><accept><family>Carlito</family></accept><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Verdana</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Tahoma</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Segoe UI</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Trebuchet MS</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Lucida Sans</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Lucida Grande</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Gill Sans</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Century Gothic</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Candara</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Corbel</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Franklin Gothic</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Helvetica Neue</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Geneva</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Homerton</family><default><family>sans-serif</family></default></alias>\n"
  " <alias binding=\"same\"><family>Courier New</family><accept><family>Liberation Mono</family></accept><default><family>monospace</family></default></alias>\n"
  " <alias binding=\"same\"><family>Courier</family><accept><family>Liberation Mono</family></accept><default><family>monospace</family></default></alias>\n"
  " <alias binding=\"same\"><family>Consolas</family><default><family>monospace</family></default></alias>\n"
  " <alias binding=\"same\"><family>Lucida Console</family><default><family>monospace</family></default></alias>\n"
  " <alias binding=\"same\"><family>Monaco</family><default><family>monospace</family></default></alias>\n"
  " <alias binding=\"same\"><family>Andale Mono</family><default><family>monospace</family></default></alias>\n"
  " <alias binding=\"same\"><family>Corpus</family><default><family>monospace</family></default></alias>\n"
  "</fontconfig>\n";

/* the generic families' fallbacks, appended after the set-up's rules;
   "append" puts them right after the generic name, ahead of any
   catch-all a config adds at the end */
static const char generic_rules[] =
  "<?xml version=\"1.0\"?>\n"
  "<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n"
  "<fontconfig>\n"
  " <match target=\"pattern\"><test qual=\"any\" name=\"family\"><string>sans</string></test>"
  "<edit name=\"family\" mode=\"assign\" binding=\"same\"><string>sans-serif</string></edit></match>\n"
  " <match target=\"pattern\"><test qual=\"any\" name=\"family\"><string>mono</string></test>"
  "<edit name=\"family\" mode=\"assign\" binding=\"same\"><string>monospace</string></edit></match>\n"
  " <match target=\"pattern\"><test qual=\"any\" name=\"family\"><string>system-ui</string></test>"
  "<edit name=\"family\" mode=\"append\" binding=\"weak\"><string>sans-serif</string></edit></match>\n"
  " <match target=\"pattern\"><test qual=\"any\" name=\"family\"><string>sans-serif</string></test>"
  "<edit name=\"family\" mode=\"append\" binding=\"weak\"><string>DejaVu Sans</string>"
  "<string>Liberation Sans</string><string>FreeSans</string><string>Homerton</string></edit></match>\n"
  " <match target=\"pattern\"><test qual=\"any\" name=\"family\"><string>serif</string></test>"
  "<edit name=\"family\" mode=\"append\" binding=\"weak\"><string>DejaVu Serif</string>"
  "<string>Liberation Serif</string><string>FreeSerif</string><string>Trinity</string></edit></match>\n"
  " <match target=\"pattern\"><test qual=\"any\" name=\"family\"><string>monospace</string></test>"
  "<edit name=\"family\" mode=\"append\" binding=\"weak\"><string>DejaVu Sans Mono</string>"
  "<string>Liberation Mono</string><string>FreeMono</string><string>Corpus</string></edit></match>\n"
  " <match target=\"pattern\"><test qual=\"any\" name=\"family\"><string>emoji</string></test>"
  "<edit name=\"family\" mode=\"append\" binding=\"strong\"><string>Noto Color Emoji</string>"
  "<string>Apple Color Emoji</string><string>Twitter Color Emoji</string><string>Twemoji</string>"
  "<string>EmojiOne Mozilla</string><string>Emoji Two</string><string>JoyPixels</string>"
  "<string>Noto Emoji</string><string>Symbola</string></edit></match>\n"
  "</fontconfig>\n";

/* fontconfig's conf.d/10-scale-bitmap-fonts.conf (MIT, the fontconfig
   authors): bitmap fonts, colour emoji among them, scaled to the size
   asked for; added when the set-up's config didn't read it */
static const char bitmap_scaling_rules[] =
  "<?xml version=\"1.0\"?>\n"
  "<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n"
  "<fontconfig>\n"
  "<match target=\"font\"> <test name=\"outline\" compare=\"eq\"> <bool>false</bool> </test> <edit name=\"pixe"
  "lsizefixupfactor\" mode=\"assign\"> <divide> <name target=\"pattern\">pixelsize</name> <name target=\"font"
  "\" >pixelsize</name> </divide> </edit> </match> <match target=\"font\"> <test name=\"outline\" compare=\"e"
  "q\"> <bool>false</bool> </test> <test name=\"scalable\" compare=\"eq\"> <bool>false</bool> </test> <test "
  "name=\"hinting\" compare=\"eq\"> <bool>true</bool> </test> <edit name=\"scalingnotneeded\" mode=\"assign\"> "
  "<and> <less> <name>pixelsizefixupfactor</name> <double>1.2</double> </less> <more> <name>pixelsizefi"
  "xupfactor</name> <double>0.8</double> </more> </and> </edit> </match> <match target=\"font\"> <test na"
  "me=\"scalingnotneeded\" compare=\"eq\"> <bool>true</bool> </test> <edit name=\"pixelsizefixupfactor\" mode"
  "=\"assign\"> <double>1.0</double> </edit> </match> <match target=\"font\"> <test name=\"outline\" compare="
  "\"eq\"> <bool>false</bool> </test> <test name=\"pixelsizefixupfactor\" compare=\"not_eq\"> <double>1.0</do"
  "uble> </test> <edit name=\"matrix\" mode=\"assign\"> <times> <name>matrix</name> <matrix> <name>pixelsiz"
  "efixupfactor</name> <double>0</double> <double>0</double> <name>pixelsizefixupfactor</name> </matrix"
  "> </times> </edit> <edit name=\"size\" mode=\"assign\"> <divide> <name>size</name> <name>pixelsizefixupf"
  "actor</name> </divide> </edit> </match>"
  "</fontconfig>\n";

static gboolean
has_config_file (FcConfig *config, const char *leaf_part)
{
  FcStrList *l = FcConfigGetConfigFiles (config);
  FcChar8 *f;
  gboolean found = FALSE;

  while (l && !found && (f = FcStrListNext (l)) != NULL)
    if (strstr ((const char *) f, leaf_part) != NULL)
      found = TRUE;
  if (l)
    FcStrListDone (l);
  return found;
}

/* one Log line: what fontconfig has */
static void
log_fonts (FcConfig *config, const char *setup, gboolean scaling_added, int n_riscos)
{
  FcPattern *pat = FcPatternCreate ();
  FcObjectSet *os = FcObjectSetBuild (FC_FAMILY, NULL);
  FcFontSet *fs = FcFontList (config, pat, os);
  FcFontSet *app = FcConfigGetFonts (config, FcSetApplication);
  GHashTable *seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  GString *names = g_string_new (NULL), *dirs = g_string_new (NULL), *confs = g_string_new (NULL);
  int n_conf;
  FcStrList *dl;
  FcChar8 *dir;
  char *sans, *serif, *mono, *emoji;
  int i, n_fonts = 0;

  /* FcFontList gives one pattern per family here (only FC_FAMILY asked);
     count the fonts from the sets */
  for (i = 0; i < 2; i++)
    {
      FcFontSet *set = FcConfigGetFonts (config, i == 0 ? FcSetSystem : FcSetApplication);
      n_fonts += set ? set->nfont : 0;
    }
  for (i = 0; fs && i < fs->nfont; i++)
    {
      FcChar8 *f;
      if (FcPatternGetString (fs->fonts[i], FC_FAMILY, 0, &f) == FcResultMatch &&
          !g_hash_table_contains (seen, f))
        {
          g_hash_table_add (seen, g_strdup ((const char *) f));
          if (g_hash_table_size (seen) <= 8)
            g_string_append_printf (names, "%s%s", names->len ? ", " : "", f);
        }
    }
  if (g_hash_table_size (seen) > 8)
    g_string_append (names, ", ...");
  n_conf = 0;
  dl = FcConfigGetConfigFiles (config);
  while (dl && (dir = FcStrListNext (dl)) != NULL)
    {
      if (n_conf < 3)
        g_string_append_printf (confs, "%s%s", confs->len ? " " : "", dir);
      n_conf++;
    }
  if (dl)
    FcStrListDone (dl);
  if (n_conf > 3)
    g_string_append (confs, " ...");
  dl = FcConfigGetFontDirs (config);
  while (dl && (dir = FcStrListNext (dl)) != NULL)
    g_string_append_printf (dirs, "%s%s", dirs->len ? " " : "", dir);
  if (dl)
    FcStrListDone (dl);
  sans = match_family (config, "Sans");
  serif = match_family (config, "Serif");
  mono = match_family (config, "Monospace");
  emoji = match_family (config, "emoji");
  g_printerr ("riscos-gtk: fonts: %s: %d fonts (%d the program's own, %d the desktop's) in %u families (%s); "
              "folders %s; %d config files (%s); Sans is %s, Serif %s, Monospace %s, emoji %s%s%s\n",
              setup, n_fonts, (app ? app->nfont : 0) - n_riscos, n_riscos, g_hash_table_size (seen), names->str,
              dirs->len ? dirs->str : "(none)", n_conf, confs->str, sans, serif, mono, emoji,
              scaling_added ? "; bitmap fonts scaled by riscos-gtk's rules" : "",
              /* no emoji font: say where colour emoji come from */
              strstr (emoji, "Emoji") == NULL && strstr (emoji, "Symbola") == NULL
              ? "; for colour emoji install PackMan's TTF-Noto-Color-Emoji (it needs FontConfig)" : "");
  g_free (emoji);
  g_free (sans);
  g_free (serif);
  g_free (mono);
  g_string_free (names, TRUE);
  g_string_free (dirs, TRUE);
  g_string_free (confs, TRUE);
  g_hash_table_destroy (seen);
  if (fs)
    FcFontSetDestroy (fs);
  FcObjectSetDestroy (os);
  FcPatternDestroy (pat);
}

void
_gdk_riscos_fontconfig_init (void)
{
  const char *own_conf = g_getenv ("FONTCONFIG_FILE");
  const char *choice = g_getenv ("RISCOS_GTK_FONTS");
  char *fc_dir = NULL, *sys_conf = NULL, *sys_path = NULL, *own_fonts = NULL;
  char *saved_conf = own_conf ? g_strdup (own_conf) : NULL;
  char *setup = NULL;
  FcConfig *config = NULL;
  gboolean scaling_added = FALSE;
  GString *riscos_fonts;
  int n_riscos;

  /* another program's settings mustn't redirect ours */
  g_unsetenv ("FONTCONFIG_SYSROOT");
  /* the program's own fonts: its fonts folder, where its own fonts.conf is */
  if (saved_conf)
    own_fonts = g_path_get_dirname (saved_conf);

  if (g_strcmp0 (choice, "own") != 0 && (fc_dir = unixfc_dir ()) != NULL)
    {
      sys_conf = g_build_filename (fc_dir, "fonts", "fonts.conf", NULL);
      sys_path = g_build_filename (fc_dir, "fonts", NULL);
      if (access (sys_conf, R_OK) == 0)
        {
          g_setenv ("FONTCONFIG_FILE", sys_conf, TRUE);
          g_setenv ("FONTCONFIG_PATH", sys_path, TRUE);
          config = FcInitLoadConfigAndFonts ();
          if (config == NULL)
            {
              g_printerr ("riscos-gtk: fonts: UnixFC's set-up (%s) didn't load; using the program's own\n",
                          sys_conf);
              if (saved_conf)
                g_setenv ("FONTCONFIG_FILE", saved_conf, TRUE);
              else
                g_unsetenv ("FONTCONFIG_FILE");
              g_unsetenv ("FONTCONFIG_PATH");
            }
          else
            setup = g_strdup_printf ("the system's (UnixFC, %s)", sys_conf);
        }
      else
        g_printerr ("riscos-gtk: fonts: UnixFC$Dir is set but %s can't be read; using the program's own\n",
                    sys_conf);
    }
  if (config == NULL)
    {
      config = FcInitLoadConfigAndFonts ();
      setup = g_strdup_printf ("the program's own (%s%s)", saved_conf ? saved_conf : "fontconfig's default",
                               g_strcmp0 (choice, "own") == 0
                               ? "; RISCOS_GTK_FONTS is own"
                               : fc_dir == NULL
                               ? "; UnixFC$Dir isn't set: PackMan's FontConfig isn't installed, or hasn't been booted since it was"
                               : "");
    }
  if (config == NULL)
    {
      g_printerr ("riscos-gtk: fonts: no fontconfig set-up loaded\n");
      goto out;
    }
  scaling_added = !has_config_file (config, "scale-bitmap-fonts");
  if (!FcConfigParseAndLoadFromMemory (config, (const FcChar8 *) document_rules, FcFalse))
    g_printerr ("riscos-gtk: fonts: the document families' rules didn't load\n");
  if (!FcConfigParseAndLoadFromMemory (config, (const FcChar8 *) generic_rules, FcFalse))
    g_printerr ("riscos-gtk: fonts: the generic families' fallback rules didn't load\n");
  if (scaling_added &&
      !FcConfigParseAndLoadFromMemory (config, (const FcChar8 *) bitmap_scaling_rules, FcFalse))
    g_printerr ("riscos-gtk: fonts: the bitmap scaling rules didn't load\n");
  add_own_fonts (config, own_fonts);
  /* the desktop's own fonts (Homerton, Trinity, Corpus, Font$Path's) */
  riscos_fonts = g_string_new (NULL);
  n_riscos = _gdk_riscos_fonts_add (config, riscos_fonts);
  FcConfigSetCurrent (config);
  g_printerr ("riscos-gtk: fonts: %s\n", riscos_fonts->str);
  g_string_free (riscos_fonts, TRUE);
  log_fonts (config, setup, scaling_added, n_riscos);
  FcConfigDestroy (config);          /* FcConfigSetCurrent holds it */

out:
  g_free (setup);
  g_free (fc_dir);
  g_free (sys_conf);
  g_free (sys_path);
  g_free (own_fonts);
  g_free (saved_conf);
}
