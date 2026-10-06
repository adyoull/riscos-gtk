/*
 * l3afpad-fake.c - L3afpad (the first real GTK program ported) on the fake
 * RISC OS Wimp: start it, type, open its menu with Menu over the text,
 * save through the Save box onto another task's window (a Filer, say),
 * drop a RISC OS (Latin-1) text file into the text, and quit with the
 * window's close icon. L3afpad's sources are built with
 * -Dmain=l3afpad_main and -D__riscos (its RISC OS changes).
 *
 *   l3afpad-fake APPDIR OUTDIR
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int l3afpad_main (int argc, char **argv);

#define OTHER_TASK 0x5B00
#define FOREIGN    0x5551

static int n_tests, n_failed, stage, step, waits, main_handle, opened_seen, menu_seen;
static const char *outdir;
static GtkWidget *main_win, *view;
static char typed[256], menu_titles[8][32], file_menu[16][48], title_after_save[256], text_after_drop[256];
static int n_titles, n_file_items, save_as_index = -1, save_ds_seen, save_ds_type, save_dl_seen;
static char save_ds_leaf[64], save_target[512], save_target_ro[512], drop_file[512], drop_file_ro[512];
static int save_box_seen, drop_ack, quit_asked;

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

static void
queue (int reason, int *block, int action, int your_ref, int bytes)
{
  block[0] = (bytes + 3) & ~3;
  block[1] = OTHER_TASK;
  block[2] = fake_new_ref ();
  block[3] = your_ref;
  block[4] = action;
  fake_queue_message (reason, block);
}

static GtkWidget *
find_type (GtkWidget *w, GType type)
{
  GList *kids, *l;
  GtkWidget *found = NULL;

  if (G_TYPE_CHECK_INSTANCE_TYPE (w, type))
    return w;
  if (!GTK_IS_CONTAINER (w))
    return NULL;
  kids = gtk_container_get_children (GTK_CONTAINER (w));
  for (l = kids; l && !found; l = l->next)
    found = find_type (l->data, type);
  g_list_free (kids);
  return found;
}

static char *
buffer_text (void)
{
  GtkTextBuffer *b = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  GtkTextIter s, e;
  gtk_text_buffer_get_bounds (b, &s, &e);
  return gtk_text_buffer_get_text (b, &s, &e, FALSE);
}

/* A toplevel other than the main window with a file icon (a Save box) */
static GtkWidget *
save_box (void)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  GtkWidget *found = NULL;

  for (l = tops; l; l = l->next)
    if (l->data != main_win && gtk_widget_get_mapped (l->data) &&
        find_type (l->data, GTK_TYPE_EVENT_BOX))
      found = l->data;
  g_list_free (tops);
  return found;
}

static int
fake_handle_of (GtkWidget *top, int *ox, int *oy)
{
  int i;

  gdk_window_get_origin (gtk_widget_get_window (top), ox, oy);
  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle && fake_windows[i].handle != main_handle &&
        fake_windows[i].handle != FOREIGN && (fake_windows[i].x0 >> 1) == *ox &&
        1024 - (fake_windows[i].y1 >> 1) == *oy)
      return fake_windows[i].handle;
  return 0;
}

/* The open Wimp menu's entries (menu block at m) */
static int
menu_entries (const int *m, char out[][48], int max, const int **subs)
{
  const int *it = m + 7;
  int i, n = 0;

  for (i = 0; i < max; i++, it += 6)
    {
      if (it[2] & 0x100)
        g_strlcpy (out[i], (const char *) (long) it[3], 48);
      else
        {
          memcpy (out[i], it + 3, 12);
          out[i][12] = 0;
        }
      if (subs)
        subs[i] = it[1] != -1 ? (const int *) (long) it[1] : NULL;
      n = i + 1;
      if (it[0] & 0x80)
        break;
    }
  return n;
}

/* ---- the other task (a Filer window) ---- */

static void
message_hook (int reason, int *b, int dest, int icon)
{
  int q[64];

  memset (q, 0, sizeof q);
  switch (b[4])
    {
    case 1:                           /* DataSave from the Save box */
      if (dest == FOREIGN && b[3] == 0)
        {
          save_ds_seen++;
          save_ds_type = b[10];
          g_strlcpy (save_ds_leaf, (const char *) &b[11], sizeof save_ds_leaf);
          memcpy (&q[5], &b[5], 16);
          q[9] = 100;
          q[10] = b[10];
          g_strlcpy ((char *) &q[11], save_target_ro, 200);
          queue (17, q, 2, b[2], 44 + (int) strlen (save_target_ro) + 1);
        }
      break;
    case 3:                           /* DataLoad: saved */
      if (reason == 18 && dest == OTHER_TASK && b[3] != 0)
        {
          save_dl_seen++;
          memcpy (q, b, b[0]);
          queue (17, q, 4, b[2], b[0]);
        }
      break;
    case 4:                           /* DataLoadAck: our drop taken */
      drop_ack++;
      break;
    }
}

/* OS_File 5 / 255 / 18 on files in outdir; DragASprite */
static void
unixify (const char *ro, char *out, int size)
{
  char *d = out;
  *d++ = '/';
  for (; *ro && d < out + size - 1; ro++)
    *d++ = *ro == '.' ? '/' : *ro == '/' ? '.' : *ro;
  *d = 0;
}

static int
swi_hook (int no, _kernel_swi_regs *r, _kernel_oserror **e)
{
  static _kernel_oserror notfound = { 0xD6, "Not found" };
  char path[600];
  struct stat st;

  switch (no & ~0x20000)
    {
    case 0x08:
      if (r->r[0] == 5 || r->r[0] == 255)
        {
          unixify ((const char *) (long) r->r[1], path, sizeof path);
          if (stat (path, &st) != 0)
            {
              if (r->r[0] == 5)
                {
                  r->r[0] = 0;
                  return 1;
                }
              *e = &notfound;
              return 1;
            }
          if (r->r[0] == 5)
            {
              r->r[0] = 1;
              r->r[2] = (int) 0xFFFFFF00;
              r->r[4] = (int) st.st_size;
              return 1;
            }
          {
            FILE *f = fopen (path, "rb");
            if (!f)
              {
                *e = &notfound;
                return 1;
              }
            if (fread ((void *) (long) r->r[2], 1, st.st_size, f) != (size_t) st.st_size)
              *e = &notfound;
            fclose (f);
          }
          return 1;
        }
      if (r->r[0] == 18)
        return 1;
      *e = &notfound;
      return 1;
    case 0x42400: case 0x42401:      /* DragASprite_Start / Stop */
      return 1;
    }
  return 0;
}

/* L3_SHOTS: the screen as OUTDIR/shot-NAME.ppm */
static void
shot (const char *name)
{
  char path[600];
  FILE *f;
  int x, y;

  if (!getenv ("L3_SHOTS"))
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
}

/* ---- the steps ---- */

static gboolean
step_ready (void)
{
  char *t;
  gboolean ok;

  switch (step)
    {
    case 1:
      t = buffer_text ();
      ok = strcmp (t, "Hello RISC OS") == 0;
      g_free (t);
      return ok;
    case 2:
      return menu_seen && fake_menu != NULL;
    case 3:
      return save_box () != NULL;
    case 4:
      return save_dl_seen > 0;
    case 6:
      return drop_ack > 0;
    default:
      return waits > 10;
    }
}

static void
type (const char *s)
{
  for (; *s; s++)
    {
      add (8, (unsigned char) *s, 0, 0);
      nulls (1);
    }
}

static void
next_step (void)
{
  int q[64], x, y;

  memset (q, 0, sizeof q);
  step++;
  menu_seen = 0;
  switch (step)
    {
    case 1:      /* type */
      type (getenv ("L3_TYPE") ? getenv ("L3_TYPE") : "Hello RISC OS");
      nulls (6);
      break;
    case 2:      /* Menu over the text: the program's menu */
      shot ("typed");
      {
        char *t = buffer_text ();
        g_strlcpy (typed, t, sizeof typed);
        g_free (t);
        gtk_widget_translate_coordinates (view, main_win, 40, 40, &x, &y);
        add (FAKE_MOVE, x, y, 0);
        add (6, x, y, 2);
        add (FAKE_RELEASE, 0, 0, 0);
        nulls (10);
      }
      break;
    case 3:      /* File > Save As... */
      {
        const int *subs[16];
        char tops[8][48];
        int i;
        n_titles = menu_entries (fake_menu, tops, 8, subs);
        for (i = 0; i < n_titles; i++)
          g_strlcpy (menu_titles[i], tops[i], sizeof menu_titles[i]);
        if (n_titles > 0 && subs[0])
          {
            n_file_items = menu_entries (subs[0], file_menu, 16, NULL);
            for (i = 0; i < n_file_items; i++)
              if (strncmp (file_menu[i], "Save As", 7) == 0)
                save_as_index = i;
          }
        if (save_as_index >= 0)
          add (9, 0, save_as_index, -1);
        nulls (20);
      }
      break;
    case 4:      /* drag the Save box's icon onto the other task's window */
      shot ("savebox");
      {
        GtkWidget *box = save_box ();
        GtkWidget *icon = box ? find_type (box, GTK_TYPE_EVENT_BOX) : NULL;
        int ox, oy, h, i, sx, sy, tx, ty;
        GtkAllocation a;

        save_box_seen = box != NULL;
        if (!icon || !(h = fake_handle_of (box, &ox, &oy)))
          {
            nulls (4);
            break;
          }
        fake_target_window = h;
        gtk_widget_get_allocation (icon, &a);
        gtk_widget_translate_coordinates (icon, box, 0, 0, &x, &y);
        sx = x + a.width / 2;
        sy = y + a.height / 2;
        tx = 50 - ox;
        ty = 450 - oy;
        add (FAKE_MOVE, sx, sy, 0);
        add (6, sx, sy, 4);
        nulls (3);
        for (i = 1; i <= 8; i++)
          {
            add (FAKE_MOVE, sx + (tx - sx) * i / 8, sy + (ty - sy) * i / 8, 0);
            nulls (2);
          }
        add (FAKE_SLEEP, 30, 0, 0);
        add (FAKE_RELEASE, 0, 0, 0);
        nulls (6);
        for (i = 0; i < 12; i++)
          add (FAKE_SLEEP, 100, 0, 0);
        nulls (6);
      }
      break;
    case 5:
      g_strlcpy (title_after_save, gtk_window_get_title (GTK_WINDOW (main_win)), sizeof title_after_save);
      fake_target_window = main_handle;
      nulls (6);
      break;
    case 6:      /* a Filer drops a RISC OS text file (Latin-1) on the text */
      {
        int i;
        gtk_widget_translate_coordinates (view, main_win, 40, 40, &x, &y);
        q[5] = main_handle;
        q[6] = -1;
        for (i = 0; i < FAKE_MAX_WINDOWS; i++)
          if (fake_windows[i].handle == main_handle)
            {
              q[7] = fake_windows[i].x0 + x * 2;
              q[8] = fake_windows[i].y1 - y * 2;
            }
        q[9] = 6;
        q[10] = 0xFFF;
        g_strlcpy ((char *) &q[11], drop_file_ro, 200);
        queue (17, q, 3, 0, 44 + (int) strlen (drop_file_ro) + 1);
        nulls (20);
      }
      break;
    case 7:
      shot ("dropped");
      {
        char *t = buffer_text ();
        g_strlcpy (text_after_drop, t, sizeof text_after_drop);
        g_free (t);
        nulls (4);
      }
      break;
    default:
      return;
    }
  printf ("# step %d (DataSave %d, DataLoad %d, box %d, Save As at %d, drop ack %d)\n", step,
          save_ds_seen, save_dl_seen, save_box_seen, save_as_index, drop_ack);
  fflush (stdout);
}

static gboolean
dbg_key (GtkWidget *w, GdkEventKey *ev, gpointer name)
{
  if (getenv ("L3_DEBUG"))
    printf ("# %s key %s %x state %x\n", (const char *) name,
            ev->type == GDK_KEY_PRESS ? "press" : "release", ev->keyval, ev->state);
  return FALSE;
}

static void
dbg_ddr (GtkWidget *w, GdkDragContext *c, gint x, gint y, GtkSelectionData *sd, guint info,
         guint t, gpointer d)
{
  if (getenv ("L3_DEBUG"))
    printf ("# drag-data-received (%s) info %u target %s len %d\n", (const char *) d, info,
            gdk_atom_name (gtk_selection_data_get_target (sd)), gtk_selection_data_get_length (sd));
}

static void
dbg_handler (GdkEvent *e, gpointer d)
{
  if (e->type == GDK_KEY_PRESS || e->type == GDK_KEY_RELEASE)
    printf ("# gdk %s %x window %p\n", e->type == GDK_KEY_PRESS ? "press" : "release",
            e->key.keyval, (void *) e->key.window);
  gtk_main_do_event (e);
}

static void
hook (int reason)
{
  int i;

  if (fake_menu && fake_menus_opened != opened_seen)
    {
      opened_seen = fake_menus_opened;
      menu_seen = 1;
    }
  if (stage == 0)
    {
      GList *l, *tops = gtk_window_list_toplevels ();
      for (l = tops; l; l = l->next)
        if (gtk_widget_get_mapped (l->data) && (view = find_type (l->data, GTK_TYPE_TEXT_VIEW)))
          {
            main_win = l->data;
            break;
          }
      g_list_free (tops);
      if (main_win && fake_plots > 0)
        {
          stage = 1;
          for (i = 0; i < FAKE_MAX_WINDOWS; i++)
            if (fake_windows[i].handle)
              main_handle = fake_windows[i].handle;
          fake_target_window = main_handle;
          g_signal_connect (main_win, "key-press-event", G_CALLBACK (dbg_key), "window");
          if (getenv ("L3_NOMNEMONIC"))
            g_object_set (gtk_settings_get_default (), "gtk-enable-mnemonics", FALSE, NULL);
          if (getenv ("L3_DEBUG2"))
            gdk_event_handler_set (dbg_handler, NULL, NULL);
          g_signal_connect_after (view, "key-press-event", G_CALLBACK (dbg_key), "view-after");
          g_signal_connect (view, "drag-data-received", G_CALLBACK (dbg_ddr), "first");
          fake_open_window (FOREIGN, 0, 2048 - 1200, 220, 2048 - 600, 0, 0);
          add (12, 0, 0, 0);         /* Gain_Caret */
          nulls (10);
        }
      return;
    }
  if (stage == 1 && fake_wimp_script_pos () >= fake_wimp_script_len - 3)
    {
      if (step >= 7)
        {
          if (!quit_asked)
            {
              quit_asked = 1;
              /* the document has changed since the save: the close icon
                 asks first (a GTK dialogue), so mark it unmodified */
              gtk_text_buffer_set_modified (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), FALSE);
              nulls (4);
            }
          return;                     /* the script ends: Close_Window_Request */
        }
      if (step_ready () || waits > 150)
        {
          waits = 0;
          next_step ();
        }
      else
        {
          waits++;
          nulls (3);
        }
    }
}

int
main (int argc, char **argv)
{
  char logpath[600], *line, *ro;
  static char logbuf[65536];
  char *targv[2];
  int saved_out = dup (1), saved_err = dup (2);
  FILE *lf;
  struct stat st;

  if (argc < 3)
    return 2;
  outdir = argv[2];
  {
    char *conf = g_build_filename (argv[1], "fonts", "fonts.conf", NULL);
    char *share = g_build_filename (argv[1], "share", NULL);
    g_setenv ("FONTCONFIG_FILE", conf, TRUE);
    g_setenv ("XDG_DATA_DIRS", share, TRUE);
  }
  g_setenv ("XDG_CONFIG_HOME", outdir, TRUE);
  g_setenv ("L3AFPAD_NO_LOG", "1", TRUE);
  if (getenv ("L3_DEBUG"))
    g_setenv ("RISCOS_GTK_DEBUG", "1", TRUE);
  g_setenv ("RISCOS_GTK_DEFAULT_FILETYPE", "fff", TRUE);
  g_snprintf (save_target, sizeof save_target, "%s/Notes", outdir);
  ro = gdk_riscos_path_from_unix (save_target);
  g_strlcpy (save_target_ro, ro, sizeof save_target_ro);
  g_free (ro);
  unlink (save_target);
  g_snprintf (drop_file, sizeof drop_file, "%s/Recipe", outdir);
  ro = gdk_riscos_path_from_unix (drop_file);
  g_strlcpy (drop_file_ro, ro, sizeof drop_file_ro);
  g_free (ro);
  g_file_set_contents (drop_file, " Caf\xe9", -1, NULL);   /* Latin-1, as RISC OS writes */

  fake_set_screen (1280, 1024, 0, 5);
  fake_sprite_plot_converts = 0;
  fake_wimp_nulls = 400;
  fake_wimp_hook = hook;
  fake_message_hook = message_hook;
  fake_swi_hook = swi_hook;
  g_snprintf (logpath, sizeof logpath, "%s/L3afpad.Log", outdir);
  fflush (stdout);
  if (freopen (logpath, "w", stderr) == NULL)
    return 3;
  targv[0] = "l3afpad";
  targv[1] = NULL;
  l3afpad_main (1, targv);
  fflush (stdout); fflush (stderr);
  dup2 (saved_out, 1); dup2 (saved_err, 2);
  if ((lf = fopen (logpath, "r")))
    {
      size_t n = fread (logbuf, 1, sizeof logbuf - 1, lf);
      logbuf[n] = 0;
      fclose (lf);
    }

  check (stage == 1, "L3afpad's window opens and is drawn");
  check (strcmp (typed, "Hello RISC OS") == 0, "typing goes into the text");
  printf ("# typed \"%s\"\n", typed);
  check (n_titles >= 4 && strstr (menu_titles[0], "File") && strstr (menu_titles[1], "Edit"),
         "Menu over the text opens L3afpad's own menu (File, Edit, ...), not the text view's");
  {
    int i;
    printf ("# menu:");
    for (i = 0; i < n_titles; i++)
      printf (" [%s]", menu_titles[i]);
    printf ("\n# File menu:");
    for (i = 0; i < n_file_items; i++)
      printf (" [%s]", file_menu[i]);
    printf ("\n");
  }
  check (save_as_index >= 0 && save_box_seen, "File > Save As opens a RISC OS Save box");
  check (save_ds_seen == 1 && save_ds_type == 0xFFF,
         "dragging its icon to another window offers the text as type Text (&FFF)");
  printf ("# DataSave \"%s\" &%03X\n", save_ds_leaf, save_ds_type);
  {
    char *got = NULL;
    gboolean saved = g_file_get_contents (save_target, &got, NULL, NULL);
    check (saved && got && strstr (got, "Hello RISC OS") && save_dl_seen == 1,
           "L3afpad saves to the name the receiver gave (then DataLoad)");
    g_free (got);
  }
  check (strstr (title_after_save, "Notes") != NULL, "the window's title becomes the saved file's");
  printf ("# title \"%s\"\n", title_after_save);
  {
    const char *c = strstr (text_after_drop, "Caf\xc3\xa9");
    check (drop_ack == 1 && c && !strstr (c + 1, "Caf\xc3\xa9"),
           "a RISC OS text file dropped on the text is inserted once (Latin-1 read as such)");
  }
  printf ("# after the drop: \"%s\"\n", text_after_drop);
  check (strstr (logbuf, "CRITICAL") == NULL && strstr (logbuf, "WARNING") == NULL,
         "no GTK warnings or criticals in the log");
  check (stat (save_target, &st) == 0, "and it quits when its window is closed");

  printf ("# log:\n");
  for (line = strtok (logbuf, "\n"); line; line = strtok (NULL, "\n"))
    printf ("#   %s\n", line);
  printf ("1..%d\n# l3afpad-fake: %d checks, %d failed\n", n_tests, n_tests, n_failed);
  return n_failed;
}
