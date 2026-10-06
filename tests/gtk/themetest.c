/*
 * themetest.c - riscos-gtk's theme test: every common GTK 3 widget in the
 * RISCOS theme, on four pages, with a menu bar, pop-ups, tooltips and a
 * dialogue. Every interaction is logged, so a run on the Pi says what
 * worked as well as how it looked.
 *
 *   themetest [APPDIR [LOGFILE [PAGE]]]
 *
 * APPDIR (a UnixLib path on RISC OS, e.g. /<GTKThemeTest$Dir>) holds
 * fonts/fonts.conf and share/themes/RISCOS, as for GTKHello. PAGE (0-3)
 * opens on that page (for screenshots). Without APPDIR it uses the
 * machine's own fonts and themes (for Linux screenshots).
 *
 * Built for RISC OS as !GTKThemeTest, for arm-linux with the fake Wimp
 * (tests/gtk/themetest-fake.c) and for the build machine's own GTK
 * (theme/shoot.sh). MIT licence (see LICENSE).
 */
#include "themetest.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

GtkWidget *tt_window, *tt_notebook, *tt_status, *tt_combo, *tt_menubar, *tt_context_area, *tt_apply;
GtkWidget *tt_entry, *tt_entry_empty;
GtkWidget *tt_save_button, *tt_open_button, *tt_drop_area;
GtkWidget *tt_save_dialog_button, *tt_open_dialog_button;
char tt_last_file[256];             /* the last file chosen or dropped */
int tt_events;

static void
say (const char *fmt, ...)
{
  char text[256];
  va_list ap;

  va_start (ap, fmt);
  g_vsnprintf (text, sizeof text, fmt, ap);
  va_end (ap);
  printf ("theme: %s\n", text);
  fflush (stdout);
  tt_events++;
  if (tt_status)
    {
      gtk_statusbar_pop (GTK_STATUSBAR (tt_status), 0);
      gtk_statusbar_push (GTK_STATUSBAR (tt_status), 0, text);
    }
}

/* ---- signal handlers: each one just reports what happened ---- */

static void
on_button (GtkButton *b, gpointer name)
{
  say ("button \"%s\" clicked", (const char *) name);
}

static void
on_toggled (GtkToggleButton *b, gpointer name)
{
  /* GTK leaves a "mixed" (inconsistent) option mixed whatever the user
     does: the program has to clear it. The first click makes it on. */
  if (gtk_toggle_button_get_inconsistent (b))
    gtk_toggle_button_set_inconsistent (b, FALSE);
  say ("%s %s", (const char *) name, gtk_toggle_button_get_active (b) ? "on" : "off");
}

static void
on_entry_activate (GtkEntry *e, gpointer name)
{
  say ("%s: Return with \"%s\"", (const char *) name, gtk_entry_get_text (e));
}

static void
on_spin (GtkSpinButton *s, gpointer name)
{
  say ("%s = %d", (const char *) name, gtk_spin_button_get_value_as_int (s));
}

static void
on_combo (GtkComboBox *c, gpointer name)
{
  gchar *t = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (c));
  say ("%s: \"%s\" chosen", (const char *) name, t ? t : "");
  g_free (t);
}

static void
on_scale (GtkRange *r, gpointer name)
{
  say ("%s = %d", (const char *) name, (int) gtk_range_get_value (r));
}

static void
on_switch (GObject *o, GParamSpec *p, gpointer name)
{
  say ("%s %s", (const char *) name, gtk_switch_get_active (GTK_SWITCH (o)) ? "on" : "off");
}

static void
on_page (GtkNotebook *nb, GtkWidget *page, guint n, gpointer data)
{
  say ("page %u: %s", n, gtk_notebook_get_tab_label_text (nb, page));
}

static void
on_row (GtkTreeSelection *sel, gpointer name)
{
  GtkTreeModel *m;
  GtkTreeIter it;
  gchar *s;

  if (gtk_tree_selection_get_selected (sel, &m, &it))
    {
      gtk_tree_model_get (m, &it, 0, &s, -1);
      say ("%s: \"%s\" selected", (const char *) name, s);
      g_free (s);
    }
}

static void
on_menu (GtkMenuItem *mi, gpointer data)
{
  if (GTK_IS_CHECK_MENU_ITEM (mi))
    {
      if (GTK_IS_RADIO_MENU_ITEM (mi) && !gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (mi)))
        return;
      say ("menu \"%s\" %s", gtk_menu_item_get_label (mi),
           gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (mi)) ? "ticked" : "unticked");
    }
  else
    say ("menu \"%s\"", gtk_menu_item_get_label (mi));
}

static void
on_quit (GtkMenuItem *mi, gpointer data)
{
  say ("menu \"Quit\"");
  gtk_widget_destroy (tt_window);
}

static void
on_message (GtkButton *b, gpointer data)
{
  GtkWidget *d = gtk_message_dialog_new (GTK_WINDOW (tt_window), GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                         "Save changes to \"Accounts\" before closing?");
  int r;

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (d),
                                            "If you don't save, your changes will be lost.");
  gtk_dialog_add_buttons (GTK_DIALOG (d), "Discard", 1, "Cancel", GTK_RESPONSE_CANCEL,
                          "Save", GTK_RESPONSE_ACCEPT, NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (d), GTK_RESPONSE_ACCEPT);
  say ("dialogue opened");
  r = gtk_dialog_run (GTK_DIALOG (d));
  say ("dialogue closed: %s", r == GTK_RESPONSE_ACCEPT ? "Save" : r == 1 ? "Discard"
       : r == GTK_RESPONSE_CANCEL ? "Cancel" : "closed");
  gtk_widget_destroy (d);
}

/* GtkFileChooserNative: on RISC OS a Save box (drag the icon to a Filer
   window) or an Open box (drag a file to it) */
static void
on_chooser (GtkButton *b, gpointer data)
{
  gboolean save = GPOINTER_TO_INT (data);
  GtkFileChooserNative *n = gtk_file_chooser_native_new (save ? "Save as" : "Open",
                                                         GTK_WINDOW (tt_window),
                                                         save ? GTK_FILE_CHOOSER_ACTION_SAVE
                                                              : GTK_FILE_CHOOSER_ACTION_OPEN,
                                                         NULL, NULL);
  int r;

  if (save)
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (n), "Accounts.csv");
  say ("%s box opened", save ? "Save" : "Open");
  r = gtk_native_dialog_run (GTK_NATIVE_DIALOG (n));
  if (r == GTK_RESPONSE_ACCEPT)
    {
      char *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (n));
      g_strlcpy (tt_last_file, path ? path : "", sizeof tt_last_file);
      if (save && path)
        {
          /* save something, so the receiver has a file */
          GError *err = NULL;
          if (!g_file_set_contents (path, "Month,Total\nOctober,2026\n", -1, &err))
            {
              say ("Save failed: %s", err->message);
              g_error_free (err);
            }
          else
            say ("saved %s", path);
        }
      else
        say ("chosen: %s", path ? path : "(none)");
      g_free (path);
    }
  else
    say ("%s box cancelled", save ? "Save" : "Open");
  g_object_unref (n);
}

/* The same with GtkFileChooserDialog, as most programs do it: on RISC OS
   a Save box or an Open box too */
static void
on_chooser_dialog (GtkButton *b, gpointer data)
{
  gboolean save = GPOINTER_TO_INT (data);
  GtkWidget *d = gtk_file_chooser_dialog_new (save ? "Save File" : "Open File",
                                              GTK_WINDOW (tt_window),
                                              save ? GTK_FILE_CHOOSER_ACTION_SAVE
                                                   : GTK_FILE_CHOOSER_ACTION_OPEN,
                                              "_Cancel", GTK_RESPONSE_CANCEL,
                                              save ? "_Save" : "_Open", GTK_RESPONSE_ACCEPT,
                                              NULL);
  int r;

  if (save)
    {
      gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (d), TRUE);
      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (d), "Accounts.csv");
    }
  say ("%s dialogue opened", save ? "Save" : "Open");
  r = gtk_dialog_run (GTK_DIALOG (d));
  if (r == GTK_RESPONSE_ACCEPT)
    {
      char *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (d));
      g_strlcpy (tt_last_file, path ? path : "", sizeof tt_last_file);
      if (save && path)
        {
          GError *err = NULL;
          if (!g_file_set_contents (path, "Month,Total\nNovember,2026\n", -1, &err))
            {
              say ("Save failed: %s", err->message);
              g_error_free (err);
            }
          else
            say ("saved %s (dialogue)", path);
        }
      else
        say ("chosen: %s (dialogue)", path ? path : "(none)");
      g_free (path);
    }
  else
    say ("%s dialogue cancelled (%d)", save ? "Save" : "Open", r);
  gtk_widget_destroy (d);
}

/* A file dropped from the Filer arrives as a file:// URI */
static void
on_drop_received (GtkWidget *w, GdkDragContext *ctx, gint x, gint y,
                  GtkSelectionData *sel, guint info, guint time, gpointer data)
{
  char **uris = gtk_selection_data_get_uris (sel);
  char *path = uris && uris[0] ? g_filename_from_uri (uris[0], NULL, NULL) : NULL;

  if (path)
    {
      g_strlcpy (tt_last_file, path, sizeof tt_last_file);
      gtk_label_set_text (GTK_LABEL (gtk_bin_get_child (GTK_BIN (w))), path);
      say ("dropped: %s", path);
    }
  gtk_drag_finish (ctx, path != NULL, FALSE, time);
  g_free (path);
  g_strfreev (uris);
}

static gboolean
on_context (GtkWidget *w, GdkEventButton *ev, gpointer menu)
{
  if (ev->type == GDK_BUTTON_PRESS && ev->button == 3)
    {
      say ("Menu button: context menu");
      gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) ev);
      return TRUE;                   /* used: not the window menu */
    }
  return FALSE;
}

static void
on_destroy (GtkWidget *w, gpointer data)
{
  printf ("theme: window closed, %d events\n", tt_events);
  fflush (stdout);
  tt_window = NULL;
  tt_status = NULL;
  gtk_main_quit ();
}

/* ---- building blocks ---- */

static GtkWidget *
button (const char *label, const char *tip)
{
  GtkWidget *b = gtk_button_new_with_label (label);
  g_signal_connect (b, "clicked", G_CALLBACK (on_button), (gpointer) label);
  if (tip)
    gtk_widget_set_tooltip_text (b, tip);
  return b;
}

static GtkWidget *
labelled (const char *text, GtkWidget *w)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *l = gtk_label_new (text);
  gtk_label_set_xalign (GTK_LABEL (l), 1);
  gtk_widget_set_size_request (l, 110, -1);
  gtk_box_pack_start (GTK_BOX (box), l, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);
  return box;
}

static GtkWidget *
frame (const char *title, GtkWidget *child)
{
  GtkWidget *f = gtk_frame_new (title);
  gtk_container_set_border_width (GTK_CONTAINER (child), 8);
  gtk_container_add (GTK_CONTAINER (f), child);
  return f;
}

static GtkWidget *
vbox (int spacing)
{
  return gtk_box_new (GTK_ORIENTATION_VERTICAL, spacing);
}

static GtkWidget *
check (const char *label, gboolean on, gboolean sensitive, gboolean mixed)
{
  GtkWidget *c = gtk_check_button_new_with_label (label);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (c), on);
  gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (c), mixed);
  gtk_widget_set_sensitive (c, sensitive);
  g_signal_connect (c, "toggled", G_CALLBACK (on_toggled), (gpointer) label);
  return c;
}

/* ---- page 1: buttons and fields ---- */

static GtkWidget *
page_buttons (void)
{
  GtkWidget *page = vbox (10), *row, *col, *w, *link;

  gtk_container_set_border_width (GTK_CONTAINER (page), 12);

  /* action buttons */
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start (GTK_BOX (row), button ("Action", "An action button (R5)"), FALSE, FALSE, 0);
  w = button ("Shaded", NULL);
  gtk_widget_set_sensitive (w, FALSE);
  gtk_box_pack_start (GTK_BOX (row), w, FALSE, FALSE, 0);
  w = gtk_toggle_button_new_with_label ("Toggle");
  g_signal_connect (w, "toggled", G_CALLBACK (on_toggled), "Toggle");
  gtk_box_pack_start (GTK_BOX (row), w, FALSE, FALSE, 0);
  link = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (link), "linked");
  gtk_box_pack_start (GTK_BOX (link), button ("Left", NULL), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (link), button ("Middle", NULL), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (link), button ("Right", NULL), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (row), link, FALSE, FALSE, 0);
  w = gtk_link_button_new_with_label ("https://www.riscosopen.org", "Link button");
  gtk_box_pack_end (GTK_BOX (row), w, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (page), frame ("Action buttons", row), FALSE, FALSE, 0);

  /* fields */
  col = vbox (6);
  tt_entry = w = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (w), "Accounts2026/csv");
  g_signal_connect (w, "activate", G_CALLBACK (on_entry_activate), "Writable");
  gtk_widget_set_tooltip_text (w, "A writable icon (R7): click and type");
  gtk_box_pack_start (GTK_BOX (col), labelled ("Writable", w), FALSE, FALSE, 0);
  tt_entry_empty = w = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (w), "Type here");
  g_signal_connect (w, "activate", G_CALLBACK (on_entry_activate), "Empty");
  gtk_box_pack_start (GTK_BOX (col), labelled ("Empty", w), FALSE, FALSE, 0);
  w = gtk_entry_new ();
  gtk_entry_set_visibility (GTK_ENTRY (w), FALSE);
  gtk_entry_set_invisible_char (GTK_ENTRY (w), '*');
  gtk_entry_set_text (GTK_ENTRY (w), "secret");
  gtk_box_pack_start (GTK_BOX (col), labelled ("Password", w), FALSE, FALSE, 0);
  w = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (w), "Can't be changed");
  gtk_widget_set_sensitive (w, FALSE);
  gtk_box_pack_start (GTK_BOX (col), labelled ("Shaded", w), FALSE, FALSE, 0);
  w = gtk_label_new ("12,480 bytes");
  gtk_label_set_xalign (GTK_LABEL (w), 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "display-field");
  gtk_box_pack_start (GTK_BOX (col), labelled ("Display field", w), FALSE, FALSE, 0);
  w = gtk_spin_button_new_with_range (0, 99, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), 2);
  g_signal_connect (w, "value-changed", G_CALLBACK (on_spin), "Number range");
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (row), w, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (col), labelled ("Number range", row), FALSE, FALSE, 0);
  tt_combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (tt_combo), "Latin-1 (RISC OS)");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (tt_combo), "UTF-8");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (tt_combo), "UTF-16");
  gtk_combo_box_set_active (GTK_COMBO_BOX (tt_combo), 0);
  g_signal_connect (tt_combo, "changed", G_CALLBACK (on_combo), "Pop-up");
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (row), tt_combo, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (col), labelled ("Pop-up", row), FALSE, FALSE, 0);
  w = gtk_combo_box_text_new_with_entry ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (w), "Homerton");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (w), "Trinity");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (w), "Corpus");
  gtk_combo_box_set_active (GTK_COMBO_BOX (w), 0);
  g_signal_connect (w, "changed", G_CALLBACK (on_combo), "Font");
  gtk_box_pack_start (GTK_BOX (col), labelled ("Writable + menu", w), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (page), frame ("Fields", col), FALSE, FALSE, 0);

  w = gtk_label_new ("Text: normal, <b>bold</b>, <i>italic</i>, "
                     "<span foreground='#dd0000'>red</span>, <span foreground='#00cc00'>green</span>, "
                     "<span foreground='#004499'>dark blue</span>; £ € é ß ü");
  gtk_label_set_use_markup (GTK_LABEL (w), TRUE);
  gtk_label_set_xalign (GTK_LABEL (w), 0);
  gtk_box_pack_start (GTK_BOX (page), w, FALSE, FALSE, 0);
  return page;
}

/* ---- page 2: options, sliders, progress ---- */

static GtkWidget *
page_options (void)
{
  GtkWidget *page = gtk_grid_new (), *col, *w, *r1, *r2, *r3, *hb;

  gtk_container_set_border_width (GTK_CONTAINER (page), 12);
  gtk_grid_set_row_spacing (GTK_GRID (page), 10);
  gtk_grid_set_column_spacing (GTK_GRID (page), 10);

  col = vbox (2);
  gtk_box_pack_start (GTK_BOX (col), check ("Include headings", TRUE, TRUE, FALSE), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (col), check ("Quote all fields", FALSE, TRUE, FALSE), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (col), check ("All sheets (mixed)", FALSE, TRUE, TRUE), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (col), check ("Set filetype (shaded)", TRUE, FALSE, FALSE), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (col), check ("Off and shaded", FALSE, FALSE, FALSE), FALSE, FALSE, 0);
  gtk_widget_set_hexpand (col, TRUE);
  gtk_grid_attach (GTK_GRID (page), frame ("Option icons", col), 0, 0, 1, 1);

  col = vbox (2);
  r1 = gtk_radio_button_new_with_label (NULL, "CSV (comma separated)");
  r2 = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (r1), "Tab separated");
  r3 = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (r1), "Fireworkz (shaded)");
  gtk_widget_set_sensitive (r3, FALSE);
  g_signal_connect (r1, "toggled", G_CALLBACK (on_toggled), "CSV");
  g_signal_connect (r2, "toggled", G_CALLBACK (on_toggled), "Tab separated");
  gtk_box_pack_start (GTK_BOX (col), r1, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (col), r2, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (col), r3, FALSE, FALSE, 0);
  hb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  w = gtk_switch_new ();
  g_signal_connect (w, "notify::active", G_CALLBACK (on_switch), "Switch");
  gtk_box_pack_start (GTK_BOX (hb), gtk_label_new ("Switch"), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hb), w, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (col), hb, FALSE, FALSE, 6);
  gtk_widget_set_hexpand (col, TRUE);
  gtk_grid_attach (GTK_GRID (page), frame ("Radio icons", col), 1, 0, 1, 1);

  col = vbox (8);
  w = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 9, 1);
  gtk_range_set_value (GTK_RANGE (w), 6);
  gtk_scale_set_draw_value (GTK_SCALE (w), FALSE);
  g_signal_connect (w, "value-changed", G_CALLBACK (on_scale), "Compression");
  gtk_box_pack_start (GTK_BOX (col), labelled ("Compression", w), FALSE, FALSE, 0);
  w = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  gtk_range_set_value (GTK_RANGE (w), 75);
  gtk_scale_set_value_pos (GTK_SCALE (w), GTK_POS_RIGHT);
  g_signal_connect (w, "value-changed", G_CALLBACK (on_scale), "Volume");
  gtk_box_pack_start (GTK_BOX (col), labelled ("Volume", w), FALSE, FALSE, 0);
  w = gtk_progress_bar_new ();
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (w), 0.42);
  gtk_box_pack_start (GTK_BOX (col), labelled ("Progress", w), FALSE, FALSE, 0);
  w = gtk_progress_bar_new ();
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (w), 0.8);
  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (w), "80%");
  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (w), TRUE);
  gtk_box_pack_start (GTK_BOX (col), labelled ("With text", w), FALSE, FALSE, 0);
  w = gtk_level_bar_new_for_interval (0, 10);
  gtk_level_bar_set_value (GTK_LEVEL_BAR (w), 6);
  gtk_box_pack_start (GTK_BOX (col), labelled ("Level", w), FALSE, FALSE, 0);
  gtk_grid_attach (GTK_GRID (page), frame ("Sliders and bars", col), 0, 1, 1, 1);

  hb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 16);
  w = gtk_scale_new_with_range (GTK_ORIENTATION_VERTICAL, 0, 10, 1);
  gtk_range_set_inverted (GTK_RANGE (w), TRUE);
  gtk_range_set_value (GTK_RANGE (w), 4);
  gtk_widget_set_size_request (w, -1, 140);
  g_signal_connect (w, "value-changed", G_CALLBACK (on_scale), "Vertical");
  gtk_box_pack_start (GTK_BOX (hb), w, FALSE, FALSE, 0);
  w = gtk_level_bar_new_for_interval (0, 1);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (w), GTK_ORIENTATION_VERTICAL);
  gtk_level_bar_set_inverted (GTK_LEVEL_BAR (w), TRUE);
  gtk_level_bar_set_value (GTK_LEVEL_BAR (w), 0.3);
  gtk_box_pack_start (GTK_BOX (hb), w, FALSE, FALSE, 0);
  col = vbox (6);
  gtk_box_pack_start (GTK_BOX (col), gtk_label_new ("Separators:"), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (col), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
  w = gtk_label_new ("Dim label");
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "dim-label");
  gtk_box_pack_start (GTK_BOX (col), w, FALSE, FALSE, 0);
  w = gtk_label_new ("Shaded label");
  gtk_widget_set_sensitive (w, FALSE);
  gtk_box_pack_start (GTK_BOX (col), w, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hb), col, TRUE, TRUE, 0);
  gtk_grid_attach (GTK_GRID (page), frame ("Vertical", hb), 1, 1, 1, 1);
  return page;
}

/* ---- page 3: lists, a tree and text ---- */

static GtkWidget *
scrolled (GtkWidget *child, int h)
{
  GtkWidget *sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_widget_set_size_request (sw, -1, h);
  gtk_container_add (GTK_CONTAINER (sw), child);
  return sw;
}

static GtkWidget *
page_lists (void)
{
  static const char *names[] = { "Sheet1", "Sheet2", "Summary", "Q1", "Q2", "Q3", "Q4",
                                 "Notes", "Archive", "Rates", "Staff", "Assets" };
  GtkListStore *ls = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);
  GtkTreeStore *ts = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  GtkWidget *page = gtk_grid_new (), *tv, *tree, *text;
  GtkTreeIter it, top, sub;
  GtkTextBuffer *buf;
  GtkTextIter end;
  int i;

  gtk_container_set_border_width (GTK_CONTAINER (page), 12);
  gtk_grid_set_row_spacing (GTK_GRID (page), 10);
  gtk_grid_set_column_spacing (GTK_GRID (page), 10);

  for (i = 0; i < 40; i++)
    {
      char desc[32];
      g_snprintf (desc, sizeof desc, "Row %d of 40", i + 1);
      gtk_list_store_append (ls, &it);
      gtk_list_store_set (ls, &it, 0, names[i % 12], 1, (i * 37) % 500, 2, desc, -1);
    }
  tv = gtk_tree_view_new_with_model (GTK_TREE_MODEL (ls));
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv),
    gtk_tree_view_column_new_with_attributes ("Sheet", gtk_cell_renderer_text_new (), "text", 0, NULL));
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv),
    gtk_tree_view_column_new_with_attributes ("Rows", gtk_cell_renderer_text_new (), "text", 1, NULL));
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv),
    gtk_tree_view_column_new_with_attributes ("Description", gtk_cell_renderer_text_new (), "text", 2, NULL));
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (tv)), "changed", G_CALLBACK (on_row), "List");
  gtk_widget_set_hexpand (tv, TRUE);
  gtk_grid_attach (GTK_GRID (page), frame ("List (scroll bars)", scrolled (tv, 180)), 0, 0, 1, 1);

  gtk_tree_store_append (ts, &top, NULL);
  gtk_tree_store_set (ts, &top, 0, "$", 1, "Root", -1);
  gtk_tree_store_append (ts, &sub, &top);
  gtk_tree_store_set (ts, &sub, 0, "!Boot", 1, "Application", -1);
  gtk_tree_store_append (ts, &sub, &top);
  gtk_tree_store_set (ts, &sub, 0, "Apps", 1, "Directory", -1);
  gtk_tree_store_append (ts, &it, &sub);
  gtk_tree_store_set (ts, &it, 0, "!Edit", 1, "Application", -1);
  gtk_tree_store_append (ts, &it, &sub);
  gtk_tree_store_set (ts, &it, 0, "!Draw", 1, "Application", -1);
  gtk_tree_store_append (ts, &sub, &top);
  gtk_tree_store_set (ts, &sub, 0, "Documents", 1, "Directory", -1);
  tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (ts));
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree),
    gtk_tree_view_column_new_with_attributes ("Name", gtk_cell_renderer_text_new (), "text", 0, NULL));
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree),
    gtk_tree_view_column_new_with_attributes ("Type", gtk_cell_renderer_text_new (), "text", 1, NULL));
  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree), gtk_tree_path_new_first (), FALSE);
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree)), "changed", G_CALLBACK (on_row), "Tree");
  gtk_widget_set_hexpand (tree, TRUE);
  gtk_grid_attach (GTK_GRID (page), frame ("Tree (expanders)", scrolled (tree, 180)), 1, 0, 1, 1);

  text = gtk_text_view_new ();
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text), GTK_WRAP_NONE);
  buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text));
  gtk_text_buffer_set_text (buf, "A text view. Click to put the caret here and type; "
                            "select with Select-drag. The lines are long so that the "
                            "horizontal scroll bar appears too.\n", -1);
  for (i = 1; i <= 30; i++)
    {
      char line[96];
      g_snprintf (line, sizeof line, "Line %d: The quick brown fox jumps over the lazy dog. "
                  "0123456789\n", i);
      gtk_text_buffer_get_end_iter (buf, &end);
      gtk_text_buffer_insert (buf, &end, line, -1);
    }
  {
    GtkWidget *sw = scrolled (text, 150);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
    gtk_grid_attach (GTK_GRID (page), frame ("Text (both scroll bars)", sw), 0, 1, 2, 1);
  }
  return page;
}

/* ---- page 4: dialogues, pop-ups, tooltips ---- */

static GtkWidget *context_menu;

static GtkWidget *
menu_item (GtkWidget *menu, const char *label, const char *accel, gboolean sensitive)
{
  GtkWidget *mi = gtk_menu_item_new_with_label (label);
  if (accel)
    {
      guint key;
      GdkModifierType mods;
      gtk_accelerator_parse (accel, &key, &mods);
      gtk_accel_label_set_accel (GTK_ACCEL_LABEL (gtk_bin_get_child (GTK_BIN (mi))), key, mods);
    }
  gtk_widget_set_sensitive (mi, sensitive);
  g_signal_connect (mi, "activate", G_CALLBACK (on_menu), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
  return mi;
}

static GtkWidget *
page_popups (void)
{
  GtkWidget *page = vbox (10), *row, *w, *area, *l, *pop, *pb, *files_rows, *row1;

  gtk_container_set_border_width (GTK_CONTAINER (page), 12);

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  w = gtk_button_new_with_label ("Message dialogue...");
  g_signal_connect (w, "clicked", G_CALLBACK (on_message), NULL);
  gtk_widget_set_tooltip_text (w, "Opens a GtkMessageDialog with Discard, Cancel and Save");
  gtk_box_pack_start (GTK_BOX (row), w, FALSE, FALSE, 0);

  /* a menu button with a popover */
  pb = gtk_menu_button_new ();
  gtk_button_set_label (GTK_BUTTON (pb), "Popover");
  pop = gtk_popover_new (pb);
  l = vbox (4);
  gtk_container_set_border_width (GTK_CONTAINER (l), 8);
  gtk_box_pack_start (GTK_BOX (l), gtk_label_new ("A popover (drawn inside the window)"), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (l), check ("Popover option", TRUE, TRUE, FALSE), FALSE, FALSE, 0);
  gtk_widget_show_all (l);
  gtk_container_add (GTK_CONTAINER (pop), l);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (pb), pop);
  gtk_box_pack_start (GTK_BOX (row), pb, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (page), frame ("Dialogues", row), FALSE, FALSE, 0);

  /* files: Save box, Open box, a drop target */
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  tt_save_button = w = gtk_button_new_with_label ("Save as...");
  g_signal_connect (w, "clicked", G_CALLBACK (on_chooser), GINT_TO_POINTER (TRUE));
  gtk_widget_set_tooltip_text (w, "A Save box: drag its icon to a Filer window");
  gtk_box_pack_start (GTK_BOX (row), w, FALSE, FALSE, 0);
  tt_open_button = w = gtk_button_new_with_label ("Open...");
  g_signal_connect (w, "clicked", G_CALLBACK (on_chooser), GINT_TO_POINTER (FALSE));
  gtk_widget_set_tooltip_text (w, "An Open box: drag a file to it from a Filer window");
  gtk_box_pack_start (GTK_BOX (row), w, FALSE, FALSE, 0);
  {
    /* two rows: GtkFileChooserNative and the drop area; GtkFileChooserDialog */
    GtkWidget *rows = vbox (6);
    gtk_box_pack_start (GTK_BOX (rows), row, FALSE, FALSE, 0);
    files_rows = rows;
    row1 = row;
    row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start (GTK_BOX (rows), row, FALSE, FALSE, 0);
  }
  tt_save_dialog_button = w = gtk_button_new_with_label ("Save (dialogue)...");
  g_signal_connect (w, "clicked", G_CALLBACK (on_chooser_dialog), GINT_TO_POINTER (TRUE));
  gtk_widget_set_tooltip_text (w, "GtkFileChooserDialog, as most programs use: a Save box on RISC OS");
  gtk_box_pack_start (GTK_BOX (row), w, FALSE, FALSE, 0);
  tt_open_dialog_button = w = gtk_button_new_with_label ("Open (dialogue)...");
  g_signal_connect (w, "clicked", G_CALLBACK (on_chooser_dialog), GINT_TO_POINTER (FALSE));
  gtk_widget_set_tooltip_text (w, "GtkFileChooserDialog: an Open box on RISC OS");
  gtk_box_pack_start (GTK_BOX (row), w, FALSE, FALSE, 0);
  tt_drop_area = gtk_event_box_new ();
  l = gtk_label_new ("Drop a file here from a Filer window");
  gtk_widget_set_size_request (l, 260, 40);
  gtk_container_add (GTK_CONTAINER (tt_drop_area), l);
  gtk_drag_dest_set (tt_drop_area, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_uri_targets (tt_drop_area);
  g_signal_connect (tt_drop_area, "drag-data-received", G_CALLBACK (on_drop_received), NULL);
  gtk_box_pack_start (GTK_BOX (row1), tt_drop_area, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (page), frame ("Files", files_rows), FALSE, FALSE, 0);

  /* context menu on Menu click */
  context_menu = gtk_menu_new ();
  menu_item (context_menu, "Select all", "<Control>a", TRUE);
  menu_item (context_menu, "Clear selection", "<Control>z", TRUE);
  gtk_menu_shell_append (GTK_MENU_SHELL (context_menu), gtk_separator_menu_item_new ());
  menu_item (context_menu, "Info", NULL, TRUE);
  menu_item (context_menu, "Shaded entry", NULL, FALSE);
  gtk_widget_show_all (context_menu);
  area = tt_context_area = gtk_event_box_new ();
  l = gtk_label_new ("Click Menu (the middle button) here for a context menu.\n"
                     "Hover over buttons and fields for interactive help (tooltips).");
  gtk_label_set_justify (GTK_LABEL (l), GTK_JUSTIFY_CENTER);
  gtk_widget_set_size_request (l, -1, 80);
  gtk_container_add (GTK_CONTAINER (area), l);
  gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (area, "button-press-event", G_CALLBACK (on_context), context_menu);
  gtk_box_pack_start (GTK_BOX (page), frame ("Menus", area), FALSE, FALSE, 0);

  /* an info bar */
  w = gtk_info_bar_new ();
  gtk_info_bar_set_message_type (GTK_INFO_BAR (w), GTK_MESSAGE_INFO);
  gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (w))),
                     gtk_label_new ("An info bar (cream, like interactive help)"));
  gtk_box_pack_start (GTK_BOX (page), w, FALSE, FALSE, 0);
  w = gtk_info_bar_new ();
  gtk_info_bar_set_message_type (GTK_INFO_BAR (w), GTK_MESSAGE_WARNING);
  gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (w))),
                     gtk_label_new ("A warning bar"));
  gtk_box_pack_start (GTK_BOX (page), w, FALSE, FALSE, 0);

  w = gtk_calendar_new ();
  gtk_calendar_select_month (GTK_CALENDAR (w), 9, 2026);
  gtk_calendar_select_day (GTK_CALENDAR (w), 4);
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (row), w, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (page), frame ("Calendar", row), FALSE, FALSE, 0);
  return page;
}

/* ---- the menu bar ---- */

static GtkWidget *
menubar (void)
{
  GtkWidget *mb = gtk_menu_bar_new (), *top, *menu, *sub, *ck, *ra, *rb, *z1, *z2, *z3, *zoom;

  menu = gtk_menu_new ();
  menu_item (menu, "Open...", "<Control>o", TRUE);
  menu_item (menu, "Save", "F3", TRUE);
  menu_item (menu, "Save as...", "<Shift>F3", TRUE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
  top = gtk_menu_item_new_with_label ("Quit");
  g_signal_connect (top, "activate", G_CALLBACK (on_quit), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), top);
  top = gtk_menu_item_new_with_label ("File");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (top), menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (mb), top);

  menu = gtk_menu_new ();
  menu_item (menu, "Undo", "F8", FALSE);
  menu_item (menu, "Redo", "F9", FALSE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
  menu_item (menu, "Cut", "<Control>x", TRUE);
  menu_item (menu, "Copy", "<Control>c", TRUE);
  menu_item (menu, "Paste", "<Control>v", TRUE);
  top = gtk_menu_item_new_with_label ("Edit");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (top), menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (mb), top);

  menu = gtk_menu_new ();
  ck = gtk_check_menu_item_new_with_label ("Show grid");
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (ck), TRUE);
  g_signal_connect (ck, "toggled", G_CALLBACK (on_menu), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), ck);
  ra = gtk_radio_menu_item_new_with_label (NULL, "Portrait");
  rb = gtk_radio_menu_item_new_with_label_from_widget (GTK_RADIO_MENU_ITEM (ra), "Landscape");
  g_signal_connect (ra, "toggled", G_CALLBACK (on_menu), NULL);
  g_signal_connect (rb, "toggled", G_CALLBACK (on_menu), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), ra);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), rb);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
  sub = gtk_menu_new ();
  z1 = gtk_radio_menu_item_new_with_label (NULL, "50%");
  z2 = gtk_radio_menu_item_new_with_label_from_widget (GTK_RADIO_MENU_ITEM (z1), "100%");
  z3 = gtk_radio_menu_item_new_with_label_from_widget (GTK_RADIO_MENU_ITEM (z1), "200%");
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (z2), TRUE);
  g_signal_connect (z1, "toggled", G_CALLBACK (on_menu), NULL);
  g_signal_connect (z2, "toggled", G_CALLBACK (on_menu), NULL);
  g_signal_connect (z3, "toggled", G_CALLBACK (on_menu), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (sub), z1);
  gtk_menu_shell_append (GTK_MENU_SHELL (sub), z2);
  gtk_menu_shell_append (GTK_MENU_SHELL (sub), z3);
  zoom = gtk_menu_item_new_with_label ("Zoom");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (zoom), sub);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), zoom);
  top = gtk_menu_item_new_with_label ("View");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (top), menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (mb), top);

  menu = gtk_menu_new ();
  menu_item (menu, "About GTKThemeTest", NULL, TRUE);
  top = gtk_menu_item_new_with_label ("Help");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (top), menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (mb), top);
  return mb;
}

static gboolean
report_size (gpointer data)
{
  int w, h, mw, nw, mh, nh;
  gtk_window_get_size (GTK_WINDOW (tt_window), &w, &h);
  gtk_widget_get_preferred_width (tt_window, &mw, &nw);
  gtk_widget_get_preferred_height (tt_window, &mh, &nh);
  printf ("theme: window %s: %dx%d (min %dx%d, natural %dx%d); notebook height %d; Apply default style %d, has focus %d\n",
          (const char *) data, w, h, mw, mh, nw, nh, gtk_widget_get_allocated_height (tt_notebook),
          gtk_style_context_has_class (gtk_widget_get_style_context (tt_apply), "default"),
          gtk_widget_has_focus (tt_apply));
  fflush (stdout);
  return FALSE;
}

static gboolean
click_apply (gpointer data)
{
  gtk_widget_grab_focus (tt_apply);      /* as a click does */
  gtk_button_clicked (GTK_BUTTON (tt_apply));
  return FALSE;
}

static gboolean
open_file_menu (gpointer data)
{
  GList *items = gtk_container_get_children (GTK_CONTAINER (tt_menubar));
  gtk_menu_shell_select_item (GTK_MENU_SHELL (tt_menubar), items->data);
  g_list_free (items);
  return FALSE;
}

int
themetest_main (int argc, char **argv)
{
  GtkWidget *outer, *row, *w;
  int page = argc > 3 ? atoi (argv[3]) : 0;

  if (argc > 2)
    {
      /* One write-open per file on RISC OS: open the log once (stderr)
         and point stdout's descriptor at it. */
      if (freopen (argv[2], "w", stderr))
        {
          setvbuf (stderr, NULL, _IONBF, 0);
          fflush (stdout);
          dup2 (fileno (stderr), fileno (stdout));
          setvbuf (stdout, NULL, _IOLBF, 0);
        }
    }
  if (argc > 1 && argv[1][0])
    {
      char *conf = g_build_filename (argv[1], "fonts", "fonts.conf", NULL);
      char *share = g_build_filename (argv[1], "share", NULL);
      g_setenv ("FONTCONFIG_FILE", conf, TRUE);
      g_setenv ("XDG_DATA_DIRS", share, TRUE);
      g_free (conf);
      g_free (share);
    }
  g_set_prgname ("GTKThemeTest");
  gtk_init (&argc, &argv);

  tt_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (tt_window), "GTK theme test");
  gtk_window_move (GTK_WINDOW (tt_window), 120, 80);
  g_signal_connect (tt_window, "destroy", G_CALLBACK (on_destroy), NULL);

  outer = vbox (0);
  gtk_container_add (GTK_CONTAINER (tt_window), outer);
  tt_menubar = menubar ();
  gtk_box_pack_start (GTK_BOX (outer), tt_menubar, FALSE, FALSE, 0);

  tt_notebook = gtk_notebook_new ();
  gtk_container_set_border_width (GTK_CONTAINER (tt_notebook), 8);
  gtk_notebook_append_page (GTK_NOTEBOOK (tt_notebook), page_buttons (), gtk_label_new ("Buttons and fields"));
  gtk_notebook_append_page (GTK_NOTEBOOK (tt_notebook), page_options (), gtk_label_new ("Options and sliders"));
  gtk_notebook_append_page (GTK_NOTEBOOK (tt_notebook), page_lists (), gtk_label_new ("Lists and text"));
  gtk_notebook_append_page (GTK_NOTEBOOK (tt_notebook), page_popups (), gtk_label_new ("Dialogues and menus"));
  gtk_box_pack_start (GTK_BOX (outer), tt_notebook, TRUE, TRUE, 0);

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (row), 8);
  w = tt_apply = button ("Apply", "The default action button (R6): Return activates it");
  gtk_widget_set_can_default (w, TRUE);
  gtk_box_pack_end (GTK_BOX (row), w, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (row), button ("Cancel", NULL), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);
  tt_status = gtk_statusbar_new ();
  gtk_box_pack_start (GTK_BOX (outer), tt_status, FALSE, FALSE, 0);

  gtk_widget_show_all (tt_window);
  gtk_widget_grab_default (w);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (tt_notebook), page >= 0 && page < 4 ? page : 0);
  g_signal_connect (tt_notebook, "switch-page", G_CALLBACK (on_page), NULL);
  printf ("theme: window shown\n");
  fflush (stdout);
  gtk_statusbar_push (GTK_STATUSBAR (tt_status), 0, "Try everything: each action is shown here and logged");

  /* THEMETEST_MENU: open the File menu by itself (screenshots) */
  if (g_getenv ("THEMETEST_MENU"))
    g_timeout_add (400, open_file_menu, NULL);
  /* THEMETEST_CLICK: click Apply by itself and report the window size */
  if (g_getenv ("THEMETEST_CLICK"))
    {
      g_timeout_add (500, report_size, (gpointer) "before");
      g_timeout_add (600, click_apply, NULL);
      g_timeout_add (900, report_size, (gpointer) "after");
    }
  /* THEMETEST_QUIT=ms: quit by itself (screenshots, tests) */
  if (g_getenv ("THEMETEST_QUIT"))
    g_timeout_add (atoi (g_getenv ("THEMETEST_QUIT")), (GSourceFunc) gtk_main_quit, NULL);
  gtk_main ();
  return 0;
}

#ifndef THEMETEST_NO_MAIN
int
main (int argc, char **argv)
{
  return themetest_main (argc, argv);
}
#endif
