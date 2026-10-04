/*
 * hello.c - riscos-gtk's M2 test: a GTK 3 window with a button that
 * redraws and clicks, in the RISCOS theme.
 *
 *   hello [APPDIR [LOGFILE]]
 *
 * APPDIR (a UnixLib path on RISC OS, e.g. /<GTKHello$Dir>) holds
 * fonts/fonts.conf (DejaVu Sans) and share/themes/RISCOS (the theme). Both
 * are set up for this program only (setenv of names without '$').
 *
 * Built for RISC OS as !GTKHello, and for arm-linux with the fake Wimp
 * (tests/gtk/hellotest.c) to run under qemu-arm-aligntrap.
 *
 * MIT licence (see LICENSE).
 */
#include "hello.h"
#include <stdio.h>
#include <string.h>

GtkWidget *hello_window, *hello_button, *hello_label, *hello_entry, *hello_check;
int hello_clicks, hello_option;
char hello_entry_text[64];

static void
on_clicked (GtkButton *button, gpointer data)
{
  char text[64];

  hello_clicks++;
  snprintf (text, sizeof text, "Clicked %d time%s", hello_clicks, hello_clicks == 1 ? "" : "s");
  gtk_label_set_text (GTK_LABEL (hello_label), text);
  printf ("hello: clicked %d\n", hello_clicks);
  fflush (stdout);
}

static void
on_destroy (GtkWidget *w, gpointer data)
{
  g_strlcpy (hello_entry_text, gtk_entry_get_text (GTK_ENTRY (hello_entry)), sizeof hello_entry_text);
  hello_option = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hello_check));
  printf ("hello: window closed, entry text \"%s\", option %s\n",
          hello_entry_text, hello_option ? "on" : "off");
  fflush (stdout);
  gtk_main_quit ();
}

int
hello_main (int argc, char **argv)
{
  GtkWidget *box, *row;

  if (argc > 2)
    {
      /* A log file (on RISC OS, a Wimp task has nowhere to print) */
      if (!freopen (argv[2], "w", stdout) || !freopen (argv[2], "a", stderr))
        return 1;
      setvbuf (stderr, NULL, _IONBF, 0);
    }
  if (argc > 1)
    {
      char *conf = g_build_filename (argv[1], "fonts", "fonts.conf", NULL);
      char *share = g_build_filename (argv[1], "share", NULL);
      g_setenv ("FONTCONFIG_FILE", conf, TRUE);
      g_setenv ("XDG_DATA_DIRS", share, TRUE);
      g_free (conf);
      g_free (share);
    }
  g_set_prgname ("GTKHello");
  gtk_init (&argc, &argv);
  g_object_set (gtk_settings_get_default (),
                "gtk-theme-name", "RISCOS",
                "gtk-font-name", "DejaVu Sans 10",
                "gtk-enable-animations", FALSE,
                "gtk-dialogs-use-header", FALSE,
                NULL);

  hello_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (hello_window), "GTK on RISC OS \xe2\x80\x94 Gr\xc3\xbc\xc3\x9f" "e");
  gtk_window_set_default_size (GTK_WINDOW (hello_window), 320, 180);
  gtk_window_move (GTK_WINDOW (hello_window), 200, 150);
  g_signal_connect (hello_window, "destroy", G_CALLBACK (on_destroy), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);
  gtk_container_add (GTK_CONTAINER (hello_window), box);

  hello_label = gtk_label_new ("Not clicked yet");
  gtk_box_pack_start (GTK_BOX (box), hello_label, FALSE, FALSE, 0);

  /* Colours: if red and blue come out swapped, the sprite's colour order
     is being plotted wrongly (TRGB sprite on this screen). */
  row = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (row),
                        "<span foreground='#CC0000'><b>Red</b></span>  "
                        "<span foreground='#008800'><b>Green</b></span>  "
                        "<span foreground='#0000CC'><b>Blue</b></span>");
  gtk_box_pack_start (GTK_BOX (box), row, FALSE, FALSE, 0);

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (box), row, FALSE, FALSE, 0);
  hello_button = gtk_button_new_with_label ("Click me");
  g_signal_connect (hello_button, "clicked", G_CALLBACK (on_clicked), NULL);
  gtk_box_pack_start (GTK_BOX (row), hello_button, FALSE, FALSE, 0);
  hello_check = gtk_check_button_new_with_label ("Option");
  gtk_box_pack_start (GTK_BOX (row), hello_check, FALSE, FALSE, 0);

  hello_entry = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (hello_entry), "Type here");
  gtk_box_pack_start (GTK_BOX (box), hello_entry, FALSE, FALSE, 0);

  gtk_widget_show_all (hello_window);
  printf ("hello: window shown\n");
  fflush (stdout);
  gtk_main ();
  printf ("hello: %d click(s)\n", hello_clicks);
  return 0;
}

#ifndef HELLO_NO_MAIN
int
main (int argc, char **argv)
{
  return hello_main (argc, argv);
}
#endif
