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
 *   - HTML mail: Claws Mail's litehtml viewer plug-in, linked in and
 *     started here (riscos_claws_plugins);
 *   - the RISC OS icons (share/claws-mail/themes/RISCOS, drawn by
 *     tools/mkclawsicons.py), chosen once (riscos_claws_prefs_read); the
 *     folder list's icons from the desktop's own sprites
 *     (riscos_claws_stock_pixbuf).
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
#include <kernel.h>
#include <gdk/gdkriscos.h>
#include "main.h"
#include "mainwindow.h"
#include "compose.h"
#include "prefs_gtk.h"
#include "version.h"
#include "folderview.h"
#include "summaryview.h"
#include "prefs_common.h"
#include "stock_pixmap.h"
#include "utils.h"
#include "defs.h"
#include "alertpanel.h"

#ifndef RISCOS_PORT_VERSION
#define RISCOS_PORT_VERSION "dev"
#endif

void riscos_claws_init (int *argc, char ***argv);
void riscos_claws_plugins (void);
void riscos_claws_prefs_read (void);
GdkPixbuf *riscos_claws_stock_pixbuf (const char *name, const char *theme);
gint riscos_claws_open_uri (const gchar *uri);
GdkPixbuf *riscos_claws_priv_pixbuf (const char *name);
GdkPixbuf *riscos_claws_stock_hires (const char *name, const char *theme);

/* the litehtml viewer plug-in, linked in (build-claws.sh renames its
   plugin_init) */
gint lh_plugin_init (gchar **error);

/* the RISC OS icons, in the application */
#define RISCOS_THEME "/<ClawsMail$Dir>/share/claws-mail/themes/RISCOS"

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

/* Menu in the main window gives the window's menu everywhere; over the
   folder list or the message list, that list's own menu is its first
   entry (riscos-gtk's gtkmenu-riscos.c), named here. Set once the main
   window exists. */
static gboolean
name_context_menus (gpointer d)
{
  MainWindow *mainwin = mainwindow_get_mainwindow ();

  if (mainwin == NULL || mainwin->folderview == NULL || mainwin->summaryview == NULL)
    return G_SOURCE_CONTINUE;
  if (mainwin->folderview->ctree)
    g_object_set_data (G_OBJECT (mainwin->folderview->ctree), "riscos-gtk-context-name", "Folder");
  if (mainwin->summaryview->ctree)
    g_object_set_data (G_OBJECT (mainwin->summaryview->ctree), "riscos-gtk-context-name", "Message");
  return G_SOURCE_REMOVE;
}

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
  g_timeout_add (200, name_context_menus, NULL);
  gdk_riscos_iconbar_set_sprite ("!clawsmail");
  gdk_riscos_iconbar_set_info ("Claws Mail", "Email client",
                               "\xc2\xa9 the Claws Mail team and others; RISC OS port Andrew Youll",
                               RISCOS_PORT_VERSION);

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

/* After Claws Mail reads its settings. The first time ClawsMail 0.7 or
   later runs (fresh settings, or ones from an earlier version), two
   defaults change once, and a later choice in Preferences is kept:
   - the RISC OS icons, unless other icons were chosen;
   - HTML mail: the HTML part of a message that has both a plain text
     and an HTML version is the one shown (Preferences > Message View >
     Text Options). */
void
riscos_claws_prefs_read (void)
{
  char *marker = g_build_filename (get_rc_dir (), "RISCOSDefaults", NULL);

  if (!g_file_test (marker, G_FILE_TEST_EXISTS))
    {
      if (prefs_common.pixmap_theme_path == NULL ||
          strcmp (prefs_common.pixmap_theme_path, DEFAULT_PIXMAP_THEME) == 0)
        {
          g_free (prefs_common.pixmap_theme_path);
          prefs_common.pixmap_theme_path = g_strdup (g_getenv ("CLAWS_RISCOS_THEME") ? g_getenv ("CLAWS_RISCOS_THEME") : RISCOS_THEME);
        }
      prefs_common.promote_html_part = TRUE;
      g_file_set_contents (marker, "ClawsMail 0.7: RISC OS icons and HTML parts chosen once\n", -1, NULL);
    }
  g_free (marker);
}

/* The plug-ins linked into the program: the litehtml viewer shows HTML
   mail (text/html parts). */
void
riscos_claws_plugins (void)
{
  gchar *error = NULL;

  if (lh_plugin_init (&error) < 0)
    {
      g_warning ("litehtml viewer: %s", error ? error : "failed");
      g_free (error);
    }
}

/* An icon drawn from the desktop's own sprites, for Claws Mail's
   stock_pixmap.c (riscos-gtk patch), when the RISC OS icons are chosen:
   - the folder list's folders: the Filer's small_dir / small_diro (so
     they match the Filer in any desktop theme), with the RISC OS theme's
     emblem_*.png on top: inbox, outbox (Sent), queue, drafts bottom
     right, mark (marked messages) top right, hrm (read messages hidden)
     bottom left; a folder that can't be selected is faded;
   - the check boxes in lists: the Wimp's option sprites.
   NULL for anything else, and if the desktop hasn't the sprite: the
   theme's PNG is used then. */
static GdkPixbuf *
fit (GdkPixbuf *src, int w, int h)
{
  GdkPixbuf *dst = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, w, h);
  int sw = gdk_pixbuf_get_width (src), sh = gdk_pixbuf_get_height (src);
  double sc = MIN (MIN ((double) w / sw, (double) h / sh), 1.0);
  int dw = MAX (1, (int) (sw * sc + 0.5)), dh = MAX (1, (int) (sh * sc + 0.5));

  gdk_pixbuf_fill (dst, 0);
  gdk_pixbuf_composite (src, dst, (w - dw) / 2, (h - dh) / 2, dw, dh, (w - dw) / 2, (h - dh) / 2,
                        sc, sc, GDK_INTERP_BILINEAR, 255);
  return dst;
}

static void
emblem (GdkPixbuf *dst, const char *theme, const char *name, int x, int y, int scale)
{
  char *f = g_strdup_printf ("%s/emblem_%s%s.png", theme, name, scale == 2 ? "@2" : "");
  GdkPixbuf *e = gdk_pixbuf_new_from_file (f, NULL);

  x *= scale;
  y *= scale;
  if (e)
    {
      int w = MIN (gdk_pixbuf_get_width (e), gdk_pixbuf_get_width (dst) - x);
      int h = MIN (gdk_pixbuf_get_height (e), gdk_pixbuf_get_height (dst) - y);
      gdk_pixbuf_composite (e, dst, x, y, w, h, x, y, 1, 1, GDK_INTERP_NEAREST, 255);
      g_object_unref (e);
    }
  g_free (f);
}

/* A desktop sprite at GDK pixels (scale 1) or at the screen's (scale 2) */
static GdkPixbuf *
desktop_sprite (const char *name, int scale)
{
  return scale == 2 ? gdk_riscos_wimp_sprite_scaled (name, 2) : gdk_riscos_wimp_sprite (name);
}

/* riscos_claws_stock_pixbuf at scale 1 or 2 (twice the pixels) */
static GdkPixbuf *
compose_stock (const char *name, const char *theme, int scale)
{
  static const char *const kinds[] = { "dir_", "inbox_", "outbox_", "queue_", "drafts_" };
  GdkPixbuf *spr, *pb;
  const char *rest = NULL;
  gboolean open, noselect = FALSE, subs = FALSE;
  int k;

  if (!theme || !g_str_has_suffix (theme, "/RISCOS") || !name)
    return NULL;
  if (!strcmp (name, "checkbox_on") || !strcmp (name, "checkbox_off"))
    {
      spr = desktop_sprite (name[9] == 'n' ? "opton" : "optoff", scale);
      if (!spr)
        return NULL;
      pb = fit (spr, 13 * scale, 13 * scale);
      g_object_unref (spr);
      return pb;
    }
  for (k = 0; k < (int) G_N_ELEMENTS (kinds) && !rest; k++)
    if (g_str_has_prefix (name, kinds[k]))
      rest = name + strlen (kinds[k]);
  if (!rest)
    return NULL;
  k--;
  if (k == 0 && g_str_has_prefix (rest, "noselect_"))
    noselect = TRUE, rest += 9;
  else if (k == 0 && g_str_has_prefix (rest, "subs_"))
    subs = TRUE, rest += 5;
  if (g_str_has_prefix (rest, "open"))
    open = TRUE;
  else if (g_str_has_prefix (rest, "close"))
    open = FALSE;
  else
    return NULL;

  spr = desktop_sprite (open ? "small_diro" : "small_dir", scale);
  if (!spr && open)
    spr = desktop_sprite ("small_dir", scale);
  if (!spr)
    spr = desktop_sprite (open ? "directoryo" : "directory", scale);
  if (!spr && open)
    spr = desktop_sprite ("directory", scale);
  if (!spr)
    return NULL;
  pb = fit (spr, 16 * scale, 16 * scale);
  g_object_unref (spr);
  if (k > 0)
    emblem (pb, theme, kinds[k][0] == 'd' ? "drafts" : k == 1 ? "inbox" : k == 2 ? "outbox" : "queue", 7, 7, scale);
  if (subs)
    emblem (pb, theme, "subs", 7, 7, scale);
  if (strstr (rest, "_hrm"))
    emblem (pb, theme, "hrm", 0, 7, scale);
  if (strstr (rest, "_mark"))
    emblem (pb, theme, "mark", 7, 0, scale);
  if (noselect)
    {
      guchar *p = gdk_pixbuf_get_pixels (pb);
      int i, n = gdk_pixbuf_get_rowstride (pb) * gdk_pixbuf_get_height (pb);
      for (i = 3; i < n; i += 4)
        p[i] = p[i] * 2 / 5;
    }
  return pb;
}

GdkPixbuf *
riscos_claws_stock_pixbuf (const char *name, const char *theme)
{
  return compose_stock (name, theme, 1);
}

/* For stock_pixmap.c at GDK scale 2 (an EX0 EY0, 180 dpi, desktop): the
   RISC OS icon drawn at twice the pixels (its @2.png, or the folder made
   from the desktop's 180 dpi or doubled sprites), tagged
   "riscos-gtk-scale" 2 so that GTK (riscos-gtk's GtkImage and
   GtkCellRendererPixbuf) and Claws Mail's lists (riscos-pixbuf-scale.h)
   show it at its usual size and the screen's resolution. NULL at scale
   1, for other themes, and where there's no such picture. */
GdkPixbuf *
riscos_claws_stock_hires (const char *name, const char *theme)
{
  GdkPixbuf *pb;

  if (gdk_riscos_scale_factor () != 2 || !theme || !g_str_has_suffix (theme, "/RISCOS") || !name)
    return NULL;
  pb = compose_stock (name, theme, 2);
  if (pb == NULL)
    {
      char *f = g_strdup_printf ("%s/%s@2.png", theme, name);
      pb = gdk_pixbuf_new_from_file (f, NULL);
      g_free (f);
    }
  if (pb)
    g_object_set_data (G_OBJECT (pb), "riscos-gtk-scale", GINT_TO_POINTER (2));
  return pb;
}

/* The RISC OS port's logo (the About box, the set-up wizard) and window
   icon, whichever icon theme is chosen: the application icon drawn big,
   in the RISC OS theme (tools/mkclawsicons.py). NULL: Claws Mail's own. */
GdkPixbuf *
riscos_claws_priv_pixbuf (const char *name)
{
  const char *theme = g_getenv ("CLAWS_RISCOS_THEME") ? g_getenv ("CLAWS_RISCOS_THEME") : RISCOS_THEME;
  char *f;
  GdkPixbuf *pb;

  if (!name || !g_str_has_prefix (name, "claws_mail_"))
    return NULL;
  if (gdk_riscos_scale_factor () == 2)
    {
      /* twice the pixels, for the 180 dpi screen (riscos_claws_stock_hires) */
      f = g_strdup_printf ("%s/%s@2.png", theme, name);
      pb = gdk_pixbuf_new_from_file (f, NULL);
      g_free (f);
      if (pb)
        {
          g_object_set_data (G_OBJECT (pb), "riscos-gtk-scale", GINT_TO_POINTER (2));
          return pb;
        }
    }
  f = g_strdup_printf ("%s/%s.png", theme, name);
  pb = gdk_pixbuf_new_from_file (f, NULL);
  g_free (f);
  return pb;
}

/* Links (in HTML mail, in text mail, Open link): to the RISC OS browser.
   - mailto: starts a message here.
   - Anything else goes to the Acorn URI handler, URI_Dispatch (&4E381;
     Acorn URI Handler functional specification: R0 flags, R1 the URI,
     R2 0; on exit R0 bit 0 set if it was rejected). A browser that's
     running claims it (Message_URI_MProcess); if none does, the URI
     handler starts Alias$Open_URI_<scheme> if that's set.
   - Iris is preferred, then NetSurf: if neither is running but the Filer
     has seen one (Iris$Dir, NetSurf$Dir), it's started first
     (Wimp_StartTask &400DE) and the link dispatched once it's running
     (polled every half second, for up to 30 seconds). */
#define URI_Dispatch 0x4E381
#ifndef Wimp_StartTask
#define Wimp_StartTask 0x400DE
#endif
#ifndef TaskManager_EnumerateTasks
#define TaskManager_EnumerateTasks 0x42681
#endif

static char *pending_uri;
static int pending_polls;

/* a task whose name starts with name (any case) is running */
static gboolean
task_running (const char *name)
{
  int buf[16 * 4];
  _kernel_swi_regs r;
  size_t n = strlen (name);

  r.r[0] = 0;
  do
    {
      int *p;
      r.r[1] = (int) (gsize) buf;
      r.r[2] = sizeof buf;
      if (_kernel_swi (TaskManager_EnumerateTasks, &r, &r) != NULL)
        return FALSE;
      for (p = buf; (char *) p < (char *) (gsize) r.r[1]; p += 4)
        if (g_ascii_strncasecmp ((const char *) (gsize) p[1], name, n) == 0)
          return TRUE;
    }
  while (r.r[0] >= 0);
  return FALSE;
}

static gboolean
browser_running (void)
{
  return task_running ("Iris") || task_running ("NetSurf");
}

static int
dispatch (const char *uri)
{
  _kernel_swi_regs r;
  _kernel_oserror *e;

  r.r[0] = 0;
  r.r[1] = (int) (gsize) uri;
  r.r[2] = 0;
  e = _kernel_swi (URI_Dispatch, &r, &r);
  if (e != NULL || (r.r[0] & 1))
    {
      alertpanel_error ("No web browser took the link:\n%s\n\n%s", uri,
                        e ? e->errmess : "Start Iris or NetSurf, then try again.");
      return -1;
    }
  return 0;
}

static gboolean
dispatch_when_started (gpointer d)
{
  if (browser_running () || ++pending_polls > 60)
    {
      char *uri = pending_uri;
      pending_uri = NULL;
      if (uri)
        dispatch (uri);
      g_free (uri);
      return G_SOURCE_REMOVE;
    }
  return G_SOURCE_CONTINUE;
}

gint
riscos_claws_open_uri (const gchar *uri)
{
  const char *app = NULL;

  if (g_ascii_strncasecmp (uri, "mailto:", 7) == 0)
    {
      compose_new (NULL, uri, NULL);
      return 0;
    }
  if (pending_uri)
    {
      /* a browser is being started: this link goes when it's running */
      g_free (pending_uri);
      pending_uri = g_strdup (uri);
      return 0;
    }
  if (!browser_running ())
    {
      if (g_getenv ("Iris$Dir"))
        app = "/<Iris$Dir>";
      else if (g_getenv ("NetSurf$Dir"))
        app = "/<NetSurf$Dir>";
    }
  if (app)
    {
      _kernel_swi_regs r;

      r.r[0] = (int) (gsize) app;
      if (_kernel_swi (Wimp_StartTask, &r, &r) == NULL)
        {
          pending_uri = g_strdup (uri);
          pending_polls = 0;
          g_timeout_add (500, dispatch_when_started, NULL);
          return 0;
        }
    }
  return dispatch (uri);
}
