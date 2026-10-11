/*
 * mtpaint-fake.c - mtPaint (3.50.14, riscos-gtk's !mtPaint) on the fake
 * RISC OS:
 *   - Menu over an 8 x 8 grid of the main window (the canvas, toolbars,
 *     palette, status bar) and a 4 x 4 grid of its Settings Toolbar window
 *     gives the program's menu (gtk/riscos/gtkmenu-riscos.c);
 *   - File > Save As: the RISC OS save dialogue (patches/
 *     mtpaint-3.50.14-riscos.diff); its file icon dragged to a Filer
 *     window saves the picture there;
 *   - File > Open: a PNG dropped on it from the Filer is loaded.
 *   mtpaint-fake OUTDIR (OUTDIR/drop.png: a PNG for Open)
 * mtPaint's objects built for the test target with -DMT_RISCOS_UI (its
 * RISC OS dialogue and buttons), main.o's main renamed mtpaint_main;
 * build/build-linuxarm.sh links them with this from MTPAINT_LA.
 * Screenshots (OUTDIR/shot-*.ppm) are taken as it goes; MF_SHOTS=dir (as
 * TT_ICONS) puts real RISC OS sprites in the fake Wimp's pool for them. Prints TAP.
 * MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

extern int mtpaint_main (int argc, char **argv);
extern void file_selector (int action);
#define FOREIGN 0x5551
#define OTHER_TASK 0x5B00
static char save_ro[512], save_unix[512], load_ro[512], load_unix[512];
static int ds_seen, dl_seen, n_tests, n_failed;
static void check (int ok, const char *name)
{ n_tests++; if (!ok) n_failed++; printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name); fflush (stdout); }
static void queue (int reason, int *block, int action, int your_ref, int bytes)
{
  block[0] = (bytes + 3) & ~3; block[1] = OTHER_TASK; block[2] = fake_new_ref ();
  block[3] = your_ref; block[4] = action;
  fake_queue_message (reason, block);
}
static void message_hook (int reason, int *b, int dest, int icon)
{
  int q[64];
  memset (q, 0, sizeof q);
  if (b[4] == 1 && dest == FOREIGN && b[3] == 0)        /* DataSave from the save icon */
    {
      ds_seen++;
      memcpy (&q[5], &b[5], 16);
      q[9] = 100; q[10] = b[10];
      g_strlcpy ((char *) &q[11], save_ro, 200);
      queue (17, q, 2, b[2], 44 + (int) strlen (save_ro) + 1);
    }
  else if (b[4] == 3 && dest == OTHER_TASK && b[3] != 0)  /* DataLoad: saved */
    dl_seen++;
}
static GtkWidget *titled (const char *t)
{
  GList *l, *tops = gtk_window_list_toplevels (); GtkWidget *f = NULL;
  for (l = tops; l; l = l->next)
    if (gtk_widget_get_mapped (l->data) && g_strcmp0 (gtk_window_get_title (l->data), t) == 0) f = l->data;
  g_list_free (tops); return f;
}
static GtkWidget *find_type (GtkWidget *w, GType t)
{
  GtkWidget *f = NULL; GList *l, *k;
  if (G_TYPE_CHECK_INSTANCE_TYPE (w, t)) return w;
  if (!GTK_IS_CONTAINER (w)) return NULL;
  k = gtk_container_get_children (GTK_CONTAINER (w));
  for (l = k; l && !f; l = l->next) f = find_type (l->data, t);
  g_list_free (k); return f;
}
static gboolean do_save (gpointer d) { file_selector (2); return FALSE; }
static gboolean do_load (gpointer d) { file_selector (1); return FALSE; }
static int stage, waits, main_handle, gx, gy, grid_results[20][20], last_menus;
static GtkWidget *mainw, *toolw;
static const char *outdir;

static void add (int reason, int a, int b, int c)
{ int *e = fake_wimp_script[fake_wimp_script_len++]; e[0]=reason; e[1]=a; e[2]=b; e[3]=c; }
static void nulls (int n) { while (n--) add (FAKE_NULL, 0, 0, 0); }
static void settle (void) { int i; for (i = 0; i < 12; i++) { add (FAKE_SLEEP, 50, 0, 0); add (FAKE_NULL, 0, 0, 0); } }

static int handle_of (GtkWidget *top)
{
  int i, ox, oy;
  gdk_window_get_origin (gtk_widget_get_window (top), &ox, &oy);
  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle && (fake_windows[i].x0 >> 1) == ox && 1024 - (fake_windows[i].y1 >> 1) == oy)
      return fake_windows[i].handle;
  return 0;
}

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
  printf ("# MF_SHOTS: %d pool sprites\n", shot_pool[1]);
}

static void shot (const char *name)
{
  char path[600]; FILE *f; int x, y;
  g_snprintf (path, sizeof path, "%s/shot-%s.ppm", outdir, name);
  if (!(f = fopen (path, "wb"))) return;
  fprintf (f, "P6\n1280 1024\n255\n");
  for (y = 0; y < 1024; y++) for (x = 0; x < 1280; x++)
    { unsigned int p = fake_screen_pixel (x, y); fputc (p & 0xFF, f); fputc ((p >> 8) & 0xFF, f); fputc ((p >> 16) & 0xFF, f); }
  fclose (f);
}

static void list_windows (void)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  int i;
  for (l = tops; l; l = l->next)
    {
      GtkWindow *w = l->data;
      GdkWindow *gw = gtk_widget_get_window (l->data);
      int ox = 0, oy = 0;
      if (gw) gdk_window_get_origin (gw, &ox, &oy);
      printf ("# window %p %s title '%s' mapped %d type %d hint %d decorated %d size %dx%d at %d,%d handle &%X\n",
              w, G_OBJECT_TYPE_NAME (w), gtk_window_get_title (w) ? gtk_window_get_title (w) : "(none)",
              gtk_widget_get_mapped (l->data), gtk_window_get_window_type (w),
              gtk_window_get_type_hint (w), gtk_window_get_decorated (w),
              gtk_widget_get_allocated_width (l->data), gtk_widget_get_allocated_height (l->data), ox, oy,
              gw && gtk_widget_get_mapped (l->data) ? handle_of (l->data) : 0);
    }
  g_list_free (tops);
  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle)
      printf ("# wimp window &%X  %d,%d - %d,%d flags &%X\n", fake_windows[i].handle, fake_windows[i].x0, fake_windows[i].y0,
              fake_windows[i].x1, fake_windows[i].y1, fake_window_flags[fake_windows[i].handle & 255]);
}

static void hook (int reason)
{
  GList *l, *tops;
  if (fake_wimp_script_pos () < fake_wimp_script_len - 1) return;
  switch (stage)
    {
    case 0:
      tops = gtk_window_list_toplevels ();
      for (l = tops; l; l = l->next)
        if (gtk_widget_get_mapped (l->data) && gtk_window_get_window_type (l->data) == GTK_WINDOW_TOPLEVEL
            && gtk_widget_get_allocated_width (l->data) > 300)
          mainw = l->data;
      g_list_free (tops);
      if (mainw && (main_handle = handle_of (mainw)))
        {
          stage = 1;
          settle ();
          return;
        }
      if (++waits > 300) { stage = 99; return; }
      nulls (5);
      return;
    case 1:
      list_windows ();
      shot ("main");
      fake_target_window = main_handle;
      last_menus = fake_menus_opened;
      gx = gy = 0;
      stage = 2;
      /* fall through: the first click */
    case 2:
      {
        static int k;
        int w = gtk_widget_get_allocated_width (mainw), h = gtk_widget_get_allocated_height (mainw);
        if (k > 0)
          grid_results[(k - 1) / 8][(k - 1) % 8] = fake_menus_opened > last_menus;
        last_menus = fake_menus_opened;
        if (k >= 64)
          {
            int x, y;
            int n = 0;
            printf ("# Menu clicks over the main window (%dx%d), 8x8 grid, 1 = a menu opened:\n", w, h);
            for (y = 0; y < 8; y++)
              {
                printf ("#   ");
                for (x = 0; x < 8; x++) { printf ("%d ", grid_results[y][x]); n += grid_results[y][x]; }
                printf ("\n");
              }
            check (n == 64, "Menu anywhere in the main window (canvas, toolbars, palette): the menu");
            stage = 3;
            {
              GList *l2, *t2 = gtk_window_list_toplevels ();
              for (l2 = t2; l2; l2 = l2->next)
                if (gtk_widget_get_mapped (l2->data) && g_strcmp0 (gtk_window_get_title (l2->data), "Settings Toolbar") == 0)
                  toolw = l2->data;
              g_list_free (t2);
            }
            if (toolw)
              gtk_window_move (GTK_WINDOW (toolw), 700, 100);
            settle ();
            return;
          }
        add (6, w * (k % 8) / 8 + w / 16, h * (k / 8) / 8 + h / 16, 2);
        add (FAKE_RELEASE, 0, 0, 0);
        nulls (6);
        add (17, 0x400C9, 0, 0);
        nulls (3);
        settle ();
        k++;
      }
      return;
    case 3:
      {
        static int k, h3, res[16], last3;
        int w, h;
        if (!toolw || !(h3 = h3 ? h3 : handle_of (toolw))) { printf ("# no Settings Toolbar\n"); stage = 99; return; }
        w = gtk_widget_get_allocated_width (toolw); h = gtk_widget_get_allocated_height (toolw);
        fake_target_window = h3;
        if (k > 0) res[k - 1] = fake_menus_opened > last3;
        last3 = fake_menus_opened;
        if (k >= 16)
          {
            int i;
            printf ("# Menu clicks over the Settings Toolbar window (%dx%d), 4x4:\n#   ", w, h);
            int n = 0;
            for (i = 0; i < 16; i++) { printf ("%d%s", res[i], i % 4 == 3 ? "\n#   " : " "); n += res[i]; }
            printf ("\n");
            shot ("tools");
            check (n >= 12, "Menu over the Settings Toolbar window (no menu of its own): the main window's menu");
            stage = 4;
            g_idle_add (do_save, NULL);
            g_main_context_wakeup (NULL);
            nulls (6);
            return;
          }
        add (6, w * (k % 4) / 4 + w / 8, h * (k / 4) / 4 + h / 8, 2);
        add (FAKE_RELEASE, 0, 0, 0);
        nulls (6);
        add (17, 0x400C9, 0, 0);
        nulls (3);
        settle ();
        k++;
      }
      return;
    case 4:   /* File > Save As: the RISC OS save dialogue */
      {
        GtkWidget *w = titled ("Save Image File"), *icon;
        static int sw;
        if (!w || !handle_of (w)) { if (++sw > 200) { printf ("# no Save window\n"); _exit (1); } nulls (4); return; }
        if (sw < 1000) { sw = 1000; gtk_window_move (GTK_WINDOW (w), 300, 150); settle (); return; }
        list_windows ();
        shot ("save");
        icon = find_type (w, GTK_TYPE_EVENT_BOX);
        printf ("# save name: %s\n", gtk_entry_get_text (GTK_ENTRY (find_type (w, GTK_TYPE_ENTRY))));
        check (icon != NULL, "Save: a file icon to drag");
        fake_open_window (FOREIGN, 0, 2048 - 1200, 220, 2048 - 600, 0, 0);
        fake_target_window = handle_of (w);
        {
          GtkAllocation a; int x, y, ox, oy, sx, sy, tx, ty, i;
          gtk_widget_get_allocation (icon, &a);
          gtk_widget_translate_coordinates (icon, w, 0, 0, &x, &y);
          gdk_window_get_origin (gtk_widget_get_window (w), &ox, &oy);
          sx = x + a.width / 2; sy = y + a.height / 2;
          tx = 50 - ox; ty = 560 - oy;
          add (FAKE_MOVE, sx, sy, 0); add (6, sx, sy, 4); nulls (3);
          for (i = 1; i <= 8; i++) { add (FAKE_MOVE, sx + (tx - sx) * i / 8, sy + (ty - sy) * i / 8, 0); nulls (2); }
          add (FAKE_SLEEP, 30, 0, 0); add (FAKE_RELEASE, 0, 0, 0); nulls (6);
          settle ();
        }
        stage = 5;
        return;
      }
    case 5:
      {
        struct stat st;
        static int tries;
        if ((stat (save_unix, &st) != 0 || st.st_size == 0 || titled ("Please Wait ...")) && ++tries < 200)
          { nulls (4); return; }
        check (ds_seen && stat (save_unix, &st) == 0 && st.st_size > 0, "Save: dragged to a Filer window, the picture is saved there");
        check (dl_seen == 0, "and the Filer isn't sent DataLoad (a safe save)");
        g_idle_add (do_load, NULL);
        g_main_context_wakeup (NULL);
        stage = 6;
        nulls (6);
        return;
      }
    case 6:   /* File > Open: drop a file from the Filer */
      {
        GtkWidget *w = titled ("Load Image File");
        static int lw;
        int q[64], h;
        if (!w || !(h = handle_of (w))) { if (++lw > 200) { printf ("# no Load window\n"); _exit (1); } nulls (4); return; }
        if (lw < 1000) { lw = 1000; gtk_window_move (GTK_WINDOW (w), 300, 150); settle (); return; }
        shot ("load");
        memset (q, 0, sizeof q);
        q[5] = h; q[6] = -1; q[7] = 400; q[8] = 1600; q[9] = 1234; q[10] = 0xB60;
        g_strlcpy ((char *) &q[11], load_ro, 200);
        queue (17, q, 3, 0, 44 + (int) strlen (load_ro) + 1);
        stage = 7;
        settle ();
        settle ();
        return;
      }
    case 7:
      {
        GList *l, *tops;
        int found = 0;
        static int tries7;
        if ((titled ("Load Image File") || titled ("Please Wait ...")) && ++tries7 < 200)
          { nulls (4); return; }
        tops = gtk_window_list_toplevels ();
        for (l = tops; l; l = l->next)
          if (gtk_widget_get_mapped (l->data) && strstr (gtk_window_get_title (l->data) ? gtk_window_get_title (l->data) : "", "drop.png"))
            found = 1;
        g_list_free (tops);
        list_windows ();
        check (!titled ("Load Image File") && found, "Open: a PNG dropped from the Filer is loaded");
        printf ("1..%d\n", n_tests);
        fflush (stdout);
        _exit (n_failed);
      }
    }
}

int main (int argc, char **argv)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  outdir = argv[1];
  {
    char *ro;
    g_snprintf (save_unix, sizeof save_unix, "%s/saved.png", outdir);
    ro = gdk_riscos_path_from_unix (save_unix); g_strlcpy (save_ro, ro, sizeof save_ro); g_free (ro);
    g_snprintf (load_unix, sizeof load_unix, "%s/drop.png", outdir);
    ro = gdk_riscos_path_from_unix (load_unix); g_strlcpy (load_ro, ro, sizeof load_ro); g_free (ro);
  }
  fake_set_screen (1280, 1024, 0, 5);
  fake_sprite_plot_converts = 0;
  fake_wimp_nulls = 40;
  fake_wimp_hook = hook;
  if (getenv ("MF_SHOTS"))
    shots_setup (getenv ("MF_SHOTS"));
  fake_message_hook = message_hook;
  argv[1] = argv[0];
  return mtpaint_main (argc - 1, argv + 1);
}
