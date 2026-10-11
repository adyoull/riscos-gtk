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
 *   the cache format are the same. The program's own fonts (DejaVu Sans,
 *   in its fonts folder) are added on top as application fonts, so GTK's
 *   default face is always there.
 * - Without it, or if the system's set-up doesn't load, the program uses
 *   its own fonts/fonts.conf (FONTCONFIG_FILE, set by the program before
 *   gtk_init: its fonts only, its cache in <Wimp$ScrapDir>).
 * - *Set RISCOS_GTK_FONTS own makes a program use its own set-up even
 *   with UnixFC installed.
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

void
_gdk_riscos_fontconfig_init (void)
{
  const char *own_conf = g_getenv ("FONTCONFIG_FILE");
  const char *choice = g_getenv ("RISCOS_GTK_FONTS");
  char *fc_dir = NULL, *sys_conf = NULL, *sys_path = NULL, *own_fonts = NULL;
  char *saved_conf = own_conf ? g_strdup (own_conf) : NULL;
  FcConfig *config;

  /* another program's settings mustn't redirect ours */
  g_unsetenv ("FONTCONFIG_SYSROOT");

  if (g_strcmp0 (choice, "own") == 0 || (fc_dir = unixfc_dir ()) == NULL)
    goto own;
  sys_conf = g_build_filename (fc_dir, "fonts", "fonts.conf", NULL);
  sys_path = g_build_filename (fc_dir, "fonts", NULL);
  if (access (sys_conf, R_OK) != 0)
    goto own;

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
      goto own;
    }
  /* the program's own fonts on top: its fonts folder, where its own
     fonts.conf is */
  if (saved_conf)
    {
      own_fonts = g_path_get_dirname (saved_conf);
      if (!FcConfigAppFontAddDir (config, (const FcChar8 *) own_fonts))
        g_printerr ("riscos-gtk: fonts: couldn't add %s\n", own_fonts);
    }
  FcConfigSetCurrent (config);
  FcConfigDestroy (config);          /* FcConfigSetCurrent holds it */
  g_printerr ("riscos-gtk: fonts: the system's (UnixFC), with the program's own from %s\n",
              own_fonts ? own_fonts : "(none)");
  goto out;

own:
  g_printerr ("riscos-gtk: fonts: the program's own (%s)\n",
              saved_conf ? saved_conf : "fontconfig's default");

out:
  g_free (fc_dir);
  g_free (sys_conf);
  g_free (sys_path);
  g_free (own_fonts);
  g_free (saved_conf);
}
