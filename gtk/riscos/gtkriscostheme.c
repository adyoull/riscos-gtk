/*
 * gtkriscostheme.c - riscos-gtk: the RISCOS theme takes the desktop's own
 * Wimp colours and Wimp sprites at start-up.
 *
 * The RISCOS theme (share/themes/RISCOS/gtk-3.0/gtk.css) is written in the
 * standard Wimp palette (@wimp0 .. @wimp15) with pictures of the RISC OS 5
 * option and radio icons, pop-up and adjuster arrows. When GTK starts on
 * RISC OS this adds a style provider just above the theme (GtkSettings
 * adds the theme at GTK_STYLE_PROVIDER_PRIORITY_SETTINGS, so this is
 * SETTINGS + 1; a program's own CSS, APPLICATION and up, still wins)
 * that
 *   - redefines @wimp0 .. @wimp15 as the desktop's Wimp colours now
 *     (Wimp_ReadPalette), so everything drawn in them follows the user's
 *     palette;
 *   - uses the Wimp's own sprites (the sprite pool, so whatever sprite set
 *     the desktop has loaded) for option icons (optoff, opton), radio icons
 *     (radiooff, radioon), the pop-up menu button (gright) and the number
 *     range adjuster arrows (left, right);
 *   - draws scroll bars with the Wimp's window border (tool) sprites, as
 *     the Wimp draws its windows' scroll bars, so they follow the desktop's
 *     theme (*ToolSprites);
 *   - makes an icon theme, "RISCOS-Wimp", of the desktop's own sprites for
 *     the icon names RISC OS has a picture for (directories, applications,
 *     file types, error box symbols), inheriting the shipped RISCOS theme
 *     (share/icons/RISCOS: Adwaita's icons for the rest, build/mkicons.py).
 * The sprites are written as PNGs to a directory in /tmp
 * (<Wimp$ScrapDir>) and removed at exit. A sprite the Wimp hasn't got
 * keeps the theme's own picture.
 *
 * *Set RISCOS_GTK_WIMP_THEME 0 turns this off (the theme as shipped);
 * RISCOS_GTK_DEBUG writes the CSS to the program's log.
 *
 * Part of GTK: LGPL version 2.1 or later. Copyright (C) 2026 Andrew Youll.
 */
#include "config.h"

#include "gtkriscostheme.h"
#include "gtkiconbar-riscos.h"
#include "gtkcssprovider.h"
#include "gtkstylecontext.h"
#include "gtkstyleprovider.h"
#include "gtkicontheme.h"
#include "gtksettings.h"
#include "gtkscrollbar.h"
#include "gtkorientable.h"
#include "gtkwindow.h"
#include "gtkdialog.h"
#include "gtkbin.h"
#include "gtkrender.h"
#include "gtkwidgetpath.h"

#include "riscos/gdkriscos.h"
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>

static char *dir;               /* the PNGs */
/* for the Log: sprites asked for, found, written */
static int pool_asked, pool_found, tool_asked, tool_found, write_failed;
static GPtrArray *files, *subdirs;
static int icons_asked, icons_found;

static void
remove_files (void)
{
  guint i;

  if (files)
    for (i = 0; i < files->len; i++)
      g_unlink (g_ptr_array_index (files, i));
  if (subdirs)
    for (i = subdirs->len; i > 0; i--)
      g_rmdir (g_ptr_array_index (subdirs, i - 1));
  if (dir)
    g_rmdir (dir);
}

/* Write pb (taken) as file.png; its file URI (and size), or NULL */
static char *
pixbuf_uri (GdkPixbuf *pb, const char *name, int *w, int *h)
{
  char *file, *uri;

  if (pb == NULL)
    return NULL;
  if (dir == NULL)
    {
      dir = g_dir_make_tmp ("riscos-gtk-XXXXXX", NULL);
      if (dir == NULL)
        {
          write_failed++;
          g_object_unref (pb);
          return NULL;
        }
      files = g_ptr_array_new_with_free_func (g_free);
      atexit (remove_files);
    }
  file = g_strdup_printf ("%s/%s.png", dir, name);
  if (!gdk_pixbuf_save (pb, file, "png", NULL, NULL))
    {
      write_failed++;
      g_free (file);
      g_object_unref (pb);
      return NULL;
    }
  *w = gdk_pixbuf_get_width (pb);
  *h = gdk_pixbuf_get_height (pb);
  g_object_unref (pb);
  g_ptr_array_add (files, file);
  uri = g_filename_to_uri (file, NULL, NULL);
  return uri;
}

/* A sprite from the Wimp's sprite pool as a PNG */
static char *
sprite_uri (const char *name, int *w, int *h)
{
  GdkPixbuf *pb = gdk_riscos_wimp_sprite (name);
  pool_asked++;
  if (pb)
    pool_found++;
  return pixbuf_uri (pb, name, w, h);
}

/* A window border (tool) sprite as a PNG (files t_NAME.png) */
static char *
tool_uri (const char *name, int *w, int *h)
{
  char *file = g_strconcat ("t_", name, NULL);
  GdkPixbuf *pb = gdk_riscos_wimp_tool_sprite (name);
  char *uri;
  tool_asked++;
  if (pb)
    tool_found++;
  uri = pixbuf_uri (pb, file, w, h);
  g_free (file);
  return uri;
}

/* One scroll bar orientation drawn with the Wimp's own window border
   sprites, as the Wimp draws a window's scroll bars (PRMs, "Naming
   Convention"): arrows (and pressed arrows), the well (end caps and a
   section repeated between them) and the slider (end caps and a middle
   section repeated). Nothing if the arrows, well or slider middle are
   missing. */
static void
scrollbar_rules (GString *css, gboolean vertical)
{
  const char *o = vertical ? "vertical" : "horizontal";
  const char *names[] = {
    vertical ? "uicon" : "licon", vertical ? "puicon" : "plicon",
    vertical ? "dicon" : "ricon", vertical ? "pdicon" : "pricon",
    vertical ? "vwelltcap" : "hwelllcap", vertical ? "vwellbcap" : "hwellrcap",
    vertical ? "vwellt" : "hwelll",
    vertical ? "vbart" : "hbarl", vertical ? "vbarb" : "hbarr", vertical ? "vbarmid" : "hbarmid",
    vertical ? "pvbart" : "phbarl", vertical ? "pvbarb" : "phbarr", vertical ? "pvbarmid" : "phbarmid",
  };
  enum { UP, PUP, DOWN, PDOWN, CAP1, CAP2, WELL, BAR1, BAR2, BARMID, PBAR1, PBAR2, PBARMID, N };
  char *u[N];
  int w[N], h[N], i, across;
  const char *start = vertical ? "top" : "left", *end = vertical ? "bottom" : "right";
  const char *rep = vertical ? "repeat-y" : "repeat-x";

  for (i = 0; i < N; i++)
    {
      w[i] = h[i] = 0;
      u[i] = tool_uri (names[i], &w[i], &h[i]);
    }
  if (!u[UP] || !u[DOWN] || !u[WELL] || !u[BARMID])
    goto out;
  across = vertical ? w[UP] : h[UP];

  /* the bar itself: the sprites draw it all (the theme's black line
     between it and the work area stays) */
  g_string_append_printf (css,
    "scrollbar.%s { background: none; %s: %dpx; }\n",
    o, vertical ? "min-width" : "min-height", across);

  /* arrows */
  g_string_append_printf (css,
    "scrollbar.%s button { min-width: %dpx; min-height: %dpx; padding: 0; border: none;"
    " background-color: transparent; background-repeat: no-repeat; background-position: center;"
    " -gtk-icon-source: none; }\n"
    "scrollbar.%s button.up { background-image: url(\"%s\"); }\n"
    "scrollbar.%s button.down { background-image: url(\"%s\"); }\n",
    o, w[UP], h[UP], o, u[UP], o, u[DOWN]);
  if (u[PUP] && u[PDOWN])
    g_string_append_printf (css,
      "scrollbar.%s button.up:active { background-image: url(\"%s\"); }\n"
      "scrollbar.%s button.down:active { background-image: url(\"%s\"); }\n",
      o, u[PUP], o, u[PDOWN]);

  /* the well (the trough): its end caps over a repeated section */
  g_string_append_printf (css, "scrollbar.%s trough { background-color: transparent;"
                          " background-image: ", o);
  if (u[CAP1] && u[CAP2])
    g_string_append_printf (css, "url(\"%s\"), url(\"%s\"), url(\"%s\");"
                            " background-repeat: no-repeat, no-repeat, %s;"
                            " background-position: %s, %s, %s; }\n",
                            u[CAP1], u[CAP2], u[WELL], rep, start, end, start);
  else
    g_string_append_printf (css, "url(\"%s\"); background-repeat: %s; }\n", u[WELL], rep);

  /* the slider: its end caps over a repeated middle; pressed while dragged */
  g_string_append_printf (css,
    "scrollbar.%s slider { margin: 0; border: none; border-radius: 0;"
    " background-color: transparent; %s: %dpx; %s: %dpx; background-image: ",
    o, vertical ? "min-width" : "min-height", across,
    vertical ? "min-height" : "min-width",
    MAX (2 * across, (vertical ? h[BAR1] + h[BAR2] : w[BAR1] + w[BAR2]) + 2));
  if (u[BAR1] && u[BAR2])
    g_string_append_printf (css, "url(\"%s\"), url(\"%s\"), url(\"%s\");"
                            " background-repeat: no-repeat, no-repeat, %s;"
                            " background-position: %s, %s, %s; }\n",
                            u[BAR1], u[BAR2], u[BARMID], rep, start, end, start);
  else
    g_string_append_printf (css, "url(\"%s\"); background-repeat: %s; }\n", u[BARMID], rep);
  if (u[PBAR1] && u[PBAR2] && u[PBARMID] && u[BAR1] && u[BAR2])
    g_string_append_printf (css,
      "scrollbar.%s slider:active { background-image: url(\"%s\"), url(\"%s\"), url(\"%s\"); }\n",
      o, u[PBAR1], u[PBAR2], u[PBARMID]);

out:
  for (i = 0; i < N; i++)
    g_free (u[i]);
}

/* Rules for a check or radio node from its off and on sprites */
static void
toggle_rules (GString *css, const char *node, const char *off, const char *on)
{
  int w0 = 0, h0 = 0, w1 = 0, h1 = 0;
  char *u0 = sprite_uri (off, &w0, &h0), *u1 = sprite_uri (on, &w1, &h1);

  if (u0 && u1)
    {
      /* RISC OS has no "mixed" icon: that state keeps the theme's picture
         (a provider above the theme wins whatever the selectors, so these
         rules must not match it) */
      g_string_append_printf (css,
        "%s { min-width: %dpx; min-height: %dpx; }\n"
        "%s:not(:indeterminate) { -gtk-icon-source: url(\"%s\"); }\n"
        "%s:checked:not(:indeterminate) { -gtk-icon-source: url(\"%s\"); }\n"
        "%s:disabled:not(:indeterminate) { -gtk-icon-source: url(\"%s\"); -gtk-icon-effect: dim; }\n"
        "%s:checked:disabled:not(:indeterminate) { -gtk-icon-source: url(\"%s\"); -gtk-icon-effect: dim; }\n",
        node, MAX (w0, w1), MAX (h0, h1), node, u0, node, u1, node, u0, node, u1);
    }
  g_free (u0);
  g_free (u1);
}

static char *
make_css (void)
{
  GString *css = g_string_new ("/* riscos-gtk: the desktop's Wimp colours and sprites */\n");
  guint32 rgb[16];
  int i, w, h;
  char *u;

  if (gdk_riscos_wimp_palette (rgb))
    for (i = 0; i < 16; i++)
      g_string_append_printf (css, "@define-color wimp%d #%06x;\n", i, rgb[i] & 0xFFFFFF);

  toggle_rules (css, "check", "optoff", "opton");
  toggle_rules (css, "radio", "radiooff", "radioon");

  /* the pop-up menu button: the Wimp's own (a whole button) */
  if ((u = sprite_uri ("gright", &w, &h)))
    {
      g_string_append_printf (css,
        "combobox arrow { min-width: %dpx; min-height: %dpx; padding: 0; border: none;"
        " background: none; -gtk-icon-source: url(\"%s\"); }\n"
        "combobox button.combo:checked arrow { background: none; }\n"
        "combobox:disabled arrow { -gtk-icon-source: url(\"%s\"); -gtk-icon-effect: dim; }\n",
        w, h, u, u);
      g_free (u);
    }

  /* number range adjusters: the Wimp's arrows in the theme's raised
     buttons (Toolbox adjusters are arrows in a 3D-bordered icon) */
  if ((u = sprite_uri ("left", &w, &h)))
    {
      g_string_append_printf (css,
        "spinbutton:not(.vertical) button.down, spinbutton:not(.vertical) button:disabled.down"
        " { background-image: url(\"%s\"); }\n", u);
      g_free (u);
    }
  if ((u = sprite_uri ("right", &w, &h)))
    {
      g_string_append_printf (css,
        "spinbutton:not(.vertical) button.up, spinbutton:not(.vertical) button:disabled.up"
        " { background-image: url(\"%s\"); }\n", u);
      g_free (u);
    }

  /* scroll bars: the Wimp's window border sprites */
  scrollbar_rules (css, TRUE);
  scrollbar_rules (css, FALSE);
  if ((u = tool_uri ("blicon", &w, &h)))
    {
      g_string_append_printf (css, "scrolledwindow junction { background-image: url(\"%s\");"
                              " background-repeat: no-repeat; background-position: left top; }\n", u);
      g_free (u);
    }
  return g_string_free (css, FALSE);
}

/* ---- the icon theme of the desktop's sprites ---- */

/* GTK icon name -> Wimp sprites to try, large (34 x 34 at EIG 1) and small
   (the Filer's small icons) */
static const struct {
  const char *icon, *context;
  const char *large[3], *small[3];
} wimp_icons[] = {
  { "folder",                   "places",    { "directory" },               { "small_dir" } },
  { "folder-open",              "places",    { "directoryo", "directory" },  { "small_diro", "small_dir" } },
  { "inode-directory",          "mimetypes", { "directory" },               { "small_dir" } },
  { "application-x-executable", "mimetypes", { "file_ff8", "application" },  { "small_ff8", "small_app" } },
  { "text-x-generic",           "mimetypes", { "file_fff" },                { "small_fff" } },
  { "text-html",                "mimetypes", { "file_faf" },                { "small_faf" } },
  { "text-csv",                 "mimetypes", { "file_dfe" },                { "small_dfe" } },
  { "image-x-generic",          "mimetypes", { "file_ff9" },                { "small_ff9" } },
  { "image-png",                "mimetypes", { "file_b60" },                { "small_b60" } },
  { "image-jpeg",               "mimetypes", { "file_c85" },                { "small_c85" } },
  { "image-gif",                "mimetypes", { "file_695" },                { "small_695" } },
  { "application-pdf",          "mimetypes", { "file_adf" },                { "small_adf" } },
  { "package-x-generic",        "mimetypes", { "file_a91", "file_ddc" },     { "small_a91", "small_ddc" } },
  { "audio-x-generic",          "mimetypes", { "file_fb1" },                { "small_fb1" } },
  { "font-x-generic",           "mimetypes", { "file_ff6" },                { "small_ff6" } },
  { "application-octet-stream", "mimetypes", { "file_ffd" },                { "small_ffd" } },
  { "unknown",                  "mimetypes", { "file_xxx" },                { "small_xxx" } },
  { "drive-harddisk",           "devices",   { "harddisc" },                { NULL } },
  { "drive-removable-media",    "devices",   { "floppydisc", "floppy" },     { NULL } },
  { "media-floppy",             "devices",   { "floppydisc", "floppy" },     { NULL } },
  { "drive-optical",            "devices",   { "cddisc" },                  { NULL } },
  { "dialog-error",             "status",    { "error" },                   { NULL } },
  { "dialog-warning",           "status",    { "warning" },                 { NULL } },
  { "dialog-information",       "status",    { "information", "info" },     { NULL } },
  { "dialog-question",          "status",    { "question" },                { NULL } },
};

static gboolean
make_dir (const char *path)
{
  if (g_mkdir (path, 0755) != 0)
    return FALSE;
  g_ptr_array_add (subdirs, g_strdup (path));
  return TRUE;
}

/* The first of names the Wimp has, written as path/icon.png */
static gboolean
wimp_icon (const char *const names[3], const char *path, const char *icon)
{
  int i;

  for (i = 0; i < 3 && names[i]; i++)
    {
      GdkPixbuf *pb = gdk_riscos_wimp_sprite (names[i]);
      char *file;

      if (pb == NULL)
        continue;
      file = g_strdup_printf ("%s/%s.png", path, icon);
      if (gdk_pixbuf_save (pb, file, "png", NULL, NULL))
        {
          g_ptr_array_add (files, file);
          g_object_unref (pb);
          return TRUE;
        }
      write_failed++;
      g_free (file);
      g_object_unref (pb);
    }
  return FALSE;
}

/* Make dir/icons/RISCOS-Wimp; TRUE if it has any icons */
static gboolean
make_icon_theme (void)
{
  char *top, *theme, *index, *large, *small;
  GString *idx;
  guint i;
  gboolean any = FALSE;
  const char *ctxs[] = { "places", "mimetypes", "devices", "status" };

  if (dir == NULL)
    {
      dir = g_dir_make_tmp ("riscos-gtk-XXXXXX", NULL);
      if (dir == NULL)
        return FALSE;
      files = g_ptr_array_new_with_free_func (g_free);
      atexit (remove_files);
    }
  if (subdirs == NULL)
    subdirs = g_ptr_array_new_with_free_func (g_free);
  top = g_build_filename (dir, "icons", NULL);
  theme = g_build_filename (top, "RISCOS-Wimp", NULL);
  make_dir (top);
  make_dir (theme);
  idx = g_string_new ("[Icon Theme]\nName=RISCOS-Wimp\n"
                      "Comment=riscos-gtk: the desktop's own sprites\n"
                      "Inherits=RISCOS,hicolor\nDirectories=");
  for (i = 0; i < G_N_ELEMENTS (ctxs); i++)
    g_string_append_printf (idx, "%slarge/%s,small/%s", i ? "," : "", ctxs[i], ctxs[i]);
  g_string_append (idx, "\n\n");
  for (i = 0; i < G_N_ELEMENTS (ctxs); i++)
    {
      char *d;
      /* the Wimp's sprites are 34 pixels (large) and about 18 (small);
         GTK scales them to the size asked for */
      g_string_append_printf (idx, "[large/%s]\nSize=34\nContext=%s\nType=Threshold\nThreshold=14\n\n"
                              "[small/%s]\nSize=18\nContext=%s\nType=Threshold\nThreshold=4\n\n",
                              ctxs[i], ctxs[i], ctxs[i], ctxs[i]);
      d = g_build_filename (theme, "large", NULL);
      if (i == 0) make_dir (d);
      g_free (d);
      d = g_build_filename (theme, "small", NULL);
      if (i == 0) make_dir (d);
      g_free (d);
      d = g_build_filename (theme, "large", ctxs[i], NULL);
      make_dir (d);
      g_free (d);
      d = g_build_filename (theme, "small", ctxs[i], NULL);
      make_dir (d);
      g_free (d);
    }
  for (i = 0; i < G_N_ELEMENTS (wimp_icons); i++)
    {
      large = g_build_filename (theme, "large", wimp_icons[i].context, NULL);
      small = g_build_filename (theme, "small", wimp_icons[i].context, NULL);
      icons_asked++;
      if (wimp_icon (wimp_icons[i].large, large, wimp_icons[i].icon) |
          (wimp_icons[i].small[0] && wimp_icon (wimp_icons[i].small, small, wimp_icons[i].icon)))
        {
          icons_found++;
          any = TRUE;
        }
      g_free (large);
      g_free (small);
    }
  index = g_build_filename (theme, "index.theme", NULL);
  if (g_file_set_contents (index, idx->str, -1, NULL))
    g_ptr_array_add (files, index);
  else
    g_free (index);
  g_string_free (idx, TRUE);
  g_free (theme);
  g_free (top);
  return any;
}

static void
icons_init (GdkDisplay *display)
{
  GdkScreen *screen = gdk_display_get_default_screen (display);

  if (make_icon_theme ())
    {
      char *top = g_build_filename (dir, "icons", NULL);
      gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_for_screen (screen), top);
      g_object_set (gtk_settings_get_for_screen (screen), "gtk-icon-theme-name", "RISCOS-Wimp", NULL);
      g_free (top);
    }
}

/* The size grip (gdkriscos-server.c) is drawn over the window's bottom
   right corner. A scroll bar that runs into that corner stops short of
   it, as a Wimp scroll bar stops at the Adjust Size icon: a vertical one
   gets a bottom margin, a horizontal one an end margin, so that it ends
   at the grip's edge.
   Checked after a window's scroll bars have been given their places. */
#define GRIP_MARGIN_KEY "riscos-gtk-grip-margin"
#define CLAIMED_KEY "riscos-gtk-grip-claimed"
#define STRIP_KEY "riscos-gtk-grip-strip"
#define UNSURE_KEY "riscos-gtk-grip-unsure"

/* the grip's column (gdkriscos-server.c draws it there) */
static void
set_column (GtkWidget *top, int x, int width)
{
  int ww = gtk_widget_get_allocated_width (top), wh = gtk_widget_get_allocated_height (top);
  int gh = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (top), "riscos-gtk-grip-h"));
  int *col = g_object_get_data (G_OBJECT (top), "riscos-gtk-grip-column");

  if (col == NULL)
    {
      col = g_new0 (int, 2);
      g_object_set_data_full (G_OBJECT (top), "riscos-gtk-grip-column", col, g_free);
    }
  if (col[0] != x || col[1] != width)
    {
      GdkRectangle corner = { MIN (x, col[0] > 0 ? col[0] : x), wh - gh, 0, gh };
      corner.width = ww - corner.x;
      col[0] = x;
      col[1] = width;
      gdk_riscos_window_set_size_grip_column (gtk_widget_get_window (top), x, width);
      gdk_window_invalidate_rect (gtk_widget_get_window (top), &corner, FALSE);
    }
}

static void
fit_scrollbar (GtkWidget *bar, gpointer data)
{
  GtkWidget *top = data;
  int gw = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (top), "riscos-gtk-grip-w"));
  int gh = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (top), "riscos-gtk-grip-h"));
  int ours = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (bar), GRIP_MARGIN_KEY));
  int ww = gtk_widget_get_allocated_width (top), wh = gtk_widget_get_allocated_height (top);
  gboolean vertical = gtk_orientable_get_orientation (GTK_ORIENTABLE (bar)) == GTK_ORIENTATION_VERTICAL;
  GtkAllocation a;
  int x, y, want, margin;

  if (!gtk_widget_get_mapped (bar) || !gtk_widget_translate_coordinates (bar, top, 0, 0, &x, &y))
    return;
  gtk_widget_get_allocation (bar, &a);
  if (a.width <= 1 && a.height <= 1)
    {
      /* not laid out yet (the window waits for its size): decide later */
      g_object_set_data (G_OBJECT (top), UNSURE_KEY, GINT_TO_POINTER (1));
      return;
    }
  margin = vertical ? gtk_widget_get_margin_bottom (bar) : gtk_widget_get_margin_end (bar);
  /* where the bar would end without our margin */
  if (vertical)
    want = gw > 0 && x + a.width > ww - gw ? y + a.height + ours - (wh - gh) : 0;
  else
    want = gw > 0 && y + a.height > wh - gh ? x + a.width + ours - (ww - gw) : 0;
  if (g_getenv ("RISCOS_GTK_DEBUG_GRIP"))
    g_printerr ("grip:   bar %s at %d,%d %dx%d (window %dx%d) ours %d want %d\n", vertical ? "v" : "h",
                x, y, a.width, a.height, ww, wh, ours, want);
  /* only a bar that runs into the corner, not one that ends well above */
  if (want <= 0 || want > (vertical ? gh : gw))
    want = 0;
  /* the grip goes in the vertical bar's column, as wide as it, when the
     bar runs down into the corner (not one with something under it, a
     status bar say, which the grip would cover) */
  if (want > 0)
    {
      g_object_set_data (G_OBJECT (top), CLAIMED_KEY, GINT_TO_POINTER (1));
      if (vertical && x < ww)
        set_column (top, x, a.width);
    }
  if (want == ours)
    return;
  margin = MAX (0, margin - ours + want);
  g_object_set_data (G_OBJECT (bar), GRIP_MARGIN_KEY, GINT_TO_POINTER (want));
  if (vertical)
    gtk_widget_set_margin_bottom (bar, margin);
  else
    gtk_widget_set_margin_end (bar, margin);
}

static void
find_scrollbars (GtkWidget *w, gpointer top)
{
  if (GTK_IS_SCROLLBAR (w))
    fit_scrollbar (w, top);
  else if (GTK_IS_CONTAINER (w))
    gtk_container_forall (GTK_CONTAINER (w), find_scrollbars, top);
}

/* A window whose bottom right corner has no scroll bar running into it
   (no scroll bars, or one with a status bar under it) would have the
   grip over its contents. As on RISC OS, where the Adjust Size icon sits
   at the foot of the vertical scroll bar, it gets one: an empty vertical
   scroll bar (the slider filling it) down its right edge, drawn as GTK
   draws its scroll bars, the contents moved in from that edge (a margin
   on the window's child) and the grip at its foot. */
static void
render_node (GtkStyleContext *parent, GtkWidgetPath *path, const char *name, const char *klass,
             cairo_t *cr, double x, double y, double w, double h, gboolean arrow_down)
{
  GtkStyleContext *ctx = gtk_style_context_new ();
  int pos = gtk_widget_path_append_type (path, G_TYPE_NONE);

  gtk_widget_path_iter_set_object_name (path, pos, name);
  if (klass)
    gtk_widget_path_iter_add_class (path, pos, klass);
  gtk_style_context_set_path (ctx, path);
  gtk_style_context_set_parent (ctx, parent);
  gtk_style_context_set_screen (ctx, gtk_style_context_get_screen (parent));
  gtk_render_background (ctx, cr, x, y, w, h);
  gtk_render_frame (ctx, cr, x, y, w, h);
  if (klass && (strcmp (klass, "up") == 0 || strcmp (klass, "down") == 0))
    {
      double size = MIN (w, h) - 4;
      if (size > 0)
        gtk_render_arrow (ctx, cr, arrow_down ? G_PI : 0, x + (w - size) / 2, y + (h - size) / 2, size);
    }
  g_object_unref (ctx);
}

/* The strip's image: GTK's empty vertical scroll bar, sw x h */
static cairo_surface_t *
strip_image (GtkWidget *top, int sw, int h)
{
  cairo_surface_t *image = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, sw, h);
  cairo_t *cr = cairo_create (image);
  GtkStyleContext *bar;
  GtkWidgetPath *path, *p;
  GtkBorder border;
  double x, y, w, b;

  path = gtk_widget_path_copy (gtk_widget_get_path (top));
  gtk_widget_path_append_type (path, GTK_TYPE_SCROLLBAR);
  gtk_widget_path_iter_set_object_name (path, -1, "scrollbar");
  gtk_widget_path_iter_add_class (path, -1, "vertical");
  bar = gtk_style_context_new ();
  gtk_style_context_set_path (bar, path);
  gtk_style_context_set_parent (bar, gtk_widget_get_style_context (top));
  gtk_style_context_set_screen (bar, gtk_widget_get_screen (top));
  gtk_render_background (bar, cr, 0, 0, sw, h);
  gtk_render_frame (bar, cr, 0, 0, sw, h);
  gtk_style_context_get_border (bar, gtk_style_context_get_state (bar), &border);
  x = border.left;
  y = border.top;
  w = sw - border.left - border.right;
  h -= border.top + border.bottom;
  b = w;                                  /* square arrow buttons */
  gtk_widget_path_append_type (path, G_TYPE_NONE);
  gtk_widget_path_iter_set_object_name (path, -1, "contents");
  p = gtk_widget_path_copy (path);
  render_node (bar, p, "button", "up", cr, x, y, w, b, FALSE);
  gtk_widget_path_unref (p);
  p = gtk_widget_path_copy (path);
  render_node (bar, p, "button", "down", cr, x, y + h - b, w, b, TRUE);
  gtk_widget_path_unref (p);
  /* nothing to scroll: the slider fills the well */
  if (h > 2 * b)
    {
      p = gtk_widget_path_copy (path);
      gtk_widget_path_append_type (p, G_TYPE_NONE);
      gtk_widget_path_iter_set_object_name (p, -1, "trough");
      render_node (bar, p, "slider", NULL, cr, x, y + b, w, h - 2 * b, FALSE);
      gtk_widget_path_unref (p);
    }
  gtk_widget_path_unref (path);
  g_object_unref (bar);
  cairo_destroy (cr);
  return image;
}

/* The strip goes outside GTK's window (gdk_riscos_window_set_size_strip):
   the window's contents keep their size and place. Remade when the
   window's height changes. */
static void
fit_strip (GtkWidget *top, int gw)
{
  int strip = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (top), STRIP_KEY));
  int made_h = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (top), "riscos-gtk-strip-h"));
  int gh = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (top), "riscos-gtk-grip-h"));
  gboolean claimed = g_object_get_data (G_OBJECT (top), CLAIMED_KEY) != NULL;
  int wh = gtk_widget_get_allocated_height (top);
  int want = gw > 0 && !claimed && !GTK_IS_DIALOG (top) && wh - gh >= 3 * gw ? gw : 0;
  GdkWindow *win = gtk_widget_get_window (top);

  if (want == strip && (want == 0 || made_h == wh))
    return;
  g_object_set_data (G_OBJECT (top), STRIP_KEY, GINT_TO_POINTER (want));
  g_object_set_data (G_OBJECT (top), "riscos-gtk-strip-h", GINT_TO_POINTER (wh));
  if (want > 0)
    {
      cairo_surface_t *image = strip_image (top, want, wh - gh);
      gdk_riscos_window_set_size_strip (win, image);
      cairo_surface_destroy (image);
    }
  else
    gdk_riscos_window_set_size_strip (win, NULL);
}

/* One look at the window's scroll bars and the corner; TRUE if the strip
   came or went */
static gboolean
grip_pass (GtkWidget *top)
{
  GdkWindow *win = gtk_widget_get_window (top);
  int gw = 0, gh = 0, before;

  if (win == NULL || !gtk_widget_get_realized (top))
    return FALSE;
  if (!gdk_riscos_window_get_size_grip (win, &gw, &gh))
    gw = gh = 0;
  g_object_set_data (G_OBJECT (top), "riscos-gtk-grip-w", GINT_TO_POINTER (gw));
  g_object_set_data (G_OBJECT (top), "riscos-gtk-grip-h", GINT_TO_POINTER (gh));
  g_object_set_data (G_OBJECT (top), CLAIMED_KEY, NULL);
  g_object_set_data (G_OBJECT (top), UNSURE_KEY, NULL);
  gtk_container_forall (GTK_CONTAINER (top), find_scrollbars, top);
  if (g_object_get_data (G_OBJECT (top), UNSURE_KEY))
    return FALSE;
  before = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (top), STRIP_KEY));
  fit_strip (top, gw);
  if (g_getenv ("RISCOS_GTK_DEBUG_GRIP"))
    g_printerr ("grip: \"%s\" %dx%d mapped %d grip %dx%d claimed %d strip %d -> %d\n",
                GTK_IS_WINDOW (top) && gtk_window_get_title (GTK_WINDOW (top)) ? gtk_window_get_title (GTK_WINDOW (top)) : "",
                gtk_widget_get_allocated_width (top), gtk_widget_get_allocated_height (top),
                gdk_window_is_visible (win), gw, gh, g_object_get_data (G_OBJECT (top), CLAIMED_KEY) != NULL,
                before, GPOINTER_TO_INT (g_object_get_data (G_OBJECT (top), STRIP_KEY)));
  return before != GPOINTER_TO_INT (g_object_get_data (G_OBJECT (top), STRIP_KEY));
}

static gboolean
grip_check (gpointer data)
{
  GtkWidget *top = data;

  g_object_set_data (G_OBJECT (top), "riscos-gtk-grip-pending", NULL);
  grip_pass (top);
  g_object_unref (top);
  return G_SOURCE_REMOVE;
}

/* A window's contents are mapped (and laid out) just before the window
   goes on the screen: look then, so a window that needs the strip opens
   with it, rather than widening once it's on the screen */
static gboolean
grip_map_hook (GSignalInvocationHint *hint, guint n, const GValue *params, gpointer data)
{
  GtkWidget *w = g_value_get_object (&params[0]);
  GtkWidget *top = w ? gtk_widget_get_parent (w) : NULL;
  GdkWindow *win;

  if (top && GTK_IS_WINDOW (top) && gtk_widget_is_toplevel (top) &&
      (win = gtk_widget_get_window (top)) && !gdk_window_is_visible (win))
    grip_pass (top);
  return TRUE;
}

static gboolean
grip_hook (GSignalInvocationHint *hint, guint n, const GValue *params, gpointer data)
{
  GtkWidget *w = g_value_get_object (&params[0]);

  if (GTK_IS_WINDOW (w) && !g_object_get_data (G_OBJECT (w), "riscos-gtk-grip-pending"))
    {
      g_object_set_data (G_OBJECT (w), "riscos-gtk-grip-pending", GINT_TO_POINTER (1));
      g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, grip_check, g_object_ref (w), NULL);
    }
  return TRUE;
}

static void
grip_init (void)
{
  if (g_strcmp0 (g_getenv ("RISCOS_GTK_SIZE_GRIP"), "0") == 0)
    return;
  g_signal_add_emission_hook (g_signal_lookup ("size-allocate", GTK_TYPE_WIDGET), 0,
                              grip_hook, NULL, NULL);
  g_signal_add_emission_hook (g_signal_lookup ("map", GTK_TYPE_WIDGET), 0,
                              grip_map_hook, NULL, NULL);
}

void
_gtk_riscos_theme_init (GdkDisplay *display)
{
  static gboolean done;
  const char *env = g_getenv ("RISCOS_GTK_WIMP_THEME");
  GtkCssProvider *provider;
  GError *error = NULL;
  char *css;

  if (done || display == NULL || !GDK_IS_RISCOS_DISPLAY (display))
    return;
  done = TRUE;
  grip_init ();
  _gtk_riscos_iconbar_init (display);
  if (env && strcmp (env, "0") == 0)
    return;

  css = make_css ();
  icons_init (display);
  /* one line in every program's log, so a report shows what was used */
  g_printerr ("riscos-gtk: theme: Wimp colours %s; Wimp sprites %d of %d; window border sprites"
              " %d of %d; icons from sprites %d of %d; %s%s\n", strstr (css, "@define-color") ? "yes" : "no",
              pool_found, pool_asked, tool_found, tool_asked, icons_found, icons_asked,
              write_failed ? "COULDN'T WRITE the PNGs in " : "PNGs in ", dir ? dir : g_get_tmp_dir ());
  if (g_getenv ("RISCOS_GTK_DEBUG"))
    g_printerr ("riscos-gtk theme:\n%s", css);
  provider = gtk_css_provider_new ();
  if (!gtk_css_provider_load_from_data (provider, css, -1, &error))
    {
      g_printerr ("riscos-gtk: the Wimp theme CSS: %s\n", error->message);
      g_error_free (error);
    }
  else
    gtk_style_context_add_provider_for_screen (gdk_display_get_default_screen (display),
                                               GTK_STYLE_PROVIDER (provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_SETTINGS + 1);
  g_object_unref (provider);
  g_free (css);
}
