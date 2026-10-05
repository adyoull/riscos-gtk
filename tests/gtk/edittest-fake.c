/*
 * edittest-fake.c - text fields on the fake RISC OS: their menu, the
 * clipboard shared with other tasks, and dragging text (within the
 * program, out to another task, and in from one). Runs tests/gtk/themetest.c
 * on tests/fake/mesa-egl/fake_riscos.c under qemu-arm-aligntrap; the
 * "other task" (0x5B00) is this file, answering the program's Wimp
 * messages as the PRMs' data transfer protocol says.
 *
 *   edittest-fake APPDIR OUTDIR
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include "themetest.h"
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define OTHER_TASK 0x5B00
#define FOREIGN    0x5551        /* another task's window, left of ours */
#define LATIN1_TEXT "Caf\xe9 R\xe9sum\xe9"     /* 11 bytes, Latin-1 */
#define UTF8_TEXT   "Caf\xc3\xa9 R\xc3\xa9sum\xc3\xa9"

static int n_tests, n_failed;
static int stage, step, waits, main_handle, opened_seen;
static const char *outdir;

/* the entry's menu, decoded */
static char menu_text[16][64];
static int menu_flags[16], menu_icon[16], menu_n, menu_seen;

/* messages */
static int claim_seen, request_ref, export_datasave, export_done, export_dest;
static char export_got[256];
static int export_len;
static char rambuf[64];
static int paste_request_seen, paste_ds_ref, paste_fetch_seen;
static int drag_datasave, drag_fields[4], drag_dest, drag_icon, drag_done, drag_ds_ref;
static char drag_got[64];
static int drag_len, dragbox_seen, drag_icon_windows;
static char ptr_seen[4][16];
static int ptr_seen_shape[4], ptr_seen_hot[4][2], ptr_seen_flags[4];
static int hand_fill = -1, hand_outline = -1, caret_bar, caret_stem;
static int drop_ds_ref, drop_fetch_seen;
static char text_after_paste[128], empty_after_drag[128], writable_after_drag[128], empty_after_drop[128];
static char text_after_dragout[128];

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
rect (GtkWidget *w, int *x, int *y, int *wd, int *ht)
{
  GtkAllocation a;
  gtk_widget_get_allocation (w, &a);
  gtk_widget_translate_coordinates (w, tt_window, 0, 0, x, y);
  *wd = a.width;
  *ht = a.height;
}

static void
menu_click (GtkWidget *w)
{
  int x, y, wd, ht;
  rect (w, &x, &y, &wd, &ht);
  add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);
  add (6, x + wd / 2, y + ht / 2, 2);      /* the Menu button */
  add (FAKE_RELEASE, 0, 0, 0);
  nulls (10);
}

/* Select pressed on from's middle, dragged in steps to pixel tx, ty (of
   the window), released there */
static void
drag (GtkWidget *from, int tx, int ty)
{
  int x, y, wd, ht, i, sx, sy;
  rect (from, &x, &y, &wd, &ht);
  sx = x + 24;                 /* on the (selected) text */
  sy = y + ht / 2;
  add (FAKE_MOVE, sx, sy, 0);
  add (6, sx, sy, 4);
  nulls (3);
  for (i = 1; i <= 8; i++)
    {
      add (FAKE_MOVE, sx + (tx - sx) * i / 8, sy + (ty - sy) * i / 8, 0);
      nulls (2);
    }
  nulls (4);
  add (FAKE_RELEASE, 0, 0, 0);
  nulls (12);
}

static void
record_menu (void)
{
  const int *m = fake_menu, *it = m + 7;
  int i;

  menu_n = 0;
  for (i = 0; i < 16; i++, it += 6)
    {
      if (it[2] & 0x100)
        g_strlcpy (menu_text[i], (const char *) (long) it[3], sizeof menu_text[i]);
      else
        {
          memcpy (menu_text[i], it + 3, 12);
          menu_text[i][12] = 0;
        }
      menu_flags[i] = it[0];
      menu_icon[i] = it[2];
      menu_n = i + 1;
      if (it[0] & 0x80)
        break;
    }
}

static int
selected_all (GtkWidget *e)
{
  int a, b;
  return gtk_editable_get_selection_bounds (GTK_EDITABLE (e), &a, &b) &&
         a == 0 && b == (int) g_utf8_strlen (gtk_entry_get_text (GTK_ENTRY (e)), -1);
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

/* The pointer sprites, pictured in the fake desktop's pointer colours
   (0 transparent = grey here) */
extern const int *_gdk_riscos_pointer_area (void);

static void
save_pointers (const char *path)
{
  const int *area = _gdk_riscos_pointer_area ();
  const char *p = (const char *) area + area[2];
  int i, x, y, total_w = 0;
  FILE *f;
  static unsigned char img[40][400][3];

  memset (img, 0x80, sizeof img);
  for (i = 0; i < area[1]; i++, p += ((const int *) p)[0])
    {
      const int *spr = (const int *) p;
      int words = spr[4] + 1, h = spr[5] + 1, w = words * 16;
      const unsigned int *im = (const unsigned int *) (p + spr[8]);
      for (y = 0; y < h && y < 40; y++)
        for (x = 0; x < w && total_w + x < 400; x++)
          {
            int c = (im[y * words + x / 16] >> ((x % 16) * 2)) & 3;
            unsigned int v = (unsigned int) fake_ptr_palette[c];   /* &BBGGRRxx */
            if (c == 0)
              continue;
            img[y][total_w + x][0] = v >> 8;
            img[y][total_w + x][1] = v >> 16;
            img[y][total_w + x][2] = v >> 24;
          }
      total_w += w + 4;
    }
  if (!(f = fopen (path, "wb")))
    return;
  fprintf (f, "P6\n400 40\n255\n");
  fwrite (img, 1, sizeof img, f);
  fclose (f);
}

/* The caret in the Writable field (it has the focus, no selection): the
   widest run of red (Wimp colour 11) pixels in a row, the tallest in a
   column */
static void
measure_caret (void)
{
  int x, y, wd, ht, i, x0 = 0, y0 = 0, px, py;

  rect (tt_entry, &x, &y, &wd, &ht);
  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle == main_handle)
      {
        x0 = (fake_windows[i].x0 >> 1) + x;
        y0 = 1024 - (fake_windows[i].y1 >> 1) + y;
      }
  for (py = y0; py < y0 + ht; py++)
    {
      int run = 0;
      for (px = x0; px < x0 + wd; px++)
        {
          run = (fake_screen_pixel (px, py) & 0xFFFFFF) == 0x0000DD ? run + 1 : 0;
          caret_bar = MAX (caret_bar, run);
        }
    }
  for (px = x0; px < x0 + wd; px++)
    {
      int run = 0;
      for (py = y0; py < y0 + ht; py++)
        {
          run = (fake_screen_pixel (px, py) & 0xFFFFFF) == 0x0000DD ? run + 1 : 0;
          caret_stem = MAX (caret_stem, run);
        }
    }
}

/* ---- the other task ---- */

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

static void
message_hook (int reason, int *b, int dest, int icon)
{
  int q[64];

  memset (q, 0, sizeof q);
  switch (b[4])
    {
    case 0xF:                         /* ClaimEntity */
      if (dest == 0 && (b[5] & 4))
        claim_seen++;
      break;
    case 0x10:                        /* DataRequest: we paste */
      if (dest == 0 && reason == 18 && (b[9] & 4) && b[10] == 0xFFF && b[11] == -1)
        {
          paste_request_seen++;
          memcpy (&q[5], &b[5], 16);
          q[9] = 11;
          q[10] = 0xFFF;
          strcpy ((char *) &q[11], "TextFile");
          queue (17, q, 1, b[2], 44 + 9);
          paste_ds_ref = q[2];
        }
      break;
    case 1:                           /* DataSave from the program */
      if (request_ref && b[3] == request_ref && dest == OTHER_TASK)
        {
          export_datasave = b[10] == 0xFFF && b[9] == 16 && strcmp ((char *) &b[11], "TextFile") == 0;
          export_dest = dest;
          q[5] = (int) (long) rambuf;
          q[6] = 8;                   /* small: two fills, then the rest */
          queue (18, q, 6, b[2], 28);
        }
      else if (dest == FOREIGN && b[3] == 0)
        {
          drag_datasave = b[10] == 0xFFF;
          drag_dest = dest;
          drag_icon = icon;
          memcpy (drag_fields, &b[5], sizeof drag_fields);
          q[5] = (int) (long) rambuf;
          q[6] = 64;
          queue (18, q, 6, b[2], 28);
          drag_ds_ref = b[2];
        }
      break;
    case 7:                           /* RAMTransmit from the program */
      if (export_datasave && !export_done)
        {
          memcpy (export_got + export_len, rambuf, b[6]);
          export_len += b[6];
          if (reason == 18 && b[6] == 8)
            {
              q[5] = (int) (long) rambuf;
              q[6] = 8;
              queue (18, q, 6, b[2], 28);
            }
          else
            export_done = reason == 17;
        }
      else if (drag_datasave && !drag_done)
        {
          memcpy (drag_got, rambuf, b[6]);
          drag_len = b[6];
          drag_done = reason == 17;
        }
      break;
    case 6:                           /* RAMFetch from the program */
      if (reason == 18 && paste_ds_ref && b[3] == paste_ds_ref && !paste_fetch_seen)
        {
          paste_fetch_seen = b[6] >= 11;
          memcpy ((void *) (long) b[5], LATIN1_TEXT, 11);
          q[5] = b[5];
          q[6] = 11;
          queue (17, q, 7, b[2], 28);
        }
      else if (reason == 18 && drop_ds_ref && b[3] == drop_ds_ref && !drop_fetch_seen)
        {
          drop_fetch_seen = b[6] >= 6;
          memcpy ((void *) (long) b[5], "Hello ", 6);
          q[5] = b[5];
          q[6] = 6;
          queue (17, q, 7, b[2], 28);
        }
      break;
    }
}

static int dbg_events;

static void
dbg_handler (GdkEvent *e, gpointer d)
{
  if (dbg_events)
    printf ("# gdk event %d window %p\n", e->type, (void *) e->any.window);
  gtk_main_do_event (e);
}

static gboolean
dbg_idle (gpointer d)
{
  if (dbg_events)
    printf ("# idle 126 ran\n");
  return dbg_events != 0;
}

static void
dbg_paint (GdkFrameClock *c, gpointer name)
{
  if (dbg_events)
    printf ("# paint %s\n", (const char *) name);
}

static void
dbg_drag_begin (GtkWidget *w, GdkDragContext *c, gpointer d)
{
  printf ("# drag-begin\n");
  dbg_events = 1;
  gdk_event_handler_set (dbg_handler, NULL, NULL);
  g_idle_add_full (GDK_PRIORITY_REDRAW + 6, dbg_idle, NULL, NULL);
  g_signal_connect (gtk_widget_get_frame_clock (tt_window), "paint", G_CALLBACK (dbg_paint), "main");
  {
    GList *l, *tops = gtk_window_list_toplevels ();
    for (l = tops; l; l = l->next)
      if (l->data != tt_window && gtk_widget_get_frame_clock (l->data))
        g_signal_connect (gtk_widget_get_frame_clock (l->data), "paint", G_CALLBACK (dbg_paint),
                          (gpointer) G_OBJECT_TYPE_NAME (l->data));
    g_list_free (tops);
  }
}

static gboolean
dbg_motion (GtkWidget *w, GdkEvent *e, gpointer d)
{
  printf ("# event %d at %g,%g state %x button %d\n", e->type, e->button.x, e->button.y,
          e->type == GDK_MOTION_NOTIFY ? e->motion.state : e->button.state,
          e->type == GDK_MOTION_NOTIFY ? 0 : (int) e->button.button);
  return FALSE;
}

/* ---- the steps ---- */

/* Step n has been scripted; is it done (so step n + 1 can start)? */
static gboolean
step_ready (void)
{
  switch (step)
    {
    case 1: case 3: case 6: case 8: case 10:
      return menu_seen && fake_menu != NULL;         /* the entry's menu is open */
    case 2: case 7: case 11:
      return fake_menu == NULL && selected_all (tt_entry);
    case 4:
      return fake_menu == NULL && claim_seen;
    case 5:
      return export_done;
    case 9:
      return strcmp (gtk_entry_get_text (GTK_ENTRY (tt_entry)), UTF8_TEXT) == 0;
    case 12:
      return drag_done;
    default:
      return waits > 10;
    }
}

static void
next_step (void)
{
  int x, y, wd, ht, q[64];

  memset (q, 0, sizeof q);
  step++;
  menu_seen = 0;
  switch (step)
    {
    case 1:      /* Menu on the Writable field */
      menu_click (tt_entry);
      break;
    case 2:      /* its menu: Select All */
      record_menu ();
      add (9, 4, -1, 0);
      nulls (8);
      break;
    case 3:      /* Menu again: Copy */
      menu_click (tt_entry);
      break;
    case 4:
      add (9, 1, -1, 0);
      nulls (8);
      break;
    case 5:      /* another task pastes: Message_DataRequest */
      q[5] = 0x6000; q[6] = -1; q[9] = 4; q[10] = 0xFFF; q[11] = -1;
      queue (18, q, 0x10, 0, 48);
      request_ref = q[2];
      nulls (10);
      break;
    case 6:      /* it takes the clipboard; Menu, Select All, Menu, Paste */
      q[5] = 4;
      queue (17, q, 0xF, 0, 24);
      nulls (6);
      menu_click (tt_entry);
      break;
    case 7:
      add (9, 4, -1, 0);
      nulls (8);
      break;
    case 8:
      menu_click (tt_entry);
      break;
    case 9:
      record_menu ();          /* Paste is offered */
      add (9, 2, -1, 0);
      nulls (10);
      break;
    case 10:     /* Menu, Select All, then drag the text onto another task's window */
      g_strlcpy (text_after_paste, gtk_entry_get_text (GTK_ENTRY (tt_entry)), sizeof text_after_paste);
      measure_caret ();
      menu_click (tt_entry);
      break;
    case 11:
      add (9, 4, -1, 0);
      nulls (8);
      break;
    case 12:
      if (getenv ("ET_DEBUG"))
        {
          int a, b;
          printf ("# entry focus %d, selection %d (%d-%d)\n", gtk_widget_has_focus (tt_entry),
                  gtk_editable_get_selection_bounds (GTK_EDITABLE (tt_entry), &a, &b), a, b);
          g_signal_connect (tt_entry, "drag-begin", G_CALLBACK (dbg_drag_begin), NULL);
          g_signal_connect (tt_entry, "motion-notify-event", G_CALLBACK (dbg_motion), NULL);
          g_signal_connect (tt_entry, "button-press-event", G_CALLBACK (dbg_motion), NULL);
        }
      drag (tt_entry, -60, 320);   /* over FOREIGN */
      break;
    case 13:     /* drag the field's text into the Empty field */
      g_strlcpy (text_after_dragout, gtk_entry_get_text (GTK_ENTRY (tt_entry)), sizeof text_after_dragout);
      gtk_editable_select_region (GTK_EDITABLE (tt_entry), 0, -1);
      rect (tt_entry_empty, &x, &y, &wd, &ht);
      drag (tt_entry, x + wd / 2, y + ht / 2);
      break;
    case 14:     /* another task saves text into the Empty field, at its start */
      g_strlcpy (empty_after_drag, gtk_entry_get_text (GTK_ENTRY (tt_entry_empty)), sizeof empty_after_drag);
      g_strlcpy (writable_after_drag, gtk_entry_get_text (GTK_ENTRY (tt_entry)), sizeof writable_after_drag);
      rect (tt_entry_empty, &x, &y, &wd, &ht);
      {
        int i;
        for (i = 0; i < FAKE_MAX_WINDOWS; i++)
          if (fake_windows[i].handle == main_handle)
            {
              q[5] = main_handle;
              q[6] = -1;
              q[7] = fake_windows[i].x0 + (x + 4) * 2;
              q[8] = fake_windows[i].y1 - (y + ht / 2) * 2;
            }
      }
      q[9] = 6;
      q[10] = 0xFFF;
      strcpy ((char *) &q[11], "Selection");
      queue (17, q, 1, 0, 44 + 10);
      drop_ds_ref = q[2];
      nulls (20);
      break;
    case 15:
      g_strlcpy (empty_after_drop, gtk_entry_get_text (GTK_ENTRY (tt_entry_empty)), sizeof empty_after_drop);
      nulls (6);
      break;
    default:
      return;  /* the script ends: the window closes */
    }
  printf ("# step %d\n", step);
  fflush (stdout);
}

static void
hook (int reason)
{
  int i;

  if (fake_dragbox_type == 5)
    {
      dragbox_seen = 1;
      /* any window of ours opened while dragging (GTK's drag icon would
         sit under the pointer and take the drop) */
      for (i = 0; i < FAKE_MAX_WINDOWS; i++)
        if (fake_windows[i].handle && fake_windows[i].handle != main_handle &&
            fake_windows[i].handle != FOREIGN && fake_windows[i].x1 > 0 &&
            fake_pointer[0] >= fake_windows[i].x0 && fake_pointer[0] < fake_windows[i].x1 &&
            fake_pointer[1] >= fake_windows[i].y0 && fake_pointer[1] < fake_windows[i].y1)
          drag_icon_windows++;
    }
  if (fake_menu && fake_menus_opened != opened_seen)
    {
      opened_seen = fake_menus_opened;
      menu_seen = 1;
    }
  if (stage == 0 && tt_window && gtk_widget_get_mapped (tt_window) && fake_plots > 0 &&
      gtk_widget_get_allocated_width (tt_entry) > 1)
    {
      stage = 1;
      for (i = 0; i < FAKE_MAX_WINDOWS; i++)
        if (fake_windows[i].handle)
          main_handle = fake_windows[i].handle;
      fake_target_window = main_handle;
      /* another task's window, left of ours (pixels 0-110, 300-600) */
      fake_open_window (FOREIGN, 0, 2048 - 1200, 220, 2048 - 600, 0, 0);
      add (12, 0, 0, 0);          /* Gain_Caret */
      nulls (10);
      /* pointer shapes: over the field, a plain part, a link button */
      {
        int x, y, wd, ht;
        GtkWidget *link = find_type (tt_window, GTK_TYPE_LINK_BUTTON);
        rect (tt_entry, &x, &y, &wd, &ht);
        add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);
        add (5, 0, 0, 0);                     /* Pointer_Entering_Window */
        nulls (6);
        add (FAKE_NULL, 0, 0, 201);           /* marker: record the pointer */
        add (FAKE_MOVE, 4, 4, 0);             /* the window's edge: no widget's cursor */
        nulls (6);
        add (FAKE_NULL, 0, 0, 202);
        if (link)
          {
            rect (link, &x, &y, &wd, &ht);
            add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);
            nulls (6);
          }
        add (FAKE_NULL, 0, 0, 203);
        add (FAKE_MOVE, -60, 320, 0);         /* off our window, over another task's */
        add (4, 0, 0, 0);                     /* Pointer_Leaving_Window */
        nulls (4);
        add (FAKE_NULL, 0, 0, 204);
        nulls (4);
      }
      return;
    }
  {
    int pos = fake_wimp_script_pos ();
    if (pos >= 0 && fake_wimp_script[pos][0] == FAKE_NULL && fake_wimp_script[pos][3] >= 201 &&
        fake_wimp_script[pos][3] <= 204)
      {
        int k = fake_wimp_script[pos][3] - 201;
        g_strlcpy (ptr_seen[k], fake_ptr_name, sizeof ptr_seen[k]);
        ptr_seen_shape[k] = fake_ptr_shape;
        ptr_seen_flags[k] = fake_ptr_flags;
        if (k == 2 && fake_ptr_sprite)
          {
            /* the hand's colours: its middle is filled, its first pixel
               from the left on that row is outline */
            const int *spr = fake_ptr_sprite;
            int words = spr[4] + 1, y = (spr[5] + 1) * 2 / 3, x;
            const unsigned int *im = (const unsigned int *) ((const char *) spr + spr[8]);
            for (x = 0; x < words * 16; x++)
              {
                int c = (im[y * words + x / 16] >> ((x % 16) * 2)) & 3;
                if (c && hand_outline < 0)
                  hand_outline = c;
                else if (c && hand_outline >= 0 && c != hand_outline)
                  {
                    hand_fill = c;
                    break;
                  }
              }
          }
        ptr_seen_hot[k][0] = fake_ptr_hx;
        ptr_seen_hot[k][1] = fake_ptr_hy;
      }
  }
  if (stage == 1 && fake_wimp_script_pos () >= fake_wimp_script_len - 3)
    {
      if (step >= 15)
        return;
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
  char logpath[600], logbuf[16384] = "", *line;
  char *targv[4];
  int saved_out = dup (1), saved_err = dup (2);
  FILE *lf;

  if (argc < 3)
    return 2;
  outdir = argv[2];
  fake_set_screen (1280, 1024, 0, 5);
  fake_sprite_plot_converts = 0;
  fake_wimp_nulls = 400;
  fake_wimp_hook = hook;
  fake_message_hook = message_hook;
  g_snprintf (logpath, sizeof logpath, "%s/GTKEditTest.Log", outdir);
  targv[0] = argv[0]; targv[1] = argv[1]; targv[2] = logpath; targv[3] = NULL;
  g_setenv ("RISCOS_GTK_MENU_DEBUG", "1", TRUE);
  g_setenv ("RISCOS_GTK_DEBUG", "1", TRUE);
  themetest_main (3, targv);
  fflush (stdout); fflush (stderr);
  dup2 (saved_out, 1); dup2 (saved_err, 2);
  if ((lf = fopen (logpath, "r")))
    {
      size_t n = fread (logbuf, 1, sizeof logbuf - 1, lf);
      logbuf[n] = 0;
      fclose (lf);
    }

  check (stage == 1, "theme test window shown and drawn");
  check (strcmp (ptr_seen[0], "ptr_write") == 0 && ptr_seen_shape[0] == 2 &&
         (ptr_seen_flags[0] & 0x20) && ptr_seen_hot[0][0] == 4 && ptr_seen_hot[0][1] == 10,
         "pointer over a text field: the Wimp's ptr_write (shape 2, desktop colours, active point in its middle)");
  check (strcmp (ptr_seen[1], "ptr_default") == 0 && ptr_seen_shape[1] == 1,
         "pointer over a plain part of the window: the normal pointer");
  check (strcmp (ptr_seen[2], "gtk_hand") == 0 && (ptr_seen_flags[2] & 0x20),
         "pointer over a link: the hand, keeping the desktop's pointer colours");
  check (hand_fill == 1 && hand_outline == 2,
         "our pointers use the desktop's pointer colours: the lighter (1) fills, the darker (2) outlines");
  check (caret_bar >= 5 && caret_stem >= 8,
         "the text caret is the RISC OS caret: red, with bars across its top and bottom");
  printf ("# caret: widest red row %d px, tallest red column %d px\n", caret_bar, caret_stem);
  check (strcmp (ptr_seen[3], "ptr_default") == 0 && ptr_seen_shape[3] == 1,
         "pointer over another task's window: the normal pointer");
  printf ("# pointers seen: %s/%d (%d,%d), %s/%d, %s/%d, %s/%d; %d pointer changes\n",
          ptr_seen[0], ptr_seen_shape[0], ptr_seen_hot[0][0], ptr_seen_hot[0][1],
          ptr_seen[1], ptr_seen_shape[1], ptr_seen[2], ptr_seen_shape[2],
          ptr_seen[3], ptr_seen_shape[3], fake_ptr_sets);
  {
    char path[600];
    g_snprintf (path, sizeof path, "%s/pointers.ppm", outdir);
    save_pointers (path);
  }
  check (menu_n >= 5 && strcmp (menu_text[0], "Cut") == 0 && strcmp (menu_text[1], "Copy") == 0 &&
         strcmp (menu_text[2], "Paste") == 0 && strncmp (menu_text[4], "Select", 6) == 0,
         "Menu on a text field opens its Cut/Copy/Paste/Delete/Select All menu");
  printf ("# entry menu:");
  {
    int i;
    for (i = 0; i < menu_n; i++)
      printf (" [%s%s]", menu_text[i], menu_icon[i] & (1 << 22) ? " shaded" : "");
    printf ("\n");
  }
  check (menu_n >= 3 && !(menu_icon[2] & (1 << 22)),
         "Paste is offered when another task may have the clipboard");
  check (claim_seen >= 1, "Copy claims the RISC OS clipboard (Message_ClaimEntity, flags 4)");
  check (export_datasave && export_dest == OTHER_TASK,
         "another task's DataRequest gets a DataSave of text (&FFF, 16 bytes)");
  check (export_done && export_len == 16 && memcmp (export_got, "Accounts2026/csv", 16) == 0,
         "it gets the copied text by RAM transfer, in pieces (RAMFetch/RAMTransmit)");
  printf ("# exported %d bytes: \"%.*s\"\n", export_len, export_len, export_got);
  check (paste_request_seen >= 1,
         "Paste after another task took the clipboard broadcasts Message_DataRequest");
  check (paste_fetch_seen, "the owner's DataSave is answered with RAMFetch");
  check (strcmp (text_after_paste, UTF8_TEXT) == 0,
         "Paste puts the other task's Latin-1 text in the field (as UTF-8)");
  printf ("# after Paste: \"%s\"\n", text_after_paste);
  check (dragbox_seen, "dragging text shows the Wimp's drag box (Wimp_DragBox type 5)");
  check (drag_icon_windows == 0, "no window under the pointer while dragging (GTK's drag icon isn't opened)");
  check (drag_datasave && drag_dest == FOREIGN && drag_fields[0] == FOREIGN,
         "text dragged onto another task's window is saved to it (DataSave to that window)");
  check (drag_done && drag_len == 11 && memcmp (drag_got, LATIN1_TEXT, 11) == 0,
         "and it gets the text in Latin-1 by RAM transfer");
  check (strcmp (text_after_dragout, UTF8_TEXT) == 0, "a drag out copies: the field keeps its text");
  check (strcmp (empty_after_drag, UTF8_TEXT) == 0,
         "text dragged from one field to another arrives there");
  printf ("# Empty field after the drag \"%s\", Writable \"%s\"\n", empty_after_drag, writable_after_drag);
  check (drop_fetch_seen && strstr (empty_after_drop, "Hello ") != NULL,
         "text saved into the window by another task (DataSave) is dropped into the field");
  printf ("# Empty field after the other task's drop: \"%s\"\n", empty_after_drop);
  check (strstr (logbuf, "window closed") != NULL, "the window closes");
  check (strstr (logbuf, "CRITICAL") == NULL && strstr (logbuf, "WARNING") == NULL &&
         strstr (logbuf, "Gtk-") == NULL && strstr (logbuf, "Gdk-") == NULL,
         "no GTK warnings or criticals in the log");

  printf ("# log:\n");
  for (line = strtok (logbuf, "\n"); line; line = strtok (NULL, "\n"))
    printf ("#   %s\n", line);
  printf ("1..%d\n# edittest-fake: %d checks, %d failed\n", n_tests, n_tests, n_failed);
  return n_failed;
}
