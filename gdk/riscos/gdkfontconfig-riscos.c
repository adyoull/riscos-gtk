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

/* one Log line: what fontconfig has */
static void
log_fonts (FcConfig *config, const char *setup)
{
  FcPattern *pat = FcPatternCreate ();
  FcObjectSet *os = FcObjectSetBuild (FC_FAMILY, NULL);
  FcFontSet *fs = FcFontList (config, pat, os);
  FcFontSet *app = FcConfigGetFonts (config, FcSetApplication);
  GHashTable *seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  GString *names = g_string_new (NULL), *dirs = g_string_new (NULL);
  FcStrList *dl;
  FcChar8 *dir;
  char *sans, *serif, *mono;
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
  dl = FcConfigGetFontDirs (config);
  while (dl && (dir = FcStrListNext (dl)) != NULL)
    g_string_append_printf (dirs, "%s%s", dirs->len ? " " : "", dir);
  if (dl)
    FcStrListDone (dl);
  sans = match_family (config, "Sans");
  serif = match_family (config, "Serif");
  mono = match_family (config, "Monospace");
  g_printerr ("riscos-gtk: fonts: %s: %d fonts (%d the program's own) in %u families (%s); "
              "folders %s; Sans is %s, Serif %s, Monospace %s\n",
              setup, n_fonts, app ? app->nfont : 0, g_hash_table_size (seen), names->str,
              dirs->len ? dirs->str : "(none)", sans, serif, mono);
  g_free (sans);
  g_free (serif);
  g_free (mono);
  g_string_free (names, TRUE);
  g_string_free (dirs, TRUE);
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
      setup = g_strdup_printf ("the program's own (%s)", saved_conf ? saved_conf : "fontconfig's default");
    }
  if (config == NULL)
    {
      g_printerr ("riscos-gtk: fonts: no fontconfig set-up loaded\n");
      goto out;
    }
  add_own_fonts (config, own_fonts);
  FcConfigSetCurrent (config);
  log_fonts (config, setup);
  FcConfigDestroy (config);          /* FcConfigSetCurrent holds it */

out:
  g_free (setup);
  g_free (fc_dir);
  g_free (sys_conf);
  g_free (sys_path);
  g_free (own_fonts);
  g_free (saved_conf);
}
