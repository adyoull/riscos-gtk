/*
 * frontend-fake.c - riscos-gtk: a native Wimp front end beside GTK's
 * windows (gdk_riscos_set_wimp_filter, gdk_riscos_add_messages,
 * gdk_riscos_window_get_wimp_handle, gdk_riscos_window_set_parent,
 * gdk_riscos_window_parent_moved), on the fake Wimp.
 *
 * The program makes a Wimp window of its own and nests an undecorated
 * GtkWindow in it. A click and a key in its own window reach its filter
 * and not GTK (the key isn't given back to the Wimp); a click in the GTK
 * window reaches GTK. Every open of the GTK window is nested; when the
 * parent moves it, parent_moved tells GTK; parent 0 makes it top-level
 * again. Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FAKE_SCRIPT_ENTRY(i, a, b, c, d) \
  (fake_wimp_script[i][0] = (a), fake_wimp_script[i][1] = (b), \
   fake_wimp_script[i][2] = (c), fake_wimp_script[i][3] = (d))

static int n_tests, failures;
static int own_handle, gtk_handle;
static int own_clicks, own_keys, gtk_filtered_clicks, gtk_presses, finished;
static GtkWidget *win;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    failures++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
}

/* the program's own window: its clicks and keys; GTK's events go on */
static gboolean
filter (int reason, int *block, gpointer data)
{
  if (reason == 6 && block[3] == own_handle)
    {
      own_clicks++;
      return TRUE;
    }
  if (reason == 8 && block[0] == own_handle)
    {
      own_keys++;
      fake_target_window = gtk_handle;   /* the next click goes to GTK's */
      return TRUE;
    }
  if (reason == 6 && block[3] == gtk_handle)
    gtk_filtered_clicks++;
  if (reason == 3)
    return TRUE;                         /* the fake's closes, after its script */
  return FALSE;
}

static gboolean
on_press (GtkWidget *w, GdkEventButton *e, gpointer d)
{
  gtk_presses++;
  return TRUE;
}

static fake_window_t *
fake_window (int handle)
{
  int i;

  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle == handle)
      return &fake_windows[i];
  return NULL;
}

static void
drain (void)
{
  int i;

  for (i = 0; i < 20; i++)
    while (gtk_events_pending ())
      gtk_main_iteration ();
}

/* once the script has run: the parent moves the child, then unnest */
static gboolean
later (gpointer data)
{
  GdkWindow *gw = gtk_widget_get_window (win);
  fake_window_t *fw = fake_window (gtk_handle);
  int x0, y0, x1, y1;

  if (gtk_presses == 0 && own_keys == 0)
    return G_SOURCE_CONTINUE;            /* not yet */
  if (fw == NULL)
    {
      check (0, "the GTK window is open on the fake Wimp");
      gtk_main_quit ();
      return G_SOURCE_REMOVE;
    }
  drain ();
  gdk_window_get_origin (gw, &x0, &y0);
  /* the Wimp moves a nested window with its parent, with no
     Open_Window_Request: 40 OS units right, 20 down */
  fake_open_window (gtk_handle, fw->x0 + 40, fw->y0 - 20, fw->x1 + 40, fw->y1 - 20, 0, 0);
  gdk_riscos_window_parent_moved (gw);
  drain ();
  gdk_window_get_origin (gw, &x1, &y1);
  check (x1 > x0 && y1 > y0, "parent_moved: GTK is told where the nested window went");

  gdk_riscos_window_set_parent (gw, 0, 0);
  check (fake_nested_window == gtk_handle && fake_nested_parent == -1,
         "parent 0: opened at the top level again (R3 = -1)");
  finished = 1;
  gtk_main_quit ();
  return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
  static const int msgs[] = { 0x502, 0x400C9, 0 };
  int wblock[23];
  _kernel_swi_regs r;
  GdkWindow *gw;
  int opens, i, found;

  alarm (120);
  fake_set_screen (1280, 1024, 0, 5);
  g_setenv ("RISCOS_GTK_ICONBAR", "0", TRUE);
  gdk_riscos_add_messages (msgs);        /* before the task starts */
  gdk_riscos_set_wimp_filter (filter, NULL);
  gtk_init (&argc, &argv);

  for (i = found = 0; i < fake_added_messages_len; i++)
    found += fake_added_messages[i] == 0x502 || fake_added_messages[i] == 0x400C9;
  check (found == 2, "add_messages before gtk_init: added once the task started");

  /* the program's own window */
  memset (wblock, 0, sizeof wblock);
  wblock[0] = 200; wblock[1] = 200; wblock[2] = 1000; wblock[3] = 800;
  wblock[6] = -1;
  r.r[1] = (int) (long) wblock;
  _kernel_swi (0x400C1, &r, &r);
  own_handle = r.r[0];
  fake_open_window (own_handle, 200, 200, 1000, 800, 0, 0);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated (GTK_WINDOW (win), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (win), 200, 100);
  gtk_widget_add_events (win, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (win, "button-press-event", G_CALLBACK (on_press), NULL);
  gtk_widget_realize (win);
  gw = gtk_widget_get_window (win);
  gdk_riscos_window_set_parent (gw, own_handle, 0xAA0);
  gtk_widget_show (win);
  drain ();
  gtk_handle = gdk_riscos_window_get_wimp_handle (gw);
  check (gtk_handle != 0 && gtk_handle != own_handle, "get_wimp_handle: the GTK window's handle");
  check (fake_nested_window == gtk_handle && fake_nested_parent == own_handle &&
         fake_nested_flags == 0xAA0, "set_parent: opened nested (R3 = parent, R4 = flags)");

  opens = fake_nested_opens;
  gtk_window_resize (GTK_WINDOW (win), 260, 140);
  drain ();
  check (fake_nested_opens > opens && fake_nested_parent == own_handle,
         "a GTK resize keeps it nested");

  /* a click and a key in the program's window, then a click in GTK's */
  fake_target_window = own_handle;
  fake_processed_key = -1;
  FAKE_SCRIPT_ENTRY (0, 6, 10, 10, 4);
  FAKE_SCRIPT_ENTRY (1, FAKE_RELEASE, 0, 0, 0);
  FAKE_SCRIPT_ENTRY (2, 8, 'a', 0, 0);
  FAKE_SCRIPT_ENTRY (3, 6, 20, 20, 4);
  FAKE_SCRIPT_ENTRY (4, FAKE_RELEASE, 0, 0, 0);
  for (i = 5; i < 25; i++)
    FAKE_SCRIPT_ENTRY (i, FAKE_NULL, 0, 0, 0);
  fake_wimp_script_len = 25;
  g_timeout_add (20, later, NULL);
  gtk_main ();

  check (own_clicks == 1, "the filter got the click in the program's window");
  check (own_keys == 1 && fake_processed_key == -1,
         "the filter got its key, and it wasn't given back to the Wimp");
  check (gtk_filtered_clicks == 1 && gtk_presses >= 1,
         "a click in the GTK window went through the filter to GTK");
  check (finished, "the run finished");
  printf ("1..%d\n", n_tests);
  return failures;
}
