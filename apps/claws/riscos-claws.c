/*
 * riscos-claws.c - Claws Mail's start on RISC OS (riscos-gtk).
 *
 *   - fonts and GTK's data (theme, icons, settings schemas) from the
 *     application directory: FONTCONFIG_FILE and XDG_DATA_DIRS for this
 *     program only (UnixLib keeps names without '$' to the program);
 *   - Claws Mail's settings (.claws-mail) and, unless another place is
 *     chosen when it first starts, its mailbox (Mail) in
 *     <Choices$Write>.ClawsMail: HOME for this program;
 *   - messages to <Wimp$ScrapDir>.ClawsMail.Log, opened once (RISC OS
 *     allows one write-open of a file) and shared by stdout and stderr;
 *   - the icon bar (riscos-gtk's gdk_riscos_iconbar_*), as riscos-gtk's
 *     other RISC OS programs: Select shows the main window; Menu gives
 *     Info, Choices... (Claws Mail's Preferences), Quit; a file dropped on
 *     the icon starts a new message with it attached (its RISC OS file
 *     type gives the MIME type and, if the name has none, an extension);
 *     a second copy hands its file to the first.
 *   Quitting (and a desktop shutdown) is Claws Mail's own: open messages
 *   are kept in Drafts, and it asks if unsent mail is queued.
 *
 * GPL version 3 or later, as Claws Mail. Copyright (C) 2026 Andrew Youll.
 */
#include "config.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include "main.h"
#include "mainwindow.h"
#include "compose.h"
#include "prefs_gtk.h"
#include "version.h"

#ifndef RISCOS_PORT_VERSION
#define RISCOS_PORT_VERSION "dev"
#endif

void riscos_claws_init (int *argc, char ***argv);

/* RISC OS file types: the MIME type of an attachment, and the extension
   for a name without one (other systems go by it) */
static const struct {
  int filetype;
  const char *mime, *ext;
} types[] = {
  { 0xFFF, "text/plain", "txt" },
  { 0xFAF, "text/html", "html" },
  { 0xF79, "text/css", "css" },
  { 0xDFE, "text/csv", "csv" },
  { 0xF81, "application/javascript", "js" },
  { 0xF80, "application/xml", "xml" },
  { 0xADF, "application/pdf", "pdf" },
  { 0xC85, "image/jpeg", "jpg" },
  { 0xB60, "image/png", "png" },
  { 0x695, "image/gif", "gif" },
  { 0xFF0, "image/tiff", "tif" },
  { 0x69C, "image/bmp", "bmp" },
  { 0xAAD, "image/svg+xml", "svg" },
  { 0xA66, "image/webp", "webp" },
  { 0xFF9, "image/x-riscos-sprite", "spr" },
  { 0xAFF, "image/x-riscos-draw", "aff" },
  { 0xA91, "application/zip", "zip" },
  { 0xDDC, "application/zip", "zip" },
  { 0xF89, "application/gzip", "gz" },
  { 0xC46, "application/x-tar", "tar" },
  { 0xAE7, "application/x-riscos-archive", "arc" },
  { 0xFC9, "audio/mpeg", "mp3" },
  { 0xFB1, "audio/x-wav", "wav" },
  { 0xBF8, "video/mpeg", "mpg" },
  { 0xAE4, "application/java-archive", "jar" },
  { 0xBDF, "application/rtf", "rtf" },
  { 0xAE6, "application/msword", "doc" },
  { 0xBA6, "application/vnd.ms-excel", "xls" },
  { 0xF91, "message/rfc822", "eml" },
};

static void
show (gpointer d)
{
  MainWindow *mainwin = mainwindow_get_mainwindow ();

  if (mainwin)
    main_window_popup (mainwin);
}

/* A file dropped on the icon (or double-clicked, for a type claimed):
   a new message with it attached. Data from another program (scrap) is
   deleted once it's loaded, so it's refused: save it to a file first. */
static gboolean
open_file (const char *path, int filetype, gboolean scrap, gpointer d)
{
  AttachInfo *ainfo;
  const char *leaf = strrchr (path, '/');
  gsize i;

  if (scrap)
    return FALSE;
  leaf = leaf ? leaf + 1 : path;
  ainfo = g_new0 (AttachInfo, 1);
  ainfo->file = g_strdup (path);
  for (i = 0; i < G_N_ELEMENTS (types); i++)
    if (types[i].filetype == filetype)
      {
        ainfo->content_type = g_strdup (types[i].mime);
        /* "Report" (a PDF) is sent as "Report.pdf"; "Report/pdf" (a
           UnixLib name "Report.pdf") already has one */
        if (strchr (leaf, '.') == NULL)
          ainfo->name = g_strdup_printf ("%s.%s", leaf, types[i].ext);
        break;
      }
  compose_new (NULL, NULL, g_list_append (NULL, ainfo));
  return TRUE;
}

static void
choices (gpointer d)
{
  prefs_gtk_open ();
}

static void
quit (gpointer d)
{
  app_will_exit (NULL, mainwindow_get_mainwindow ());
}

/* Claws Mail keeps open messages in Drafts as it quits: nothing is lost,
   so the icon bar doesn't ask */
static gboolean
unsaved (gpointer d)
{
  return FALSE;
}

static const GdkRiscosIconbarHandlers handlers = {
  show, open_file, choices, quit, unsaved
};

static gboolean
is_riscos_name (const char *name)
{
  return name[0] != '/' && name[0] != '-' && g_ascii_strncasecmp (name, "mailto:", 7) != 0 &&
         (strchr (name, ':') || name[0] == '$' || name[0] == '<' || name[0] == '@' || name[0] == '&');
}

void
riscos_claws_init (int *argc, char ***argv)
{
  int fd, i;

  g_setenv ("FONTCONFIG_FILE", "/<ClawsMail$Dir>/fonts/fonts.conf", FALSE);
  g_setenv ("XDG_DATA_DIRS", "/<ClawsMail$Dir>/share", FALSE);
  g_setenv ("HOME", "/<ClawsMail$Choices>", FALSE);
  g_set_prgname ("ClawsMail");
  g_set_application_name ("Claws Mail");

  gdk_riscos_iconbar_set_handlers (&handlers, NULL);
  gdk_riscos_iconbar_set_sprite ("!clawsmail");
  gdk_riscos_iconbar_set_info ("Claws Mail", "Email client",
                               "\xc2\xa9 the Claws Mail team and others; RISC OS port Andrew Youll",
                               VERSION " (RISC OS " RISCOS_PORT_VERSION ")");

  if (!g_getenv ("CLAWS_NO_LOG"))
    {
      fd = open ("/<Wimp$ScrapDir>/ClawsMail/Log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
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

  /* a file given by !Run (dropped on the application, a RISC OS name):
     attach it */
  for (i = 1; i < *argc && *argc == 2; i++)
    if (is_riscos_name ((*argv)[i]))
      {
        char **nargv = g_new0 (char *, *argc + 2);
        int j;

        /* if Claws Mail is already running, it gets this file instead */
        gdk_riscos_iconbar_set_start_file ((*argv)[i]);
        for (j = 0; j < i; j++)
          nargv[j] = (*argv)[j];
        nargv[i] = "--attach";
        for (j = i; j < *argc; j++)
          nargv[j + 1] = (*argv)[j];
        nargv[i + 1] = gdk_riscos_path_to_unix ((*argv)[i]);
        *argv = nargv;
        (*argc)++;
        break;
      }
}
