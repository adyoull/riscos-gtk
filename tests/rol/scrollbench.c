/*
 * scrollbench.c - on RISC OS: how fast a GTK window redraws while it
 * scrolls. A 900x640 text view of 3000 lines is scrolled 300 times (40
 * pixels a step, each step drawn and on screen before the next), timed,
 * and the program quits. The result is appended to the file named by its
 * second argument, with how windows were plotted (RISCOS_GTK_DIRECT_PLOT:
 * straight into screen memory, or with OS_SpriteOp when it's 0). The
 * window then stays 15 s, scrolled to line 1000, for a screenshot.
 * MIT licence.
 */
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>

#define STEPS 300

static GtkWidget *view, *scroller;
static const char *out;
static int step;
static gint64 start;

static gboolean
run (gpointer d)
{
  GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroller));
  GdkWindow *w = gtk_widget_get_window (gtk_widget_get_toplevel (view));
  const char *e = g_getenv ("RISCOS_GTK_DIRECT_PLOT");
  double secs;
  FILE *f;

  start = g_get_monotonic_time ();
  for (step = 0; step < STEPS; step++)
    {
      double v = gtk_adjustment_get_value (adj) + 40;
      if (v > gtk_adjustment_get_upper (adj) - gtk_adjustment_get_page_size (adj))
        v = 0;
      gtk_adjustment_set_value (adj, v);
      gdk_window_process_updates (w, TRUE);   /* drawn and on screen now */
      gdk_display_flush (gdk_display_get_default ());
      while (gtk_events_pending ())
        gtk_main_iteration ();
    }
  secs = (g_get_monotonic_time () - start) / 1e6;
  f = fopen (out, "a");
  if (f)
    {
      fprintf (f, "%s: %d scroll steps in %.2f s, %.1f a second\n",
               e && atoi (e) == 0 ? "OS_SpriteOp        " : "straight to screen ",
               STEPS, secs, STEPS / secs);
      fclose (f);
    }
  gtk_adjustment_set_value (adj, gtk_adjustment_get_upper (adj) * 999 / 3000);
  g_timeout_add (15000, (GSourceFunc) gtk_main_quit, NULL);
  return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
  GtkWidget *win;
  GString *text = g_string_new (NULL);
  int i;

  out = argc > 2 ? argv[2] : "scrollbench.txt";
  gtk_init (&argc, &argv);
  for (i = 0; i < 3000; i++)
    g_string_append_printf (text, "Line %d: the quick brown fox jumps over the lazy dog, "
                            "pack my box with five dozen liquor jugs %d\n", i + 1, i * 7);
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Scroll bench");
  gtk_window_set_default_size (GTK_WINDOW (win), 900, 640);
  scroller = gtk_scrolled_window_new (NULL, NULL);
  view = gtk_text_view_new ();
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), text->str, -1);
  gtk_container_add (GTK_CONTAINER (scroller), view);
  gtk_container_add (GTK_CONTAINER (win), scroller);
  gtk_widget_show_all (win);
  g_timeout_add (3000, run, NULL);
  gtk_main ();
  return 0;
}
