/*
 * fontconfig-fake.c - which fontconfig set-up a GTK program gets
 * (gdk/riscos/gdkfontconfig-riscos.c), on the fake RISC OS.
 *
 *   fontconfig-fake APPDIR OUTDIR unixfc    PackMan's FontConfig installed
 *   fontconfig-fake APPDIR OUTDIR own       not installed
 *   fontconfig-fake APPDIR OUTDIR forced    installed, RISCOS_GTK_FONTS=own
 *   fontconfig-fake APPDIR OUTDIR bare      installed, but its config has no
 *                                           rules for serif or monospace (as
 *                                           a UnixFC whose conf.d didn't load)
 *
 * "Installed" is a stand-in made in OUTDIR: a fonts.conf listing a
 * "system" font folder (UnixFont) with the fonts named after the mode
 * (DejaVu Serif and Sans Mono from the suite), a cache folder, and
 * the rules UnixFC's fonts.conf and conf.d have for sans, mono, sans-serif, serif
 * and monospace (fontconfig's fonts.conf and 60-latin.conf).
 * On Linux UnixFC$Dir holds the folder itself (on RISC OS the backend
 * reads it as /<UnixFC$Dir>). APPDIR's own fonts (DejaVu Sans) are the
 * program's. With UnixFC, a font chooser's Serif and Monospace are the
 * system's DejaVu Serif and Sans Mono, and the program's own font is
 * added as an application font. Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <fontconfig/fontconfig.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

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

static gboolean
has_family (const char *family)
{
  FcPattern *pat = FcPatternCreate ();
  FcObjectSet *os = FcObjectSetBuild (FC_FAMILY, NULL);
  FcFontSet *fs = FcFontList (NULL, pat, os);
  gboolean found = FALSE;
  int i;

  for (i = 0; fs && i < fs->nfont && !found; i++)
    {
      FcChar8 *f;
      if (FcPatternGetString (fs->fonts[i], FC_FAMILY, 0, &f) == FcResultMatch &&
          strcmp ((const char *) f, family) == 0)
        found = TRUE;
    }
  if (fs)
    FcFontSetDestroy (fs);
  FcObjectSetDestroy (os);
  FcPatternDestroy (pat);
  return found;
}

/* the family fontconfig gives for a name, after the config's aliases */
static char *
matched_family (const char *name)
{
  FcPattern *pat = FcNameParse ((const FcChar8 *) name), *m;
  FcResult res;
  FcChar8 *f;
  char *family = NULL;

  FcConfigSubstitute (NULL, pat, FcMatchPattern);
  FcDefaultSubstitute (pat);
  m = FcFontMatch (NULL, pat, &res);
  if (m && FcPatternGetString (m, FC_FAMILY, 0, &f) == FcResultMatch)
    family = g_strdup ((const char *) f);
  if (m)
    FcPatternDestroy (m);
  FcPatternDestroy (pat);
  return family;
}

static int
count_files (const char *dir, const char *suffix)
{
  DIR *d = opendir (dir);
  struct dirent *e;
  int n = 0;

  if (!d)
    return 0;
  while ((e = readdir (d)))
    if (g_str_has_suffix (e->d_name, suffix))
      n++;
  closedir (d);
  return n;
}

int
main (int argc, char **argv)
{
  const char *appdir, *outdir, *mode;
  char *fc, *fcfonts, *sysfonts, *cache, *conf, *own_conf;
  char *text;
  const char *now;
  int i;

  if (argc < 4)
    return 2;
  appdir = argv[1];
  outdir = argv[2];
  mode = argv[3];

  /* the stand-in for PackMan's FontConfig: its fonts (argv[4...]) in
     UnixFont, its cache folder, and serif and monospace as its conf.d
     makes them */
  fc = g_build_filename (outdir, "UnixFC", NULL);
  fcfonts = g_build_filename (fc, "fonts", NULL);
  sysfonts = g_build_filename (outdir, "UnixFont", NULL);
  cache = g_build_filename (outdir, "UnixFC-cache", NULL);
  g_mkdir_with_parents (fcfonts, 0755);
  g_mkdir_with_parents (sysfonts, 0755);
  g_mkdir_with_parents (cache, 0755);
  for (i = 4; i < argc; i++)
    {
      char *data, *base = g_path_get_basename (argv[i]);
      char *dst = g_build_filename (sysfonts, base, NULL);
      gsize len;
      if (g_file_get_contents (argv[i], &data, &len, NULL))
        {
          g_file_set_contents (dst, data, len, NULL);
          g_free (data);
        }
      g_free (dst);
      g_free (base);
    }
  {
    gboolean bare = strcmp (mode, "bare") == 0;
    text = g_strdup_printf ("<?xml version=\"1.0\"?>\n<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n"
                            "<fontconfig>\n  <dir>%s</dir>\n  <cachedir>%s</cachedir>\n%s</fontconfig>\n",
                            sysfonts, cache, bare ? "" :
                            "  <match target=\"pattern\"><test qual=\"any\" name=\"family\"><string>sans</string></test><edit name=\"family\" mode=\"assign\" binding=\"same\"><string>sans-serif</string></edit></match>\n"
                            "  <match target=\"pattern\"><test qual=\"any\" name=\"family\"><string>mono</string></test><edit name=\"family\" mode=\"assign\" binding=\"same\"><string>monospace</string></edit></match>\n"
                            "  <alias><family>sans-serif</family><prefer><family>DejaVu Sans</family></prefer></alias>\n"
                            "  <alias><family>serif</family><prefer><family>DejaVu Serif</family></prefer></alias>\n"
                            "  <alias><family>monospace</family><prefer><family>DejaVu Sans Mono</family></prefer></alias>\n");
  }
  conf = g_build_filename (fcfonts, "fonts.conf", NULL);
  g_file_set_contents (conf, text, -1, NULL);
  g_free (text);

  /* the program's own set-up, as riscos_gtk_app_init sets it */
  own_conf = g_build_filename (appdir, "fonts", "fonts.conf", NULL);
  g_setenv ("FONTCONFIG_FILE", own_conf, TRUE);
  g_setenv ("XDG_DATA_DIRS", g_build_filename (appdir, "share", NULL), TRUE);
  g_setenv ("FONTCONFIG_SYSROOT", "/somewhere/else", TRUE);    /* another program's */
  if (strcmp (mode, "own") != 0)
    g_setenv ("UnixFC$Dir", fc, TRUE);
  if (strcmp (mode, "forced") == 0)
    g_setenv ("RISCOS_GTK_FONTS", "own", TRUE);

  fake_set_screen (1280, 720, 0, 5);
  fake_wimp_nulls = 1;
  gtk_init (&argc, &argv);

  now = g_getenv ("FONTCONFIG_FILE");
  {
    char *sans = matched_family ("Sans"), *serif = matched_family ("Serif");
    char *mono = matched_family ("Monospace");
    FcFontSet *app = FcConfigGetFonts (NULL, FcSetApplication);
    char *own_dir = g_build_filename (appdir, "fonts", NULL);
    int own_files = count_files (own_dir, ".ttf");

    printf ("# mode %s: FONTCONFIG_FILE %s; DejaVu Sans %d, DejaVu Serif %d; caches in UnixFC's folder %d;"
            " Sans %s, Serif %s, Monospace %s; application fonts %d of %d\n",
            mode, now, has_family ("DejaVu Sans"), has_family ("DejaVu Serif"),
            count_files (cache, ".cache-8") + count_files (cache, ".cache-9"),
            sans, serif, mono, app ? app->nfont : 0, own_files);
    check (g_getenv ("FONTCONFIG_SYSROOT") == NULL, "another program's FONTCONFIG_SYSROOT is ignored");
    check (has_family ("DejaVu Sans"), "the program's own font (DejaVu Sans) is there");
    check (g_strcmp0 (sans, "DejaVu Sans") == 0, "Sans is DejaVu Sans");
    if (strcmp (mode, "unixfc") == 0 || strcmp (mode, "bare") == 0)
      {
        check (g_strcmp0 (now, conf) == 0, "UnixFC installed: its fonts.conf is used");
        check (has_family ("DejaVu Serif") && has_family ("DejaVu Sans Mono"),
               "and the system's fonts (UnixFont) are there");
        check (count_files (cache, ".cache-8") + count_files (cache, ".cache-9") >= 1,
               "and the font cache is the system's (UnixFC's cache folder)");
        check (g_strcmp0 (serif, "DejaVu Serif") == 0 && g_strcmp0 (mono, "DejaVu Sans Mono") == 0,
               strcmp (mode, "bare") == 0
               ? "its config has no serif or monospace rules: riscos-gtk's fallbacks give DejaVu Serif and Sans Mono"
               : "a font chooser's Serif and Monospace are the system's DejaVu Serif and Sans Mono");
        check (app && app->nfont == own_files && own_files > 0,
               "every font file in the program's fonts folder is added as its own");
      }
    else
      {
        check (g_strcmp0 (now, own_conf) == 0, strcmp (mode, "own") == 0
               ? "no UnixFC: the program's own fonts.conf is used"
               : "RISCOS_GTK_FONTS=own: the program's own fonts.conf is used");
        check (!has_family ("DejaVu Serif"), "and the system's fonts aren't");
        check (g_strcmp0 (serif, "DejaVu Sans") == 0,
               "Serif falls back to DejaVu Sans without UnixFC");
      }
    g_free (sans); g_free (serif); g_free (mono); g_free (own_dir);
  }
  printf ("1..%d\n", n_tests);
  return n_failed;
}
