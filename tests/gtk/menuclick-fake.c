/*
 * menuclick-fake.c - Menu (GDK button 3) gives the program's menu anywhere
 * in a window with a menu (gtk/riscos/gtkmenu-riscos.c), on the fake RISC
 * OS:
 *   - over a drawing area that takes every button press itself (a paint
 *     program's canvas) but opens no menu: the area has the click, then
 *     the window's menu opens all the same;
 *   - over a button in a toolbar: the window's menu;
 *   - over a list with its own context menu: the list gets the click,
 *     and its menu is shown inside the window's menu;
 *   - over a tool window with no menu, transient for the main window:
 *     the main window's menu;
 *   - over a dialogue box: nothing of the main window's.
 * Select still reaches the drawing area.
 * A pop-up window (GTK_WINDOW_POPUP) with a title gets a title bar and a
 * close icon (gdk/riscos/gdkwindow-riscos.c); its close icon sends Escape,
 * then closes it if it is still open; a pop-up with no title stays plain.
 * A window the program moves to the top of the screen (gtk_window_move
 * (w, 0, 0), as mtPaint's main window), or maximises, keeps its title bar
 * on the screen.
 * Left Alt + a letter (GDK gives the key, not the character the keyboard
 * made): the menu bar title with that mnemonic opens its menu; a letter
 * no title has types the character.
 *   menuclick-fake APPDIR
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static int n_tests, n_failed, stage, area_presses[4], area_releases[4], list_presses;
static GtkWidget *mainw, *toolw, *dialog, *area, *button, *list, *popup, *plain;
static int popup_deleted, popup_escapes;
static GtkWidget *topw, *entry;
static int h_main, h_tool, h_dialog, h_popup, h_plain;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
  fflush (stdout);
}

static void
add (int reason, int a, int b, int c)
{
  int *e = fake_wimp_script[fake_wimp_script_len++];
  e[0] = reason; e[1] = a; e[2] = b; e[3] = c;
}

static void
nulls (int n)
{
  while (n--)
    add (FAKE_NULL, 0, 0, 0);
}

static int
handle_of (GtkWidget *top)
{
  int i, ox, oy;
  gdk_window_get_origin (gtk_widget_get_window (top), &ox, &oy);
  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle && (fake_windows[i].x0 >> 1) == ox && 1024 - (fake_windows[i].y1 >> 1) == oy)
      return fake_windows[i].handle;
  return 0;
}

/* A click with buttons (4 Select, 2 Menu) in widget's middle */
static void
click (GtkWidget *top, int handle, GtkWidget *w, int buttons)
{
  int x, y;
  gtk_widget_translate_coordinates (w, top, gtk_widget_get_allocated_width (w) / 2,
                                    gtk_widget_get_allocated_height (w) / 2, &x, &y);
  fake_target_window = handle;
  add (6, x, y, buttons);
  add (FAKE_RELEASE, 0, 0, 0);
  nulls (6);
  add (17, 0x400C9, 0, 0);     /* Message_MenusDeleted: the menu closed */
  nulls (4);
}

static gboolean
on_area (GtkWidget *w, GdkEventButton *ev, gpointer d)
{
  if (ev->button <= 3)
    {
      if (ev->type == GDK_BUTTON_PRESS)
        area_presses[ev->button]++;
      else if (ev->type == GDK_BUTTON_RELEASE)
        area_releases[ev->button]++;
    }
  return TRUE;                  /* the canvas uses every button */
}

static gboolean
on_list (GtkWidget *w, GdkEventButton *ev, gpointer d)
{
  if (ev->type == GDK_BUTTON_PRESS && ev->button == 3)
    {
      GtkWidget *menu = gtk_menu_new (), *item = gtk_menu_item_new_with_label ("Rename");
      list_presses++;
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show_all (menu);
      gtk_menu_attach_to_widget (GTK_MENU (menu), w, NULL);
      gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) ev);
      return TRUE;
    }
  return FALSE;
}

static gboolean
on_popup_key (GtkWidget *w, GdkEventKey *ev, gpointer d)
{
  if (ev->keyval == GDK_KEY_Escape)
    popup_escapes++;
  return FALSE;                 /* not handled: the window is to close all the same */
}

static gboolean
on_popup_delete (GtkWidget *w, GdkEvent *ev, gpointer d)
{
  popup_deleted++;
  gtk_widget_hide (w);
  return TRUE;
}

/* the top of the window's title bar, in OS units, or -1 */
static int
title_top (GtkWidget *top)
{
  int i, h = handle_of (top);
  for (i = 0; h && i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle == h)
      return fake_windows[i].y1 + 40;     /* the fake's title bar is 40 units */
  return -1;
}

static int menus_before, last_menu_items;

static int
menu_has (const char *text)
{
  const int *m = fake_menu;
  int i;
  if (!m)
    return 0;
  for (i = 0; i < 64; i++)
    {
      const int *it = m + 7 + i * 6;
      const char *t = (it[2] & 0x100) ? (const char *) (long) it[3] : (const char *) &it[3];
      if (t && strncmp (t, text, strlen (text)) == 0)
        return 1;
      if (it[0] & 0x80)
        break;
    }
  return 0;
}

static int menu_file, menu_sel;

static void
hook (int reason)
{
  if (fake_menu)
    {
      menu_file |= menu_has ("File");
      menu_sel |= menu_has ("Selection");
    }
  if (fake_wimp_script_pos () < fake_wimp_script_len - 1)
    return;
  switch (stage)
    {
    case 0:
      if (!gtk_widget_get_mapped (mainw) || !gtk_widget_get_mapped (toolw) ||
          !(h_main = handle_of (mainw)) || !(h_tool = handle_of (toolw)))
        {
          nulls (4);
          return;
        }
      menus_before = fake_menus_opened;
      menu_file = 0;
      click (mainw, h_main, area, 2);
      stage = 1;
      return;
    case 1:
      check (fake_menus_opened == menus_before + 1 && menu_file,
             "Menu over a drawing area that takes clicks: the window's menu");
      check (area_presses[3] == 1, "the drawing area had the click first");
      click (mainw, h_main, area, 4);
      stage = 2;
      return;
    case 2:
      check (area_presses[1] == 1 && area_releases[1] == 1, "Select still reaches the drawing area");
      menus_before = fake_menus_opened;
      menu_file = 0;
      click (mainw, h_main, button, 2);
      stage = 3;
      return;
    case 3:
      check (fake_menus_opened == menus_before + 1 && menu_file, "Menu over a toolbar button: the window's menu");
      menus_before = fake_menus_opened;
      menu_file = menu_sel = 0;
      click (mainw, h_main, list, 2);
      stage = 4;
      return;
    case 4:
      check (list_presses == 1 && fake_menus_opened == menus_before + 1 && menu_file && menu_sel,
             "Menu over a list with its own menu: the list's menu inside the window's (Selection >)");
      menus_before = fake_menus_opened;
      menu_file = 0;
      click (toolw, h_tool, gtk_bin_get_child (GTK_BIN (toolw)), 2);
      stage = 5;
      return;
    case 5:
      check (fake_menus_opened == menus_before + 1 && menu_file,
             "Menu over a tool window with no menu: the main window's menu");
      gtk_widget_show_all (dialog);
      stage = 6;
      nulls (8);
      return;
    case 6:
      if (!gtk_widget_get_mapped (dialog) || !(h_dialog = handle_of (dialog)))
        {
          nulls (4);
          return;
        }
      menus_before = fake_menus_opened;
      menu_file = 0;
      click (dialog, h_dialog, gtk_dialog_get_content_area (GTK_DIALOG (dialog)), 2);
      stage = 7;
      return;
    case 7:
      check (!menu_file, "Menu over a dialogue box: not the main window's menu");
      gtk_widget_hide (dialog);
      gtk_widget_show_all (popup);
      gtk_widget_show_all (plain);
      stage = 8;
      nulls (8);
      return;
    case 8:
      if (!gtk_widget_get_mapped (popup) || !gtk_widget_get_mapped (plain) ||
          !(h_popup = handle_of (popup)) || !(h_plain = handle_of (plain)))
        {
          nulls (4);
          return;
        }
      check ((fake_window_flags[h_popup & 255] & (1 << 26)) && (fake_window_flags[h_popup & 255] & (1 << 25)),
             "a pop-up with a title: a title bar and a close icon");
      check (!(fake_window_flags[h_plain & 255] & ((1 << 26) | (1 << 25))),
             "a pop-up with no title: no title bar or close icon");
      fake_target_window = h_popup;
      add (3, 0, 0, 0);         /* Close_Window_Request */
      add (FAKE_SLEEP, 400, 0, 0);
      nulls (8);
      stage = 9;
      return;
    case 9:
      check (popup_escapes == 1, "its close icon sends it Escape");
      check (popup_deleted == 1 && !gtk_widget_get_visible (popup),
             "and, not closed by Escape, it is closed (delete-event)");
      gtk_window_move (GTK_WINDOW (topw), 0, 0);
      gtk_widget_show_all (topw);
      nulls (8);
      stage = 10;
      return;
    case 10:
      if (!gtk_widget_get_mapped (topw) || title_top (topw) < 0)
        {
          nulls (4);
          return;
        }
      {
        int t = title_top (topw), x, y;
        gtk_window_get_position (GTK_WINDOW (topw), &x, &y);
        check (t <= 2048 && t >= 2040, "a window moved to the top of the screen: its title bar is on it");
        check (y == 20, "and GTK is told where it went");
      }
      gtk_window_maximize (GTK_WINDOW (topw));
      nulls (8);
      stage = 11;
      return;
    case 11:
      {
        int t = title_top (topw), w, h;
        gtk_window_get_size (GTK_WINDOW (topw), &w, &h);
        check (t <= 2048 && w == 1280 && h == 1024 - 20,
               "maximised: the whole screen below its title bar");
      }
      /* Left Alt + E, which this keyboard makes an e-acute of: the key
         is E, so the Edit menu (mnemonic _Edit) opens */
      fake_target_window = h_main;
      gtk_widget_grab_focus (entry);
      fake_keys_down[2] = fake_keys_down[5] = 1;      /* Alt, Left Alt */
      fake_keys_down[34] = 1;                         /* E */
      add (8, 0xE9, 0, 0);
      nulls (4);
      stage = 12;
      return;
    case 12:
      check (fake_menu && menu_has ("Undo") && !menu_has ("Quit"),
             "Left Alt + E: the Edit menu (its mnemonic), whatever character the keyboard made");
      fake_keys_down[34] = 0;
      add (17, 0x400C9, 0, 0);                        /* Message_MenusDeleted */
      /* Left Alt + Z, no menu has Z: the character the keyboard made (ae) is typed */
      fake_keys_down[97] = 1;                         /* Z */
      add (8, 0xE6, 0, 0);
      nulls (4);
      stage = 13;
      return;
    case 13:
      fake_keys_down[97] = fake_keys_down[2] = fake_keys_down[5] = 0;
      check (strcmp (gtk_entry_get_text (GTK_ENTRY (entry)), "\xc3\xa6") == 0,
             "Left Alt + a letter no menu has: the character the keyboard made is typed");
      printf ("# entry \"%s\"\n", gtk_entry_get_text (GTK_ENTRY (entry)));
      printf ("1..%d\n", n_tests);
      fflush (stdout);
      _exit (n_failed);
    }
}

int
main (int argc, char **argv)
{
  char *conf = g_build_filename (argv[1], "fonts", "fonts.conf", NULL);
  char *share = g_build_filename (argv[1], "share", NULL);
  GtkWidget *vb, *bar, *file, *fmenu, *tb, *hb, *store_view;
  GtkListStore *store;
  GtkTreeIter it;

  g_setenv ("FONTCONFIG_FILE", conf, TRUE);
  g_setenv ("XDG_DATA_DIRS", share, TRUE);
  fake_set_screen (1280, 1024, 0, 5);
  fake_wimp_nulls = 20;
  fake_wimp_hook = hook;
  gtk_init (&argc, &argv);

  mainw = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (mainw), "Paint");
  gtk_window_set_default_size (GTK_WINDOW (mainw), 400, 300);
  vb = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (mainw), vb);
  bar = gtk_menu_bar_new ();
  file = gtk_menu_item_new_with_mnemonic ("_File");
  fmenu = gtk_menu_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (fmenu), gtk_menu_item_new_with_label ("Quit"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (file), fmenu);
  gtk_menu_shell_append (GTK_MENU_SHELL (bar), file);
  {
    GtkWidget *edit = gtk_menu_item_new_with_mnemonic ("_Edit"), *emenu = gtk_menu_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (emenu), gtk_menu_item_new_with_label ("Undo"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (edit), emenu);
    gtk_menu_shell_append (GTK_MENU_SHELL (bar), edit);
  }
  gtk_box_pack_start (GTK_BOX (vb), bar, FALSE, FALSE, 0);
  tb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  button = gtk_button_new_with_label ("Brush");
  gtk_box_pack_start (GTK_BOX (tb), button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vb), tb, FALSE, FALSE, 0);
  entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (vb), entry, FALSE, FALSE, 0);
  hb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (vb), hb, TRUE, TRUE, 0);
  area = gtk_drawing_area_new ();
  gtk_widget_set_size_request (area, 200, 200);
  gtk_widget_add_events (area, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  g_signal_connect (area, "button-press-event", G_CALLBACK (on_area), NULL);
  g_signal_connect (area, "button-release-event", G_CALLBACK (on_area), NULL);
  gtk_box_pack_start (GTK_BOX (hb), area, TRUE, TRUE, 0);
  store = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_list_store_insert_with_values (store, &it, -1, 0, "Layer 1", -1);
  list = store_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (store_view), -1, "Layers",
                                               gtk_cell_renderer_text_new (), "text", 0, NULL);
  gtk_widget_set_size_request (list, 120, 200);
  g_signal_connect (list, "button-press-event", G_CALLBACK (on_list), NULL);
  gtk_box_pack_start (GTK_BOX (hb), list, FALSE, FALSE, 0);
  gtk_widget_show_all (mainw);

  toolw = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (toolw), "Tools");
  gtk_window_set_transient_for (GTK_WINDOW (toolw), GTK_WINDOW (mainw));
  gtk_container_add (GTK_CONTAINER (toolw), gtk_label_new ("Tool settings"));
  gtk_window_set_default_size (GTK_WINDOW (toolw), 160, 100);
  gtk_widget_show_all (toolw);
  gtk_window_move (GTK_WINDOW (toolw), 600, 100);

  dialog = gtk_dialog_new_with_buttons ("Resize", GTK_WINDOW (mainw), 0, "OK", GTK_RESPONSE_OK, NULL);
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                     gtk_label_new ("Width and height"));
  gtk_window_set_default_size (GTK_WINDOW (dialog), 200, 120);
  gtk_window_move (GTK_WINDOW (dialog), 800, 500);

  popup = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_title (GTK_WINDOW (popup), "Palette");
  gtk_window_set_default_size (GTK_WINDOW (popup), 150, 100);
  gtk_container_add (GTK_CONTAINER (popup), gtk_label_new ("Colours"));
  g_signal_connect (popup, "key-press-event", G_CALLBACK (on_popup_key), NULL);
  g_signal_connect (popup, "delete-event", G_CALLBACK (on_popup_delete), NULL);
  gtk_window_move (GTK_WINDOW (popup), 100, 600);
  plain = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_default_size (GTK_WINDOW (plain), 120, 80);
  gtk_container_add (GTK_CONTAINER (plain), gtk_label_new ("Plain"));
  gtk_window_move (GTK_WINDOW (plain), 400, 600);

  topw = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (topw), "Top");
  gtk_window_set_default_size (GTK_WINDOW (topw), 300, 200);
  gtk_container_add (GTK_CONTAINER (topw), gtk_label_new ("At the top"));

  gtk_main ();
  return 1;
}
