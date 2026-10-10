/* riscos-gtk: from GTK 3.24.41 gdk/broadway (client side), renamed
   broadway -> riscos; see riscos-gtk LICENCES.md. LGPL-2.1-or-later. */
/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-2007 Peter Mattis, Spencer Kimball,
 * Josh MacDonald, Ryan Lortie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkwindow-riscos.h"
#include "gdkscreen-riscos.h"

#include "gdkriscosdisplay.h"
#include "gdkdisplay.h"
#include "gdkwindow.h"
#include "gdkwindowimpl.h"
#include "gdkdisplay-riscos.h"
#include "gdkprivate-riscos.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkeventsource.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Forward declarations */
static void     gdk_window_riscos_set_background     (GdkWindow      *window,
                                                   cairo_pattern_t *pattern);

static void        gdk_window_impl_riscos_finalize   (GObject            *object);

static const cairo_user_data_key_t gdk_riscos_cairo_key;

#define WINDOW_IS_TOPLEVEL_OR_FOREIGN(window) \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

#define WINDOW_IS_TOPLEVEL(window)		     \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&   \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

struct _GdkRiscosWindow {
  GdkWindow parent;
};

struct _GdkRiscosWindowClass {
  GdkWindowClass parent_class;
};

G_DEFINE_TYPE (GdkRiscosWindow, gdk_riscos_window, GDK_TYPE_WINDOW)

static void
gdk_riscos_window_class_init (GdkRiscosWindowClass *riscos_window_class)
{
}

static void
gdk_riscos_window_init (GdkRiscosWindow *riscos_window)
{
}

G_DEFINE_TYPE (GdkWindowImplRiscos,
	       gdk_window_impl_riscos,
	       GDK_TYPE_WINDOW_IMPL)

static GdkDisplay *
find_riscos_display (void)
{
  GdkDisplay *display;
  GSList *list, *l;

  display = NULL;

  list = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (l = list; l; l = l->next)
    {
      if (GDK_IS_RISCOS_DISPLAY (l->data))
        {
          display = l->data;
          break; 
        }
    }
  g_slist_free (list);

  return display;
}

static void
update_dirty_windows_and_sync (void)
{
  GList *l;
  GdkRiscosDisplay *display;
  gboolean updated_surface;

  display = GDK_RISCOS_DISPLAY (find_riscos_display ());
  g_assert (display != NULL);

  updated_surface = FALSE;
  for (l = display->toplevels; l != NULL; l = l->next)
    {
      GdkWindowImplRiscos *impl = l->data;

      if (impl->dirty)
	{
	  impl->dirty = FALSE;
	  updated_surface = TRUE;
	  _gdk_riscos_server_window_update (display->server,
					      impl->id,
					      impl->surface);
	}
    }

  /* We sync here to ensure all references to the impl->surface memory
     is done, as we may later paint new data in them. */
  if (updated_surface)
    gdk_display_sync (GDK_DISPLAY (display));
  else
    gdk_display_flush (GDK_DISPLAY (display));
}

static guint flush_id = 0;

static gboolean
flush_idle (gpointer data)
{
  flush_id = 0;

  gdk_display_flush (find_riscos_display ());

  return FALSE;
}

/* We need to flush in an idle rather than AFTER_PAINT, as the clock
   is frozen during e.g. window resizes so the paint will not happen
   and the window resize request is never flushed. */
static void
queue_flush (GdkWindow *window)
{
  if (flush_id == 0)
    {
      flush_id = gdk_threads_add_idle (flush_idle, NULL);
      g_source_set_name_by_id (flush_id, "[gtk+] flush_idle");
    }
}

static void
gdk_window_impl_riscos_init (GdkWindowImplRiscos *impl)
{
  impl->toplevel_window_type = -1;
  impl->device_cursor = g_hash_table_new_full (NULL, NULL, NULL,
                                               (GDestroyNotify) g_object_unref);
}

static void
gdk_window_impl_riscos_finalize (GObject *object)
{
  GdkWindow *wrapper;
  GdkWindowImplRiscos *impl;
  GdkRiscosDisplay *riscos_display;

  g_return_if_fail (GDK_IS_WINDOW_IMPL_RISCOS (object));

  impl = GDK_WINDOW_IMPL_RISCOS (object);

  wrapper = impl->wrapper;

  _gdk_riscos_window_grab_check_destroy (wrapper);

  riscos_display = GDK_RISCOS_DISPLAY (gdk_window_get_display (impl->wrapper));

  g_hash_table_remove (riscos_display->id_ht, GINT_TO_POINTER(impl->id));

  if (impl->cursor)
    g_object_unref (impl->cursor);

  g_hash_table_destroy (impl->device_cursor);

  riscos_display->toplevels = g_list_remove (riscos_display->toplevels, impl);

  G_OBJECT_CLASS (gdk_window_impl_riscos_parent_class)->finalize (object);
}

void
_gdk_riscos_screen_init_root_window (GdkScreen * screen)
{
  GdkWindow *window;
  GdkWindowImplRiscos *impl;
  GdkRiscosScreen *riscos_screen;

  riscos_screen = GDK_RISCOS_SCREEN (screen);

  g_assert (riscos_screen->root_window == NULL);

  riscos_screen->root_window = g_object_new (GDK_TYPE_RISCOS_WINDOW, NULL);

  window = riscos_screen->root_window;
  window->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_RISCOS, NULL);
  window->impl_window = window;
  window->visual = gdk_screen_get_system_visual (screen);

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  impl->screen = screen;
  impl->wrapper = window;
  impl->id = 0;

  window->window_type = GDK_WINDOW_ROOT;
  window->depth = 24;

  window->x = 0;
  window->y = 0;
  window->abs_x = 0;
  window->abs_y = 0;
  window->width = gdk_screen_get_width (screen);
  window->height = gdk_screen_get_height (screen);
  window->viewable = TRUE;

  _gdk_window_update_size (riscos_screen->root_window);
}

static void
on_frame_clock_after_paint (GdkFrameClock *clock,
                            GdkWindow     *window)
{
  update_dirty_windows_and_sync ();
}

static void
connect_frame_clock (GdkWindow *window)
{
  if (WINDOW_IS_TOPLEVEL (window))
    {
      GdkFrameClock *frame_clock = gdk_window_get_frame_clock (window);

      g_signal_connect (frame_clock, "after-paint",
                        G_CALLBACK (on_frame_clock_after_paint), window);
    }
}

void
_gdk_riscos_display_create_window_impl (GdkDisplay    *display,
					  GdkWindow     *window,
					  GdkWindow     *real_parent,
					  GdkScreen     *screen,
					  GdkEventMask   event_mask,
					  GdkWindowAttr *attributes,
					  gint           attributes_mask)
{
  GdkWindowImplRiscos *impl;
  GdkRiscosDisplay *riscos_display;

  riscos_display = GDK_RISCOS_DISPLAY (display);

  impl = g_object_new (GDK_TYPE_WINDOW_IMPL_RISCOS, NULL);
  window->impl = (GdkWindowImpl *)impl;
  /* riscos-gtk: GTK gives a popup's type hint here, not with
     gdk_window_set_type_hint (a drag icon: GDK_WINDOW_TYPE_HINT_DND) */
  if (attributes_mask & GDK_WA_TYPE_HINT)
    impl->type_hint = attributes->type_hint;
  impl->id = _gdk_riscos_server_new_window (riscos_display->server,
					      window->x,
					      window->y,
					      window->width,
					      window->height,
					      window->window_type == GDK_WINDOW_TEMP);
  g_hash_table_insert (riscos_display->id_ht, GINT_TO_POINTER(impl->id), window);
  /* riscos: the title comes with the attributes (as on X11/win32/quartz) */
  if (window->window_type == GDK_WINDOW_TOPLEVEL)
    _gdk_riscos_server_window_set_title (riscos_display->server, impl->id,
                                         (attributes_mask & GDK_WA_TITLE) && attributes->title ?
                                         attributes->title : g_get_application_name ());
  impl->wrapper = window;

  impl->screen = screen;

  g_assert (window->window_type == GDK_WINDOW_TOPLEVEL ||
	    window->window_type == GDK_WINDOW_TEMP);
  g_assert (GDK_WINDOW_TYPE (window->parent) == GDK_WINDOW_ROOT);

  riscos_display->toplevels = g_list_prepend (riscos_display->toplevels, impl);

  connect_frame_clock (window);
}

void
_gdk_riscos_window_resize_surface (GdkWindow *window)
{
  GdkWindowImplRiscos *impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  if (impl->surface)
    {
      cairo_surface_destroy (impl->surface);

      impl->surface = _gdk_riscos_server_create_surface (gdk_window_get_width (impl->wrapper),
							   gdk_window_get_height (impl->wrapper));
    }

  if (impl->ref_surface)
    {
      cairo_surface_set_user_data (impl->ref_surface, &gdk_riscos_cairo_key,
				   NULL, NULL);
      impl->ref_surface = NULL;
    }

  gdk_window_invalidate_rect (window, NULL, TRUE);
}

static void
ref_surface_destroyed (void *data)
{
  GdkWindowImplRiscos *impl = data;

  impl->ref_surface = NULL;
}

/* 2 in an EX0 EY0 (180 dpi) mode: GTK lays out in 90 dpi pixels and
   draws at the screen's resolution (gdkriscos-server.c read_screen) */
static gint
gdk_window_riscos_get_scale_factor (GdkWindow *window)
{
  return _gdk_riscos_server_get_scale ();
}

static cairo_surface_t *
gdk_window_riscos_ref_cairo_surface (GdkWindow *window)
{
  GdkWindowImplRiscos *impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  int w, h;

  if (GDK_IS_WINDOW_IMPL_RISCOS (window) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  w = gdk_window_get_width (impl->wrapper);
  h = gdk_window_get_height (impl->wrapper);

  /* Create actual backing store if missing */
  if (!impl->surface)
    impl->surface = _gdk_riscos_server_create_surface (w, h);

  /* Create a destroyable surface referencing the real one */
  if (!impl->ref_surface)
    {
      impl->ref_surface =
	cairo_surface_create_for_rectangle (impl->surface,
					    0, 0,
					    w, h);
      if (impl->ref_surface)
	cairo_surface_set_user_data (impl->ref_surface, &gdk_riscos_cairo_key,
				     impl, ref_surface_destroyed);
    }
  else
    cairo_surface_reference (impl->ref_surface);

  return impl->ref_surface;
}

static void
_gdk_riscos_window_destroy (GdkWindow *window,
			      gboolean   recursing,
			      gboolean   foreign_destroy)
{
  GdkWindowImplRiscos *impl;
  GdkRiscosDisplay *riscos_display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  _gdk_riscos_selection_window_destroyed (window);
  _gdk_riscos_window_grab_check_destroy (window);

  if (impl->ref_surface)
    {
      cairo_surface_finish (impl->ref_surface);
      cairo_surface_set_user_data (impl->ref_surface, &gdk_riscos_cairo_key,
				   NULL, NULL);
    }

  if (impl->surface)
    {
      cairo_surface_destroy (impl->surface);
      impl->surface = NULL;
    }

  riscos_display = GDK_RISCOS_DISPLAY (gdk_window_get_display (window));
  g_hash_table_remove (riscos_display->id_ht, GINT_TO_POINTER(impl->id));

  _gdk_riscos_server_destroy_window (riscos_display->server,
				       impl->id);
}

static void
gdk_riscos_window_destroy_foreign (GdkWindow *window)
{
}

/* This function is called when the XWindow is really gone.
 */
static void
gdk_riscos_window_destroy_notify (GdkWindow *window)
{
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("GdkWindow %p unexpectedly destroyed", window);

      _gdk_window_destroy (window, TRUE);
    }

  g_object_unref (window);
}

static void
gdk_window_riscos_show (GdkWindow *window, gboolean already_mapped)
{
  GdkWindowImplRiscos *impl;
  GdkRiscosDisplay *riscos_display;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  impl->visible = TRUE;

  if (window->event_mask & GDK_STRUCTURE_MASK)
    _gdk_make_event (GDK_WINDOW (window), GDK_MAP, NULL, FALSE);

  if (window->parent && window->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
    _gdk_make_event (GDK_WINDOW (window), GDK_MAP, NULL, FALSE);

  riscos_display = GDK_RISCOS_DISPLAY (gdk_window_get_display (window));
  /* riscos-gtk: GTK's drag icon isn't shown: the Wimp draws a dashed box
     while dragging (gdkdnd-riscos.c), as RISC OS programs do */
  if (impl->type_hint == GDK_WINDOW_TYPE_HINT_DND)
    return;
  /* riscos-gtk: a program's window or dialogue box that GTK hasn't placed
     (no position asked for: GTK_WIN_POS_NONE, no gtk_window_move) opens
     in the middle of the screen the first time, as RISC OS programs'
     windows do; after that it reopens where it was */
  if (!impl->placed && window->window_type == GDK_WINDOW_TOPLEVEL)
    {
      impl->placed = TRUE;
      if (!(impl->geometry_hints_mask & (GDK_HINT_POS | GDK_HINT_USER_POS)) &&
          g_strcmp0 (g_getenv ("RISCOS_GTK_CENTRE"), "0") != 0)
        {
          int sw, sh, x, y;
          _gdk_riscos_server_get_screen_size (riscos_display->server, &sw, &sh);
          x = MAX (0, (sw - window->width) / 2);
          y = MAX (0, (sh - window->height) / 2);
          window->x = x;
          window->y = y;
          _gdk_riscos_server_window_move_resize (riscos_display->server, impl->id, TRUE,
                                                 x, y, window->width, window->height);
        }
    }
  if (_gdk_riscos_server_window_show (riscos_display->server, impl->id))
    queue_flush (window);

}

static void
gdk_window_riscos_hide (GdkWindow *window)
{
  GdkWindowImplRiscos *impl;
  GdkRiscosDisplay *riscos_display;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  impl->visible = FALSE;

  if (window->event_mask & GDK_STRUCTURE_MASK)
    _gdk_make_event (GDK_WINDOW (window), GDK_UNMAP, NULL, FALSE);

  if (window->parent && window->parent->event_mask & GDK_SUBSTRUCTURE_MASK)
    _gdk_make_event (GDK_WINDOW (window), GDK_UNMAP, NULL, FALSE);

  riscos_display = GDK_RISCOS_DISPLAY (gdk_window_get_display (window));

  _gdk_riscos_window_grab_check_unmap (window,
					 _gdk_riscos_server_get_next_serial (riscos_display->server));

  if (_gdk_riscos_server_window_hide (riscos_display->server, impl->id))
    queue_flush (window);

  _gdk_window_clear_update_area (window);
}

static void
gdk_window_riscos_withdraw (GdkWindow *window)
{
  gdk_window_riscos_hide (window);
}

static void
gdk_window_riscos_move_resize (GdkWindow *window,
				 gboolean   with_move,
				 gint       x,
				 gint       y,
				 gint       width,
				 gint       height)
{
  GdkWindowImplRiscos *impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  GdkRiscosDisplay *riscos_display;
  gboolean size_changed;

  size_changed = FALSE;

  riscos_display = GDK_RISCOS_DISPLAY (gdk_window_get_display (window));

  if (width > 0 || height > 0)
    {
      if (width < 1)
	width = 1;

      if (height < 1)
	height = 1;

      if (width != window->width ||
	  height != window->height)
	{
	  size_changed = TRUE;

	  /* Resize clears the content */
	  impl->dirty = TRUE;
	  impl->last_synced = FALSE;

	  window->width = width;
	  window->height = height;
	  _gdk_riscos_window_resize_surface (window);
	}
    }

  _gdk_riscos_server_window_move_resize (riscos_display->server,
					   impl->id,
					   with_move,
					   x, y,
					   window->width, window->height);
  queue_flush (window);
  if (size_changed)
    window->resize_count++;
}

static gboolean
gdk_window_riscos_reparent (GdkWindow *window,
			      GdkWindow *new_parent,
			      gint       x,
			      gint       y)
{
  return FALSE;
}

static void
gdk_window_riscos_raise (GdkWindow *window)
{
}

static void
gdk_window_riscos_restack_under (GdkWindow *window,
				   GList *native_siblings /* in requested order, first is bottom-most */)
{
}

static void
gdk_window_riscos_restack_toplevel (GdkWindow *window,
				      GdkWindow *sibling,
				      gboolean   above)
{
}

static void
gdk_window_riscos_lower (GdkWindow *window)
{
}


static void
gdk_riscos_window_focus (GdkWindow *window,
			   guint32    timestamp)
{
  GdkWindowImplRiscos *impl;
  GdkRiscosDisplay *riscos_display;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !window->accept_focus)
    return;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  riscos_display = GDK_RISCOS_DISPLAY (gdk_window_get_display (window));
  _gdk_riscos_server_window_focus (riscos_display->server,
				     impl->id);
}

/* riscos-gtk: the Wimp window's furniture from what GTK asked for, as
   RISC OS windows have it (Style Guide): a title bar unless undecorated;
   a close icon unless the window can't be closed (a dialogue box with a
   Cancel button: gtk_window_set_deletable); toggle and adjust size if it
   can be resized; a back icon on ordinary windows only, not dialogue
   boxes. */
static gboolean
func_on (GdkWindowImplRiscos *impl, GdkWMFunction f)
{
  if (!impl->functions_set)
    return TRUE;
  if (impl->functions & GDK_FUNC_ALL)
    return !(impl->functions & f);          /* ALL but those listed */
  return (impl->functions & f) != 0;
}

static void
update_furniture (GdkWindow *window)
{
  GdkWindowImplRiscos *impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  unsigned f = 0;
  gboolean fixed = (impl->geometry_hints_mask & GDK_HINT_MIN_SIZE) &&
                   (impl->geometry_hints_mask & GDK_HINT_MAX_SIZE) &&
                   impl->geometry_hints.min_width == impl->geometry_hints.max_width &&
                   impl->geometry_hints.min_height == impl->geometry_hints.max_height;
  gboolean dialog = impl->type_hint == GDK_WINDOW_TYPE_HINT_DIALOG ||
                    impl->type_hint == GDK_WINDOW_TYPE_HINT_UTILITY ||
                    impl->type_hint == GDK_WINDOW_TYPE_HINT_SPLASHSCREEN;

  if (GDK_WINDOW_DESTROYED (window) || !WINDOW_IS_TOPLEVEL (window))
    return;
  if (!impl->decorations_set || impl->decorations != 0)
    {
      f |= RISCOS_FURNITURE_TITLE;
      if (func_on (impl, GDK_FUNC_CLOSE))
        f |= RISCOS_FURNITURE_CLOSE;
      if (func_on (impl, GDK_FUNC_RESIZE) && !fixed)
        f |= RISCOS_FURNITURE_RESIZE;
      if (!dialog)
        f |= RISCOS_FURNITURE_BACK;
    }
  _gdk_riscos_server_window_set_furniture (GDK_RISCOS_DISPLAY (gdk_window_get_display (window))->server,
                                           impl->id, f);
}

/**
 * gdk_riscos_window_set_transient_dbox:
 * @window: a toplevel
 * @transient: %TRUE to open it as a transient dialogue box
 *
 * A transient dialogue box is opened as RISC OS opens a dialogue box from
 * a menu (Wimp_CreateMenu with its window handle): a click anywhere else
 * closes it, and GTK gets a delete event.
 */
void
gdk_riscos_window_set_transient_dbox (GdkWindow *window, gboolean transient)
{
  GdkWindowImplRiscos *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  if (!GDK_IS_WINDOW_IMPL_RISCOS (window->impl))
    return;
  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  _gdk_riscos_server_window_set_as_menu (GDK_RISCOS_DISPLAY (gdk_window_get_display (window))->server,
                                         impl->id, transient);
}

/**
 * gdk_riscos_window_get_size_grip:
 * @window: a toplevel
 * @width: (out): the grip's width in pixels
 * @height: (out): its height
 *
 * A resizable document window (not a dialogue box) has a size grip (the
 * desktop's Adjust Size icon sprite) drawn in its bottom right corner.
 *
 * Returns: %TRUE if @window has one
 */
gboolean
gdk_riscos_window_get_size_grip (GdkWindow *window, gint *width, gint *height)
{
  GdkWindowImplRiscos *impl;
  int w = 0, h = 0;
  gboolean has;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  if (!GDK_IS_WINDOW_IMPL_RISCOS (window->impl))
    return FALSE;
  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  has = _gdk_riscos_server_window_get_grip (GDK_RISCOS_DISPLAY (gdk_window_get_display (window))->server,
                                            impl->id, &w, &h);
  if (width)
    *width = w;
  if (height)
    *height = h;
  return has;
}

/**
 * gdk_riscos_window_set_size_grip_column:
 * @window: a toplevel
 * @x: the left of the window's vertical scroll bar, pixels in the window
 * @width: its width (0: the grip goes back to the window's right edge)
 *
 * Draws the size grip under the scroll bar, as wide as it, as the Wimp's
 * Adjust Size icon sits under its vertical scroll bar.
 */
void
gdk_riscos_window_set_size_grip_column (GdkWindow *window, gint x, gint width)
{
  GdkWindowImplRiscos *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  if (!GDK_IS_WINDOW_IMPL_RISCOS (window->impl))
    return;
  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  _gdk_riscos_server_window_set_grip_column (GDK_RISCOS_DISPLAY (gdk_window_get_display (window))->server,
                                             impl->id, x, width, -1);
}

/**
 * gdk_riscos_window_set_size_grip_place:
 * @window: a toplevel
 * @x: the left of the window's vertical scroll bar, pixels in the window
 * @y: the grip's top (the scroll bar's foot), or -1 for the window's foot
 * @width: the bar's width (0: the grip goes back to the window's corner)
 *
 * As gdk_riscos_window_set_size_grip_column(), with the grip at the foot
 * of a scroll bar that ends above the window's foot (a status bar under
 * it), so that it doesn't cover what's under the bar.
 */
void
gdk_riscos_window_set_size_grip_place (GdkWindow *window, gint x, gint y, gint width)
{
  GdkWindowImplRiscos *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  if (!GDK_IS_WINDOW_IMPL_RISCOS (window->impl))
    return;
  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  _gdk_riscos_server_window_set_grip_column (GDK_RISCOS_DISPLAY (gdk_window_get_display (window))->server,
                                             impl->id, x, width, y);
}

/**
 * gdk_riscos_window_set_size_strip:
 * @window: a toplevel
 * @image: (nullable): an image of an empty vertical scroll bar, or %NULL
 *
 * For a window with no scroll bar in its bottom right corner: @image is
 * shown down its right edge, outside the window (the Wimp window grows
 * by its width, as the Wimp's own scroll bar is outside a window's work
 * area), with the size grip at its foot, so the grip doesn't cover the
 * window's contents. %NULL takes it away.
 */
void
gdk_riscos_window_set_size_strip (GdkWindow *window, cairo_surface_t *image)
{
  GdkWindowImplRiscos *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  if (!GDK_IS_WINDOW_IMPL_RISCOS (window->impl))
    return;
  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  _gdk_riscos_server_window_set_strip (GDK_RISCOS_DISPLAY (gdk_window_get_display (window))->server,
                                       impl->id, image);
}

static void
gdk_riscos_window_set_type_hint (GdkWindow        *window,
				   GdkWindowTypeHint hint)
{
  GdkWindowImplRiscos *impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  impl->type_hint = hint;
  update_furniture (window);
  /* a drag icon is never on the screen (see gdk_window_riscos_show) */
  if (hint == GDK_WINDOW_TYPE_HINT_DND)
    _gdk_riscos_server_window_hide (GDK_RISCOS_DISPLAY (gdk_window_get_display (window))->server,
                                    impl->id);
}

static GdkWindowTypeHint
gdk_riscos_window_get_type_hint (GdkWindow *window)
{
  return GDK_WINDOW_IMPL_RISCOS (window->impl)->type_hint;
}

static void
gdk_riscos_window_set_modal_hint (GdkWindow *window,
				    gboolean   modal)
{
  GdkRiscosDisplay *display;
  GdkWindowImplRiscos *impl;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  impl->modal_hint = modal;

  display = GDK_RISCOS_DISPLAY (gdk_window_get_display (impl->wrapper));
  _gdk_riscos_server_window_set_modal_hint (display->server, impl->id, impl->modal_hint);
}

static void
gdk_riscos_window_set_skip_taskbar_hint (GdkWindow *window,
					   gboolean   skips_taskbar)
{
}

static void
gdk_riscos_window_set_skip_pager_hint (GdkWindow *window,
					 gboolean   skips_pager)
{
}

static void
gdk_riscos_window_set_urgency_hint (GdkWindow *window,
				      gboolean   urgent)
{
}

static void
gdk_riscos_window_set_geometry_hints (GdkWindow         *window,
					const GdkGeometry *geometry,
					GdkWindowHints     geom_mask)
{
  GdkWindowImplRiscos *impl;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  impl->geometry_hints = *geometry;
  impl->geometry_hints_mask = geom_mask;
  update_furniture (window);
}

static void
gdk_riscos_window_set_title (GdkWindow   *window,
			       const gchar *title)
{
  GdkWindowImplRiscos *impl;
  GdkRiscosDisplay *riscos_display;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  riscos_display = GDK_RISCOS_DISPLAY (gdk_window_get_display (window));
  _gdk_riscos_server_window_set_title (riscos_display->server, impl->id, title);
}

static void
gdk_riscos_window_set_role (GdkWindow   *window,
			      const gchar *role)
{
}

static void
gdk_riscos_window_set_startup_id (GdkWindow   *window,
				    const gchar *startup_id)
{
}

static void
gdk_riscos_window_set_transient_for (GdkWindow *window,
				       GdkWindow *parent)
{
  GdkRiscosDisplay *display;
  GdkWindowImplRiscos *impl;
  int parent_id;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  parent_id = 0;
  if (parent)
    parent_id = GDK_WINDOW_IMPL_RISCOS (parent->impl)->id;

  impl->transient_for = parent_id;

  display = GDK_RISCOS_DISPLAY (gdk_window_get_display (impl->wrapper));
  _gdk_riscos_server_window_set_transient_for (display->server, impl->id, impl->transient_for);
}

static void
gdk_window_riscos_set_background (GdkWindow      *window,
				    cairo_pattern_t *pattern)
{
  return;
}

static void
gdk_window_riscos_set_device_cursor (GdkWindow *window,
				       GdkDevice *device,
				       GdkCursor *cursor)
{
  GdkWindowImplRiscos *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_IS_DEVICE (device));

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  if (!cursor)
    g_hash_table_remove (impl->device_cursor, device);
  else
    {
      _gdk_riscos_cursor_update_theme (cursor);
      g_hash_table_replace (impl->device_cursor,
                            device, g_object_ref (cursor));
    }

  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_DEVICE_GET_CLASS (device)->set_window_cursor (device, window, cursor);
      /* riscos-gtk: the RISC OS pointer shape for it (gdkpointer-riscos.c) */
      _gdk_riscos_server_window_set_pointer (GDK_RISCOS_DISPLAY (gdk_window_get_display (window))->server,
                                             impl->id, _gdk_riscos_cursor_get_shape (cursor));
    }
}

static void
gdk_window_riscos_get_geometry (GdkWindow *window,
				  gint      *x,
				  gint      *y,
				  gint      *width,
				  gint      *height)
{
  GdkWindowImplRiscos *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  /* TODO: These should really roundtrip to the client to get the current data */

  if (x)
    *x = impl->wrapper->x;
  if (y)
    *y = impl->wrapper->y;
  if (width)
    *width = impl->wrapper->width;
  if (height)
    *height = impl->wrapper->height;

}

static void
gdk_window_riscos_get_root_coords (GdkWindow *window,
				     gint       x,
				     gint       y,
				     gint      *root_x,
				     gint      *root_y)
{
  GdkWindowImplRiscos *impl;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  if (root_x)
    *root_x = x + impl->wrapper->x;
  if (root_y)
    *root_y = y + impl->wrapper->y;
}

static void
gdk_riscos_window_get_frame_extents (GdkWindow    *window,
				       GdkRectangle *rect)
{
  g_return_if_fail (rect != NULL);

  /* TODO: This should take wm frame into account */

  rect->x = window->x;
  rect->y = window->y;
  rect->width = window->width;
  rect->height = window->height;
}

static gboolean
gdk_window_riscos_get_device_state (GdkWindow       *window,
				      GdkDevice       *device,
				      gdouble         *x,
				      gdouble         *y,
				      GdkModifierType *mask)
{
  GdkWindow *child;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  GDK_DEVICE_GET_CLASS (device)->query_state (device, window,
                                              NULL, &child,
                                              NULL, NULL,
                                              x, y, mask);
  return child != NULL;
}

static GdkEventMask
gdk_window_riscos_get_events (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window))
    return 0;

  return 0;
}

static void
gdk_window_riscos_set_events (GdkWindow    *window,
				GdkEventMask  event_mask)
{
  if (!GDK_WINDOW_DESTROYED (window))
    {
    }
}

static void
gdk_window_riscos_shape_combine_region (GdkWindow       *window,
					  const cairo_region_t *shape_region,
					  gint             offset_x,
					  gint             offset_y)
{
}

static void
gdk_window_riscos_input_shape_combine_region (GdkWindow       *window,
						const cairo_region_t *shape_region,
						gint             offset_x,
						gint             offset_y)
{
}


static void
gdk_riscos_window_set_override_redirect (GdkWindow *window,
					   gboolean override_redirect)
{
}

static void
gdk_riscos_window_set_accept_focus (GdkWindow *window,
				      gboolean accept_focus)
{
  accept_focus = accept_focus != FALSE;

  if (window->accept_focus != accept_focus)
    {
      window->accept_focus = accept_focus;
    }
}

static void
gdk_riscos_window_set_focus_on_map (GdkWindow *window,
				      gboolean focus_on_map)
{
  focus_on_map = focus_on_map != FALSE;

  if (window->focus_on_map != focus_on_map)
    {
      window->focus_on_map = focus_on_map;
    }
}


static void
gdk_riscos_window_set_icon_list (GdkWindow *window,
				   GList     *pixbufs)
{
}

static void
gdk_riscos_window_set_icon_name (GdkWindow   *window,
				   const gchar *name)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  g_object_set_qdata (G_OBJECT (window), g_quark_from_static_string ("gdk-icon-name-set"),
		      GUINT_TO_POINTER (name != NULL));
}

static void
gdk_riscos_window_iconify (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
}

static void
gdk_riscos_window_deiconify (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
}

static void
gdk_riscos_window_stick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_riscos_window_unstick (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_riscos_window_maximize (GdkWindow *window)
{
  GdkWindowImplRiscos *impl;
  GdkScreen *screen;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  if (impl->maximized)
    return;

  impl->maximized = TRUE;

  gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_MAXIMIZED);

  impl->pre_maximize_x = window->x;
  impl->pre_maximize_y = window->y;
  impl->pre_maximize_width = window->width;
  impl->pre_maximize_height = window->height;

  screen = gdk_window_get_screen (window);

  gdk_window_move_resize (window, 0, 0,
			  gdk_screen_get_width (screen),
			  gdk_screen_get_height (screen));
}

static void
gdk_riscos_window_unmaximize (GdkWindow *window)
{
  GdkWindowImplRiscos *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  if (!impl->maximized)
    return;

  impl->maximized = FALSE;

  gdk_synthesize_window_state (window, GDK_WINDOW_STATE_MAXIMIZED, 0);

  gdk_window_move_resize (window,
			  impl->pre_maximize_x,
			  impl->pre_maximize_y,
			  impl->pre_maximize_width,
			  impl->pre_maximize_height);
}

static void
gdk_riscos_window_fullscreen (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_riscos_window_unfullscreen (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_riscos_window_set_keep_above (GdkWindow *window,
				    gboolean   setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static void
gdk_riscos_window_set_keep_below (GdkWindow *window, gboolean setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

}

static GdkWindow *
gdk_riscos_window_get_group (GdkWindow *window)
{
  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return NULL;

  return window;
}

static void
gdk_riscos_window_set_group (GdkWindow *window,
			       GdkWindow *leader)
{
}

static void
gdk_riscos_window_set_decorations (GdkWindow      *window,
				     GdkWMDecoration decorations)
{
  GdkWindowImplRiscos *impl;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  impl->decorations = decorations;
  impl->decorations_set = TRUE;
  update_furniture (window);
}

static gboolean
gdk_riscos_window_get_decorations (GdkWindow       *window,
				     GdkWMDecoration *decorations)
{
  gboolean result = FALSE;

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return FALSE;
  if (GDK_WINDOW_IMPL_RISCOS (window->impl)->decorations_set)
    {
      *decorations = GDK_WINDOW_IMPL_RISCOS (window->impl)->decorations;
      result = TRUE;
    }
  return result;
}

static void
gdk_riscos_window_set_functions (GdkWindow    *window,
				   GdkWMFunction functions)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;
  GDK_WINDOW_IMPL_RISCOS (window->impl)->functions = functions;
  GDK_WINDOW_IMPL_RISCOS (window->impl)->functions_set = TRUE;
  update_furniture (window);
}

static cairo_region_t *
gdk_riscos_window_get_shape (GdkWindow *window)
{
  return NULL;
}

static cairo_region_t *
gdk_riscos_window_get_input_shape (GdkWindow *window)
{
  return NULL;
}

static void
gdk_riscos_window_end_paint (GdkWindow *window)
{
  GdkWindowImplRiscos *impl;
  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);
  impl->dirty = TRUE;
}

typedef struct _MoveResizeData MoveResizeData;

struct _MoveResizeData
{
  GdkDisplay *display;

  GdkWindow *moveresize_window;
  GdkWindow *moveresize_emulation_window;
  gboolean is_resize;
  GdkWindowEdge resize_edge;
  gint moveresize_button;
  gint moveresize_x;
  gint moveresize_y;
  gint moveresize_orig_x;
  gint moveresize_orig_y;
  gint moveresize_orig_width;
  gint moveresize_orig_height;
  long moveresize_process_time;
  GdkWindowHints moveresize_geom_mask;
  GdkGeometry moveresize_geometry;
  RiscosInputMsg *moveresize_pending_event;
};

static MoveResizeData *
get_move_resize_data (GdkDisplay *display,
		      gboolean    create)
{
  GdkRiscosDisplay *riscos_display;
  MoveResizeData *mv_resize;

  riscos_display = GDK_RISCOS_DISPLAY (display);

  mv_resize = riscos_display->move_resize_data;

  if (!mv_resize && create)
    {
      mv_resize = g_new0 (MoveResizeData, 1);
      mv_resize->display = display;

      riscos_display->move_resize_data = mv_resize;
    }

  return mv_resize;
}

static void
update_pos (MoveResizeData *mv_resize,
	    gint            new_root_x,
	    gint            new_root_y)
{
  gint dx, dy;

  dx = new_root_x - mv_resize->moveresize_x;
  dy = new_root_y - mv_resize->moveresize_y;

  if (mv_resize->is_resize)
    {
      gint x, y, w, h;

      x = mv_resize->moveresize_orig_x;
      y = mv_resize->moveresize_orig_y;

      w = mv_resize->moveresize_orig_width;
      h = mv_resize->moveresize_orig_height;

      switch (mv_resize->resize_edge)
	{
	case GDK_WINDOW_EDGE_NORTH_WEST:
	  x += dx;
	  y += dy;
	  w -= dx;
	  h -= dy;
	  break;
	case GDK_WINDOW_EDGE_NORTH:
	  y += dy;
	  h -= dy;
	  break;
	case GDK_WINDOW_EDGE_NORTH_EAST:
	  y += dy;
	  h -= dy;
	  w += dx;
	  break;
	case GDK_WINDOW_EDGE_SOUTH_WEST:
	  h += dy;
	  x += dx;
	  w -= dx;
	  break;
	case GDK_WINDOW_EDGE_SOUTH_EAST:
	  w += dx;
	  h += dy;
	  break;
	case GDK_WINDOW_EDGE_SOUTH:
	  h += dy;
	  break;
	case GDK_WINDOW_EDGE_EAST:
	  w += dx;
	  break;
	case GDK_WINDOW_EDGE_WEST:
	  x += dx;
	  w -= dx;
	  break;
	}

      x = MAX (x, 0);
      y = MAX (y, 0);
      w = MAX (w, 1);
      h = MAX (h, 1);

      if (mv_resize->moveresize_geom_mask)
	{
	  gdk_window_constrain_size (&mv_resize->moveresize_geometry,
				     mv_resize->moveresize_geom_mask,
				     w, h, &w, &h);
	}

      gdk_window_move_resize (mv_resize->moveresize_window, x, y, w, h);
    }
  else
    {
      gint x, y;

      x = mv_resize->moveresize_orig_x + dx;
      y = mv_resize->moveresize_orig_y + dy;

      gdk_window_move (mv_resize->moveresize_window, x, y);
    }
}

static void
finish_drag (MoveResizeData *mv_resize)
{
  gdk_window_destroy (mv_resize->moveresize_emulation_window);
  mv_resize->moveresize_emulation_window = NULL;
  g_object_unref (mv_resize->moveresize_window);
  mv_resize->moveresize_window = NULL;
  g_clear_pointer (&mv_resize->moveresize_pending_event, g_free);
}

static gboolean
moveresize_lookahead (GdkDisplay *display,
		      MoveResizeData *mv_resize,
		      RiscosInputMsg *event)
{
  GdkRiscosDisplay *riscos_display;

  riscos_display = GDK_RISCOS_DISPLAY (display);

  return !_gdk_riscos_server_lookahead_event (riscos_display->server, "mb");
}

gboolean
_gdk_riscos_moveresize_handle_event (GdkDisplay *display,
				       RiscosInputMsg *event)
{
  guint button_mask = 0;
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (!mv_resize || !mv_resize->moveresize_window)
    return FALSE;

  button_mask = GDK_BUTTON1_MASK << (mv_resize->moveresize_button - 1);

  switch (event->base.type)
    {
    case RISCOS_EVENT_TOUCH:
      if (event->touch.touch_type == 2) /* END */
        {
          update_pos (mv_resize,
                      event->touch.root_x,
                      event->touch.root_y);

          finish_drag (mv_resize);
        }
      else if (event->touch.touch_type == 1) /* UPDATE */
        {
          if (mv_resize->moveresize_window->resize_count > 0)
            {
              if (mv_resize->moveresize_pending_event)
                *mv_resize->moveresize_pending_event = *event;
              else
                mv_resize->moveresize_pending_event =
                  g_memdup (event, sizeof (RiscosInputMsg));

              break;
            }
          update_pos (mv_resize,
                      event->touch.root_x,
                      event->touch.root_y);
        }

      break;

    case RISCOS_EVENT_POINTER_MOVE:
      if (mv_resize->moveresize_window->resize_count > 0)
	{
	  if (mv_resize->moveresize_pending_event)
	    *mv_resize->moveresize_pending_event = *event;
	  else
	    mv_resize->moveresize_pending_event =
	      g_memdup (event, sizeof (RiscosInputMsg));

	  break;
	}
      if (!moveresize_lookahead (display, mv_resize, event))
	break;

      update_pos (mv_resize,
		  event->pointer.root_x,
		  event->pointer.root_y);

      /* This should never be triggered in normal cases, but in the
       * case where the drag started without an implicit grab being
       * in effect, we could miss the release if it occurs before
       * we grab the pointer; this ensures that we will never
       * get a permanently stuck grab.
       */
      if ((event->pointer.state & button_mask) == 0)
	finish_drag (mv_resize);
      break;

    case RISCOS_EVENT_BUTTON_RELEASE:
      update_pos (mv_resize,
		  event->pointer.root_x,
		  event->pointer.root_y);

      if (event->button.button == mv_resize->moveresize_button)
	finish_drag (mv_resize);
      break;
    }
  return TRUE;
}

gboolean
_gdk_riscos_moveresize_configure_done (GdkDisplay *display,
					 GdkWindow  *window)
{
  RiscosInputMsg *tmp_event;
  MoveResizeData *mv_resize = get_move_resize_data (display, FALSE);

  if (!mv_resize || window != mv_resize->moveresize_window)
    return FALSE;

  if (mv_resize->moveresize_pending_event)
    {
      tmp_event = mv_resize->moveresize_pending_event;
      mv_resize->moveresize_pending_event = NULL;
      _gdk_riscos_moveresize_handle_event (display, tmp_event);
      g_free (tmp_event);
    }

  return TRUE;
}

static void
create_moveresize_window (MoveResizeData *mv_resize,
			  guint32         timestamp)
{
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkGrabStatus status;

  g_assert (mv_resize->moveresize_emulation_window == NULL);

  attributes.x = -100;
  attributes.y = -100;
  attributes.width = 10;
  attributes.height = 10;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = 0;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  mv_resize->moveresize_emulation_window =
    gdk_window_new (gdk_screen_get_root_window (gdk_display_get_default_screen (mv_resize->display)),
		    &attributes,
		    attributes_mask);

  gdk_window_show (mv_resize->moveresize_emulation_window);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  status = gdk_pointer_grab (mv_resize->moveresize_emulation_window,
			     FALSE,
			     GDK_BUTTON_RELEASE_MASK |
			     GDK_POINTER_MOTION_MASK,
			     NULL,
			     NULL,
			     timestamp);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  if (status != GDK_GRAB_SUCCESS)
    {
      /* If this fails, some other client has grabbed the window
       * already.
       */
      finish_drag (mv_resize);
    }

  mv_resize->moveresize_process_time = 0;
}

static void
calculate_unmoving_origin (MoveResizeData *mv_resize)
{
  GdkRectangle rect;
  gint width, height;

  if (mv_resize->moveresize_geom_mask & GDK_HINT_WIN_GRAVITY &&
      mv_resize->moveresize_geometry.win_gravity == GDK_GRAVITY_STATIC)
    {
      gdk_window_get_origin (mv_resize->moveresize_window,
			     &mv_resize->moveresize_orig_x,
			     &mv_resize->moveresize_orig_y);
    }
  else
    {
      gdk_window_get_frame_extents (mv_resize->moveresize_window, &rect);
      gdk_window_get_geometry (mv_resize->moveresize_window,
			       NULL, NULL, &width, &height);

      switch (mv_resize->moveresize_geometry.win_gravity)
	{
	case GDK_GRAVITY_NORTH_WEST:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y;
	  break;
	case GDK_GRAVITY_NORTH:
	  mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
	  mv_resize->moveresize_orig_y = rect.y;
	  break;
	case GDK_GRAVITY_NORTH_EAST:
	  mv_resize->moveresize_orig_x = rect.x + rect.width - width;
	  mv_resize->moveresize_orig_y = rect.y;
	  break;
	case GDK_GRAVITY_WEST:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
	  break;
	case GDK_GRAVITY_CENTER:
	  mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
	  mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
	  break;
	case GDK_GRAVITY_EAST:
	  mv_resize->moveresize_orig_x = rect.x + rect.width - width;
	  mv_resize->moveresize_orig_y = rect.y + rect.height / 2 - height / 2;
	  break;
	case GDK_GRAVITY_SOUTH_WEST:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y + rect.height - height;
	  break;
	case GDK_GRAVITY_SOUTH:
	  mv_resize->moveresize_orig_x = rect.x + rect.width / 2 - width / 2;
	  mv_resize->moveresize_orig_y = rect.y + rect.height - height;
	  break;
	case GDK_GRAVITY_SOUTH_EAST:
	  mv_resize->moveresize_orig_x = rect.x + rect.width - width;
	  mv_resize->moveresize_orig_y = rect.y + rect.height - height;
	  break;
	default:
	  mv_resize->moveresize_orig_x = rect.x;
	  mv_resize->moveresize_orig_y = rect.y;
	  break;
	}
    }
}

static void
gdk_riscos_window_begin_resize_drag (GdkWindow     *window,
				       GdkWindowEdge  edge,
                                       GdkDevice     *device,
				       gint           button,
				       gint           root_x,
				       gint           root_y,
				       guint32        timestamp)
{
  MoveResizeData *mv_resize;
  GdkWindowImplRiscos *impl;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (impl->maximized)
    return;

  mv_resize = get_move_resize_data (gdk_window_get_display (window), TRUE);

  mv_resize->is_resize = TRUE;
  mv_resize->moveresize_button = button;
  mv_resize->resize_edge = edge;
  mv_resize->moveresize_x = root_x;
  mv_resize->moveresize_y = root_y;
  mv_resize->moveresize_window = g_object_ref (window);

  mv_resize->moveresize_orig_width = gdk_window_get_width (window);
  mv_resize->moveresize_orig_height = gdk_window_get_height (window);

  mv_resize->moveresize_geom_mask = impl->geometry_hints_mask;
  mv_resize->moveresize_geometry = impl->geometry_hints;

  calculate_unmoving_origin (mv_resize);

  create_moveresize_window (mv_resize, timestamp);
}

static void
gdk_riscos_window_begin_move_drag (GdkWindow *window,
                                     GdkDevice *device,
				     gint       button,
				     gint       root_x,
				     gint       root_y,
				     guint32    timestamp)
{
  MoveResizeData *mv_resize;
  GdkWindowImplRiscos *impl;

  impl = GDK_WINDOW_IMPL_RISCOS (window->impl);

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL_OR_FOREIGN (window))
    return;

  if (impl->maximized)
    return;

  mv_resize = get_move_resize_data (gdk_window_get_display (window), TRUE);

  mv_resize->is_resize = FALSE;
  mv_resize->moveresize_button = button;
  mv_resize->moveresize_x = root_x;
  mv_resize->moveresize_y = root_y;
  mv_resize->moveresize_window = g_object_ref (window);

  mv_resize->moveresize_orig_width = gdk_window_get_width (window);
  mv_resize->moveresize_orig_height = gdk_window_get_height (window);

  mv_resize->moveresize_geom_mask = impl->geometry_hints_mask;
  mv_resize->moveresize_geometry = impl->geometry_hints;

  calculate_unmoving_origin (mv_resize);

  create_moveresize_window (mv_resize, timestamp);
}

static gboolean
gdk_riscos_window_beep (GdkWindow *window)
{
  return FALSE;
}

static void
gdk_riscos_window_set_opacity (GdkWindow *window,
				 gdouble    opacity)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window) ||
      !WINDOW_IS_TOPLEVEL (window))
    return;

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;
}

static void
gdk_riscos_window_set_composited (GdkWindow *window,
				    gboolean   composited)
{
}

void
_gdk_riscos_display_before_process_all_updates (GdkDisplay *display)
{
}

void
_gdk_riscos_display_after_process_all_updates (GdkDisplay *display)
{
}

guint32
gdk_riscos_get_last_seen_time (GdkWindow  *window)
{
  GdkDisplay *display;

  display = gdk_window_get_display (window);
  return _gdk_riscos_server_get_last_seen_time (GDK_RISCOS_DISPLAY (display)->server);
}

static void
gdk_window_impl_riscos_class_init (GdkWindowImplRiscosClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_CLASS (klass);

  object_class->finalize = gdk_window_impl_riscos_finalize;

  impl_class->ref_cairo_surface = gdk_window_riscos_ref_cairo_surface;
  impl_class->get_scale_factor = gdk_window_riscos_get_scale_factor;
  impl_class->show = gdk_window_riscos_show;
  impl_class->hide = gdk_window_riscos_hide;
  impl_class->withdraw = gdk_window_riscos_withdraw;
  impl_class->set_events = gdk_window_riscos_set_events;
  impl_class->get_events = gdk_window_riscos_get_events;
  impl_class->raise = gdk_window_riscos_raise;
  impl_class->lower = gdk_window_riscos_lower;
  impl_class->restack_under = gdk_window_riscos_restack_under;
  impl_class->restack_toplevel = gdk_window_riscos_restack_toplevel;
  impl_class->move_resize = gdk_window_riscos_move_resize;
  impl_class->set_background = gdk_window_riscos_set_background;
  impl_class->reparent = gdk_window_riscos_reparent;
  impl_class->set_device_cursor = gdk_window_riscos_set_device_cursor;
  impl_class->get_geometry = gdk_window_riscos_get_geometry;
  impl_class->get_root_coords = gdk_window_riscos_get_root_coords;
  impl_class->get_device_state = gdk_window_riscos_get_device_state;
  impl_class->shape_combine_region = gdk_window_riscos_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_window_riscos_input_shape_combine_region;
  impl_class->destroy = _gdk_riscos_window_destroy;
  impl_class->destroy_foreign = gdk_riscos_window_destroy_foreign;
  impl_class->get_shape = gdk_riscos_window_get_shape;
  impl_class->get_input_shape = gdk_riscos_window_get_input_shape;
  impl_class->end_paint = gdk_riscos_window_end_paint;
  impl_class->beep = gdk_riscos_window_beep;

  impl_class->focus = gdk_riscos_window_focus;
  impl_class->set_type_hint = gdk_riscos_window_set_type_hint;
  impl_class->get_type_hint = gdk_riscos_window_get_type_hint;
  impl_class->set_modal_hint = gdk_riscos_window_set_modal_hint;
  impl_class->set_skip_taskbar_hint = gdk_riscos_window_set_skip_taskbar_hint;
  impl_class->set_skip_pager_hint = gdk_riscos_window_set_skip_pager_hint;
  impl_class->set_urgency_hint = gdk_riscos_window_set_urgency_hint;
  impl_class->set_geometry_hints = gdk_riscos_window_set_geometry_hints;
  impl_class->set_title = gdk_riscos_window_set_title;
  impl_class->set_role = gdk_riscos_window_set_role;
  impl_class->set_startup_id = gdk_riscos_window_set_startup_id;
  impl_class->set_transient_for = gdk_riscos_window_set_transient_for;
  impl_class->get_frame_extents = gdk_riscos_window_get_frame_extents;
  impl_class->set_override_redirect = gdk_riscos_window_set_override_redirect;
  impl_class->set_accept_focus = gdk_riscos_window_set_accept_focus;
  impl_class->set_focus_on_map = gdk_riscos_window_set_focus_on_map;
  impl_class->set_icon_list = gdk_riscos_window_set_icon_list;
  impl_class->set_icon_name = gdk_riscos_window_set_icon_name;
  impl_class->iconify = gdk_riscos_window_iconify;
  impl_class->deiconify = gdk_riscos_window_deiconify;
  impl_class->stick = gdk_riscos_window_stick;
  impl_class->unstick = gdk_riscos_window_unstick;
  impl_class->maximize = gdk_riscos_window_maximize;
  impl_class->unmaximize = gdk_riscos_window_unmaximize;
  impl_class->fullscreen = gdk_riscos_window_fullscreen;
  impl_class->unfullscreen = gdk_riscos_window_unfullscreen;
  impl_class->set_keep_above = gdk_riscos_window_set_keep_above;
  impl_class->set_keep_below = gdk_riscos_window_set_keep_below;
  impl_class->get_group = gdk_riscos_window_get_group;
  impl_class->set_group = gdk_riscos_window_set_group;
  impl_class->set_decorations = gdk_riscos_window_set_decorations;
  impl_class->get_decorations = gdk_riscos_window_get_decorations;
  impl_class->set_functions = gdk_riscos_window_set_functions;
  impl_class->begin_resize_drag = gdk_riscos_window_begin_resize_drag;
  impl_class->begin_move_drag = gdk_riscos_window_begin_move_drag;
  impl_class->set_opacity = gdk_riscos_window_set_opacity;
  impl_class->set_composited = gdk_riscos_window_set_composited;
  impl_class->destroy_notify = gdk_riscos_window_destroy_notify;
  impl_class->register_dnd = _gdk_riscos_window_register_dnd;
  impl_class->drag_begin = _gdk_riscos_window_drag_begin;
  impl_class->sync_rendering = _gdk_riscos_window_sync_rendering;
  impl_class->simulate_key = _gdk_riscos_window_simulate_key;
  impl_class->simulate_button = _gdk_riscos_window_simulate_button;
  impl_class->get_property = _gdk_riscos_window_get_property;
  impl_class->change_property = _gdk_riscos_window_change_property;
  impl_class->delete_property = _gdk_riscos_window_delete_property;
  impl_class->get_drag_protocol = _gdk_riscos_window_get_drag_protocol;
}
