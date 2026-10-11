/*
 * schemas-fake.c - a GTK program that ships no GSettings schemas can
 * still make a file chooser (gdk/riscos/gdkschemas-riscos.c). Without
 * riscos-gtk's fallback GLib stops the program ("No GSettings schemas are
 * installed on the system"), as Geany built by GCCSDK's autobuilder did
 * on every Open and Save.
 *
 *   schemas-fake APPDIR none        XDG_DATA_DIRS without schemas
 *   schemas-fake APPDIR installed   APPDIR/share has them
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>

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

int
main (int argc, char **argv)
{
  const char *appdir = argv[1], *mode = argv[2];
  char *share, *emptydir, *conf;
  const char *dir;
  GtkWidget *dialog;
  GSettings *settings;

  conf = g_build_filename (appdir, "fonts", "fonts.conf", NULL);
  share = g_build_filename (appdir, "share", NULL);
  emptydir = g_build_filename (g_get_tmp_dir (), "schemas-fake-empty", NULL);
  g_mkdir_with_parents (emptydir, 0755);
  g_setenv ("FONTCONFIG_FILE", conf, TRUE);
  /* the theme and icons, but (for "none") no glib-2.0/schemas */
  g_setenv ("XDG_DATA_DIRS", strcmp (mode, "none") == 0 ? emptydir : share, TRUE);
  g_setenv ("XDG_DATA_HOME", emptydir, TRUE);
  g_unsetenv ("GSETTINGS_SCHEMA_DIR");
  fake_set_screen (1280, 720, 0, 5);
  fake_wimp_nulls = 1;
  gtk_init (&argc, &argv);

  dir = g_getenv ("GSETTINGS_SCHEMA_DIR");
  printf ("# mode %s: GSETTINGS_SCHEMA_DIR %s\n", mode, dir ? dir : "(unset)");
  if (strcmp (mode, "none") == 0)
    check (dir != NULL && strstr (dir, "riscos-gtk-schemas") != NULL,
           "no schemas shipped: riscos-gtk's own are put in scrap for GLib");
  else
    check (dir == NULL, "schemas shipped: riscos-gtk leaves GSETTINGS_SCHEMA_DIR alone");

  /* what GTK's file chooser does when it's made */
  settings = g_settings_new ("org.gtk.Settings.FileChooser");
  check (settings != NULL, "org.gtk.Settings.FileChooser can be read");
  dialog = gtk_file_chooser_dialog_new ("Open File", NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Open", GTK_RESPONSE_ACCEPT, NULL);
  check (GTK_IS_FILE_CHOOSER (dialog), "a GtkFileChooserDialog is made, and the program carries on");
  gtk_widget_destroy (dialog);
  g_object_unref (settings);
  printf ("1..%d\n", n_tests);
  return n_failed;
}
