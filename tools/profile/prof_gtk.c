/*
 * prof_gtk.c - a typical GTK window on the fake RISC OS, doing one thing
 * N times, for instruction profiles (insprof.c, symsum.py). No pictures
 * are saved and nothing is checked, so the counts are GTK's, the
 * backend's and the fake's (symsum.py lists the fake apart).
 *   prof_gtk APPDIR MODE N
 * MODE:
 *   start    open the window and draw it once (N ignored)
 *   redraw   the whole window drawn again, N times
 *   type     a character typed into the text, N times
 *   scroll   Page Down in the text, N times
 *   menu     the window's menu opened and closed with Escape, N times
 * The window: a 600x500 text view with 400 lines in a scrolled window,
 * a row of buttons, an entry, a check box and a combo, as an editor's.
 * Run it with N = 0 and N = 40 and subtract (symsum.py --base) for the
 * cost of one. MIT licence.
 */
#include <gtk/gtk.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GtkWidget *win, *view, *entry;
static const char *mode;
static int n, done, stage, waits, drawn, chars;

static void
add (int reason, int a, int b, int c)
{
  int *e = fake_wimp_script[fake_wimp_script_len++];
  e[0] = reason; e[1] = a; e[2] = b; e[3] = c;
}

static void
settle (void)
{
  int i;
  for (i = 0; i < 3; i++)
    {
      add (FAKE_SLEEP, 20, 0, 0);
      add (FAKE_NULL, 0, 0, 0);
    }
}

static gboolean
on_draw (GtkWidget *w, cairo_t *cr, gpointer d)
{
  drawn++;
  return FALSE;
}

static void
hook (int reason)
{

  if (fake_wimp_script_pos () < fake_wimp_script_len - 1)
    return;
  if (stage == 0)
    {
      if (win && gtk_widget_get_mapped (win) && drawn > 0 && fake_plots > 0)
        {
          int i, best = -1, tx = 0, ty = 0;
          stage = 1;
          /* events to the window that's open (the newest made may be
             another, never opened) */
          for (i = 0; i < FAKE_MAX_WINDOWS; i++)
            if (fake_windows[i].handle && (best < 0 || fake_windows[i].stack > fake_windows[best].stack))
              best = i;
          if (best >= 0)
            fake_target_window = fake_windows[best].handle;
          /* the caret in the text (window coordinates: the view is in
             the scrolled window's own GdkWindow) */
          gtk_widget_translate_coordinates (view, win, 40, 40, &tx, &ty);
          add (FAKE_MOVE, tx, ty, 0);
          add (6, tx, ty, 4);
          add (FAKE_RELEASE, 0, 0, 0);
          settle ();
        }
      else if (++waits > 400)
        stage = 99;
      else
        add (FAKE_NULL, 0, 0, 0);
      return;
    }
  if (stage == 1)
    {
      if (strcmp (mode, "start") == 0 || done >= n)
        {
          stage = 2;
          chars = gtk_text_buffer_get_char_count (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
          gtk_main_quit ();
          return;
        }
      done++;
      if (strcmp (mode, "redraw") == 0)
        gtk_widget_queue_draw (win);
      else if (strcmp (mode, "type") == 0)
        add (8, 'a' + done % 26, 0, 0);
      else if (strcmp (mode, "scroll") == 0)
        add (8, 0x19E, 0, 0);           /* Shift-Down: Page Down */
      else if (strcmp (mode, "menu") == 0)
        {
          int tx = 0, ty = 0;
          gtk_widget_translate_coordinates (view, win, 100, 100, &tx, &ty);
          add (FAKE_MOVE, tx, ty, 0);
          add (6, tx, ty, 2);                 /* Menu */
          add (FAKE_RELEASE, 0, 0, 0);
          settle ();
          add (8, 27, 0, 0);                  /* Escape */
        }
      settle ();
    }
}

int
main (int argc, char **argv)
{
  GtkWidget *box, *row, *sw, *b;
  GString *text = g_string_new (NULL);
  int i;

  if (argc < 4)
    return 2;
  mode = argv[2];
  n = atoi (argv[3]);
  g_setenv ("FONTCONFIG_FILE", g_build_filename (argv[1], "fonts", "fonts.conf", NULL), TRUE);
  g_setenv ("XDG_DATA_DIRS", g_build_filename (argv[1], "share", NULL), TRUE);
  fake_set_screen (1280, 1024, 0, 5);
  fake_wimp_nulls = 40;
  fake_wimp_hook = hook;
  gtk_init (&argc, &argv);
  /* no blinking: each blink would be counted as part of the action */
  g_object_set (gtk_settings_get_default (), "gtk-cursor-blink", FALSE, NULL);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Profile");
  gtk_window_set_default_size (GTK_WINDOW (win), 600, 500);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  for (i = 0; i < 5; i++)
    {
      char l[16];
      g_snprintf (l, sizeof l, "Button %d", i + 1);
      b = gtk_button_new_with_label (l);
      gtk_container_add (GTK_CONTAINER (row), b);
    }
  gtk_container_add (GTK_CONTAINER (row), gtk_check_button_new_with_label ("Wrap"));
  b = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (b), "One");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (b), "Two");
  gtk_combo_box_set_active (GTK_COMBO_BOX (b), 0);
  gtk_container_add (GTK_CONTAINER (row), b);
  gtk_container_add (GTK_CONTAINER (box), row);
  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), "A line of text in an entry");
  gtk_container_add (GTK_CONTAINER (box), entry);
  for (i = 0; i < 400; i++)
    g_string_append_printf (text, "Line %d: the quick brown fox jumps over the lazy dog, %d times.\n", i + 1, i * 7);
  view = gtk_text_view_new ();
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), text->str, -1);
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_container_add (GTK_CONTAINER (sw), view);
  gtk_container_add (GTK_CONTAINER (box), sw);
  gtk_container_add (GTK_CONTAINER (win), box);
  g_signal_connect (win, "draw", G_CALLBACK (on_draw), NULL);
  gtk_widget_show_all (win);
  gtk_widget_grab_focus (view);
  gtk_main ();
  printf ("prof_gtk %s: %d done, %d draws, %d characters\n", mode, done, drawn, chars);
  return 0;
}
