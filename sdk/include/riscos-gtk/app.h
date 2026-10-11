/*
 * riscos-gtk/app.h - riscos-gtk devkit: the start-up every RISC OS GTK
 * program needs, in one call before gtk_init() or g_application_run():
 *
 *   #include <gtk/gtk.h>
 *   #include <riscos-gtk/app.h>
 *
 *   int main (int argc, char **argv)
 *   {
 *     riscos_gtk_app_init ("Hello", &argc, &argv);
 *     ...
 *   }
 *
 * It
 *   - points fontconfig and GLib/GTK at the application's own folder
 *     (FONTCONFIG_FILE = /<NAME$Dir>/fonts/fonts.conf, XDG_DATA_DIRS =
 *     /<NAME$Dir>/share: the theme, icons and GSettings schemas that
 *     riscos-gtk-package puts there), unless already set;
 *   - puts the program's own settings and data where RISC OS keeps them:
 *     XDG_CONFIG_HOME and XDG_DATA_HOME = <Choices$Write>.NAME (GSettings'
 *     keyfile, GKeyFile files under g_get_user_config_dir(), GTK's
 *     recently-used list), XDG_CACHE_HOME = <Wimp$ScrapDir>.NAME.cache,
 *     and GSETTINGS_BACKEND = keyfile so that GSettings are kept there,
 *     unless already set;
 *   - sends stdout and stderr to <Wimp$ScrapDir>.NAME.Log (unless
 *     RISCOS_GTK_NO_LOG is set), opening the file once: RISC OS allows only
 *     one write-open of a file, so freopen()ing it twice fails;
 *   - names the program (g_set_prgname, the icon bar's Info box);
 *   - turns RISC OS file names on the command line (from a double-click
 *     in the Filer, "ADFS::HD4.$.Docs.Note/txt") into UnixLib names
 *     ("/ADFS::HD4/$/Docs/Note.txt") and, for the first, tells riscos-gtk
 *     it's the start-up file: if a copy of the program is already
 *     running, that copy gets the file and this one goes.
 *
 * NAME is the application's name without the "!": !Hello's NAME$Dir is
 * Hello$Dir, set by its !Run.
 *
 * riscos_gtk_open_uri (uri) opens a web address in the user's browser
 * through RISC OS's URI handler (URI_Dispatch), starting Iris or NetSurf
 * first if no browser is running. GTK's gtk_show_uri_on_window() can't
 * on RISC OS (there is no desktop launcher for GIO to use).
 *
 * Header only: include it in one source file. MIT licence.
 */
#ifndef RISCOS_GTK_APP_H
#define RISCOS_GTK_APP_H

#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef GDK_WINDOWING_RISCOS
#include <gdk/gdkriscos.h>
#endif

G_BEGIN_DECLS

/* TRUE if name is a RISC OS file name rather than a UnixLib one */
static inline gboolean
riscos_gtk_is_riscos_name (const char *name)
{
  return name[0] != '/' && name[0] != '-' &&
         (strchr (name, ':') != NULL || name[0] == '$' || name[0] == '<' ||
          name[0] == '@' || name[0] == '&' || name[0] == '%');
}

static inline void
riscos_gtk_app_init (const char *name, int *argc, char ***argv)
{
  char *s;
  int i, fd;
  gboolean first = TRUE;

  s = g_strdup_printf ("/<%s$Dir>/fonts/fonts.conf", name);
  g_setenv ("FONTCONFIG_FILE", s, FALSE);
  g_free (s);
  s = g_strdup_printf ("/<%s$Dir>/share", name);
  g_setenv ("XDG_DATA_DIRS", s, FALSE);
  g_free (s);
  s = g_strdup_printf ("/<Choices$Write>/%s", name);
  g_setenv ("XDG_CONFIG_HOME", s, FALSE);
  g_setenv ("XDG_DATA_HOME", s, FALSE);
  g_free (s);
  s = g_strdup_printf ("/<Wimp$ScrapDir>/%s/cache", name);
  g_setenv ("XDG_CACHE_HOME", s, FALSE);
  g_free (s);
  /* GSettings: kept in a file there (GLib would otherwise pick its
     memory backend, with no D-Bus for dconf, and forget them) */
  g_setenv ("GSETTINGS_BACKEND", "keyfile", FALSE);

  /* the scrap folder, and fontconfig's cache in it (fonts.conf names
     <Wimp$ScrapDir>.NAME.fontcache; !Run makes it too) */
#ifdef __riscos
  s = g_strdup_printf ("/<Wimp$ScrapDir>/%s/fontcache", name);
  g_mkdir_with_parents (s, 0755);
  g_free (s);
#endif

  if (!g_getenv ("RISCOS_GTK_NO_LOG"))
    {
      s = g_strdup_printf ("/<Wimp$ScrapDir>/%s", name);
      g_mkdir_with_parents (s, 0755);
      g_free (s);
      s = g_strdup_printf ("/<Wimp$ScrapDir>/%s/Log", name);
      fd = open (s, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      g_free (s);
      if (fd >= 0)
        {
          fflush (stdout);
          fflush (stderr);
          dup2 (fd, 1);
          dup2 (fd, 2);
          if (fd > 2)
            close (fd);
          setvbuf (stdout, NULL, _IOLBF, 0);
          setvbuf (stderr, NULL, _IONBF, 0);
        }
    }

  g_set_prgname (name);

  if (argc && argv)
    for (i = 1; i < *argc; i++)
      if (riscos_gtk_is_riscos_name ((*argv)[i]))
        {
#ifdef GDK_WINDOWING_RISCOS
          if (first)
            gdk_riscos_iconbar_set_start_file ((*argv)[i]);
          (*argv)[i] = gdk_riscos_path_to_unix ((*argv)[i]);
#endif
          first = FALSE;
        }
  (void) first;
}

/* ---- opening a web address ------------------------------------------ */

#if defined (GDK_WINDOWING_RISCOS) && defined (__riscos)
#include <kernel.h>

static char *riscos_gtk_pending_uri;
static int riscos_gtk_pending_polls;

static inline gboolean
riscos_gtk_task_running (const char *name)
{
  int buf[16 * 4], *p;
  _kernel_swi_regs r;
  size_t n = strlen (name);

  r.r[0] = 0;
  do
    {
      r.r[1] = (int) (gsize) buf;
      r.r[2] = sizeof buf;
      if (_kernel_swi (0x42681 /* TaskManager_EnumerateTasks */, &r, &r) != NULL)
        return FALSE;
      for (p = buf; (char *) p < (char *) (gsize) r.r[1]; p += 4)
        if (g_ascii_strncasecmp ((const char *) (gsize) p[1], name, n) == 0)
          return TRUE;
    }
  while (r.r[0] >= 0);
  return FALSE;
}

static inline gboolean
riscos_gtk_browser_running (void)
{
  return riscos_gtk_task_running ("Iris") || riscos_gtk_task_running ("NetSurf");
}

static inline gboolean
riscos_gtk_uri_dispatch (const char *uri)
{
  _kernel_swi_regs r;
  _kernel_oserror *e;
  char *msg;

  r.r[0] = 0;                     /* flags: dispatch now */
  r.r[1] = (int) (gsize) uri;
  r.r[2] = 0;
  e = _kernel_swi (0x4E381 /* URI_Dispatch */, &r, &r);
  if (e == NULL && !(r.r[0] & 1))  /* bit 0 set: nothing took it */
    return TRUE;
  msg = g_strdup_printf ("No web browser took the address %s. %s", uri,
                         e ? e->errmess : "Start a browser, then try again.");
  gdk_riscos_report_error (msg);
  g_free (msg);
  return FALSE;
}

static inline gboolean
riscos_gtk_dispatch_when_started (gpointer data)
{
  if (riscos_gtk_browser_running () || ++riscos_gtk_pending_polls > 60)
    {
      char *uri = riscos_gtk_pending_uri;
      riscos_gtk_pending_uri = NULL;
      if (uri)
        riscos_gtk_uri_dispatch (uri);
      g_free (uri);
      return G_SOURCE_REMOVE;
    }
  return G_SOURCE_CONTINUE;
}

static inline gboolean
riscos_gtk_open_uri (const char *uri)
{
  const char *app = NULL;
  _kernel_swi_regs r;

  if (riscos_gtk_pending_uri)
    {
      /* a browser is starting: this address goes when it's running */
      g_free (riscos_gtk_pending_uri);
      riscos_gtk_pending_uri = g_strdup (uri);
      return TRUE;
    }
  if (!riscos_gtk_browser_running ())
    {
      if (g_getenv ("Iris$Dir"))
        app = "/<Iris$Dir>";
      else if (g_getenv ("NetSurf$Dir"))
        app = "/<NetSurf$Dir>";
    }
  r.r[0] = (int) (gsize) app;
  if (app && _kernel_swi (0x400DE /* Wimp_StartTask */, &r, &r) == NULL)
    {
      riscos_gtk_pending_uri = g_strdup (uri);
      riscos_gtk_pending_polls = 0;
      g_timeout_add (500, riscos_gtk_dispatch_when_started, NULL);
      return TRUE;
    }
  return riscos_gtk_uri_dispatch (uri);
}
#else
#include <gio/gio.h>
static inline gboolean
riscos_gtk_open_uri (const char *uri)
{
  return g_app_info_launch_default_for_uri (uri, NULL, NULL);
}
#endif

G_END_DECLS

#endif
