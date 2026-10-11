/*
 * browser-fake.c - the Filer-style directory browser (gtk/riscos/
 * gtkfilechooser-riscos.c) on the fake RISC OS: GtkFileChooserNative with
 * RISCOS_GTK_CHOOSER=filer.
 *
 *   browser-fake APPDIR OUTDIR
 *
 * Open: the folder's objects with their types; Menu > Display > Small
 * icons / Full info (Wimp menus); into a directory and Parent; the
 * program's filter; Places (filing systems; SDFS:$); a directory dropped
 * from the Filer; a file chosen. Save: the name and its icon, a new name
 * saved in the folder, recent folders kept. A Save box, then Menu >
 * Directory browser. A folder chosen. Several files chosen.
 *
 * BF_SHOTS=dir (as TT_ICONS: Tools/NAME22.png, Sprites22/NAME.png):
 * screenshots with real RISC OS 5 sprites, OUTDIR/shot-*.ppm and the
 * windows' places in OUTDIR/shots.txt.
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>

#define OTHER_TASK 0x5B00

static int n_tests, n_failed;
static const char *outdir;
static char testdir[512], testdir_ro[512], root[512];
static int stage, waits, handle;
static GtkFileChooserNative *native;
static int response = 99;
static GSList *chosen;

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

/* time for GTK's frame clock to redraw (before a screenshot) */
static void
settle (void)
{
  int i;
  for (i = 0; i < 12; i++)
    {
      add (FAKE_SLEEP, 50, 0, 0);
      add (FAKE_NULL, 0, 0, 0);
    }
}

/* ---- screenshots (as edittest-fake's) ---- */

static int shot_tools[262144], shot_pool[1048576];

static int
pixbuf_to_sprite (int *area, int size, GdkPixbuf *pb, const char *name)
{
  int w = gdk_pixbuf_get_width (pb), h = gdk_pixbuf_get_height (pb);
  int mwords = (w + 31) / 32, bytes = 44 + w * h * 4 + mwords * 4 * h, x, y, *spr;
  guchar *px = gdk_pixbuf_get_pixels (pb);
  int stride = gdk_pixbuf_get_rowstride (pb), alpha = gdk_pixbuf_get_has_alpha (pb);

  if (area[3] + bytes > size * 4)
    return 0;
  spr = (int *) ((char *) area + area[3]);
  memset (spr, 0, bytes);
  spr[0] = bytes;
  strncpy ((char *) &spr[1], name, 12);
  spr[4] = w - 1; spr[5] = h - 1; spr[6] = 0; spr[7] = 31;
  spr[8] = 44; spr[9] = 44 + w * h * 4;
  spr[10] = (6 << 27) | (90 << 14) | (90 << 1) | 1;
  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      {
        guchar *q = px + y * stride + x * (alpha ? 4 : 3);
        unsigned char *m = (unsigned char *) spr + spr[9] + y * mwords * 4;
        spr[11 + y * w + x] = q[0] | q[1] << 8 | q[2] << 16;
        if (!alpha || q[3] > 127)
          m[x >> 3] |= 1 << (x & 7);
      }
  area[1]++;
  area[3] += bytes;
  return 1;
}

static void
shot_dir (int *area, const char *dir, const char *sub)
{
  char *path = g_build_filename (dir, sub, NULL);
  GDir *d = g_dir_open (path, 0, NULL);
  const char *f;

  while (d && (f = g_dir_read_name (d)))
    if (g_str_has_suffix (f, ".png"))
      {
        char *name = g_strndup (f, strlen (f) - 4), *file = g_build_filename (path, f, NULL);
        GdkPixbuf *pb = gdk_pixbuf_new_from_file (file, NULL);
        if (pb)
          {
            pixbuf_to_sprite (area, area[0] / 4, pb, name);
            g_object_unref (pb);
          }
        g_free (name);
        g_free (file);
      }
  if (d)
    g_dir_close (d);
  g_free (path);
}

static const int *
shot_pool_lookup (const char *name)
{
  const char *p = (const char *) shot_pool + shot_pool[2];
  int i;
  for (i = 0; i < shot_pool[1]; i++, p += ((const int *) p)[0])
    if (strncmp (p + 4, name, 12) == 0)
      return (const int *) p;
  return NULL;
}

static void
shots_setup (const char *dir)
{
  shot_tools[0] = sizeof shot_tools;
  shot_pool[0] = sizeof shot_pool;
  shot_tools[2] = shot_tools[3] = shot_pool[2] = shot_pool[3] = 16;
  shot_dir (shot_tools, dir, "Tools");
  shot_dir (shot_pool, dir, "Sprites22");
  fake_tool_area_override = shot_tools;
  fake_pool_lookup = shot_pool_lookup;
  fake_pool_area_override = shot_pool;
  printf ("# BF_SHOTS: %d pool sprites\n", shot_pool[1]);
}

static void
shot (const char *name, GtkWidget *top)
{
  char path[600];
  FILE *f;
  int x, y, ox, oy;

  if (!getenv ("BF_SHOTS"))
    return;
  g_snprintf (path, sizeof path, "%s/shot-%s.ppm", outdir, name);
  if (!(f = fopen (path, "wb")))
    return;
  fprintf (f, "P6\n1280 1024\n255\n");
  for (y = 0; y < 1024; y++)
    for (x = 0; x < 1280; x++)
      {
        unsigned int p = fake_screen_pixel (x, y);
        fputc (p & 0xFF, f);
        fputc ((p >> 8) & 0xFF, f);
        fputc ((p >> 16) & 0xFF, f);
      }
  fclose (f);
  g_snprintf (path, sizeof path, "%s/shots.txt", outdir);
  if (top && (f = fopen (path, "a")))
    {
      gdk_window_get_origin (gtk_widget_get_window (top), &ox, &oy);
      fprintf (f, "%s %d %d %d %d %s\n", name, ox, oy, gtk_widget_get_allocated_width (top),
               gtk_widget_get_allocated_height (top), gtk_window_get_title (GTK_WINDOW (top)));
      fclose (f);
    }
}

/* ---- finding things ---- */

static GtkWidget *
window_titled (const char *title)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  GtkWidget *found = NULL;
  for (l = tops; l; l = l->next)
    if (gtk_widget_get_mapped (l->data) && g_strcmp0 (gtk_window_get_title (l->data), title) == 0)
      found = l->data;
  g_list_free (tops);
  return found;
}

static int
handle_of (GtkWidget *top)
{
  int i, ox, oy;
  gdk_window_get_origin (gtk_widget_get_window (top), &ox, &oy);
  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle && (fake_windows[i].x0 >> 1) == ox
        && 1024 - (fake_windows[i].y1 >> 1) == oy)
      return fake_windows[i].handle;
  return 0;
}

static GtkWidget *
find_type (GtkWidget *w, GType t)
{
  GtkWidget *f = NULL;
  GList *l, *k;
  if (G_TYPE_CHECK_INSTANCE_TYPE (w, t))
    return w;
  if (!GTK_IS_CONTAINER (w))
    return NULL;
  k = gtk_container_get_children (GTK_CONTAINER (w));
  for (l = k; l && !f; l = l->next)
    f = find_type (l->data, t);
  g_list_free (k);
  return f;
}

static void
find_all (GtkWidget *w, GType t, GList **out)
{
  GList *l, *k;
  if (G_TYPE_CHECK_INSTANCE_TYPE (w, t))
    *out = g_list_append (*out, w);
  if (!GTK_IS_CONTAINER (w))
    return;
  k = gtk_container_get_children (GTK_CONTAINER (w));
  for (l = k; l; l = l->next)
    find_all (l->data, t, out);
  g_list_free (k);
}

static GtkWidget *
button_labelled (GtkWidget *top, const char *label)
{
  GList *all = NULL, *l;
  GtkWidget *found = NULL;
  find_all (top, GTK_TYPE_BUTTON, &all);
  for (l = all; l; l = l->next)
    if (g_strcmp0 (gtk_button_get_label (l->data), label) == 0)
      found = l->data;
  g_list_free (all);
  return found;
}

/* the path bar (the first entry) and the name (the last) */
static GtkEntry *
entry_n (GtkWidget *top, int last)
{
  GList *all = NULL;
  GtkEntry *e;
  find_all (top, GTK_TYPE_ENTRY, &all);
  e = all ? GTK_ENTRY (last ? g_list_last (all)->data : all->data) : NULL;
  g_list_free (all);
  return e;
}

static GtkTreeModel *
model_of (GtkWidget *top)
{
  GtkWidget *iv = find_type (top, GTK_TYPE_ICON_VIEW);
  GtkWidget *tv = find_type (top, GTK_TYPE_TREE_VIEW);
  if (iv)
    return gtk_icon_view_get_model (GTK_ICON_VIEW (iv));
  return tv ? gtk_tree_view_get_model (GTK_TREE_VIEW (tv)) : NULL;
}

static int
n_objects (GtkWidget *top)
{
  GtkTreeModel *m = model_of (top);
  return m ? gtk_tree_model_iter_n_children (m, NULL) : -1;
}

/* the model path of the object called name (column 2), or NULL */
static GtkTreePath *
path_of (GtkWidget *top, const char *name)
{
  GtkTreeModel *m = model_of (top);
  GtkTreeIter it;
  gboolean ok = m && gtk_tree_model_get_iter_first (m, &it);
  while (ok)
    {
      char *n;
      gtk_tree_model_get (m, &it, 2, &n, -1);
      if (g_strcmp0 (n, name) == 0)
        {
          g_free (n);
          return gtk_tree_model_get_path (m, &it);
        }
      g_free (n);
      ok = gtk_tree_model_iter_next (m, &it);
    }
  return NULL;
}

static char *
column_text (GtkWidget *top, const char *name, int col)
{
  GtkTreePath *p = path_of (top, name);
  GtkTreeIter it;
  char *t = NULL;
  if (p && gtk_tree_model_get_iter (model_of (top), &it, p))
    gtk_tree_model_get (model_of (top), &it, col, &t, -1);
  if (p)
    gtk_tree_path_free (p);
  return t;
}

/* the names in the order shown */
static char *
names_shown (GtkWidget *top)
{
  GtkTreeModel *m = model_of (top);
  GtkTreeIter it;
  GString *s = g_string_new (NULL);
  gboolean ok = m && gtk_tree_model_get_iter_first (m, &it);
  while (ok)
    {
      char *n;
      gtk_tree_model_get (m, &it, 2, &n, -1);
      g_string_append_printf (s, "%s%s", s->len ? " " : "", n);
      g_free (n);
      ok = gtk_tree_model_iter_next (m, &it);
    }
  return g_string_free (s, FALSE);
}

static void
select_name (GtkWidget *top, const char *name, gboolean add_to)
{
  GtkWidget *iv = find_type (top, GTK_TYPE_ICON_VIEW);
  GtkTreePath *p = path_of (top, name);
  if (!p || !iv)
    return;
  if (!add_to)
    gtk_icon_view_unselect_all (GTK_ICON_VIEW (iv));
  gtk_icon_view_select_path (GTK_ICON_VIEW (iv), p);
  gtk_tree_path_free (p);
}

static void
activate_name (GtkWidget *top, const char *name)
{
  GtkWidget *iv = find_type (top, GTK_TYPE_ICON_VIEW);
  GtkTreePath *p = path_of (top, name);
  if (p && iv)
    gtk_icon_view_item_activated (GTK_ICON_VIEW (iv), p);
  if (p)
    gtk_tree_path_free (p);
}

/* Menu over the objects, then Menu_Selection a, b */
static void
menu_choose (int a, int b)
{
  add (6, 300, 220, 2);
  add (FAKE_RELEASE, 0, 0, 0);
  nulls (6);
  add (9, a, b, -1);
  nulls (8);
  settle ();
}

static gchar *
first_path (void)
{
  return chosen ? g_file_get_path (chosen->data) : g_strdup ("");
}

static void
on_response (GtkNativeDialog *d, int r, gpointer data)
{
  response = r;
  g_slist_free_full (chosen, g_object_unref);
  chosen = r == GTK_RESPONSE_ACCEPT ? gtk_file_chooser_get_files (GTK_FILE_CHOOSER (d)) : NULL;
}

static GtkFileChooserNative *
new_native (const char *title, GtkFileChooserAction action, const char *name, gboolean multiple)
{
  GtkFileChooserNative *n = gtk_file_chooser_native_new (title, NULL, action, NULL, NULL);
  GFile *f = g_file_new_for_path (testdir);
  gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (n), f, NULL);
  g_object_unref (f);
  if (name)
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (n), name);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (n), multiple);
  g_signal_connect (n, "response", G_CALLBACK (on_response), NULL);
  response = 99;
  return n;
}

static gboolean
show_open (gpointer d)
{
  GtkFileFilter *all = gtk_file_filter_new (), *txt = gtk_file_filter_new ();
  native = new_native ("Open File", GTK_FILE_CHOOSER_ACTION_OPEN, NULL, FALSE);
  gtk_file_filter_set_name (all, "All files");
  gtk_file_filter_add_pattern (all, "*");
  gtk_file_filter_set_name (txt, "Text");
  gtk_file_filter_add_mime_type (txt, "text/plain");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), all);
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (native), txt);
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
  return G_SOURCE_REMOVE;
}

static gboolean
show_save (gpointer d)
{
  native = new_native ("Save File", GTK_FILE_CHOOSER_ACTION_SAVE, "Letter.txt", FALSE);
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
  return G_SOURCE_REMOVE;
}

static gboolean
show_folder (gpointer d)
{
  native = new_native ("Choose Folder", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL, FALSE);
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
  return G_SOURCE_REMOVE;
}

static gboolean
show_multi (gpointer d)
{
  native = new_native ("Open Files", GTK_FILE_CHOOSER_ACTION_OPEN, NULL, TRUE);
  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
  return G_SOURCE_REMOVE;
}

static void
drop_dir (int h, const char *ro, int type)
{
  int q[64];
  memset (q, 0, sizeof q);
  q[1] = OTHER_TASK; q[2] = fake_new_ref (); q[4] = 3;
  q[5] = h; q[6] = -1; q[7] = 400; q[8] = 1400; q[9] = 0; q[10] = type;
  g_strlcpy ((char *) &q[11], ro, 200);
  q[0] = (44 + (int) strlen (ro) + 1 + 3) & ~3;
  fake_queue_message (17, q);
}

/* ---- the steps ---- */

static void
hook (int reason)
{
  GtkWidget *w, *b;
  char *t, *u;

  if (fake_wimp_script_pos () < fake_wimp_script_len - 1)
    return;
  switch (stage)
    {
    case 0:
      g_idle_add (show_open, NULL);
      stage = 1;
      nulls (6);
      return;
    case 1:                     /* the browser, large icons */
      w = window_titled ("Open File");
      if (!w || !(handle = handle_of (w)) || n_objects (w) <= 0)
        break;
      fake_target_window = handle;
      t = names_shown (w);
      printf ("# objects: %s\n", t);
      check (find_type (w, GTK_TYPE_ICON_VIEW) && n_objects (w) == 10,
             "Open: a Filer-style window of the folder's 10 objects (large icons)");
      check (g_strcmp0 (t, "!Edit data/csv Documents Letters manual/pdf notes/txt photo/png Report/txt Run Sprites") == 0,
             "sorted by name, as the Filer, with RISC OS names");
      g_free (t);
      u = g_strconcat (testdir_ro, NULL);
      check (g_strcmp0 (gtk_entry_get_text (entry_n (w, 0)), u) == 0, "the folder's full name in the bar");
      g_free (u);
      select_name (w, "Report/txt", FALSE);
      nulls (6);
      settle ();
      stage = 100;
      return;
    case 100:
      w = window_titled ("Open File");
      shot ("open-large", w);
      menu_choose (0, 1);
      stage = 2;
      return;
    case 2:
      printf ("# menus opened %d\n", fake_menus_opened);
      w = window_titled ("Open File");
      b = w ? find_type (w, GTK_TYPE_ICON_VIEW) : NULL;
      check (b && gtk_icon_view_get_item_orientation (GTK_ICON_VIEW (b)) == GTK_ORIENTATION_HORIZONTAL,
             "Menu > Display > Small icons (a Wimp menu)");
      shot ("open-small", w);
      menu_choose (0, 2);
      stage = 3;
      return;
    case 3:
      w = window_titled ("Open File");
      t = w ? column_text (w, "Report/txt", 8) : NULL;
      u = w ? column_text (w, "Sprites", 8) : NULL;
      check (w && find_type (w, GTK_TYPE_TREE_VIEW) && !find_type (w, GTK_TYPE_ICON_VIEW)
             && g_strcmp0 (t, "Text") == 0 && g_strcmp0 (u, "Sprite") == 0,
             "Menu > Display > Full info: types by name (OS_FSControl 18)");
      g_free (t); g_free (u);
      t = w ? column_text (w, "manual/pdf", 7) : NULL;
      check (g_strcmp0 (t, "12K") == 0, "and sizes as the Filer gives them");
      g_free (t);
      shot ("open-full", w);
      menu_choose (0, 0);
      stage = 4;
      return;
    case 4:
      w = window_titled ("Open File");
      activate_name (w, "Documents");
      stage = 5;
      nulls (4);
      return;
    case 5:
      w = window_titled ("Open File");
      check (g_str_has_suffix (gtk_entry_get_text (entry_n (w, 0)), ".Documents") && n_objects (w) == 2,
             "double-click a directory: into it");
      gtk_button_clicked (GTK_BUTTON (button_labelled (w, "Parent")));
      check (g_strcmp0 (gtk_entry_get_text (entry_n (w, 0)), testdir_ro) == 0 && n_objects (w) == 10,
             "Parent: back");
      gtk_combo_box_set_active (GTK_COMBO_BOX (find_type (w, GTK_TYPE_COMBO_BOX)), 1);
      t = names_shown (w);
      printf ("# filtered: %s\n", t);
      check (g_strcmp0 (t, "!Edit data/csv Documents Letters notes/txt Report/txt") == 0,
             "the program's filter (Text): directories and text files (CSV is text)");
      g_free (t);
      settle ();
      stage = 50;
      return;
    case 50:
      w = window_titled ("Open File");
      shot ("open-filter", w);
      gtk_combo_box_set_active (GTK_COMBO_BOX (find_type (w, GTK_TYPE_COMBO_BOX)), 0);
      {
        GtkWidget *side = find_type (w, GTK_TYPE_LIST_BOX);
        GList *rows = side ? gtk_container_get_children (GTK_CONTAINER (side)) : NULL, *l;
        GString *names = g_string_new (NULL);
        GtkListBoxRow *sdfs = NULL;
        for (l = rows; l; l = l->next)
          {
            const char *dir = g_object_get_data (l->data, "riscos-gtk-dir");
            g_string_append_printf (names, "%s ", dir ? dir : "-");
            if (g_strcmp0 (dir, "SDFS:$") == 0)
              sdfs = l->data;
          }
        printf ("# sidebar: %s\n", names->str);
        check (strstr (names->str, "RAM:$") && strstr (names->str, "Resources:$") && sdfs
               && strstr (names->str, "HostFS:$") && !strstr (names->str, "null:$")
               && strstr (names->str, testdir_ro),
               "the sidebar: discs (OS_FSControl 33, those with directories) and the program's folder");
        check (gtk_list_box_get_selected_row (GTK_LIST_BOX (side)) != NULL,
               "the folder shown is selected in the sidebar");
        if (sdfs)
          gtk_list_box_select_row (GTK_LIST_BOX (side), sdfs);
        g_string_free (names, TRUE);
        g_list_free (rows);
      }
      stage = 7;
      nulls (6);
      return;
    case 7:
      w = window_titled ("Open File");
      check (g_strcmp0 (gtk_entry_get_text (entry_n (w, 0)), "SDFS:$") == 0 && n_objects (w) == 3,
             "a click on SDFS in the sidebar: its root");
      {
        char *letters = g_strconcat (testdir_ro, ".Letters", NULL);
        drop_dir (handle, letters, 0x1000);
        g_free (letters);
      }
      nulls (8);
      stage = 8;
      return;
    case 8:
      w = window_titled ("Open File");
      check (g_str_has_suffix (gtk_entry_get_text (entry_n (w, 0)), ".Letters"),
             "a directory dropped from the Filer is shown");
      gtk_entry_set_text (entry_n (w, 0), testdir_ro);
      g_signal_emit_by_name (entry_n (w, 0), "activate");
      select_name (w, "Report/txt", FALSE);
      check (g_strcmp0 (gtk_entry_get_text (entry_n (w, 1)), "Report/txt") == 0,
             "a folder's name typed in the bar; a file selected: its name below");
      gtk_button_clicked (GTK_BUTTON (button_labelled (w, "Open")));
      stage = 9;
      nulls (4);
      return;
    case 9:
      t = first_path ();
      u = g_build_filename (testdir, "Report.txt", NULL);
      check (response == GTK_RESPONSE_ACCEPT && g_strcmp0 (t, u) == 0, "Open: that file is chosen");
      g_free (t); g_free (u);
      g_clear_object (&native);
      g_idle_add (show_save, NULL);
      stage = 10;
      nulls (6);
      return;
    case 10:                    /* Save */
      w = window_titled ("Save File");
      if (!w || !(handle = handle_of (w)) || n_objects (w) <= 0)
        break;
      fake_target_window = handle;
      check (g_strcmp0 (gtk_entry_get_text (entry_n (w, 1)), "Letter/txt") == 0
             && find_type (w, GTK_TYPE_EVENT_BOX) != NULL,
             "Save: the browser with the name and its file icon (to drag)");
      settle ();
      stage = 110;
      return;
    case 110:
      w = window_titled ("Save File");
      shot ("save-large", w);
      gtk_entry_set_text (entry_n (w, 1), "Minutes/txt");
      gtk_button_clicked (GTK_BUTTON (button_labelled (w, "Save")));
      stage = 11;
      nulls (4);
      return;
    case 11:
      t = first_path ();
      u = g_build_filename (testdir, "Minutes.txt", NULL);
      check (response == GTK_RESPONSE_ACCEPT && g_strcmp0 (t, u) == 0, "Save: saved in the folder shown");
      g_free (t); g_free (u);
      {
        char *ini = g_build_filename (g_get_user_config_dir (), "riscos-gtk", "browser.ini", NULL), *text = NULL;
        g_file_get_contents (ini, &text, NULL, NULL);
        check (text && strstr (text, testdir_ro) && strstr (text, "View=0"),
               "the folder kept as a recent one, with the view");
        g_free (text); g_free (ini);
      }
      g_clear_object (&native);
      g_unsetenv ("RISCOS_GTK_CHOOSER");
      g_idle_add (show_save, NULL);
      stage = 12;
      nulls (6);
      return;
    case 12:                    /* a Save box, then Menu > Directory browser */
      w = window_titled ("Save File");
      if (!w || !(handle = handle_of (w)))
        break;
      fake_target_window = handle;
      check (!gtk_window_get_resizable (GTK_WINDOW (w)) && !find_type (w, GTK_TYPE_ICON_VIEW)
             && button_labelled (w, "Browse...") != NULL,
             "without RISCOS_GTK_CHOOSER: the Save box, with Browse...");
      settle ();
      stage = 120;
      return;
    case 120:
      w = window_titled ("Save File");
      shot ("savebox", w);
      add (6, 20, 20, 2);
      add (FAKE_RELEASE, 0, 0, 0);
      nulls (6);
      add (9, 0, -1, -1);
      nulls (8);
      stage = 13;
      return;
    case 13:
      w = window_titled ("Save File");
      check (w && find_type (w, GTK_TYPE_ICON_VIEW) && n_objects (w) == 10
             && g_strcmp0 (gtk_entry_get_text (entry_n (w, 1)), "Letter/txt") == 0,
             "Menu over the box > Directory browser...: the browser, same name");
      gtk_button_clicked (GTK_BUTTON (button_labelled (w, "Cancel")));
      stage = 14;
      nulls (4);
      return;
    case 14:
      check (response == GTK_RESPONSE_CANCEL && !window_titled ("Save File"), "Cancel");
      g_clear_object (&native);
      g_idle_add (show_open, NULL);
      stage = 140;
      nulls (6);
      return;
    case 140:                   /* an Open box, then its Browse... button */
      w = window_titled ("Open File");
      if (!w || !(handle = handle_of (w)))
        break;
      b = button_labelled (w, "Browse...");
      check (b && !find_type (w, GTK_TYPE_ICON_VIEW), "the Open box has Browse...");
      if (b)
        gtk_button_clicked (GTK_BUTTON (b));
      stage = 141;
      nulls (6);
      return;
    case 141:
      w = window_titled ("Open File");
      check (w && find_type (w, GTK_TYPE_ICON_VIEW) && n_objects (w) == 10,
             "Browse...: the directory browser, in the program's folder");
      gtk_button_clicked (GTK_BUTTON (button_labelled (w, "Cancel")));
      g_clear_object (&native);
      g_setenv ("RISCOS_GTK_CHOOSER", "filer", TRUE);
      g_idle_add (show_folder, NULL);
      stage = 15;
      nulls (6);
      return;
    case 15:                    /* choose a folder */
      w = window_titled ("Choose Folder");
      if (!w || !(handle = handle_of (w)) || n_objects (w) <= 0)
        break;
      t = names_shown (w);
      check (g_strcmp0 (t, "!Edit Documents Letters") == 0, "Choose a folder: directories only");
      g_free (t);
      select_name (w, "Letters", FALSE);
      gtk_button_clicked (GTK_BUTTON (button_labelled (w, "Choose")));
      stage = 16;
      nulls (4);
      return;
    case 16:
      t = first_path ();
      u = g_build_filename (testdir, "Letters", NULL);
      check (response == GTK_RESPONSE_ACCEPT && g_strcmp0 (t, u) == 0, "the folder selected is chosen");
      g_free (t); g_free (u);
      g_clear_object (&native);
      g_idle_add (show_multi, NULL);
      stage = 17;
      nulls (6);
      return;
    case 17:                    /* several files */
      w = window_titled ("Open Files");
      if (!w || !(handle = handle_of (w)) || n_objects (w) <= 0)
        break;
      select_name (w, "notes/txt", FALSE);
      select_name (w, "photo/png", TRUE);
      gtk_button_clicked (GTK_BUTTON (button_labelled (w, "Open")));
      stage = 18;
      nulls (4);
      return;
    case 18:
      check (response == GTK_RESPONSE_ACCEPT && g_slist_length (chosen) == 2,
             "a program that takes several files: two selected, both chosen");
      g_clear_object (&native);
      stage = 99;
      gtk_main_quit ();
      return;
    default:
      return;
    }
  /* waiting for a window */
  if (++waits > 200)
    {
      printf ("# stage %d: gave up waiting\n", stage);
      stage = 99;
      gtk_main_quit ();
    }
  nulls (4);
}

static void
make_file (const char *dir, const char *name, int size, time_t when)
{
  char *p = g_build_filename (dir, name, NULL);
  char *data = g_malloc0 (size + 1);
  struct utimbuf ut = { when, when };
  memset (data, 'x', size);
  g_file_set_contents (p, data, size, NULL);
  utime (p, &ut);
  g_free (data);
  g_free (p);
}

int
main (int argc, char **argv)
{
  const char *appdir = argv[1];
  char *conf, *share, *p, *ro, *cfg;
  time_t base = 1791640000;   /* October 2026 */

  outdir = argv[2];
  g_snprintf (testdir, sizeof testdir, "%s/Work", outdir);
  g_snprintf (root, sizeof root, "%s/root", outdir);
  /* pictures with RISC OS names: BF_SHOT_ROOT (e.g. /SDFS::RISCOSpi/$)
     is a link to OUTDIR/root, and the folder is Work in it (SDFS's root
     then has 4 objects, not 3) */
  if (getenv ("BF_SHOTS") && getenv ("BF_SHOT_ROOT"))
    g_snprintf (testdir, sizeof testdir, "%s/Work", getenv ("BF_SHOT_ROOT"));
  p = g_strdup_printf ("rm -rf '%s' '%s' '%s/shots.txt' '%s/config'", testdir, root, outdir, outdir);
  if (system (p) != 0)
    return 2;
  g_free (p);
  g_mkdir_with_parents (root, 0755);
  g_mkdir_with_parents (testdir, 0755);
  p = g_build_filename (testdir, "Documents", NULL); g_mkdir_with_parents (p, 0755);
  make_file (p, "Plan.txt", 900, base); make_file (p, "Budget.csv", 2100, base); g_free (p);
  p = g_build_filename (testdir, "Letters", NULL); g_mkdir_with_parents (p, 0755);
  make_file (p, "Bank.txt", 300, base); g_free (p);
  p = g_build_filename (testdir, "!Edit", NULL); g_mkdir_with_parents (p, 0755); g_free (p);
  make_file (testdir, "Report.txt", 3120, base - 3600);
  make_file (testdir, "notes.txt", 210, base - 86400);
  make_file (testdir, "photo.png", 184320, base - 2 * 86400);
  make_file (testdir, "data.csv", 5400, base - 5 * 3600);
  make_file (testdir, "manual.pdf", 12000, base - 30 * 86400);
  make_file (testdir, "Sprites,ff9", 8200, base - 7 * 86400);
  make_file (testdir, "Run,feb", 120, base - 9 * 86400);
  g_mkdir_with_parents (root, 0755);
  p = g_build_filename (root, "Apps", NULL); g_mkdir_with_parents (p, 0755); g_free (p);
  p = g_build_filename (root, "Documents", NULL); g_mkdir_with_parents (p, 0755); g_free (p);
  make_file (root, "ReadMe.txt", 400, base);
  ro = gdk_riscos_path_from_unix (testdir);
  g_strlcpy (testdir_ro, ro, sizeof testdir_ro);
  g_free (ro);

  conf = g_build_filename (appdir, "fonts", "fonts.conf", NULL);
  share = g_build_filename (appdir, "share", NULL);
  cfg = g_build_filename (outdir, "config", NULL);
  g_setenv ("FONTCONFIG_FILE", conf, TRUE);
  g_setenv ("XDG_DATA_DIRS", share, TRUE);
  g_setenv ("XDG_CONFIG_HOME", cfg, TRUE);
  g_setenv ("FAKE_FS_ROOT", root, TRUE);
  g_setenv ("RISCOS_GTK_CHOOSER", "filer", TRUE);
  g_setenv ("TZ", "UTC", TRUE);
  fake_set_screen (1280, 1024, 0, 5);
  fake_sprite_plot_converts = 0;
  fake_wimp_nulls = 10;
  fake_wimp_hook = hook;
  fake_wimp_desktop = 1;
  if (getenv ("BF_SHOTS"))
    shots_setup (getenv ("BF_SHOTS"));
  gtk_init (&argc, &argv);
  /* a main window, so the Wimp task runs before any chooser */
  {
    GtkWidget *win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (win), "Browser test");
    gtk_window_set_default_size (GTK_WINDOW (win), 200, 100);
    gtk_window_move (GTK_WINDOW (win), 1000, 800);
    gtk_widget_show (win);
  }
  gtk_main ();
  check (stage == 99, "all steps ran");
  printf ("1..%d\n", n_tests);
  return n_failed;
}
