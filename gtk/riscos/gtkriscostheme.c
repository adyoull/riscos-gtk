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
 *     theme (*ToolSprites).
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
#include "gtkcssprovider.h"
#include "gtkstylecontext.h"
#include "gtkstyleprovider.h"

#include "riscos/gdkriscos.h"
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>

static char *dir;               /* the PNGs */
/* for the Log: sprites asked for, found, written */
static int pool_asked, pool_found, tool_asked, tool_found, write_failed;
static GPtrArray *files;

static void
remove_files (void)
{
  guint i;

  if (files)
    for (i = 0; i < files->len; i++)
      g_unlink (g_ptr_array_index (files, i));
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
  if (env && strcmp (env, "0") == 0)
    return;

  css = make_css ();
  /* one line in every program's log, so a report shows what was used */
  g_printerr ("riscos-gtk: theme: Wimp colours %s; Wimp sprites %d of %d; window border sprites"
              " %d of %d; %s%s\n", strstr (css, "@define-color") ? "yes" : "no",
              pool_found, pool_asked, tool_found, tool_asked,
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
