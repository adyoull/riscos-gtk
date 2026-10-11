/*
 * gdkschemas-riscos.c - riscos-gtk: GTK's own GSettings schemas for a
 * program that ships none.
 *
 * GTK's file chooser, colour chooser and emoji chooser read GSettings
 * (org.gtk.Settings.FileChooser and so on). If GLib finds no compiled
 * schema for them, it stops the program: "No GSettings schemas are
 * installed on the system", or "Settings schema ... is not installed".
 * riscos-gtk's own packages ship share/glib-2.0/schemas/gschemas.compiled,
 * but a program packaged another way (GCCSDK's autobuilder, say) often
 * doesn't, and then every Open and Save crashes it. Geany did.
 *
 * So when the display opens, before anything asks GSettings, this checks
 * the places GLib will look (GSETTINGS_SCHEMA_DIR, the XDG data folders'
 * glib-2.0/schemas). If none has GTK's file chooser schema, it writes
 * GTK's compiled schemas, built into riscos-gtk (gdkschemas-riscos.h,
 * tools/mkgtkschemas.sh), to <Wimp$ScrapDir>.riscos-gtk-schemas and puts
 * that folder at the front of GSETTINGS_SCHEMA_DIR (this program's
 * environment only). A program's own schemas are still found where they
 * were.
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as GTK.
 */
#include "config.h"

#include "gdkprivate-riscos.h"
#include "gdkschemas-riscos.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>
#include <unistd.h>

#define GTK_SCHEMA "org.gtk.Settings.FileChooser"

static gboolean
dir_has_schema (const char *dir)
{
  GSettingsSchemaSource *source;
  GSettingsSchema *schema = NULL;
  char *file = g_build_filename (dir, "gschemas.compiled", NULL);
  gboolean exists = g_file_test (file, G_FILE_TEST_EXISTS);

  g_free (file);
  if (!exists)
    return FALSE;
  source = g_settings_schema_source_new_from_directory (dir, NULL, FALSE, NULL);
  if (source)
    {
      schema = g_settings_schema_source_lookup (source, GTK_SCHEMA, FALSE);
      g_settings_schema_source_unref (source);
    }
  if (schema)
    g_settings_schema_unref (schema);
  return schema != NULL;
}

static gboolean
installed (const char *env)
{
  const char * const *dirs;
  char **list, *dir;
  gboolean found = FALSE;
  int i;

  if (env && *env)
    {
      list = g_strsplit (env, G_SEARCHPATH_SEPARATOR_S, 0);
      for (i = 0; list[i] && !found; i++)
        found = list[i][0] && dir_has_schema (list[i]);
      g_strfreev (list);
    }
  dir = g_build_filename (g_get_user_data_dir (), "glib-2.0", "schemas", NULL);
  found = found || dir_has_schema (dir);
  g_free (dir);
  for (dirs = g_get_system_data_dirs (); *dirs && !found; dirs++)
    {
      dir = g_build_filename (*dirs, "glib-2.0", "schemas", NULL);
      found = dir_has_schema (dir);
      g_free (dir);
    }
  return found;
}

void
_gdk_riscos_schemas_init (void)
{
  const char *env = g_getenv ("GSETTINGS_SCHEMA_DIR");
  char *dir, *file, *path;
  GError *error = NULL;

  if (installed (env))
    return;

#ifdef __riscos
  dir = g_strdup ("/<Wimp$ScrapDir>/riscos-gtk-schemas");
#else
  dir = g_strdup_printf ("%s/riscos-gtk-schemas-%u", g_get_tmp_dir (), (unsigned) getuid ());
#endif
  file = g_build_filename (dir, "gschemas.compiled", NULL);
  g_mkdir_with_parents (dir, 0755);
  if (!g_file_set_contents (file, (const char *) riscos_gtk_schemas, sizeof riscos_gtk_schemas, &error))
    {
      g_printerr ("riscos-gtk: GSettings: no schemas for GTK's file chooser installed, and "
                  "couldn't write riscos-gtk's own: %s\n", error->message);
      g_error_free (error);
    }
  else
    {
      path = env && *env ? g_strconcat (dir, G_SEARCHPATH_SEPARATOR_S, env, NULL) : g_strdup (dir);
      g_setenv ("GSETTINGS_SCHEMA_DIR", path, TRUE);
      g_printerr ("riscos-gtk: GSettings: the program has no schemas for GTK's file chooser; "
                  "using riscos-gtk's own (%s)\n", dir);
      g_free (path);
    }
  g_free (file);
  g_free (dir);
}
