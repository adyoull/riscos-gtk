/*
 * riscos-l3afpad.c - L3afpad's start on RISC OS (riscos-gtk).
 *
 *   - fonts and GTK's data (theme, icons, settings schemas) from the
 *     application directory: FONTCONFIG_FILE and XDG_DATA_DIRS for this
 *     program only (UnixLib keeps names without '$' to the program);
 *   - messages to <Wimp$ScrapDir>.L3afpad.Log, opened once (RISC OS allows
 *     one write-open of a file) and shared by stdout and stderr;
 *   - a file given by !Run (a RISC OS name, e.g. a text file dropped on
 *     the application) becomes a UnixLib name, as L3afpad expects;
 *   - files saved without an extension are text (&FFF), not data.
 *
 * GPL version 2 or later, as L3afpad. Copyright (C) 2026 Andrew Youll.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <gdk/gdkriscos.h>

void riscos_l3afpad_init (gint *argc, gchar ***argv);

static gboolean
is_riscos_name (const char *name)
{
  return name[0] != '/' && (strchr (name, ':') || name[0] == '$' || name[0] == '<'
                            || name[0] == '@' || name[0] == '&');
}

void
riscos_l3afpad_init (gint *argc, gchar ***argv)
{
  int fd, i;

  g_setenv ("FONTCONFIG_FILE", "/<L3afpad$Dir>/fonts/fonts.conf", FALSE);
  g_setenv ("XDG_DATA_DIRS", "/<L3afpad$Dir>/share", FALSE);
  g_setenv ("RISCOS_GTK_DEFAULT_FILETYPE", "fff", FALSE);

  if (!g_getenv ("L3AFPAD_NO_LOG"))
    {
      fd = open ("/<Wimp$ScrapDir>/L3afpad/Log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

  for (i = 1; i < *argc; i++)
    if ((*argv)[i][0] != '-' && is_riscos_name ((*argv)[i]))
      (*argv)[i] = gdk_riscos_path_to_unix ((*argv)[i]);
}
