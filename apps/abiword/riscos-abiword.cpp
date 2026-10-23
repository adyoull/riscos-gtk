/*
 * riscos-abiword.cpp - AbiWord's start on RISC OS (riscos-gtk's !AbiWord).
 *
 * Takes the place of AbiWord's src/wp/main/gtk/UnixMain.cpp (not linked):
 *   - AbiWord's data (strings, dialogue .ui files, templates, the XSLT for
 *     MathML and Word's equations) from /<AbiWord$Dir>/share/abiword-3.0
 *     (ABIWORD_DATADIR), fonts and GTK's data from the application
 *     directory (FONTCONFIG_FILE, XDG_DATA_DIRS);
 *   - settings (AbiWord's profile) in <Choices$Write>.AbiWord
 *     (XDG_CONFIG_HOME);
 *   - messages to <Wimp$ScrapDir>.AbiWord.Log;
 *   - files given by !Run (RISC OS names) become UnixLib names;
 *   - the icon bar: Select shows AbiWord's window; Menu gives Info,
 *     Choices... (AbiWord's Preferences) and Quit; Word files double-
 *     clicked, files dropped on the icon or saved to it open in a window
 *     (the current one if it's an empty, unchanged new document);
 *     PreQuit with unsaved documents asks.
 * The file formats (OpenDocument, Word .docx, RTF, HTML...) are AbiWord's
 * plugins, linked in (configure --enable-builtin-plugins).
 *
 * GPL version 2 or later, as AbiWord. Copyright (C) 2026 Andrew Youll.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>

#include "ap_UnixApp.h"
#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_UnixFrameImpl.h"
#include "ev_EditMethod.h"
#include "ie_types.h"
#include "ut_go_file.h"

/* AbiWord's built-in resources (its icons: src/abi-resources.c, made by
   glib-compile-resources) register themselves from a constructor; nothing
   else refers to them, so a static link would leave them out */
extern "C" GResource *abi_get_resource (void);

#ifndef RISCOS_PORT_VERSION
#define RISCOS_PORT_VERSION "dev"
#endif

/* The window the user last worked in, or any */
static XAP_Frame *
current_frame (void)
{
  XAP_App *app = XAP_App::getApp ();
  XAP_Frame *f;

  if (!app)
    return NULL;
  f = app->getLastFocussedFrame ();
  if (!f && app->getFrameCount () > 0)
    f = app->getFrame (0);
  return f;
}

static void
show_window (gpointer)
{
  XAP_App *app = XAP_App::getApp ();
  XAP_Frame *f = current_frame ();

  if (!f && app)
    {
      /* every window closed: a new document */
      f = app->newFrame ();
      if (f)
        f->loadDocument ((const char *) NULL, IEFT_Unknown);
    }
  if (f)
    f->raise ();
}

/* a new document nobody has typed in (AbiWord's own "replace the empty
   window" rule when a file is opened) */
static bool
empty_and_unchanged (XAP_Frame *f)
{
  const char *name;

  if (!f || f->isDirty ())
    return false;
  name = f->getFilename ();
  return !name || !*name;
}

static gboolean
open_file (const char *unix_path, int filetype, gboolean scrap, gpointer)
{
  XAP_App *app = XAP_App::getApp ();
  XAP_Frame *f = current_frame ();
  bool fresh = false;
  UT_Error err;

  if (!app)
    return FALSE;
  if (!empty_and_unchanged (f))
    {
      f = app->newFrame ();
      fresh = true;
    }
  if (!f)
    return FALSE;
  /* as a URI, as for files given when AbiWord starts (and AbiWord keeps it as the
     document's name: some exporters, e.g. HTML's, can't take a plain
     filename when the document is saved again) */
  {
    char *uri = UT_go_shell_arg_to_uri (unix_path);
    err = f->loadDocument (uri ? uri : unix_path, IEFT_Unknown, true);
    g_free (uri);
  }
  if (err != UT_OK && fresh)
    /* as AbiWord does for a file it can't open: an empty window */
    f->loadDocument ((const char *) NULL, IEFT_Unknown);
  f->raise ();
  return err == UT_OK;
}

static void
choices (gpointer)
{
  if (current_frame ())
    ev_EditMethod_invoke ("dlgOptions", "");
}

static gboolean
unsaved (gpointer)
{
  XAP_App *app = XAP_App::getApp ();
  UT_sint32 i;

  if (!app)
    return FALSE;
  for (i = 0; i < app->getFrameCount (); i++)
    if (app->getFrame (i) && app->getFrame (i)->isDirty ())
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

/* the tests (tests/gtk/abiword-fake.cpp) call it riscos_abiword_main */
#ifdef ABI_RISCOS_TEST
#define RISCOS_MAIN riscos_abiword_main
#else
#define RISCOS_MAIN main
#endif

int
RISCOS_MAIN (int argc, char **argv)
{
  static const int types[] = { 0xAE6 };       /* Microsoft Word */
  int fd, i;

  (void) abi_get_resource ();
  g_setenv ("ABIWORD_DATADIR", "/<AbiWord$Dir>/share/abiword-3.0", FALSE);
  g_setenv ("FONTCONFIG_FILE", "/<AbiWord$Dir>/fonts/fonts.conf", FALSE);
  g_setenv ("XDG_DATA_DIRS", "/<AbiWord$Dir>/share", FALSE);
  if (!g_getenv ("XDG_CONFIG_HOME") && g_getenv ("Choices$Write"))
    {
      mkdir ("/<Choices$Write>/AbiWord", 0755);
      g_setenv ("XDG_CONFIG_HOME", "/<Choices$Write>/AbiWord", TRUE);
    }

  gdk_riscos_iconbar_set_handlers (&handlers, NULL);
  gdk_riscos_iconbar_set_info ("AbiWord", "Word processor",
                               "\xc2\xa9 The AbiWord developers; RISC OS port Andrew Youll",
                               RISCOS_PORT_VERSION);
  for (i = 0; i < (int) G_N_ELEMENTS (types); i++)
    gdk_riscos_iconbar_add_filetype (types[i]);

  if (!g_getenv ("ABIWORD_NO_LOG"))
    {
      fd = open ("/<Wimp$ScrapDir>/AbiWord/Log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
        /* if AbiWord is already running, it gets this file instead */
        gdk_riscos_iconbar_set_start_file (argv[i]);
        argv[i] = gdk_riscos_path_to_unix (argv[i]);
      }
  return AP_UnixApp::main (PACKAGE_NAME, argc, argv);
}
