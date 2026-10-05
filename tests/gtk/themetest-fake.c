/*
 * themetest-fake.c - runs tests/gtk/themetest.c on the fake RISC OS
 * (tests/fake/mesa-egl/fake_riscos.c) under qemu-arm-aligntrap.
 *
 * It uses the program as a person would: a pop-up (combo box), the File
 * and View menus from the menu bar (a choice, then an Adjust choice that
 * must keep the menu open, then closing it as a click elsewhere does),
 * every notebook tab, and a context menu on Menu. Menus are Wimp menus
 * (gtk/riscos/gtkmenu-riscos.c), so it checks the menu blocks the program
 * hands to Wimp_CreateMenu, where they open, and what each choice did
 * (the program logs every action). Each page is pictured.
 *
 *   themetest-fake APPDIR OUTDIR
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include "themetest.h"
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int n_tests, n_failed;
static int stage, last_page = -1, polls_on_page, saved[4];
static int win_w, win_h, opened_seen;
static const char *outdir;

/* Each Wimp menu opened, decoded */
#define MAX_MENUS 64
static struct
{
  char title[64];
  char text[16][96];
  int flags[16], icon[16], sub[16], n, x, y;
} menus[MAX_MENUS];
static int n_menus;
static int moved_x0 = -1, moved_y1 = -1, moved_y0, after_x0 = -1, after_y1 = -1, after_y0, tooltip_windows, main_handle, pos_stage;
static int edit_open_after_leave = -1, edit_title_ok;
static int nb_h0 = -1, nb_h1 = -1, def0 = -1, def1 = -1, apply_clicked_seen;
static int wm_x, wm_y, menubar_h = -1;            /* where the window menu should open */
static int combo_right, combo_top, ctx_x, ctx_y;

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

/* Widget rectangle in pixels from the toplevel's top left */
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
click (GtkWidget *w, int buttons)
{
  int x, y, wd, ht;
  rect (w, &x, &y, &wd, &ht);
  add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);
  add (6, x + wd / 2, y + ht / 2, buttons);
  add (FAKE_RELEASE, 0, 0, 0);
  nulls (6);
}

static void
menu_click (GtkWidget *w)
{
  int x, y, wd, ht;
  rect (w, &x, &y, &wd, &ht);
  add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);
  add (6, x + wd / 2, y + ht / 2, 2);      /* the Menu button */
  add (FAKE_RELEASE, 0, 0, 0);
  nulls (6);
}

static void
save_ppm (const char *name, int x0, int y0, int w, int h)
{
  char path[512];
  FILE *f;
  int x, y;

  g_snprintf (path, sizeof path, "%s/%s", outdir, name);
  if (!(f = fopen (path, "wb")))
    return;
  fprintf (f, "P6\n%d %d\n255\n", w, h);
  for (y = y0; y < y0 + h; y++)
    for (x = x0; x < x0 + w; x++)
      {
        unsigned int p = fake_screen_pixel (x, y);   /* TBGR */
        fputc (p & 0xFF, f);
        fputc ((p >> 8) & 0xFF, f);
        fputc ((p >> 16) & 0xFF, f);
      }
  fclose (f);
}

static void
record_block (const int *m, int x, int y)
{
  const int *it = m + 7;
  int k = n_menus < MAX_MENUS ? n_menus++ : MAX_MENUS - 1, i;

  memset (&menus[k], 0, sizeof menus[k]);
  if (it[0] & 0x100)
    g_strlcpy (menus[k].title, (const char *) (long) m[0], sizeof menus[k].title);
  else
    memcpy (menus[k].title, m, 12);
  for (i = 0; i < 16; i++, it += 6)
    {
      if (it[2] & 0x100)
        g_strlcpy (menus[k].text[i], (const char *) (long) it[3], sizeof menus[k].text[i]);
      else
        memcpy (menus[k].text[i], it + 3, 12);
      menus[k].flags[i] = it[0];
      menus[k].icon[i] = it[2];
      menus[k].sub[i] = it[1] != -1;
      menus[k].n = i + 1;
      if (it[0] & 0x80)
        break;
    }
  menus[k].x = x;
  menus[k].y = y;
  /* and its submenus (the Wimp opens them from their arrows) */
  for (i = 0, it = m + 7; i < menus[k].n; i++, it += 6)
    if (it[1] != -1)
      record_block ((const int *) (long) it[1], -1, -1);
}

static void
record_menu (void)
{
  record_block (fake_menu, fake_menu_x, fake_menu_y);
}

/* The k-th menu recorded whose title and first entry are these */
static int
find_menu2 (const char *title, const char *first, int from)
{
  int k;
  for (k = from; k < n_menus; k++)
    if (strcmp (menus[k].title, title) == 0 && strncmp (menus[k].text[0], first, strlen (first)) == 0)
      return k;
  return -1;
}

/* The theme in the Wimp's colours and sprites (gtkriscostheme.c): pixels
   of the fake desktop's window colour, and of the fake option and radio
   sprites' unique colours, on each page */
static int bg_pixels[4], optoff_pixels[4], radiooff_pixels[4], uicon_pixels[4], vbar_pixels[4];

/* ---- real RISC OS sprites (TT_ICONS): PNGs made into sprites ---- */

/* A 32bpp sprite (90 dpi) with a 1bpp mask from a PNG, appended to the
   sprite area at area (size words) */
static int
png_to_sprite (int *area, int size, const char *file, const char *name)
{
  GdkPixbuf *pb = gdk_pixbuf_new_from_file (file, NULL);
  int w, h, x, y, mwords, bytes, *spr;
  guchar *px;
  int stride, has_alpha;

  if (!pb)
    return 0;
  w = gdk_pixbuf_get_width (pb);
  h = gdk_pixbuf_get_height (pb);
  mwords = (w + 31) / 32;
  bytes = 44 + w * h * 4 + mwords * 4 * h;
  if (area[3] + bytes > size * 4)
    {
      g_object_unref (pb);
      return 0;
    }
  spr = (int *) ((char *) area + area[3]);
  memset (spr, 0, bytes);
  spr[0] = bytes;
  strncpy ((char *) &spr[1], name, 12);
  spr[4] = w - 1; spr[5] = h - 1; spr[6] = 0; spr[7] = 31;
  spr[8] = 44; spr[9] = 44 + w * h * 4;
  spr[10] = (6 << 27) | (90 << 14) | (90 << 1) | 1;
  px = gdk_pixbuf_get_pixels (pb);
  stride = gdk_pixbuf_get_rowstride (pb);
  has_alpha = gdk_pixbuf_get_has_alpha (pb);
  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      {
        guchar *p = px + y * stride + x * (has_alpha ? 4 : 3);
        unsigned char *m = (unsigned char *) spr + spr[9] + y * mwords * 4;
        spr[11 + y * w + x] = p[0] | p[1] << 8 | p[2] << 16;     /* &BBGGRR */
        if (!has_alpha || p[3] > 127)
          m[x >> 3] |= 1 << (x & 7);
      }
  area[1]++;
  area[3] += bytes;
  g_object_unref (pb);
  return 1;
}

static int icon_tools[65536], icon_pool[65536];

static const int *
icon_pool_lookup (const char *name)
{
  const char *p = (const char *) icon_pool + icon_pool[2];
  int i;
  for (i = 0; i < icon_pool[1]; i++, p += ((const int *) p)[0])
    if (strncmp (p + 4, name, 12) == 0)
      return (const int *) p;
  return NULL;
}

/* TT_ICONS=dir: dir/Tools/NAME22.png (window borders, named NAME22 as in
   a Tools file) and dir/Sprites22/NAME.png (the sprite pool) */
static void
load_icons (const char *dir)
{
  GDir *d;
  const char *f;
  char *path;

  icon_tools[0] = sizeof icon_tools; icon_tools[2] = icon_tools[3] = 16;
  icon_pool[0] = sizeof icon_pool; icon_pool[2] = icon_pool[3] = 16;
  path = g_build_filename (dir, "Tools", NULL);
  if ((d = g_dir_open (path, 0, NULL)))
    {
      while ((f = g_dir_read_name (d)))
        if (g_str_has_suffix (f, ".png"))
          {
            char *name = g_strndup (f, strlen (f) - 4), *file = g_build_filename (path, f, NULL);
            png_to_sprite (icon_tools, G_N_ELEMENTS (icon_tools), file, name);
            g_free (name);
            g_free (file);
          }
      g_dir_close (d);
    }
  g_free (path);
  path = g_build_filename (dir, "Sprites22", NULL);
  if ((d = g_dir_open (path, 0, NULL)))
    {
      while ((f = g_dir_read_name (d)))
        if (g_str_has_suffix (f, ".png"))
          {
            char *name = g_strndup (f, strlen (f) - 4), *file = g_build_filename (path, f, NULL);
            png_to_sprite (icon_pool, G_N_ELEMENTS (icon_pool), file, name);
            g_free (name);
            g_free (file);
          }
      g_dir_close (d);
    }
  g_free (path);
  fake_tool_area_override = icon_tools;
  fake_pool_lookup = icon_pool_lookup;
  printf ("# TT_ICONS: %d tool sprites, %d pool sprites from %s\n", icon_tools[1], icon_pool[1], dir);
}

/* A scrolled window drawn offscreen (GtkOffscreenWindow): pixels of the
   fake tool sprites' colours (uicon #a01020, vbarmid #aa1020) */
static int offscreen_stage, offscreen_polls;
static GtkWidget *offscreen;

static void
offscreen_step (void)
{
  if (offscreen_stage == 0)
    {
      GtkWidget *sw = gtk_scrolled_window_new (NULL, NULL);
      GtkWidget *area = gtk_drawing_area_new ();
      offscreen = gtk_offscreen_window_new ();
      if (getenv ("TT_ICONS"))
        {
          /* a preview: a few widgets in the real sprites */
          GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
          GtkWidget *col = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
          GtkWidget *tv = gtk_text_view_new (), *w;
          GtkTextBuffer *buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));
          GtkWidget *combo = gtk_combo_box_text_new ();
          int i;
          for (i = 0; i < 40; i++)
            gtk_text_buffer_insert_at_cursor (buf, "The quick brown fox jumps over the lazy dog. 0123456789\n", -1);
          gtk_container_add (GTK_CONTAINER (sw), tv);
          gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
          gtk_widget_set_size_request (sw, 300, 200);
          gtk_box_pack_start (GTK_BOX (box), sw, FALSE, FALSE, 0);
          w = gtk_check_button_new_with_label ("Option off");
          gtk_box_pack_start (GTK_BOX (col), w, FALSE, FALSE, 0);
          w = gtk_check_button_new_with_label ("Option on");
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
          gtk_box_pack_start (GTK_BOX (col), w, FALSE, FALSE, 0);
          w = gtk_radio_button_new_with_label (NULL, "Radio on");
          gtk_box_pack_start (GTK_BOX (col), w, FALSE, FALSE, 0);
          gtk_box_pack_start (GTK_BOX (col), gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (w), "Radio off"), FALSE, FALSE, 0);
          gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Pop-up");
          gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
          gtk_box_pack_start (GTK_BOX (col), combo, FALSE, FALSE, 0);
          gtk_box_pack_start (GTK_BOX (col), gtk_spin_button_new_with_range (0, 99, 1), FALSE, FALSE, 0);
          gtk_box_pack_start (GTK_BOX (box), col, FALSE, FALSE, 0);
          gtk_container_set_border_width (GTK_CONTAINER (box), 8);
          gtk_container_add (GTK_CONTAINER (offscreen), box);
        }
      else
        {
          gtk_widget_set_size_request (area, 100, 1000);
          gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
          gtk_container_add (GTK_CONTAINER (sw), area);
          gtk_widget_set_size_request (sw, 120, 200);
          gtk_container_add (GTK_CONTAINER (offscreen), sw);
        }
      gtk_widget_show_all (offscreen);
      offscreen_stage = 1;
    }
  else if (offscreen_stage == 1 && ++offscreen_polls == 20)
    {
      cairo_surface_t *surf = gtk_offscreen_window_get_surface (GTK_OFFSCREEN_WINDOW (offscreen));
      cairo_surface_t *img;
      cairo_t *cr;
      int w = gtk_widget_get_allocated_width (offscreen), h = gtk_widget_get_allocated_height (offscreen);
      int x, y, stride;
      unsigned char *d;

      img = cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);
      cr = cairo_create (img);
      cairo_set_source_surface (cr, surf, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
      cairo_surface_flush (img);
      d = cairo_image_surface_get_data (img);
      stride = cairo_image_surface_get_stride (img);
      for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
          {
            unsigned int p = *(unsigned int *) (d + y * stride + x * 4) & 0xFFFFFF;
            if (p == 0xA01020)
              uicon_pixels[2]++;
            else if (p == 0xAA1020)
              vbar_pixels[2]++;
          }
      {
        char path[600];
        g_snprintf (path, sizeof path, "%s/scrollbar.png", outdir);
        cairo_surface_write_to_png (img, path);
      }
      cairo_surface_destroy (img);
      gtk_widget_destroy (offscreen);
      offscreen_stage = 2;
    }
}

static void
count_theme_pixels (int page)
{
  int x, y;
  for (y = 60; y < 860; y++)
    for (x = 100; x < 860; x++)
      {
        unsigned int p = fake_screen_pixel (x, y) & 0xFFFFFF;   /* TBGR */
        if (p == 0xE0D0C8)
          bg_pixels[page]++;
        else if (p == 0x3377BB)
          optoff_pixels[page]++;
        else if (p == 0x34AB12)
          radiooff_pixels[page]++;

      }
}

/* The sprites as gdk_riscos_wimp_sprite decodes them */
static int spr_ok[6];
static char spr_note[256];

static guint32
px_at (GdkPixbuf *pb, int x, int y, int *alpha)
{
  guchar *p = gdk_pixbuf_get_pixels (pb) + y * gdk_pixbuf_get_rowstride (pb) + x * 4;
  *alpha = p[3];
  return p[0] << 16 | p[1] << 8 | p[2];
}

static void
check_sprites (void)
{
  GdkPixbuf *pb;

  int a0, a1;
  guint32 c0, c1;

  if ((pb = gdk_riscos_wimp_sprite ("optoff")))      /* 8bpp, 256-colour default, 1bpp mask */
    {
      c0 = px_at (pb, 0, 0, &a0);
      c1 = px_at (pb, 1, 0, &a1);
      spr_ok[0] = gdk_pixbuf_get_width (pb) == 12 && a0 == 0 && a1 == 255 && c1 == 0xBB7733 &&
                  px_at (pb, 5, 5, &a0) == 0xFFFFFF;
      g_object_unref (pb);
    }
  if ((pb = gdk_riscos_wimp_sprite ("opton")))
    {
      spr_ok[1] = px_at (pb, 5, 5, &a0) == 0xFF3333;
      g_object_unref (pb);
    }
  if ((pb = gdk_riscos_wimp_sprite ("radiooff")))    /* 4bpp, own palette */
    {
      c0 = px_at (pb, 1, 5, &a0);
      spr_ok[2] = c0 == 0x12AB34 && a0 == 255 && (px_at (pb, 0, 0, &a1), a1 == 0) &&
                  px_at (pb, 5, 5, &a0) == 0xFFFFFF;
      g_object_unref (pb);
    }
  if ((pb = gdk_riscos_wimp_sprite ("radioon")))     /* 32bpp, alpha mask */
    {
      c0 = px_at (pb, 0, 0, &a0);
      px_at (pb, 11, 0, &a1);
      spr_ok[3] = c0 == 0x3456DE && a0 == 255 && a1 == 128;
      g_object_unref (pb);
    }
  if ((pb = gdk_riscos_wimp_sprite ("gright")))      /* numbered mode, old mask */
    {
      c0 = px_at (pb, 2, 5, &a0);
      px_at (pb, 5, 2, &a1);
      spr_ok[4] = c0 == 0x008888 && a0 == 255 && a1 == 0;
      g_object_unref (pb);
    }
  if ((pb = gdk_riscos_wimp_sprite ("left")))        /* 1bpp, 45 dpi rows doubled */
    {
      c0 = px_at (pb, 1, 2, &a0);
      c1 = px_at (pb, 1, 3, &a1);
      spr_ok[5] = gdk_pixbuf_get_width (pb) == 8 && gdk_pixbuf_get_height (pb) == 8 &&
                  c0 == 0 && c1 == 0 && px_at (pb, 0, 2, &a0) == 0xFFFFFF;
      g_snprintf (spr_note, sizeof spr_note, "left %dx%d", gdk_pixbuf_get_width (pb),
                  gdk_pixbuf_get_height (pb));
      g_object_unref (pb);
    }
}

static void
hook (int reason)
{
  int x, y, wd, ht, i;

  if (stage == 1 && !apply_clicked_seen && gtk_widget_has_focus (tt_apply))
    apply_clicked_seen = 1;
  else if (apply_clicked_seen == 1 && fake_wimp_script_pos () > 0)
    {
      apply_clicked_seen = 2;   /* a poll later: laid out again */
    }
  else if (apply_clicked_seen == 2)
    {
      apply_clicked_seen = 3;
      nb_h1 = gtk_widget_get_allocated_height (tt_notebook);
      def1 = gtk_style_context_has_class (gtk_widget_get_style_context (tt_apply), "default");
    }
  if (main_handle)
    for (i = 0; i < FAKE_MAX_WINDOWS; i++)
      if (fake_windows[i].handle == main_handle)
        {
          if (pos_stage == 1 && moved_x0 < 0 && fake_windows[i].x0 != 240)
            {
              moved_x0 = fake_windows[i].x0;
              moved_y1 = fake_windows[i].y1;
              moved_y0 = fake_windows[i].y0;
            }
          if (pos_stage == 1 && moved_x0 >= 0)
            {
              after_x0 = fake_windows[i].x0;
              after_y1 = fake_windows[i].y1;
              after_y0 = fake_windows[i].y0;
            }
        }
  {
    int open = 0;
    for (i = 0; i < FAKE_MAX_WINDOWS; i++)
      if (fake_windows[i].handle && fake_windows[i].handle != main_handle && !fake_menu)
        open++;
    if (pos_stage == 1 && open > tooltip_windows)
      tooltip_windows = open;
  }
  {
    int pos = fake_wimp_script_pos ();
    if (pos >= 0 && fake_wimp_script[pos][0] == FAKE_NULL && fake_wimp_script[pos][3] == 77 &&
        edit_open_after_leave < 0)
      {
        edit_open_after_leave = fake_menu != NULL;
        if (fake_menu && (fake_menu[7] & 0x100))
          edit_title_ok = strcmp ((const char *) (long) fake_menu[0], "GTKThemeTest") == 0 &&
                          strcmp ((const char *) (long) fake_menu[7 + 6 + 3], "Edit") == 0;
      }
  }
  if (fake_menu && fake_menus_opened != opened_seen)
    {
      opened_seen = fake_menus_opened;
      record_menu ();
    }
  if (stage == 0 && tt_window && gtk_widget_get_mapped (tt_window) && fake_plots > 0 &&
      gtk_widget_get_allocated_width (tt_notebook) > 1)
    {
      GtkWidget *plain;

      stage = 1;
      check_sprites ();
      menubar_h = gtk_widget_get_allocated_height (tt_menubar);
      nb_h0 = gtk_widget_get_allocated_height (tt_notebook);
      def0 = gtk_style_context_has_class (gtk_widget_get_style_context (tt_apply), "default");
      for (i = 0; i < FAKE_MAX_WINDOWS; i++)
        if (fake_windows[i].handle)
          main_handle = fake_windows[i].handle;
      fake_target_window = main_handle;   /* not its tooltips */
      win_w = gtk_widget_get_allocated_width (tt_window);
      win_h = gtk_widget_get_allocated_height (tt_window);

      /* expected places, OS units (window top left at pixel 120,80;
         EIG 1; 1024-pixel-high screen) */
      rect (tt_combo, &x, &y, &wd, &ht);
      combo_right = (120 + x + wd) * 2;
      combo_top = (1024 - (80 + y)) * 2;
      {
        /* the window menu is opened by Menu on a plain part of page 1 */
        GList *kids = gtk_container_get_children (GTK_CONTAINER (
                        gtk_notebook_get_nth_page (GTK_NOTEBOOK (tt_notebook), 0)));
        plain = g_list_last (kids)->data;       /* the styled-text label */
        g_list_free (kids);
        rect (plain, &x, &y, &wd, &ht);
        wm_x = (120 + x + wd / 2) * 2 - 64;
        wm_y = (1024 - (80 + y + ht / 2)) * 2 - 1;
      }

      /* 0. the window has the input focus (the Wimp sends Gain_Caret when
            the program takes the caret) */
      add (12, 0, 0, 0);
      nulls (4);
      /* 1. the pop-up: choose its second entry */
      click (tt_combo, 4);
      add (9, 1, -1, 0);
      nulls (8);
      /* 2. Menu in the window: File > Save */
      menu_click (plain);
      add (9, 0, 1, 0);
      nulls (8);
      /* 3. Menu again: View > Show grid with Adjust (stays open), then a
            click elsewhere closes it (Message_MenusDeleted) */
      menu_click (plain);
      add (FAKE_BUTTONS, 1, 0, 0);
      add (9, 2, 0, 0);
      nulls (8);
      add (FAKE_BUTTONS, 0, 0, 0);
      add (17, 0x400C9, 0, 0);
      nulls (8);
      /* 3a. Menu pressed, then the pointer goes off the window onto the
            menu (leave events) and the button is released there; then a
            click elsewhere closes it */
      {
        rect (plain, &x, &y, &wd, &ht);
        add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);
        add (6, x + wd / 2, y + ht / 2, 2);
        nulls (3);
        add (FAKE_MOVE, win_w + 40, y + ht + 30, 0);   /* off the window: over the menu */
        nulls (3);
        add (FAKE_RELEASE, 0, 0, 0);
        for (i = 0; i < 8; i++)
          add (FAKE_SLEEP, 100, 0, 0);
        add (FAKE_BUTTONS, 0, 0, 0);
        pos_stage = 0;
      }
      add (FAKE_NULL, 0, 0, 77);                      /* marker: check the menu here */
      add (17, 0x400C9, 0, 0);
      nulls (8);
      /* 3b. the Wimp moves the window (a title bar drag), then a button is
            clicked: the window must stay where it was put */
      add (2, 0, 200, -100);
      nulls (8);
      pos_stage = 1;
      rect (tt_apply, &x, &y, &wd, &ht);
      add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);   /* rest on Apply: its tooltip */
      for (i = 0; i < 12; i++)
        add (FAKE_SLEEP, 100, 0, 0);
      pos_stage = 1;
      add (6, x + wd / 2, y + ht / 2, 4);
      add (FAKE_RELEASE, 0, 0, 0);
      nulls (10);
      /* 4. every tab */
      for (i = 1; i < 4; i++)
        {
          GtkWidget *tab = gtk_notebook_get_tab_label (GTK_NOTEBOOK (tt_notebook),
                             gtk_notebook_get_nth_page (GTK_NOTEBOOK (tt_notebook), i));
          click (tab, 4);
          nulls (12);
        }
      nulls (30);                 /* 5. is added once page 4 is drawn */
      /* then the fake sends Close_Window_Request */
    }
  if (stage == 1 && offscreen_stage < 2)
    offscreen_step ();
  if (stage == 1 && tt_notebook)
    {
      int cur = gtk_notebook_get_current_page (GTK_NOTEBOOK (tt_notebook));
      if (cur != last_page)
        {
          last_page = cur;
          polls_on_page = 0;
        }
      else if (++polls_on_page == 10 && cur >= 0 && cur < 4 && !saved[cur])
        {
          char name[32];
          saved[cur] = 1;
          g_snprintf (name, sizeof name, "themetest-page%d.ppm", cur);
          save_ppm (name, 100, 60, 760, 800);
          count_theme_pixels (cur);

          if (cur == 3)
            {
              /* 5. Menu click in the context area, then Info */
              rect (tt_context_area, &x, &y, &wd, &ht);
              for (i = 0; i < FAKE_MAX_WINDOWS; i++)
                if (fake_windows[i].handle == main_handle)
                  {
                    ctx_x = fake_windows[i].x0 + (x + wd / 2) * 2;
                    ctx_y = fake_windows[i].y1 - (y + ht / 2) * 2 - 1;
                  }
              add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);
              add (6, x + wd / 2, y + ht / 2, 2);   /* Menu button */
              add (FAKE_RELEASE, 0, 0, 0);
              nulls (8);
              add (9, 2, -1, 0);
              nulls (10);
            }
        }
    }
}

static void
critical_handler (const gchar *domain, GLogLevelFlags level, const gchar *message, gpointer data)
{
  GdkEvent *ev = gtk_get_current_event ();
  GtkWidget *w = ev ? gtk_get_event_widget (ev) : NULL;
  fprintf (stderr, "CRITICAL: %s (event %d, widget %s, window %p, grab %s)\n", message,
           ev ? (int) ev->type : -1, w ? G_OBJECT_TYPE_NAME (w) : "-",
           ev ? (void *) ev->any.window : NULL,
           gtk_grab_get_current () ? G_OBJECT_TYPE_NAME (gtk_grab_get_current ()) : "-");
  if (ev)
    gdk_event_free (ev);
}

int
main (int argc, char **argv)
{
  char logpath[600], logbuf[16384] = "", *line;
  char *targv[4];
  int saved_out = dup (1), saved_err = dup (2);
  FILE *lf;
  int pages, f, v, c, p, k, i, w;

  if (argc < 3)
    return 2;
  outdir = argv[2];
  fake_set_screen (1280, 1024, 0, 5);
  fake_sprite_plot_converts = 0;
  fake_wimp_nulls = 400;             /* the hook scripts the rest once drawn */
  fake_wimp_hook = hook;
  /* the desktop's window colour isn't the standard grey: the theme follows */
  fake_wimp_palette[1] = 0xE0D0C810;     /* #c8d0e0 */
  if (getenv ("TT_ICONS"))
    {
      fake_wimp_palette[1] = 0xDDDDDD10;   /* the standard palette for a preview */
      load_icons (getenv ("TT_ICONS"));
    }
  g_snprintf (logpath, sizeof logpath, "%s/GTKThemeTest.Log", outdir);
  targv[0] = argv[0]; targv[1] = argv[1]; targv[2] = logpath; targv[3] = NULL;
  g_setenv ("RISCOS_GTK_MENU_DEBUG", "1", TRUE);
  if (getenv ("TT_DEBUG"))
    g_setenv ("RISCOS_GTK_DEBUG", "1", TRUE);
  if (getenv ("TT_CRIT"))
    g_log_set_handler ("Gtk", G_LOG_LEVEL_CRITICAL, critical_handler, NULL);
  themetest_main (3, targv);
  fflush (stdout); fflush (stderr);
  dup2 (saved_out, 1); dup2 (saved_err, 2);
  if ((lf = fopen (logpath, "r")))
    {
      size_t n = fread (logbuf, 1, sizeof logbuf - 1, lf);
      logbuf[n] = 0;
      fclose (lf);
    }

  p = find_menu2 ("GTKThemeTest", "Latin-1", 0);     /* the pop-up */
  w = find_menu2 ("GTKThemeTest", "File", 0);        /* the window menu */
  f = find_menu2 ("File", "Open...", 0);             /* its submenus */
  v = find_menu2 ("View", "Show grid", 0);
  c = find_menu2 ("GTKThemeTest", "Select all", 0);  /* the context menu */
  pages = (strstr (logbuf, "page 1: Options") != NULL) + (strstr (logbuf, "page 2: Lists") != NULL)
          + (strstr (logbuf, "page 3: Dialogues") != NULL);

  check (stage == 1, "theme test window shown and drawn");
  check (win_w > 500 && win_w < 1000 && win_h > 500 && win_h < 950,
         "the window has a sensible size");
  printf ("# window %dx%d\n", win_w, win_h);

  check (p >= 0 && menus[p].n == 3 && strcmp (menus[p].text[1], "UTF-8") == 0 &&
         (menus[p].flags[0] & 1) == 0,
         "pop-up: a Wimp menu of its three entries");
  check (p >= 0 && abs (menus[p].x - combo_right) <= 4 && abs (menus[p].y - combo_top) <= 4,
         "pop-up: opens to the right of the field, level with its top");
  if (p >= 0)
    printf ("# pop-up menu at %d,%d (field's right/top %d,%d)\n", menus[p].x, menus[p].y,
            combo_right, combo_top);
  check (strstr (logbuf, "Pop-up: \"UTF-8\" chosen") != NULL, "pop-up: choosing UTF-8 sets it");

  check (f >= 0 && menus[f].n == 4 && strncmp (menus[f].text[0], "Open...", 7) == 0 &&
         strstr (menus[f].text[0], "^O") && strstr (menus[f].text[2], "Shift-F3") &&
         (menus[f].flags[2] & 2) && strcmp (menus[f].text[3], "Quit") == 0 &&
         (menus[f].flags[3] & 0x80),
         "File: Wimp menu with short-cuts, a dotted line before Quit, Quit last");
  check (w >= 0 && menus[w].n == 4 && strcmp (menus[w].text[1], "Edit") == 0 &&
         strcmp (menus[w].text[2], "View") == 0 && strcmp (menus[w].text[3], "Help") == 0 &&
         menus[w].sub[0] && menus[w].sub[1] && menus[w].sub[2] && menus[w].sub[3],
         "Menu in the window: the program's menu, File/Edit/View/Help leading to submenus");
  check (w >= 0 && abs (menus[w].x - wm_x) <= 4 && abs (menus[w].y - wm_y) <= 4,
         "Menu in the window: opens 64 OS units left of the pointer");
  if (w >= 0)
    printf ("# window menu at %d,%d (expected %d,%d)\n", menus[w].x, menus[w].y, wm_x, wm_y);
  check (menubar_h >= 0 && menubar_h <= 1,
         "the GTK menu bar takes no room in the window");
  check (strstr (logbuf, "menu \"Save\"") != NULL, "File: choosing Save activates it");

  check (v >= 0 && (menus[v].flags[0] & 1) && (menus[v].flags[0] & 2) &&
         strcmp (menus[v].text[0], "Show grid") == 0 &&
         (menus[v].flags[1] & 1) && !(menus[v].flags[2] & 1),
         "View: ticks for Show grid and Portrait, none for Landscape");
  check (v >= 0 && menus[v].n == 4 && strcmp (menus[v].text[3], "Zoom") == 0,
         "View: Zoom (with its submenu) last");
  check (strstr (logbuf, "menu \"Show grid\" unticked") != NULL,
         "View: an Adjust choice activates the item");
  /* View appears in each window menu: the 1st (File > Save), the 2nd
     (the Adjust choice) and the 2nd reopened after it */
  k = v >= 0 ? find_menu2 ("View", "Show grid", v + 1) : -1;
  k = k >= 0 ? find_menu2 ("View", "Show grid", k + 1) : -1;
  check (k >= 0 && !(menus[k].flags[0] & 1),
         "View: Adjust reopens the menu, Show grid now unticked");

  check (edit_open_after_leave == 1 && edit_title_ok,
         "the menu stays open, its text intact, when the pointer moves off the window onto it");
  check (strstr (logbuf, "button \"Apply\" clicked") != NULL,
         "after a release off the window (over the menu), the next click reaches its button");
  check (nb_h0 > 0 && nb_h1 == nb_h0 && def0 == 1 && def1 == 1,
         "Apply is the default button from the start, and its focus moves nothing (the tabs)");
  printf ("# notebook height %d -> %d after Apply took focus; default style %d -> %d\n", nb_h0, nb_h1, def0, def1);
  check (moved_x0 == 240 + 200 && after_x0 == moved_x0 && after_y1 == moved_y1 && after_y0 == moved_y0,
         "the window stays where the Wimp moved it (Apply clicked, tabs, menus)");
  printf ("# moved to %d,%d (bottom %d); after the tooltip and clicking Apply %d,%d (bottom %d); %d tooltip window(s)\n",
          moved_x0, moved_y1, moved_y0, after_x0, after_y1, after_y0, tooltip_windows);
  check (pages == 3, "the three other tabs open when clicked");
  check (saved[0] && saved[1] && saved[2] && saved[3], "each page drawn and pictured");

  check (c >= 0 && menus[c].n == 4 && strcmp (menus[c].text[2], "Info") == 0 &&
         (menus[c].flags[1] & 2) && (menus[c].icon[3] & (1 << 22)),
         "context menu: dotted line after Clear selection, last entry shaded");
  check (c >= 0 && abs (menus[c].x - (ctx_x - 64)) <= 4 && abs (menus[c].y - ctx_y) <= 4,
         "context menu: opens 64 OS units left of the pointer");
  check (strstr (logbuf, "menu \"Info\"") != NULL, "context menu: choosing Info activates it");

  check (spr_ok[0] && spr_ok[1], "Wimp sprites: 8bpp with the standard palette and a 1bpp mask");
  check (spr_ok[2], "Wimp sprites: 4bpp with its own palette");
  check (spr_ok[3], "Wimp sprites: 32bpp with a RISC OS 5 alpha mask");
  check (spr_ok[4], "Wimp sprites: numbered mode with an old-style mask");
  check (spr_ok[5], "Wimp sprites: 1bpp in Wimp colours, a 45 dpi sprite doubled");
  printf ("# sprites %d%d%d%d%d%d (%s)\n", spr_ok[0], spr_ok[1], spr_ok[2], spr_ok[3], spr_ok[4], spr_ok[5], spr_note);
  check (bg_pixels[0] > 100000 && bg_pixels[1] > 100000,
         "the theme uses the desktop's Wimp colours (window background)");
  check (optoff_pixels[1] > 20 && radiooff_pixels[1] > 20,
         "option and radio icons are the Wimp's sprites (optoff, radiooff)");
  printf ("# page pixels: background %d/%d/%d/%d; optoff %d, radiooff %d on page 2\n",
          bg_pixels[0], bg_pixels[1], bg_pixels[2], bg_pixels[3], optoff_pixels[1], radiooff_pixels[1]);
  check (uicon_pixels[2] == 14 * 14 && vbar_pixels[2] > 20,
         "scroll bars are drawn with the Wimp's window border sprites (uicon, vbarmid)");
  printf ("# offscreen scrolled window: %d uicon pixels, %d vbarmid pixels\n", uicon_pixels[2], vbar_pixels[2]);
  check (strstr (logbuf, "window closed") != NULL, "the window closes");
  check (strstr (logbuf, "CRITICAL") == NULL && strstr (logbuf, "WARNING") == NULL &&
         strstr (logbuf, "Gtk-") == NULL, "no GTK warnings or criticals in the log");

  printf ("# Wimp menus opened (%d):\n", n_menus);
  for (k = 0; k < n_menus; k++)
    {
      printf ("#   \"%s\" at %d,%d:", menus[k].title, menus[k].x, menus[k].y);
      for (i = 0; i < menus[k].n; i++)
        printf (" [%s%s%s%s]", menus[k].flags[i] & 1 ? "tick " : "", menus[k].text[i],
                menus[k].icon[i] & (1 << 22) ? " shaded" : "", menus[k].flags[i] & 2 ? " ---" : "");
      printf ("\n");
    }
  printf ("# log:\n");
  for (line = strtok (logbuf, "\n"); line; line = strtok (NULL, "\n"))
    printf ("#   %s\n", line);
  printf ("1..%d\n# themetest-fake: %d checks, %d failed\n", n_tests, n_tests, n_failed);
  return n_failed;
}
