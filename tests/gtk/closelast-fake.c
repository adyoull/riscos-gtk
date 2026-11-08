/*
 * closelast-fake.c - riscos-gtk: closing a program's last window leaves
 * it on the icon bar, on the fake Wimp.
 *
 * The program quits when its window is destroyed (gtk_main_quit from
 * "destroy"), as Gnumeric, AbiWord and GTK's examples do. On RISC OS that
 * quit, made while the window's close (Close_Window_Request) is handled,
 * is ignored (gtkmain.c, gtkiconbar-riscos.c), and a later Quit isn't.
 *   closelast-fake          the window closes; the program carries on;
 *                           a quit made later (as Quit on the icon bar's
 *                           menu) ends it
 *   closelast-fake quit     with RISCOS_GTK_QUIT_ON_CLOSE=1: the close
 *                           ends it, as on other systems
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int n_tests, failures;
static int destroyed, polls_after, quit_later;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    failures++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
}

static void
on_destroy (GtkWidget *w, gpointer d)
{
  destroyed = 1;
  gtk_main_quit ();
}

/* Before each Wimp_Poll returns: once the window has gone, count the
   polls (the program is still running), then quit as the icon bar's
   Quit would (outside a window's close) */
static void
hook (int reason)
{
  if (!destroyed)
    return;
  if (++polls_after == 40 && !quit_later)
    {
      quit_later = 1;
      gtk_main_quit ();
    }
}

int
main (int argc, char **argv)
{
  gboolean expect_quit = argc > 1 && strcmp (argv[1], "quit") == 0;
  GtkWidget *win;

  alarm (120);
  fake_wimp_hook = hook;          /* no script: the fake closes the window */
  gtk_init (&argc, &argv);
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "closelast");
  gtk_window_set_default_size (GTK_WINDOW (win), 200, 100);
  g_signal_connect (win, "destroy", G_CALLBACK (on_destroy), NULL);
  gtk_widget_show (win);
  gtk_main ();

  check (destroyed, "the window was closed and destroyed");
  check (gdk_riscos_iconbar_enabled (), "the program has an icon on the icon bar");
  if (expect_quit)
    check (polls_after < 40 && !quit_later,
           "RISCOS_GTK_QUIT_ON_CLOSE=1: the close ended the program");
  else
    {
      check (polls_after >= 40, "the program carried on after its last window closed");
      check (quit_later, "a quit made afterwards (the icon bar's Quit) ended it");
    }
  printf ("1..%d\n", n_tests);
  return failures;
}
