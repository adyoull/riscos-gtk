/*
 * damage-fake.c - only what GTK repainted is copied into the window's
 * sprite and plotted (gdkwindow-riscos.c end_paint, gdkriscos-server.c
 * _gdk_riscos_server_window_update), and the screen is still right:
 * after typing, and after updates made while the window was closed, the
 * screen is the same as when the whole window is drawn again.
 *   damage-fake APPDIR [trgb]
 * Run in the desktop's usual TBGR mode (cairo's pixels copied with red and
 * blue swapped), in a TRGB mode (cairo draws in the sprite), and with
 * FAKE_EIG=0 (GDK scale 2). MIT licence.
 */
#include <gtk/gtk.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GtkWidget *win, *entry;
static int stage, waits, drawn, failed, checks, typed;
static int handle, wx, wy, ww, wh;
static unsigned int *snap;
static int last_area, window_area, typed_ok, typed_rects;
static int same_after_typing = -1, same_after_closed = -1, trgb;
static int gtk_after_typing = -1, gtk_after_closed = -1;

static void
check (int ok, const char *what)
{
  checks++;
  if (!ok)
    failed++;
  printf ("%s %d - %s\n", ok ? "ok" : "not ok", checks, what);
}

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
  for (i = 0; i < 4; i++)
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

/* the window's visible area on the fake screen, GDK pixels */
static void
find_window (void)
{
  int i, best = -1;
  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle && (best < 0 || fake_windows[i].stack > fake_windows[best].stack))
      best = i;
  if (best < 0)
    return;
  handle = fake_windows[best].handle;
  wx = fake_windows[best].x0 >> 1;
  wy = 1024 - (fake_windows[best].y1 >> 1);
  ww = (fake_windows[best].x1 - fake_windows[best].x0) >> 1;
  wh = (fake_windows[best].y1 - fake_windows[best].y0) >> 1;
}

static void
take_snap (void)
{
  int x, y;
  find_window ();
  g_free (snap);
  snap = g_new (unsigned int, ww * wh);
  for (y = 0; y < wh; y++)
    for (x = 0; x < ww; x++)
      snap[y * ww + x] = fake_screen_pixel (wx + x, wy + y) & 0xFFFFFF;
}

static int
same_as_snap (void)
{
  int x, y, diff = 0;
  for (y = 0; y < wh; y++)
    for (x = 0; x < ww; x++)
      if ((fake_screen_pixel (wx + x, wy + y) & 0xFFFFFF) != snap[y * ww + x])
        diff++;
  if (diff)
    printf ("# %d pixels differ\n", diff);
  return diff == 0;
}

/* The window as GTK draws it now, compared with the screen (not the
   bottom right corner, where the backend paints the resize grip). The
   screen's pixels are &00BBGGRR in a TBGR mode, &00RRGGBB in TRGB. */
static int
same_as_gtk (void)
{
  int scale = getenv ("FAKE_EIG") && atoi (getenv ("FAKE_EIG")) == 0 ? 2 : 1;
  cairo_surface_t *s;
  cairo_t *cr;
  const guint8 *data;
  int x, y, stride, diff = 0;

  find_window ();
  s = cairo_image_surface_create (CAIRO_FORMAT_RGB24, ww * scale, wh * scale);
  cairo_surface_set_device_scale (s, scale, scale);
  cr = cairo_create (s);
  gtk_widget_draw (win, cr);
  cairo_destroy (cr);
  cairo_surface_flush (s);
  data = cairo_image_surface_get_data (s);
  stride = cairo_image_surface_get_stride (s);
  for (y = 0; y < wh; y++)
    for (x = 0; x < ww; x++)
      {
        guint32 p = ((const guint32 *) (data + y * scale * stride))[x * scale] & 0xFFFFFF;
        guint32 q = fake_screen_pixel (wx + x, wy + y) & 0xFFFFFF;
        if (x >= ww - 48 && y >= wh - 48)
          continue;
        if (!trgb)
          q = (q & 0xFF00) | ((q >> 16) & 0xFF) | ((q & 0xFF) << 16);
        if (p != q)
          diff++;
      }
  cairo_surface_destroy (s);
  if (diff)
    printf ("# %d pixels not as GTK draws them\n", diff);
  return diff == 0;
}

static void
hook (int reason)
{
  GtkAllocation a;

  if (fake_wimp_script_pos () < fake_wimp_script_len - 1)
    return;
  switch (stage)
    {
    case 0:
      if (win && gtk_widget_get_mapped (win) && drawn > 0 && fake_plots > 0)
        {
          stage = 1;
          /* events to the window opened (the newest made may be another) */
          find_window ();
          fake_target_window = handle;
          gtk_widget_get_allocation (entry, &a);
          add (FAKE_MOVE, a.x + a.width - 10, a.y + a.height / 2, 0);
          add (6, a.x + a.width - 10, a.y + a.height / 2, 4);
          add (FAKE_RELEASE, 0, 0, 0);
          settle ();
        }
      else if (++waits > 400)
        {
          stage = 99;
          gtk_main_quit ();
        }
      else
        add (FAKE_NULL, 0, 0, 0);
      return;
    case 1:                        /* type, one key at a time */
      if (typed > 0)
        {
          int area = ((fake_update_rect[2] - fake_update_rect[0]) >> 1) *
                     ((fake_update_rect[3] - fake_update_rect[1]) >> 1);
          find_window ();
          window_area = ww * wh;
          last_area = area;
          typed_rects++;
          if (area > 0 && area * 4 < window_area)
            typed_ok++;
        }
      if (typed < 6)
        {
          add (8, 'a' + typed++, 0, 0);
          settle ();
          return;
        }
      gtk_after_typing = same_as_gtk ();
      take_snap ();
      stage = 2;
      gtk_widget_queue_draw (win);
      settle ();
      return;
    case 2:
      same_after_typing = same_as_snap ();
      /* closed: text changed while it can't be seen, then opened again */
      stage = 3;
      gtk_widget_hide (win);
      settle ();
      return;
    case 3:
      gtk_entry_set_text (GTK_ENTRY (entry), "Changed while closed");
      gtk_widget_show (win);
      stage = 4;
      settle ();
      settle ();
      return;
    case 4:                        /* back in the field (opened again: maybe a new handle) */
      find_window ();
      fake_target_window = handle;
      gtk_widget_get_allocation (entry, &a);
      add (FAKE_MOVE, a.x + a.width - 10, a.y + a.height / 2, 0);
      add (6, a.x + a.width - 10, a.y + a.height / 2, 4);
      add (FAKE_RELEASE, 0, 0, 0);
      stage = 5;
      settle ();
      return;
    case 5:
      gtk_after_closed = same_as_gtk ();
      take_snap ();
      gtk_widget_queue_draw (win);
      stage = 6;
      settle ();
      return;
    case 6:
      same_after_closed = same_as_snap ();
      stage = 7;
      gtk_main_quit ();
      add (FAKE_NULL, 0, 0, 0);    /* not closed before gtk_main returns */
      return;
    default:
      add (FAKE_NULL, 0, 0, 0);
      return;
    }
}

int
main (int argc, char **argv)
{
  GtkWidget *box;

  if (argc < 2)
    return 2;
  g_setenv ("FONTCONFIG_FILE", g_build_filename (argv[1], "fonts", "fonts.conf", NULL), TRUE);
  g_setenv ("XDG_DATA_DIRS", g_build_filename (argv[1], "share", NULL), TRUE);
  trgb = argc > 2 && strcmp (argv[2], "trgb") == 0;
  fake_set_screen (1280, 1024, trgb, 5);
  fake_wimp_nulls = 40;
  fake_wimp_hook = hook;
  gtk_init (&argc, &argv);
  g_object_set (gtk_settings_get_default (), "gtk-cursor-blink", FALSE, NULL);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Damage");
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 300);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), "Some text ");
  gtk_container_add (GTK_CONTAINER (box), entry);
  gtk_container_add (GTK_CONTAINER (box), gtk_label_new ("A label under the field, and the rest of the window"));
  gtk_container_add (GTK_CONTAINER (box), gtk_button_new_with_label ("A button"));
  gtk_container_add (GTK_CONTAINER (win), box);
  g_signal_connect (win, "draw", G_CALLBACK (on_draw), NULL);
  gtk_widget_show_all (win);
  gtk_widget_grab_focus (entry);
  gtk_main ();

  printf ("# %s, %s: entry \"%s\"; the last key's update %d of %d pixels\n",
          argc > 2 ? argv[2] : "tbgr", getenv ("FAKE_EIG") ? "EIG 0" : "EIG 1",
          gtk_entry_get_text (GTK_ENTRY (entry)), last_area, window_area);
  check (stage == 7, "the window opens and the steps run");
  check (strncmp (gtk_entry_get_text (GTK_ENTRY (entry)), "Changed", 7) == 0 ||
         stage < 3, "the keys and the change reach the field");
  check (typed_rects > 0 && typed_ok == typed_rects,
         "a key typed updates only part of the window (under a quarter of it)");
  check (gtk_after_typing == 1, "after typing the screen shows what GTK draws (the field's new text)");
  check (same_after_typing == 1, "and is as when the whole window is drawn again");
  check (gtk_after_closed == 1, "after a change while closed, opened again, the screen shows what GTK draws");
  check (same_after_closed == 1, "and is as when the whole window is drawn again");
  printf ("1..%d\n# damage-fake: %d checks, %d failed\n", checks, checks, failed);
  return failed != 0;
}
