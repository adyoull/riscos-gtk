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
static int font_index = -1, options_index = -1, font_dialog_seen, font_families;
static unsigned int grip_pixel, grip_drag_type = -1, grip_presses_before, grip_presses_after;
static int font_dialog_grip = -1;
/* the icon bar */
static int icon_clicked, window_at_start = -1, window_after_select;
static char ib_items[8][48];
static int n_ib_items, ib_info_sub, ib_info_is_window, ib_shaded;
static int choices_seen, choices_wrap_applied;
static char choices_text[512];
static int hidden_after_close, empty_after_close;
static int dataopen_ref, dataopen_ack, iconload_ref, iconload_ack, prequit_ack, prequit_dialog;
static char text_after_dataopen[128], text_after_iconload[128];
static int visible_after_dataopen, still_running_after_cancel, quit_chosen, forced_quit;
static int main_centred = -1, choices_centred = -1, main_cx, main_cy, choices_cx, choices_cy;
static int printsave_seen, typeknown_seen, print_box_seen, print_box_title_ok, print_box_gadgets, print_button_at[4];
static int print_index = -1, preview_index = -1, preview_seen, preview_back_off, preview_print_shown;
static char preview_label[64];
static int reports_before, ib_menu_x, ib_menu_y, ib_click_x;
static char prequit_text[256];
static char letter[512], letter_ro[512], note3[512], note3_ro[512];
static int presses, bar_bottom = -1, win_h, grip_h_px;

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

/* Is Wimp window handle in the middle of the 1280 x 1024 (2560 x 2048 OS
   units) screen? (its work area's centre within 4 OS units) */
static int
centred (int handle, int *cx, int *cy)
{
  int i;

  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (handle && fake_windows[i].handle == handle)
      {
        *cx = (fake_windows[i].x0 + fake_windows[i].x1) / 2;
        *cy = (fake_windows[i].y0 + fake_windows[i].y1) / 2;
        return abs (*cx - 1280) <= 4 && abs (*cy - 1024) <= 4;
      }
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
    case 4:                           /* DataLoadAck */
      if (dataopen_ref && b[3] == dataopen_ref)
        dataopen_ack++;               /* the DataOpen: L3afpad loads it */
      else if (iconload_ref && b[3] == iconload_ref)
        iconload_ack++;
      else
        drop_ack++;                   /* our drop taken */
      break;
    case 0x80142:                     /* PrintSave: !Printers says the printer's free */
      if (reason == 18 && dest == 0)
        {
          printsave_seen++;
          memcpy (q, b, 256);
          queue (17, q, 0x80140, b[2], 256);          /* PrintFile */
          memset (q, 0, sizeof q);
          q[10] = 0xFF9;
          queue (18, q, 0x80145, 0, 48);              /* PrintTypeOdd */
        }
      break;
    case 0x80146:                     /* PrintTypeKnown: it prints itself */
      typeknown_seen++;
      break;
    case 8:                           /* PreQuit acknowledged: it objects */
      if (reason == 19)
        prequit_ack++;
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

static GtkWidget *
dialog_titled (const char *title, GType type)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  GtkWidget *d = NULL;

  for (l = tops; l; l = l->next)
    if (G_TYPE_CHECK_INSTANCE_TYPE (l->data, type) && gtk_widget_get_mapped (l->data) &&
        (title == NULL || g_strcmp0 (gtk_window_get_title (l->data), title) == 0))
      d = l->data;
  g_list_free (tops);
  return d;
}

static GtkWidget *
preview_window (void)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  GtkWidget *d = NULL;

  for (l = tops; l; l = l->next)
    if (gtk_widget_get_mapped (l->data) && gtk_window_get_title (l->data) &&
        g_str_has_prefix (gtk_window_get_title (l->data), "Preview:"))
      d = l->data;
  g_list_free (tops);
  return d;
}

/* A widget of type with the label text, in top */
static GtkWidget *
labelled (GtkWidget *w, GType type, const char *text)
{
  GtkWidget *found = NULL;

  if (G_TYPE_CHECK_INSTANCE_TYPE (w, type))
    {
      const char *t = GTK_IS_BUTTON (w) ? gtk_button_get_label (GTK_BUTTON (w))
                    : GTK_IS_LABEL (w) ? gtk_label_get_text (GTK_LABEL (w)) : NULL;
      if (t && strcmp (t, text) == 0)
        return w;
    }
  if (GTK_IS_CONTAINER (w))
    {
      GList *kids = gtk_container_get_children (GTK_CONTAINER (w)), *l;
      for (l = kids; l && !found; l = l->next)
        found = labelled (l->data, type, text);
      g_list_free (kids);
    }
  return found;
}

static GtkWidget *
page_label (GtkWidget *w)
{
  GtkWidget *found = NULL;

  if (GTK_IS_LABEL (w) && g_str_has_prefix (gtk_label_get_text (GTK_LABEL (w)), "Page "))
    return w;
  if (GTK_IS_CONTAINER (w))
    {
      GList *kids = gtk_container_get_children (GTK_CONTAINER (w)), *l;
      for (l = kids; l && !found; l = l->next)
        found = page_label (l->data);
      g_list_free (kids);
    }
  return found;
}

static void
open_window_menu (void)
{
  int x, y;
  gtk_widget_translate_coordinates (view, main_win, 40, 40, &x, &y);
  add (FAKE_MOVE, x, y, 0);
  add (6, x, y, 2);
  add (FAKE_RELEASE, 0, 0, 0);
  nulls (10);
}

static void
iconbar_click (int buttons)
{
  fake_target_window = -2;
  fake_target_icon = fake_iconbar_icons;
  add (6, 0, 0, buttons);
  nulls (10);
}

static void
broadcast_file (int action, int window, int icon, const char *ro, int *ref)
{
  int q[64];
  memset (q, 0, sizeof q);
  q[5] = window;
  q[6] = icon;
  q[10] = 0xFFF;
  g_strlcpy ((char *) &q[11], ro, 200);
  queue (action == 5 ? 18 : 17, q, action, 0, 44 + (int) strlen (ro) + 1);
  *ref = q[2];
}

static GtkWidget *
font_dialog (void)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  GtkWidget *d = NULL;

  for (l = tops; l; l = l->next)
    if (GTK_IS_FONT_CHOOSER_DIALOG (l->data) && gtk_widget_get_mapped (l->data))
      d = l->data;
  g_list_free (tops);
  return d;
}

static gboolean
count_rows (GtkTreeModel *m, GtkTreePath *p, GtkTreeIter *it, gpointer d)
{
  (*(int *) d)++;
  return FALSE;
}

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
    case 8:
      return menu_seen && fake_menu != NULL;
    case 9:
      return font_dialog () != NULL;
    case 13:
    case 21:
    case 24:
    case 27:
      return menu_seen && fake_menu != NULL;
    case 14:
      return dialog_titled ("L3afpad choices", GTK_TYPE_DIALOG) != NULL;
    case 16:
      return !gtk_widget_get_visible (main_win);
    case 17:
      t = buffer_text ();
      ok = strstr (t, "Opened by DataOpen") != NULL;
      g_free (t);
      return ok;
    case 18:
      t = buffer_text ();
      ok = strstr (t, "Dropped on the icon") != NULL;
      g_free (t);
      return ok;
    case 19:
      return prequit_ack > 0;
    case 22:
      return dialog_titled ("Fake LaserJet", GTK_TYPE_WINDOW) != NULL;
    case 23:
      return fake_print_ended > 0 || fake_print_aborted > 0;
    case 25:
      return preview_window () != NULL;
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
      main_centred = centred (main_handle, &main_cx, &main_cy);
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
    case 8:      /* Menu again, for Options > Font */
      gtk_widget_translate_coordinates (view, main_win, 40, 40, &x, &y);
      add (FAKE_MOVE, x, y, 0);
      add (6, x, y, 2);
      add (FAKE_RELEASE, 0, 0, 0);
      nulls (10);
      break;
    case 9:      /* Options > Font...: GTK's font dialogue */
      {
        const int *subs[16];
        char tops[8][48], items[16][48];
        int i, n = menu_entries (fake_menu, tops, 8, subs);
        for (i = 0; i < n; i++)
          if (strstr (tops[i], "Options"))
            options_index = i;
        if (options_index >= 0 && subs[options_index])
          {
            int m = menu_entries (subs[options_index], items, 16, NULL);
            for (i = 0; i < m; i++)
              if (strncmp (items[i], "Font", 4) == 0)
                font_index = i;
          }
        if (font_index >= 0)
          add (9, options_index, font_index, -1);
        nulls (40);
      }
      break;
    case 10:     /* look at it, then Cancel */
      shot ("font");
      {
        GtkWidget *d = font_dialog ();
        GtkWidget *tv = d ? find_type (d, GTK_TYPE_TREE_VIEW) : NULL;
        font_dialog_seen = d != NULL;
        if (d)
          font_dialog_grip = gdk_riscos_window_get_size_grip (gtk_widget_get_window (d), NULL, NULL);
        if (tv && gtk_tree_view_get_model (GTK_TREE_VIEW (tv)))
          gtk_tree_model_foreach (gtk_tree_view_get_model (GTK_TREE_VIEW (tv)), count_rows,
                                  &font_families);
        if (d)
          gtk_dialog_response (GTK_DIALOG (d), GTK_RESPONSE_CANCEL);
        nulls (10);
      }
      break;
    case 11:     /* the size grip in the bottom right corner */
      {
        GtkAllocation a;
        int i;
        gtk_widget_get_allocation (main_win, &a);
        for (i = 0; i < FAKE_MAX_WINDOWS; i++)
          if (fake_windows[i].handle == main_handle)
            {
              int px = (fake_windows[i].x1 >> fake_screen.xeig) - 3;
              int py = fake_screen.h - 1 - (fake_windows[i].y0 >> fake_screen.yeig) - 2;
              grip_pixel = fake_screen.mem[py * fake_screen.w + px] & 0xFFFFFF;
            }
        grip_presses_before = presses;
        fake_dragbox_type = -1;
        add (FAKE_MOVE, a.width - 3, a.height - 3, 0);
        add (6, a.width - 3, a.height - 3, 4);
        nulls (2);
        add (FAKE_RELEASE, 0, 0, 0);
        nulls (4);
      }
      break;
    case 12:
      shot ("grip");
      {
        GtkWidget *sw = find_type (main_win, GTK_TYPE_SCROLLED_WINDOW);
        GtkWidget *bar = sw ? gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (sw)) : NULL;
        int bx, by, gw;
        win_h = gtk_widget_get_allocated_height (main_win);
        gdk_riscos_window_get_size_grip (gtk_widget_get_window (main_win), &gw, &grip_h_px);
        if (bar && gtk_widget_translate_coordinates (bar, main_win, 0, 0, &bx, &by))
          bar_bottom = by + gtk_widget_get_allocated_height (bar);
      }
      grip_drag_type = fake_dragbox_type;
      grip_presses_after = presses;
      nulls (4);
      break;
    case 13:     /* Menu on the icon bar icon */
      iconbar_click (2);
      break;
    case 14:     /* its menu; Choices... */
      fake_target_window = main_handle;
      fake_target_icon = -1;
      ib_menu_x = fake_menu_x;
      ib_menu_y = fake_menu_y;
      ib_click_x = fake_pointer[0];
      {
        int i;
        n_ib_items = menu_entries (fake_menu, ib_items, 8, NULL);
        ib_info_sub = fake_menu[7 + 1];
        for (i = 0; i < n_ib_items; i++)
          if (fake_menu[7 + 6 * i + 2] & (1 << 22))
            ib_shaded |= 1 << i;
        ib_info_is_window = ib_info_sub > 0 && ib_info_sub < 0x10000 &&
                            (fake_window_flags[ib_info_sub & 255] & (1 << 26));
        for (i = 0; i < n_ib_items; i++)
          if (strncmp (ib_items[i], "Choices", 7) == 0)
            add (9, i, -1, -1);
        nulls (20);
      }
      break;
    case 15:     /* the Choices window: word wrap on, tab width 4, Set */
      shot ("choices");
      {
        GtkWidget *d = dialog_titled ("L3afpad choices", GTK_TYPE_DIALOG);
        GList *checks = NULL;
        GtkWidget *grid;
        choices_seen = d != NULL;
        if (d)
          {
            int ox, oy;
            choices_centred = centred (fake_handle_of (d, &ox, &oy), &choices_cx, &choices_cy);
          }
        if (d)
          {
            grid = find_type (gtk_dialog_get_content_area (GTK_DIALOG (d)), GTK_TYPE_GRID);
            if (grid)
              {
                GList *kids = gtk_container_get_children (GTK_CONTAINER (grid)), *l;
                for (l = kids; l; l = l->next)
                  if (GTK_IS_CHECK_BUTTON (l->data) &&
                      strcmp (gtk_button_get_label (l->data), "Word wrap") == 0)
                    gtk_toggle_button_set_active (l->data, TRUE);
                  else if (GTK_IS_SPIN_BUTTON (l->data))
                    gtk_spin_button_set_value (l->data, 4);
                g_list_free (kids);
              }
            gtk_dialog_response (GTK_DIALOG (d), GTK_RESPONSE_OK);
          }
        g_list_free (checks);
        nulls (10);
      }
      break;
    case 16:     /* the close icon: emptied and hidden, still running */
      {
        char *path = g_build_filename (outdir, "Choices", "Choices", NULL), *c = NULL;
        if (g_file_get_contents (path, &c, NULL, NULL))
          g_strlcpy (choices_text, c, sizeof choices_text);
        g_free (c);
        g_free (path);
        choices_wrap_applied = gtk_text_view_get_wrap_mode (GTK_TEXT_VIEW (view)) == GTK_WRAP_WORD;
      }
      gtk_text_buffer_set_modified (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), FALSE);
      add (3, 0, 0, 0);                /* Close_Window_Request */
      nulls (10);
      break;
    case 17:     /* a text file double-clicked in the Filer: DataOpen */
      {
        char *t = buffer_text ();
        hidden_after_close = !gtk_widget_get_visible (main_win);
        empty_after_close = t[0] == 0;
        g_free (t);
        broadcast_file (5, 0, 0, letter_ro, &dataopen_ref);
        nulls (20);
      }
      break;
    case 18:     /* a text file dropped on the icon: DataLoad to window -2 */
      {
        char *t = buffer_text ();
        g_strlcpy (text_after_dataopen, t, sizeof text_after_dataopen);
        g_free (t);
        visible_after_dataopen = gtk_widget_get_visible (main_win);
        broadcast_file (3, -2, fake_iconbar_icons, note3_ro, &iconload_ref);
        nulls (20);
      }
      break;
    case 19:     /* unsaved text, then the desktop shuts down: PreQuit; Cancel */
      {
        char *t = buffer_text ();
        int q[64];
        g_strlcpy (text_after_iconload, t, sizeof text_after_iconload);
        g_free (t);
        gtk_text_buffer_insert_at_cursor (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), "!", 1);
        reports_before = fake_reports;
        fake_report_answer = 2;        /* Cancel */
        memset (q, 0, sizeof q);
        queue (18, q, 8, 0, 24);
        nulls (20);
      }
      break;
    case 20:
      prequit_dialog = fake_reports == reports_before + 1 &&
                       strstr (fake_report_buttons, "Discard") != NULL &&
                       (fake_report_flags & (4 << 9)) != 0;
      g_strlcpy (prequit_text, fake_report_text, sizeof prequit_text);
      nulls (10);
      break;
    case 21:     /* still here: Menu over the text, for File > Print... */
      still_running_after_cancel = 1;
      open_window_menu ();
      break;
    case 22:
      if (getenv ("L3_DEBUG"))
        {
          GdkRiscosPrinter pi;
          gboolean ok = gdk_riscos_printer_info (&pi);
          printf ("# printer_info %d \"%s\" %d dpi, paper %.1f x %.1f\n", ok, pi.name, pi.dpi_x,
                  pi.paper_width, pi.paper_height);
        }
      {
        const int *subs[16];
        char tops[8][48], items[16][48];
        int i, m;
        menu_entries (fake_menu, tops, 8, subs);
        m = subs[0] ? menu_entries (subs[0], items, 16, NULL) : 0;
        for (i = 0; i < m; i++)
          if (g_str_has_prefix (items[i], "Print..."))
            print_index = i;
          else if (g_str_has_prefix (items[i], "Print Preview"))
            preview_index = i;
        if (print_index >= 0)
          add (9, 0, print_index, -1);
        nulls (20);
      }
      break;
    case 23:     /* the Print box: 2 copies, Print */
      shot ("printbox");
      {
        GtkWidget *d = dialog_titled ("Fake LaserJet", GTK_TYPE_WINDOW);
        static const char *const gadgets[] = { "All", "From", "to", "Copies", "Scale", "%",
                                               "Upright", "Sideways", "Draft", "Save", "Cancel",
                                               "Print", NULL };
        print_box_seen = d != NULL;
        print_box_title_ok = d != NULL;
        if (d)
          {
            int i;
            GtkWidget *print = labelled (d, GTK_TYPE_BUTTON, "Print"), *spin;
            GtkAllocation a;
            for (i = 0; gadgets[i]; i++)
              if (labelled (d, GTK_TYPE_WIDGET, gadgets[i]))
                print_box_gadgets++;
            if (print)
              {
                gtk_widget_get_allocation (print, &a);
                print_button_at[0] = a.x;
                print_button_at[1] = a.y;
                print_button_at[2] = a.width;
                print_button_at[3] = a.height;
              }
            spin = find_type (d, GTK_TYPE_SPIN_BUTTON);
            if (spin)
              gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), 2);
            if (print)
              gtk_button_clicked (GTK_BUTTON (print));
          }
        nulls (20);
      }
      break;
    case 24:     /* then File > Print Preview */
      if (getenv ("L3_DEBUG"))
        {
          GtkWidget *bar = g_object_get_data (G_OBJECT (main_win), "riscos-gtk-window-menu");
          printf ("# bar %p sensitive %d, view sensitive %d, grab %p\n", (void *) bar,
                  bar ? gtk_widget_is_sensitive (bar) : -1, gtk_widget_is_sensitive (view),
                  (void *) gtk_grab_get_current ());
        }
      open_window_menu ();
      break;
    case 25:
      printf ("# Print at %d, Print Preview at %d in the File menu\n", print_index, preview_index);
      if (preview_index >= 0)
        add (9, 0, preview_index, -1);
      nulls (30);
      break;
    case 26:
      shot ("preview");
      {
        GtkWidget *pw = preview_window ();
        preview_seen = pw != NULL;
        if (pw)
          {
            GtkWidget *back = labelled (pw, GTK_TYPE_BUTTON, "Back");
            GtkWidget *print = labelled (pw, GTK_TYPE_BUTTON, "Print");
            preview_back_off = back && !gtk_widget_is_sensitive (back);
            preview_print_shown = print && gtk_widget_get_visible (print);
            /* "Page 1 of n" */
            {
              GtkWidget *lab = page_label (pw);
              if (lab)
                g_strlcpy (preview_label, gtk_label_get_text (GTK_LABEL (lab)), sizeof preview_label);
            }
            gtk_widget_destroy (pw);
          }
        nulls (10);
      }
      break;
    case 27:     /* still here; Menu on the icon again (the text still unsaved) */
      still_running_after_cancel = 1;
      iconbar_click (2);
      break;
    case 28:     /* Quit */
      fake_target_window = main_handle;
      fake_target_icon = -1;
      {
        int i;
        char items[8][48];
        int n = menu_entries (fake_menu, items, 8, NULL);
        reports_before = fake_reports;
        fake_report_answer = 3;        /* Discard */
        for (i = 0; i < n; i++)
          if (strcmp (items[i], "Quit") == 0)
            {
              add (9, i, -1, -1);
              quit_chosen = 1;
            }
        nulls (20);
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
count_press (GtkWidget *w, GdkEventButton *ev, gpointer d)
{
  presses++;
  return FALSE;
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
  if (stage == 0 && !icon_clicked)
    {
      GList *l, *tops;
      if (!fake_iconbar_icons)
        return;
      /* it starts on the icon bar, without a window */
      tops = gtk_window_list_toplevels ();
      window_at_start = 0;
      for (l = tops; l; l = l->next)
        if (gtk_widget_get_mapped (l->data) && find_type (l->data, GTK_TYPE_TEXT_VIEW))
          window_at_start = 1;
      g_list_free (tops);
      icon_clicked = 1;
      iconbar_click (4);              /* Select on the icon */
      return;
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
          fake_target_icon = -1;
          window_after_select = 1;
          g_signal_connect (main_win, "key-press-event", G_CALLBACK (dbg_key), "window");
          g_signal_connect (main_win, "button-press-event", G_CALLBACK (count_press), NULL);
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
      if (step >= 28)
        {
          /* Quit was chosen: L3afpad should end; if not, stop it */
          if (++quit_asked > 200)
            {
              forced_quit = 1;
              gtk_main_quit ();
              return;
            }
          nulls (3);
          return;
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

/* the Wimp pool has L3afpad's sprite (its !Boot loads !Sprites) */
static const int *
pool_lookup (const char *name)
{
  static int spr[12] = { 48, 0, 0, 0, 0, 0, 0, 31, 44, 44, 0x301680B5, 0 };
  if (strcmp (name, "!l3afpad") != 0)
    return NULL;
  memcpy (&spr[1], "!l3afpad\0\0\0\0", 12);
  return spr;
}

int
main (int argc, char **argv)
{
  char logpath[600], *line, *ro;
  static char logbuf[65536];
  char *targv[2];
  int saved_out = dup (1), saved_err = dup (2);
  FILE *lf;

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
  {
    char *c = g_build_filename (outdir, "Choices", NULL);
    g_setenv ("L3AFPAD_CHOICES", c, TRUE);
    g_free (c);
  }
  fake_pool_lookup = pool_lookup;
  g_snprintf (letter, sizeof letter, "%s/Letter", outdir);
  ro = gdk_riscos_path_from_unix (letter);
  g_strlcpy (letter_ro, ro, sizeof letter_ro);
  g_free (ro);
  g_file_set_contents (letter, "Opened by DataOpen\n", -1, NULL);
  g_snprintf (note3, sizeof note3, "%s/Note3", outdir);
  ro = gdk_riscos_path_from_unix (note3);
  g_strlcpy (note3_ro, ro, sizeof note3_ro);
  g_free (ro);
  g_file_set_contents (note3, "Dropped on the icon\n", -1, NULL);
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

  check (fake_iconbar_icons == 1 && strcmp (fake_iconbar_sprite, "!l3afpad") == 0 && window_at_start == 0,
         "it starts on the icon bar (sprite !l3afpad), without a window");
  printf ("# icon bar: %d icon(s), sprite \"%s\", flags &%X\n", fake_iconbar_icons,
          fake_iconbar_sprite, fake_iconbar_flags);
  check (stage == 1 && window_after_select, "Select on the icon opens L3afpad's window, drawn");
  check (main_centred == 1, "L3afpad's window opens in the middle of the screen");
  printf ("# window centre %d,%d OS units (screen 2560 x 2048)\n", main_cx, main_cy);
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
  check (font_dialog_seen && font_families > 0,
         "Options > Font opens GTK's font dialogue, listing the fonts");
  printf ("# font dialogue: Options at %d, Font at %d, %d rows\n", options_index, font_index,
          font_families);
  check (grip_pixel == 0x2010BD,
         "the window shows the desktop's size icon (sicon) in its bottom right corner");
  check (grip_drag_type == 2 && grip_presses_after == grip_presses_before,
         "Select on it starts the Wimp's size drag (DragBox 2), not a GTK click");
  printf ("# grip pixel &%06X, DragBox type %d, GTK presses %u -> %u\n", grip_pixel,
          (int) grip_drag_type, grip_presses_before, grip_presses_after);
  check (font_dialog_grip == 0, "a dialogue box (the font dialogue) has no size icon");
  check (grip_h_px > 0 && bar_bottom > 0 && bar_bottom <= win_h - grip_h_px,
         "the text's scroll bar stops above the size icon, as a Wimp scroll bar does");
  printf ("# scroll bar ends at %d, window %d high, grip %d\n", bar_bottom, win_h, grip_h_px);
  {
    int i;
    printf ("# icon bar menu:");
    for (i = 0; i < n_ib_items; i++)
      printf (" [%s]%s", ib_items[i], ib_shaded & (1 << i) ? "(shaded)" : "");
    printf (", Info -> &%X\n", (unsigned) ib_info_sub);
  }
  check (n_ib_items == 3 && strcmp (ib_items[0], "Info") == 0 && strcmp (ib_items[1], "Choices...") == 0 &&
         strcmp (ib_items[2], "Quit") == 0 && ib_shaded == 0,
         "Menu on the icon: Info, Choices..., Quit (as Reel's and !FFmpeg's)");
  check (ib_menu_x == ib_click_x - 64 && ib_menu_y == 96 + 3 * 44,
         "the menu opens 64 OS units left of the click, its top at 96 + 44 a line");
  printf ("# menu at %d,%d (click x %d)\n", ib_menu_x, ib_menu_y, ib_click_x);
  check (fake_iconbar_flags == 0x301A, "the icon: sprite, centred, click (&301A)");
  check (ib_info_is_window, "Info leads to the Info box (a Wimp window with a title)");
  check (choices_seen && choices_wrap_applied && strstr (choices_text, "\n1\n") != NULL,
         "Choices... opens the Choices window; Set applies word wrap and saves the Choices file");
  printf ("# Choices file: %s\n", g_strescape (choices_text, NULL));
  check (choices_centred == 1, "the Choices window opens in the middle of the screen");
  printf ("# Choices centre %d,%d OS units\n", choices_cx, choices_cy);
  check (hidden_after_close && empty_after_close,
         "the close icon empties and hides the window; L3afpad stays on the icon bar");
  check (dataopen_ack == 1 && strstr (text_after_dataopen, "Opened by DataOpen") && visible_after_dataopen,
         "a text file double-clicked in the Filer (DataOpen) is taken (DataLoadAck) and opened in the window");
  check (iconload_ack == 1 && strstr (text_after_iconload, "Dropped on the icon"),
         "a text file dropped on the icon is opened");
  check (prequit_ack == 1 && prequit_dialog && still_running_after_cancel,
         "PreQuit with unsaved text: a question box (Cancel / Discard); Cancel objects, it keeps running");
  printf ("# PreQuit asked: \"%s\"\n", prequit_text);
  check (print_box_seen && print_box_title_ok,
         "File > Print... opens the RISC OS Print box, titled with the printer's name");
  check (print_box_gadgets == 12 && print_button_at[0] == 250 && print_button_at[1] == 162 &&
         print_button_at[2] >= 57 && print_button_at[2] <= 66 && print_button_at[3] == 26,
         "the Print box has the Toolbox PrintDbox's gadgets (All/From/to, Copies, Scale %, "
         "Upright/Sideways, Draft, Save/Cancel/Print)");
  printf ("# Print box: %d of 12 gadgets, Print button at %d,%d %d x %d\n", print_box_gadgets,
          print_button_at[0], print_button_at[1], print_button_at[2], print_button_at[3]);
  check (printsave_seen == 1 && typeknown_seen == 1,
         "Print asks !Printers (PrintSave), and answers its PrintTypeOdd with PrintTypeKnown");
  check (fake_print_jobs == 1 && fake_print_pages >= 1 && fake_print_copies == 2 &&
         fake_print_ended == 1 && fake_print_aborted == 0,
         "it prints through the printer driver: a job, its pages, 2 copies, EndJob");
  printf ("# printed \"%s\": %d pages, %d copies, rectangle %d,%d,%d,%d, sprite %d x %d, "
          "scale %d/%d %d/%d\n", fake_print_title, fake_print_pages, fake_print_copies,
          fake_print_rect[0], fake_print_rect[1], fake_print_rect[2], fake_print_rect[3],
          fake_print_sprite_w, fake_print_sprite_h, fake_print_scale[0], fake_print_scale[2],
          fake_print_scale[1], fake_print_scale[3]);
  check (fake_print_polls_selected == 0,
         "the job is suspended between pages: no Wimp_Poll (no redraw) while it's selected");
  printf ("# Wimp_Polls with the job selected: %d\n", fake_print_polls_selected);
  check (fake_print_rect[2] == 1488 && fake_print_rect[3] == 2105 && fake_print_sprite_w == 2480 &&
         fake_print_sprite_h == 3508 && fake_print_scale[0] == 1488 && fake_print_scale[2] == 4960,
         "each page is the whole A4 paper (1488 x 2105 OS units) at the printer's 300 dpi");
  check (fake_print_ink > 1000 && fake_print_ink_top > 100 && fake_print_ink_top < 400 &&
         fake_print_ink_left > 180 && fake_print_ink_left < 300,
         "the text is on the page, inside L3afpad's margins (20 mm left, its header in the top margin)");
  printf ("# ink: %d pixels, first row %d, first column %d\n", fake_print_ink, fake_print_ink_top,
          fake_print_ink_left);
  check (preview_seen && g_str_has_prefix (preview_label, "Page 1 of ") && preview_back_off &&
         preview_print_shown,
         "File > Print Preview shows the pages in a window (Page 1 of n, Back, Next, Print)");
  printf ("# preview: \"%s\"\n", preview_label);
  check (quit_chosen && !forced_quit && fake_reports == reports_before + 1,
         "Quit from the icon bar menu with unsaved text asks; Discard quits");
  check (strstr (logbuf, "CRITICAL") == NULL && strstr (logbuf, "WARNING") == NULL,
         "no GTK warnings or criticals in the log");

  printf ("# log:\n");
  for (line = strtok (logbuf, "\n"); line; line = strtok (NULL, "\n"))
    printf ("#   %s\n", line);
  printf ("1..%d\n# l3afpad-fake: %d checks, %d failed\n", n_tests, n_tests, n_failed);
  return n_failed;
}
