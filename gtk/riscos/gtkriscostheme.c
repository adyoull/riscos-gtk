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
 *     range adjuster arrows (left, right).
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

/* Write Wimp sprite name as a PNG; its file URI (and size), or NULL */
static char *
sprite_uri (const char *name, int *w, int *h)
{
  GdkPixbuf *pb = gdk_riscos_wimp_sprite (name);
  char *file, *uri;

  if (pb == NULL)
    return NULL;
  if (dir == NULL)
    {
      dir = g_dir_make_tmp ("riscos-gtk-XXXXXX", NULL);
      if (dir == NULL)
        {
          g_object_unref (pb);
          return NULL;
        }
      files = g_ptr_array_new_with_free_func (g_free);
      atexit (remove_files);
    }
  file = g_strdup_printf ("%s/%s.png", dir, name);
  if (!gdk_pixbuf_save (pb, file, "png", NULL, NULL))
    {
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

  /* number range adjusters */
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
