/*
 * geany-fake.c - Geany (2.1, the demo; 2.0 too) on the fake RISC OS:
 * start with a file, save it (Ctrl+S path: document_save_file), Open...
 * (an Open box, or the directory browser with RISCOS_GTK_CHOOSER=filer;
 * a file dropped on it), Save as... (a Save box; its icon dragged to a
 * Filer window), then close the main window. Geany's crash on Open and
 * Save when it ships no GSettings schemas was found with this
 * (gdk/riscos/gdkschemas-riscos.c).
 *   geany-fake OUTDIR -c CONFIGDIR FILE1
 * OUTDIR/file2.txt is the file dropped on the Open box; OUTDIR/saved-as.txt
 * where Save as writes. Prints TAP.
 * Build: Geany configured for arm-linux-gnueabihf against this target's
 * sysroot (--disable-shared --enable-static --disable-plugins
 * --disable-nls --disable-vte --disable-socket, PKG_CONFIG="pkg-config
 * --static", PKG_CONFIG_SYSROOT_DIR), make in scintilla, ctags,
 * src/tagmanager, then make -C src libgeany.la and make -C data install;
 * build/build-linuxarm.sh links this with GEANY_LA/src/.libs/libgeany.a.
 * GF_SHOTS=dir (as TT_ICONS): screenshots in OUTDIR. MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

#define OTHER_TASK 0x5B00
#define FOREIGN    0x5551

typedef struct GeanyDocument GeanyDocument;
struct GeanyDocument { gboolean is_valid; gint index; gboolean has_tags; gchar *file_name; };
extern int main_lib (int argc, char **argv);
extern GeanyDocument *document_get_current (void);
extern gboolean document_save_file (GeanyDocument *doc, gboolean force);
extern void dialogs_show_open_file (void);
extern gboolean dialogs_show_save_as (void);

static int n_tests, n_failed, stage, main_handle, waits;
static char file1[512], file2[512], file2_ro[512], saveas[512], saveas_ro[512];
static int shot_main, shot_open, shot_save;
static int open_ack, save_ds_seen, save_dl_seen, open_dl_ref;
static char after_open[512], after_saveas[512];
static int saved_ok, saveas_ok;
static const char *outdir;

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
  printf ("# GF_SHOTS: %d pool sprites\n", shot_pool[1]);
}

static void
shot (const char *name, GtkWidget *top)
{
  char path[600];
  FILE *f;
  int x, y, ox, oy;

  if (!getenv ("GF_SHOTS"))
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

static void check (int ok, const char *name)
{
  n_tests++; if (!ok) n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name); fflush (stdout);
}
static void add (int reason, int a, int b, int c)
{
  int *e = fake_wimp_script[fake_wimp_script_len++];
  e[0] = reason; e[1] = a; e[2] = b; e[3] = c;
}
static void nulls (int n) { while (n--) add (FAKE_NULL, 0, 0, 0); }
/* time for GTK's frame clock to redraw (before a screenshot) */
static void settle (void)
{
  int i;
  for (i = 0; i < 12; i++) { add (FAKE_SLEEP, 50, 0, 0); add (FAKE_NULL, 0, 0, 0); }
}
static void queue (int reason, int *block, int action, int your_ref, int bytes)
{
  block[0] = (bytes + 3) & ~3; block[1] = OTHER_TASK; block[2] = fake_new_ref ();
  block[3] = your_ref; block[4] = action;
  fake_queue_message (reason, block);
}

static GtkWidget *box_window (const char *title)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  GtkWidget *found = NULL;
  for (l = tops; l; l = l->next)
    if (gtk_widget_get_mapped (l->data) && g_strcmp0 (gtk_window_get_title (l->data), title) == 0)
      found = l->data;
  g_list_free (tops);
  return found;
}
static int handle_at (GtkWidget *top, int *ox, int *oy)
{
  int i;
  gdk_window_get_origin (gtk_widget_get_window (top), ox, oy);
  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle && fake_windows[i].handle != FOREIGN &&
        (fake_windows[i].x0 >> 1) == *ox && 1024 - (fake_windows[i].y1 >> 1) == *oy)
      return fake_windows[i].handle;
  return 0;
}
static GtkWidget *find_type (GtkWidget *w, GType t)
{
  GtkWidget *f = NULL; GList *l, *k;
  if (G_TYPE_CHECK_INSTANCE_TYPE (w, t)) return w;
  if (!GTK_IS_CONTAINER (w)) return NULL;
  k = gtk_container_get_children (GTK_CONTAINER (w));
  for (l = k; l && !f; l = l->next) f = find_type (l->data, t);
  g_list_free (k);
  return f;
}
static GtkWidget *main_window (void)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  GtkWidget *found = NULL;
  for (l = tops; l; l = l->next)
    if (gtk_widget_get_mapped (l->data) && g_strcmp0 (gtk_widget_get_name (l->data), "GeanyMainWindow") == 0)
      found = l->data;
  g_list_free (tops);
  return found;
}

static void message_hook (int reason, int *b, int dest, int icon)
{
  int q[64];
  memset (q, 0, sizeof q);
  if (b[4] == 1 && dest == FOREIGN && b[3] == 0)        /* DataSave from the Save box */
    {
      save_ds_seen++;
      memcpy (&q[5], &b[5], 16);
      q[9] = 100; q[10] = b[10];
      g_strlcpy ((char *) &q[11], saveas_ro, 200);
      queue (17, q, 2, b[2], 44 + (int) strlen (saveas_ro) + 1);
    }
  else if (b[4] == 3 && reason == 18 && dest == OTHER_TASK && b[3] != 0)   /* DataLoad: saved */
    {
      save_dl_seen++;
      memcpy (q, b, b[0]);
      queue (17, q, 4, b[2], b[0]);
    }
  else if (b[4] == 4 && open_dl_ref && b[3] == open_dl_ref)
    open_ack++;
}

static gboolean do_save (gpointer d)
{
  fprintf (stderr, "do_save\n");
  GeanyDocument *doc = document_get_current ();
  saved_ok = doc && document_save_file (doc, TRUE);
  printf ("# saved %s: %d\n", doc ? doc->file_name : "(none)", saved_ok);
  return G_SOURCE_REMOVE;
}
static gboolean do_open (gpointer d) { fprintf (stderr, "do_open\n"); dialogs_show_open_file (); fprintf (stderr, "do_open done\n"); return G_SOURCE_REMOVE; }
static gboolean do_saveas (gpointer d) { saveas_ok = dialogs_show_save_as (); return G_SOURCE_REMOVE; }

static void hook (int reason)
{
  GtkWidget *box, *top;
  int ox, oy, h, i;
  GeanyDocument *doc;

  if (getenv ("GF_DEBUG"))
    fprintf (stderr, "hook reason %d stage %d pos %d len %d\n", reason, stage, fake_wimp_script_pos (), fake_wimp_script_len);
  if (fake_wimp_script_pos () < fake_wimp_script_len - 1)
    return;                            /* still running the last steps */
  switch (stage)
    {
    case 0:
      top = main_window ();
      doc = top ? document_get_current () : NULL;
      if (doc && doc->file_name && fake_plots > 0 && (main_handle = handle_at (top, &ox, &oy)))
        {
          printf ("# started: main window &%X, document %s\n", main_handle, doc->file_name);
          fake_open_window (FOREIGN, 0, 2048 - 1200, 220, 2048 - 600, 0, 0);
          fake_target_window = main_handle;
          stage = 1;
          g_idle_add (do_save, NULL);
          g_main_context_wakeup (NULL);
        }
      nulls (5);
      break;
    case 1:
      if (getenv ("GF_SHOTS") && !shot_main)
        {
          shot_main = 1;
          settle ();
          break;
        }
      shot ("geany-main", main_window ());
      stage = 2;
      g_idle_add (do_open, NULL);
          g_main_context_wakeup (NULL);
      nulls (10);
      break;
    case 2:
      box = box_window ("Open File");
      if (box && getenv ("GF_SHOTS") && shot_open < 2)
        {
          if (shot_open++ == 1)
            shot ("geany-open", box);
          else
            gtk_widget_queue_draw (box);
          settle ();
          break;
        }
      if (box && (h = handle_at (box, &ox, &oy)))
        {
          int q[64];
          memset (q, 0, sizeof q);
          q[5] = h; q[6] = -1;
          for (i = 0; i < FAKE_MAX_WINDOWS; i++)
            if (fake_windows[i].handle == h)
              { q[7] = fake_windows[i].x0 + 40; q[8] = fake_windows[i].y1 - 40; }
          q[9] = 1234; q[10] = 0xFFF;
          g_strlcpy ((char *) &q[11], file2_ro, 200);
          queue (17, q, 3, 0, 44 + (int) strlen (file2_ro) + 1);
          open_dl_ref = q[2];
          fake_target_window = h;
          stage = 3;
          printf ("# Open box &%X: dropped %s\n", h, file2_ro);
        }
      else if (++waits > 60)
        stage = 99;
      nulls (10);
      break;
    case 3:
      doc = document_get_current ();
      g_strlcpy (after_open, doc && doc->file_name ? doc->file_name : "", sizeof after_open);
      fake_target_window = main_handle;
      stage = 4; waits = 0;
      g_idle_add (do_saveas, NULL);
          g_main_context_wakeup (NULL);
      nulls (10);
      break;
    case 4:
      box = box_window ("Save File");
      if (box && getenv ("GF_SHOTS") && shot_save < 2)
        {
          if (shot_save++ == 1)
            shot ("geany-save", box);
          else
            gtk_widget_queue_draw (box);
          settle ();
          break;
        }
      if (box)
        {
          GtkWidget *icon = find_type (gtk_bin_get_child (GTK_BIN (box)), GTK_TYPE_EVENT_BOX);
          GtkAllocation a;
          int x, y, sx, sy, tx, ty;
          if (icon && (h = handle_at (box, &ox, &oy)))
            {
              fake_target_window = h;
              gtk_widget_get_allocation (icon, &a);
              gtk_widget_translate_coordinates (icon, box, 0, 0, &x, &y);
              sx = x + a.width / 2; sy = y + a.height / 2;
              tx = 50 - ox; ty = 450 - oy;
              add (FAKE_MOVE, sx, sy, 0); add (6, sx, sy, 4); nulls (3);
              for (i = 1; i <= 8; i++) { add (FAKE_MOVE, sx + (tx - sx) * i / 8, sy + (ty - sy) * i / 8, 0); nulls (2); }
              add (FAKE_SLEEP, 30, 0, 0); add (FAKE_RELEASE, 0, 0, 0); nulls (6);
              for (i = 0; i < 12; i++) add (FAKE_SLEEP, 100, 0, 0);
              stage = 5;
              printf ("# Save box &%X: icon dragged to the Filer\n", h);
            }
        }
      else if (++waits > 60)
        stage = 99;
      nulls (6);
      break;
    case 5:
      doc = document_get_current ();
      g_strlcpy (after_saveas, doc && doc->file_name ? doc->file_name : "", sizeof after_saveas);
      fake_target_window = main_handle;
      stage = 6;
      nulls (10);                      /* then the fake closes main_handle */
      break;
    }
}

int main (int argc, char **argv)
{
  struct stat st;
  char *ro;
  int status;

  outdir = argv[1];
  g_snprintf (file1, sizeof file1, "%s", argv[argc - 1]);
  g_snprintf (file2, sizeof file2, "%s/file2.txt", outdir);
  g_snprintf (saveas, sizeof saveas, "%s/saved-as.txt", outdir);
  g_file_set_contents (file2, "the second file\n", -1, NULL);
  unlink (saveas);
  ro = gdk_riscos_path_from_unix (file2); g_strlcpy (file2_ro, ro, sizeof file2_ro); g_free (ro);
  ro = gdk_riscos_path_from_unix (saveas); g_strlcpy (saveas_ro, ro, sizeof saveas_ro); g_free (ro);

  fake_set_screen (1280, 1024, 0, 5);
  fake_sprite_plot_converts = 0;
  fake_wimp_nulls = 40;
  fake_wimp_hook = hook;
  fake_message_hook = message_hook;
  if (getenv ("GF_SHOTS"))
    shots_setup (getenv ("GF_SHOTS"));
  argv[1] = argv[0];
  status = main_lib (argc - 1, argv + 1);

  printf ("# after Open: %s; after Save as: %s; Save as returned %d; DataSave %d, DataLoad %d\n",
          after_open, after_saveas, saveas_ok, save_ds_seen, save_dl_seen);
  check (stage >= 1, "Geany started with the file");
  check (saved_ok, "saving the file (document_save_file) worked");
  check (open_ack >= 1 && strcmp (after_open, file2) == 0, "Open...: an Open box; the file dropped on it was opened");
  check (save_ds_seen && stat (saveas, &st) == 0 && st.st_size > 0, "Save as...: a Save box; the file was saved where the Filer said");
  check (strcmp (after_saveas, saveas) == 0, "and the document now has that name");
  check (stage == 6 && status == 0, "closing the main window quit Geany cleanly");
  printf ("1..%d\n", n_tests);
  return n_failed;
}
