/*
 * riscos-gnumeric.c - Gnumeric's start on RISC OS (riscos-gtk's !Gnumeric).
 *
 * Gnumeric's own main() is renamed gnumeric_main (build/build-gnumeric.sh);
 * this one comes first:
 *   - Gnumeric's and GOffice's plugins (functions, file formats, chart
 *     types) are linked in: they are registered with GLib's static module
 *     registry (riscos_register_static_modules, made by
 *     tools/mkstaticmodules.py) before Gnumeric looks for them;
 *   - fonts and GTK's data from the application directory
 *     (FONTCONFIG_FILE, XDG_DATA_DIRS); Gnumeric's and GOffice's files from
 *     /<Gnumeric$Dir> and /<GOffice$Dir> (their patches);
 *   - settings (GOffice's key file) in <Choices$Write>.Gnumeric
 *     (XDG_CONFIG_HOME);
 *   - messages to <Wimp$ScrapDir>.Gnumeric.Log;
 *   - files given by !Run (RISC OS names) become UnixLib names;
 *   - the icon bar: Select shows Gnumeric's window; Menu gives Info,
 *     Choices... (Gnumeric's Preferences) and Quit; files double-clicked,
 *     dropped on the icon or saved to it open in a window of their own;
 *     PreQuit with unsaved workbooks asks.
 *
 * GPL version 2 or (at your option) version 3, as Gnumeric. Copyright (C)
 * 2026 Andrew Youll.
 */
#include <gnumeric-config.h>
#include <gnumeric.h>
#include <application.h>
#include <workbook-priv.h>
#include <workbook-view.h>
#include <wbc-gtk.h>
#include <gui-file.h>
#include <dialogs/dialogs.h>
#include <goffice/goffice.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gdk/gdkriscos.h>

#ifndef RISCOS_PORT_VERSION
#define RISCOS_PORT_VERSION "dev"
#endif

int gnumeric_main (int argc, char const **argv);
void riscos_register_static_modules (void);

/* The most recent workbook window */
static WBCGtk *
current_wbcg (void)
{
  GList *l;
  WBCGtk *found = NULL;

  for (l = gnm_app_workbook_list (); l && !found; l = l->next)
    {
      Workbook *wb = l->data;
      WORKBOOK_FOREACH_CONTROL (wb, view, control,
        if (!found && GNM_IS_WBC_GTK (control))
          found = WBC_GTK (control););
    }
  return found;
}

static void
show_window (gpointer d)
{
  WBCGtk *wbcg = current_wbcg ();

  if (wbcg)
    gtk_window_present (wbcg_toplevel (wbcg));
}

static gboolean
open_file (const char *unix_path, int filetype, gboolean scrap, gpointer d)
{
  WBCGtk *wbcg = current_wbcg ();
  char *uri = go_filename_to_uri (unix_path);
  gboolean ok = FALSE;

  if (wbcg && uri)
    ok = gui_file_read (wbcg, uri, NULL, NULL) != NULL;
  g_free (uri);
  show_window (d);
  return ok;
}

static void
choices (gpointer d)
{
  WBCGtk *wbcg = current_wbcg ();

  if (wbcg)
    dialog_preferences (wbcg, NULL);
}

static gboolean
unsaved (gpointer d)
{
  GList *l;

  for (l = gnm_app_workbook_list (); l; l = l->next)
    if (go_doc_is_dirty (GO_DOC (l->data)))
      return TRUE;
  return FALSE;
}

static const GdkRiscosIconbarHandlers handlers = {
  show_window, open_file, choices, NULL /* quit: File > Quit */, unsaved
};

static gboolean
is_riscos_name (const char *name)
{
  return name[0] != '/' && (strchr (name, ':') || name[0] == '$' || name[0] == '<'
                            || name[0] == '@' || name[0] == '&');
}

int
main (int argc, char **argv)
{
  static const int types[] = { 0xDFE };       /* CSV */
  int fd, i;

  riscos_register_static_modules ();
  g_setenv ("FONTCONFIG_FILE", "/<Gnumeric$Dir>/fonts/fonts.conf", FALSE);
  g_setenv ("XDG_DATA_DIRS", "/<Gnumeric$Dir>/share", FALSE);
  if (!g_getenv ("XDG_CONFIG_HOME") && g_getenv ("Choices$Write"))
    {
      mkdir ("/<Choices$Write>/Gnumeric", 0755);
      g_setenv ("XDG_CONFIG_HOME", "/<Choices$Write>/Gnumeric", TRUE);
    }

  gdk_riscos_iconbar_set_handlers (&handlers, NULL);
  gdk_riscos_iconbar_set_info ("Gnumeric", "Spreadsheet",
                               "\xc2\xa9 The Gnumeric developers; RISC OS port Andrew Youll",
                               RISCOS_PORT_VERSION);
  for (i = 0; i < (int) G_N_ELEMENTS (types); i++)
    gdk_riscos_iconbar_add_filetype (types[i]);

  if (!g_getenv ("GNUMERIC_NO_LOG"))
    {
      fd = open ("/<Wimp$ScrapDir>/Gnumeric/Log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd >= 0)
        {
          dup2 (fd, 1);
          dup2 (fd, 2);
          if (fd > 2)
            close (fd);
          setvbuf (stdout, NULL, _IOLBF, 0);
          setvbuf (stderr, NULL, _IONBF, 0);
        }
    }

  for (i = 1; i < argc; i++)
    if (argv[i][0] != '-' && is_riscos_name (argv[i]))
      {
        /* if Gnumeric is already running, it gets this file instead */
        gdk_riscos_iconbar_set_start_file (argv[i]);
        argv[i] = gdk_riscos_path_to_unix (argv[i]);
      }
  return gnumeric_main (argc, (char const **) argv);
}
