/*
 * riscos-mtpaint.c - mtPaint's start on RISC OS (riscos-gtk's !mtPaint).
 *
 * mtPaint's own main() is renamed mtpaint_main (build/build-mtpaint.sh);
 * this one comes first:
 *   - fonts and GTK's data (theme, icons, settings schemas) from the
 *     application directory (FONTCONFIG_FILE, XDG_DATA_DIRS);
 *   - mtPaint keeps its settings in $HOME/.mtpaint: HOME is
 *     <Choices$Write>.mtPaint, so they go in <Choices$Write>.mtPaint./mtpaint;
 *   - messages to <Wimp$ScrapDir>.mtPaint.Log;
 *   - files given by !Run (RISC OS names) become UnixLib names;
 *   - the icon bar: Select shows mtPaint's window; Menu gives Info,
 *     Choices... (mtPaint's Preferences) and Quit (its File > Quit);
 *     PNG, JPEG, GIF and BMP files double-clicked in the Filer,
 *     dropped on the icon or saved to it are loaded (asking first if the
 *     picture has unsaved changes); PreQuit with unsaved changes asks.
 *
 * GPL version 3 or later, as mtPaint. Copyright (C) 2026 Andrew Youll.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "global.h"
#include "mygtk.h"
#include "memory.h"
#include "vcode.h"
#include "ani.h"
#include "png.h"
#include "mainwindow.h"
#include "canvas.h"
#include "layer.h"
#include "prefs.h"

#include <gdk/gdkriscos.h>

#ifndef RISCOS_PORT_VERSION
#define RISCOS_PORT_VERSION "dev"
#endif

int mtpaint_main (int argc, char *argv[]);

static void
show_window (gpointer d)
{
  if (main_window_)
    gtk_window_present (GTK_WINDOW (GET_REAL_WINDOW (main_window_)));
}

static gboolean
open_file (const char *unix_path, int filetype, gboolean scrap, gpointer d)
{
  int res;

  show_window (d);
  if ((layers_total ? check_layers_for_changes () : check_for_changes ()) == 1)
    return TRUE;                /* Cancel: the file isn't wanted */
  res = do_a_load ((char *) unix_path, undo_load);     /* 0: loaded */
  if (scrap && res == 0)
    {
      /* from another program: not saved anywhere yet */
      set_new_filename (layer_selected, NULL);
      mem_changed = 1;
    }
  return res == 0;
}

static void
choices (gpointer d)
{
  pressed_preferences ();
}

static gboolean
unsaved (gpointer d)
{
  return mem_changed || (layers_total && layers_changed);
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
  static const int types[] = { 0xB60, 0xC85, 0x695, 0x69C };
  int fd, i;

  g_setenv ("FONTCONFIG_FILE", "/<mtPaint$Dir>/fonts/fonts.conf", FALSE);
  g_setenv ("XDG_DATA_DIRS", "/<mtPaint$Dir>/share", FALSE);
  if (!g_getenv ("HOME") && g_getenv ("Choices$Write"))
    {
      mkdir ("/<Choices$Write>/mtPaint", 0755);
      g_setenv ("HOME", "/<Choices$Write>/mtPaint", TRUE);
    }

  gdk_riscos_iconbar_set_handlers (&handlers, NULL);
  gdk_riscos_iconbar_set_info ("mtPaint", "Painting program",
                               "\xc2\xa9 Mark Tyler, Dmitry Groshev; RISC OS port Andrew Youll",
                               RISCOS_PORT_VERSION);
  for (i = 0; i < (int) G_N_ELEMENTS (types); i++)
    gdk_riscos_iconbar_add_filetype (types[i]);

  if (!g_getenv ("MTPAINT_NO_LOG"))
    {
      fd = open ("/<Wimp$ScrapDir>/mtPaint/Log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
        /* if mtPaint is already running, it gets this file instead */
        gdk_riscos_iconbar_set_start_file (argv[i]);
        argv[i] = gdk_riscos_path_to_unix (argv[i]);
      }
  return mtpaint_main (argc, argv);
}
