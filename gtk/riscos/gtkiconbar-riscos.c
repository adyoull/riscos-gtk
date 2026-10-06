/*
 * gtkiconbar-riscos.c - riscos-gtk: what a GTK program's icon bar icon does.
 *
 * The icon itself and the Wimp messages are GDK's (gdk/riscos/
 * gdkiconbar-riscos.c); this is GTK's side, done as riscos-gtk's other
 * RISC OS programs (Reel, !FFmpeg, YTDL, NetStats) do it:
 *
 *   - Select (or Adjust) on the icon: the program's window, brought to the
 *     front (or made: GtkApplication activate, or the windows it hid);
 *   - Menu on the icon: the icon bar menu, titled with the program's name,
 *     64 OS units left of the click with its top 96 + 44 a line up:
 *       Info  >     About this program (a Wimp window, GDK's)
 *       Choices...  only if the program has settings
 *       Quit
 *     Adjust on an item keeps the menu open;
 *   - Quit with unsaved work asks first (a RISC OS question box: Cancel /
 *     Discard);
 *   - a file double-clicked in the Filer (of a type the program claims),
 *     dropped on the icon, or saved to it by another program: opened.
 *
 * What each does comes from the program's handlers
 * (gdk_riscos_iconbar_set_handlers), else its GtkApplication (activate,
 * open, the "preferences" and "quit" actions), else its own menu items,
 * found by their labels (Preferences/Settings/Choices, Quit).
 *
 * A GtkApplication is held, so that closing its last window leaves the
 * program on the icon bar, as RISC OS programs stay.
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as the GTK code it
 * plugs into.
 */
#include "config.h"

#include "gtkiconbar-riscos.h"
#include "gtkapplication.h"
#include "gtkcontainer.h"
#include "gtkmain.h"
#include "gtkmenubar.h"
#include "gtkmenuitem.h"
#include "gtkmenushell.h"
#include "gtkwindow.h"
#include "gtkwidget.h"

#include "riscos/gdkriscos.h"
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>

enum { ITEM_INFO, ITEM_CHOICES, ITEM_QUIT, N_ITEMS };
static const char *item_labels[N_ITEMS] = { "Info", "Choices...", "Quit" };

static struct {
  GdkDisplay *display;
  int *menu;                       /* the Wimp menu block */
  char title[64];
  int items[N_ITEMS];              /* the menu's lines: which item each is */
  int n;
  int click_x;
  gboolean app_held;
} ib;

static gboolean
trace_on (void)
{
  return g_getenv ("RISCOS_GTK_DEBUG") != NULL;
}

static const GdkRiscosIconbarHandlers *
handlers (gpointer *data)
{
  return gdk_riscos_iconbar_get_handlers (data);
}

static GApplication *
application (void)
{
  GApplication *app = g_application_get_default ();

  if (app && !ib.app_held && g_application_get_is_registered (app))
    {
      /* closing the last window leaves it on the icon bar */
      g_application_hold (app);
      ib.app_held = TRUE;
    }
  return app;
}

static gboolean
app_action (const char *name, gboolean activate)
{
  GApplication *app = application ();

  if (app == NULL || !G_IS_ACTION_GROUP (app) ||
      !g_action_group_has_action (G_ACTION_GROUP (app), name) ||
      !g_action_group_get_action_enabled (G_ACTION_GROUP (app), name))
    return FALSE;
  if (activate)
    g_action_group_activate_action (G_ACTION_GROUP (app), name, NULL);
  return TRUE;
}

/* ---- the program's windows and menu items ---------------------------- */

static gboolean
is_main_window (GtkWidget *w)
{
  return GTK_IS_WINDOW (w) &&
         gtk_window_get_window_type (GTK_WINDOW (w)) == GTK_WINDOW_TOPLEVEL &&
         gtk_window_get_type_hint (GTK_WINDOW (w)) == GDK_WINDOW_TYPE_HINT_NORMAL &&
         gtk_window_get_transient_for (GTK_WINDOW (w)) == NULL;
}

static char *
normalise (const char *label)
{
  GString *s = g_string_new (NULL);
  const char *p;

  for (p = label; *p; p++)
    if (*p != '_')
      g_string_append_c (s, g_ascii_tolower (*p));
  while (s->len && (s->str[s->len - 1] == '.' || s->str[s->len - 1] == ' '))
    g_string_truncate (s, s->len - 1);
  if (g_str_has_suffix (s->str, "\xe2\x80\xa6"))
    g_string_truncate (s, s->len - 3);
  return g_string_free (s, FALSE);
}

static GtkWidget *
find_item_in (GtkWidget *w, const char *const *names)
{
  GtkWidget *found = NULL;

  if (GTK_IS_MENU_ITEM (w))
    {
      const char *label = gtk_menu_item_get_label (GTK_MENU_ITEM (w));
      GtkWidget *sub = gtk_menu_item_get_submenu (GTK_MENU_ITEM (w));
      if (label && sub == NULL)
        {
          char *n = normalise (label);
          int i;
          for (i = 0; names[i] && !found; i++)
            if (strcmp (n, names[i]) == 0)
              found = w;
          g_free (n);
        }
      if (found == NULL && sub)
        found = find_item_in (sub, names);
      return found;
    }
  if (GTK_IS_CONTAINER (w))
    {
      GList *kids = gtk_container_get_children (GTK_CONTAINER (w)), *l;
      for (l = kids; l && !found; l = l->next)
        found = find_item_in (l->data, names);
      g_list_free (kids);
    }
  return found;
}

/* One of the program's menu items (menu bars of its windows), by label */
static GtkWidget *
find_item (const char *const *names)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  GtkWidget *found = NULL;

  for (l = tops; l && !found; l = l->next)
    if (is_main_window (l->data))
      found = find_item_in (l->data, names);
  g_list_free (tops);
  return found;
}

static const char *const choices_names[] = { "preferences", "settings", "choices", "options", NULL };
static const char *const quit_names[] = { "quit", "exit", NULL };

static gboolean
activate_item (const char *const *names, gboolean activate)
{
  GtkWidget *item = find_item (names);

  if (item == NULL || !gtk_widget_is_sensitive (item))
    return FALSE;
  if (activate)
    gtk_menu_item_activate (GTK_MENU_ITEM (item));
  return TRUE;
}

/* ---- what the icon does ---------------------------------------------- */

static void
do_show (void)
{
  gpointer data;
  const GdkRiscosIconbarHandlers *h = handlers (&data);
  GList *l, *tops;
  gboolean shown = FALSE;

  if (h && h->show)
    {
      h->show (data);
      return;
    }
  /* the program's windows: brought to the front */
  tops = gtk_window_list_toplevels ();
  for (l = tops; l; l = l->next)
    if (is_main_window (l->data) && gtk_widget_get_visible (l->data))
      {
        gtk_window_present (l->data);
        shown = TRUE;
      }
  g_list_free (tops);
  if (!shown && application ())
    {
      g_application_activate (application ());
      return;
    }
  if (!shown)
    {
      /* none on screen: the ones it hid (closed and kept) */
      tops = gtk_window_list_toplevels ();
      for (l = tops; l; l = l->next)
        if (is_main_window (l->data) && !gtk_widget_get_visible (l->data) &&
            gtk_widget_get_realized (l->data))
          gtk_window_present (l->data);
      g_list_free (tops);
    }
}

static gboolean
has_choices (void)
{
  gpointer data;
  const GdkRiscosIconbarHandlers *h = handlers (&data);

  if (h)
    return h->choices != NULL;
  return app_action ("preferences", FALSE) || app_action ("prefs", FALSE) ||
         activate_item (choices_names, FALSE);
}

static void
do_quit (void)
{
  gpointer data;
  const GdkRiscosIconbarHandlers *h = handlers (&data);

  if (h && h->unsaved && h->unsaved (data))
    {
      char *msg = g_strdup_printf ("%s has unsaved changes. Discard them and quit?",
                                   g_get_application_name () ? g_get_application_name () : "This program");
      gboolean go = gdk_riscos_ask (msg, "Discard");
      g_free (msg);
      if (!go)
        return;
    }
  if (h && h->quit)
    h->quit (data);
  else if (app_action ("quit", TRUE))
    ;
  else if (!activate_item (quit_names, TRUE))
    {
      /* no Quit of its own: close its windows, and end */
      GList *l, *tops = gtk_window_list_toplevels ();
      for (l = tops; l; l = l->next)
        if (is_main_window (l->data))
          gtk_widget_destroy (l->data);
      g_list_free (tops);
      if (application ())
        g_application_quit (application ());
      else if (gtk_main_level () > 0)
        gtk_main_quit ();
      else
        exit (0);
    }
}

static void
do_item (int item)
{
  gpointer data;
  const GdkRiscosIconbarHandlers *h = handlers (&data);

  switch (item)
    {
    case ITEM_CHOICES:
      if (h && h->choices)
        h->choices (data);
      else if (!app_action ("preferences", TRUE) && !app_action ("prefs", TRUE))
        activate_item (choices_names, TRUE);
      break;
    case ITEM_QUIT:
      do_quit ();
      break;
    default:
      break;                        /* Info: its window is the submenu */
    }
}

static void
do_open_file (const char *path, int type, gboolean scrap)
{
  gpointer data;
  const GdkRiscosIconbarHandlers *h = handlers (&data);
  GApplication *app = application ();

  if (h && h->open_file)
    h->open_file (path, type, scrap, data);
  else if (app && (g_application_get_flags (app) & G_APPLICATION_HANDLES_OPEN))
    {
      GFile *f = g_file_new_for_path (path);
      g_application_open (app, &f, 1, "");
      g_object_unref (f);
    }
  else
    {
      char *msg = g_strdup_printf ("%s can't load files dropped on its icon.",
                                   g_get_application_name () ? g_get_application_name () : "This program");
      gdk_riscos_report_error (msg);
      g_free (msg);
    }
  if (scrap)
    g_unlink (path);
}

/* ---- the icon bar menu (as Reel's and !FFmpeg's) ------------------------ */

static void menu_chosen (const int *selection, gboolean adjust, gpointer data);

static void
build_menu (void)
{
  int i, width, info = gdk_riscos_iconbar_info_window ();
  const char *name = g_get_application_name ();
  size_t longest;

  ib.n = 0;
  ib.items[ib.n++] = ITEM_INFO;
  if (has_choices ())
    ib.items[ib.n++] = ITEM_CHOICES;
  ib.items[ib.n++] = ITEM_QUIT;

  g_free (ib.menu);
  ib.menu = g_malloc0 (28 + 24 * ib.n);
  gdk_riscos_utf8_to_latin1 (name ? name : "GTK", ib.title, sizeof ib.title);
  longest = strlen (ib.title);
  if (strlen (ib.title) <= 12)
    memcpy (ib.menu, ib.title, strlen (ib.title) < 12 ? strlen (ib.title) + 1 : 12);
  else
    {
      ib.menu[0] = (int) ib.title;   /* a long name: indirected (flag in the first item) */
      ib.menu[1] = -1;
      ib.menu[2] = sizeof ib.title;
    }
  ((unsigned char *) ib.menu)[12] = 7;   /* title black on grey, items black on white */
  ((unsigned char *) ib.menu)[13] = 2;
  ((unsigned char *) ib.menu)[14] = 7;
  ((unsigned char *) ib.menu)[15] = 0;
  for (i = 0; i < ib.n; i++)
    longest = MAX (longest, strlen (item_labels[ib.items[i]]));
  width = (int) longest * 16 + 16;
  ib.menu[4] = width;
  ib.menu[5] = 44;
  ib.menu[6] = 0;
  for (i = 0; i < ib.n; i++)
    {
      int *it = ib.menu + 7 + 6 * i;
      const char *label = item_labels[ib.items[i]];
      it[0] = (i == ib.n - 1 ? 0x80 : 0) | (i == 0 && strlen (ib.title) > 12 ? 0x100 : 0);
      it[1] = ib.items[i] == ITEM_INFO && info ? info : -1;   /* Info: About this program */
      it[2] = 0x07000021;            /* text, filled, black on white */
      memcpy (&it[3], label, strlen (label) < 12 ? strlen (label) + 1 : 12);
    }
}

static void
open_menu (int click_x)
{
  int x, y;

  ib.click_x = click_x;
  build_menu ();
  gdk_riscos_iconbar_menu_position (click_x, ib.n, &x, &y);
  if (trace_on ())
    g_printerr ("riscos-gtk: icon bar menu at %d,%d (%d lines)\n", x, y, ib.n);
  gdk_riscos_display_open_menu (ib.display, ib.menu, x, y, menu_chosen, NULL);
}

typedef struct {
  int selection;
  gboolean closed, adjust;
} Chosen;

static gboolean
menu_idle (gpointer data)
{
  Chosen *c = data;
  int item = !c->closed && c->selection >= 0 && c->selection < ib.n ? ib.items[c->selection] : -1;

  if (item >= 0)
    {
      if (!(c->adjust && item != ITEM_QUIT))
        gdk_riscos_display_close_menu (ib.display);
      do_item (item);
      /* Adjust keeps the menu open (not after Quit) */
      if (c->adjust && item != ITEM_QUIT)
        open_menu (ib.click_x);
    }
  gdk_riscos_display_menu_done (ib.display);
  g_free (c);
  return G_SOURCE_REMOVE;
}

static void
menu_chosen (const int *selection, gboolean adjust, gpointer data)
{
  Chosen *c = g_new0 (Chosen, 1);

  c->closed = selection == NULL;
  c->adjust = adjust;
  c->selection = selection ? selection[0] : -1;
  g_idle_add_full (G_PRIORITY_HIGH_IDLE, menu_idle, c, NULL);
}

/* ---- events from the Wimp poll ------------------------------------ */

typedef struct {
  GdkRiscosIconbarEventType type;
  int x, y, filetype;
  char *path;
  gboolean scrap;
} Event;

static gboolean
event_idle (gpointer data)
{
  Event *e = data;

  switch (e->type)
    {
    case GDK_RISCOS_ICONBAR_SELECT:
    case GDK_RISCOS_ICONBAR_ADJUST:
      do_show ();
      break;
    case GDK_RISCOS_ICONBAR_MENU:
      open_menu (e->x);
      break;
    case GDK_RISCOS_ICONBAR_OPEN_FILE:
      do_open_file (e->path, e->filetype, e->scrap);
      break;
    }
  g_free (e->path);
  g_free (e);
  return G_SOURCE_REMOVE;
}

static void
event_func (GdkRiscosIconbarEvent *ev, gpointer data)
{
  Event *e = g_new0 (Event, 1);

  e->type = ev->type;
  e->x = ev->x;
  e->y = ev->y;
  e->filetype = ev->filetype;
  e->path = g_strdup (ev->path);
  e->scrap = ev->scrap;
  g_idle_add_full (G_PRIORITY_HIGH_IDLE, event_idle, e, NULL);
}

void
_gtk_riscos_iconbar_init (GdkDisplay *display)
{
  static gboolean done;

  if (done || display == NULL)
    return;
  done = TRUE;
  ib.display = display;
  gdk_riscos_iconbar_create (display, event_func, NULL);
}
