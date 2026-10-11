/*
 * riscos-geany.c - Geany's start on RISC OS (riscos-gtk's !Geany demo).
 *
 * Geany is linked as its library (libgeany.a) with this main():
 *   - fonts and GTK's data (theme, icons, settings schemas) from the
 *     application directory (FONTCONFIG_FILE, XDG_DATA_DIRS), Geany's own
 *     data from /<Geany$Dir>/share/geany (patches/geany-2.1-riscos.diff),
 *     its settings in <Choices$Write>.geany (XDG_CONFIG_HOME);
 *   - messages to <Wimp$ScrapDir>.Geany.Log;
 *   - files given by !Run (RISC OS names) become UnixLib names;
 *   - files saved without an extension are text (&FFF);
 *   - the icon bar: Select shows Geany's window, Menu gives Info,
 *     Choices... (Geany's Preferences) and Quit; text, HTML, XML, CSS,
 *     JavaScript, JSON and CSV files double-clicked in the Filer, dropped
 *     on the icon or saved to it open in a tab; Quit and PreQuit with
 *     unsaved documents ask (Cancel / Discard).
 *
 * GPL version 2 or later, as Geany. Copyright (C) 2026 Andrew Youll.
 */
#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>

#include "document.h"
#include "main.h"
#include "ui_utils.h"

#ifndef RISCOS_PORT_VERSION
#define RISCOS_PORT_VERSION "dev"
#endif

static void
show_window (gpointer d)
{
  if (main_widgets.window)
    gtk_window_present (GTK_WINDOW (main_widgets.window));
}

static gboolean
open_file (const char *unix_path, int filetype, gboolean scrap, gpointer d)
{
  GeanyDocument *doc;

  if (scrap)
    {
      /* from another program: a new, untitled document with its text */
      gchar *text = NULL;
      if (!g_file_get_contents (unix_path, &text, NULL, NULL))
        return FALSE;
      doc = document_new_file (NULL, NULL, text);
      g_free (text);
    }
  else
    doc = document_open_file (unix_path, FALSE, NULL, NULL);
  show_window (d);
  return doc != NULL;
}

static void
activate_item (const char *name)
{
  GtkWidget *item = main_widgets.window ? ui_lookup_widget (main_widgets.window, name) : NULL;

  if (item)
    gtk_menu_item_activate (GTK_MENU_ITEM (item));
}

static void
choices (gpointer d)
{
  activate_item ("preferences1");
}

static gboolean
unsaved (gpointer d)
{
  guint i;

  for (i = 0; documents_array && i < documents_array->len; i++)
    {
      GeanyDocument *doc = g_ptr_array_index (documents_array, i);
      if (doc->is_valid && doc->changed)
        return TRUE;
    }
  return FALSE;
}

/* Quit: riscos-gtk has asked about unsaved documents already, so Geany
   isn't to ask again */
static void
quit (gpointer d)
{
  guint i;

  for (i = 0; documents_array && i < documents_array->len; i++)
    {
      GeanyDocument *doc = g_ptr_array_index (documents_array, i);
      if (doc->is_valid && doc->changed)
        document_set_text_changed (doc, FALSE);
    }
  main_quit ();
}

static const GdkRiscosIconbarHandlers handlers = {
  show_window, open_file, choices, quit, unsaved
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
  static const int types[] = { 0xFFF, 0xFAF, 0xF80, 0xF79, 0xF81, 0xF13, 0xDFE };
  int fd, i;

  g_setenv ("FONTCONFIG_FILE", "/<Geany$Dir>/fonts/fonts.conf", FALSE);
  g_setenv ("XDG_DATA_DIRS", "/<Geany$Dir>/share", FALSE);
  g_setenv ("XDG_CONFIG_HOME", "/<Choices$Write>", FALSE);
  g_setenv ("RISCOS_GTK_DEFAULT_FILETYPE", "fff", FALSE);

  gdk_riscos_iconbar_set_handlers (&handlers, NULL);
  gdk_riscos_iconbar_set_info ("Geany", "Programmer's text editor",
                               "\xc2\xa9 The Geany contributors; RISC OS port Andrew Youll",
                               RISCOS_PORT_VERSION);
  for (i = 0; i < (int) G_N_ELEMENTS (types); i++)
    gdk_riscos_iconbar_add_filetype (types[i]);

  if (!g_getenv ("GEANY_NO_LOG"))
    {
      fd = open ("/<Wimp$ScrapDir>/Geany/Log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
        /* if Geany is already running, it gets this file instead */
        gdk_riscos_iconbar_set_start_file (argv[i]);
        argv[i] = gdk_riscos_path_to_unix (argv[i]);
      }
  return main_lib (argc, argv);
}
