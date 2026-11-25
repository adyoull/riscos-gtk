/*
 * windowless-fake.c - a task that opens no windows (a browser's network
 * or web content task) skips riscos-gtk's window start-up work: no icon
 * bar icon, no Wimp theme (CSS, PNGs, icon theme from the Wimp's
 * sprites). gdk_riscos_set_windowless or RISCOS_GTK_NO_WINDOWS=1.
 *
 *   windowless-fake APPDIR windowless|env|normal
 *   windowless-fake APPDIR theme1|theme2 CHOICES
 * theme1 and theme2 (run in that order with the same CHOICES): the Wimp
 * theme's files are written once to CHOICES/GTK/Theme, in a folder named
 * by their checksum, and the next start uses them as they are.
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

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

/* the theme's icons (from the Wimp's sprites) are on the search path */
static gboolean
wimp_icons (void)
{
  gchar **path = NULL;
  gint n = 0, i;
  gboolean found = FALSE;

  gtk_icon_theme_get_search_path (gtk_icon_theme_get_default (), &path, &n);
  for (i = 0; i < n; i++)
    if (strstr (path[i], "riscos-gtk-"))
      found = TRUE;
  g_strfreev (path);
  return found;
}

int
main (int argc, char **argv)
{
  const char *appdir = argv[1], *mode = argv[2];
  gboolean theme_mode = strncmp (mode, "theme", 5) == 0;
  char *conf = g_build_filename (appdir, "fonts", "fonts.conf", NULL);
  char *share = g_build_filename (appdir, "share", NULL);
  gboolean off = strcmp (mode, "normal") != 0 && !theme_mode;
  time_t started = time (NULL);

  g_setenv ("FONTCONFIG_FILE", conf, TRUE);
  g_setenv ("XDG_DATA_DIRS", share, TRUE);
  if (strcmp (mode, "env") == 0)
    g_setenv ("RISCOS_GTK_NO_WINDOWS", "1", TRUE);
  else if (strcmp (mode, "windowless") == 0)
    gdk_riscos_set_windowless ();
  if (theme_mode)
    g_setenv ("Choices$Write", argv[3], TRUE);
  fake_set_screen (1280, 720, 0, 5);
  fake_wimp_nulls = 1;
  gtk_init (&argc, &argv);
  {
    /* what a program's start makes GTK do: a widget's style */
    GtkWidget *b = gtk_button_new_with_label ("x");
    g_object_ref_sink (b);
    gtk_style_context_get_state (gtk_widget_get_style_context (b));
    g_object_unref (b);
  }
  printf ("# mode %s: windowless %d, icon bar icons %d, Wimp icons %d\n", mode,
          gdk_riscos_get_windowless (), fake_iconbar_icons, wimp_icons ());
  check (gdk_riscos_get_windowless () == off, off ? "the task says it has no windows" : "a normal task");
  check (off ? fake_iconbar_icons == 0 : fake_iconbar_icons > 0,
         off ? "no icon bar icon" : "an icon bar icon");
  check (off ? !wimp_icons () : wimp_icons (),
         off ? "no Wimp theme (no icons made from the Wimp's sprites)" : "the Wimp theme's icons");
  if (theme_mode)
    {
      char *root = g_build_filename (argv[3], "GTK", "Theme", NULL);
      GDir *d = g_dir_open (root, 0, NULL);
      const char *n, *leaf = NULL;
      int folders = 0;
      struct stat st;
      char *done = NULL, *index = NULL;

      while (d && (n = g_dir_read_name (d)) != NULL)
        {
          folders++;
          leaf = n;
          g_free (done);
          g_free (index);
          done = g_build_filename (root, n, "Done", NULL);
          index = g_build_filename (root, n, "icons", "RISCOS-Wimp", "index.theme", NULL);
        }
      printf ("# %s: %d theme folder(s) in %s, last %s\n", mode, folders, root, leaf ? leaf : "-");
      check (folders == 1 && done && stat (done, &st) == 0 && g_file_test (index, G_FILE_TEST_EXISTS),
             "the theme's files are in Choices, in one folder named by their checksum");
      if (strcmp (mode, "theme2") == 0)
        check (done && stat (done, &st) == 0 && st.st_mtime < started,
               "the next start uses them as they are (not written again)");
      if (d)
        g_dir_close (d);
      g_free (done);
      g_free (index);
      g_free (root);
    }
  printf ("1..%d\n", n_tests);
  return n_failed;
}
