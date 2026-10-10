/* riscos-gtk: from GTK 3.24.41 gdk/broadway (client side), renamed
   broadway -> riscos; see riscos-gtk LICENCES.md. LGPL-2.1-or-later. */
/* GDK - The GIMP Drawing Kit
 * gdkdisplay-riscos.c
 * 
 * Copyright 2001 Sun Microsystems Inc.
 * Copyright (C) 2004 Nokia Corporation
 *
 * Erwann Chenede <erwann.chenede@sun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkdisplay-riscos.h"

#include "gdkdisplay.h"
#include "gdkeventsource.h"
#include "gdkscreen.h"
#include "gdkscreen-riscos.h"
#include "gdkmonitor-riscos.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager-riscos.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>

static void   gdk_riscos_display_dispose            (GObject            *object);
static void   gdk_riscos_display_finalize           (GObject            *object);

#if 0
#define DEBUG_WEBSOCKETS 1
#endif

G_DEFINE_TYPE (GdkRiscosDisplay, gdk_riscos_display, GDK_TYPE_DISPLAY)

static void
gdk_riscos_display_init (GdkRiscosDisplay *display)
{
  display->id_ht = g_hash_table_new (NULL, NULL);

  display->monitor = g_object_new (GDK_TYPE_RISCOS_MONITOR,
                                   "display", display,
                                   NULL);
  gdk_monitor_set_manufacturer (display->monitor, "browser");
  gdk_monitor_set_model (display->monitor, "0");
  gdk_monitor_set_scale_factor (display->monitor, _gdk_riscos_server_get_scale ());
}

static void
gdk_event_init (GdkDisplay *display)
{
  GdkRiscosDisplay *riscos_display;

  riscos_display = GDK_RISCOS_DISPLAY (display);
  riscos_display->event_source = _gdk_riscos_event_source_new (display);
}

GdkDisplay *
_gdk_riscos_display_open (const gchar *display_name)
{
  GdkDisplay *display;
  GdkRiscosDisplay *riscos_display;
  GError *error = NULL;

  /* riscos-gtk: GSettings (the file choosers' settings) in memory. GLib
     would pick its keyfile backend, which writes under UnixHome and wants
     a file monitor RISC OS hasn't got ("Unable to find default local file
     monitor type", seen on the Pi). A program can still choose another
     with GSETTINGS_BACKEND. */
  g_setenv ("GSETTINGS_BACKEND", "memory", FALSE);

  display = g_object_new (GDK_TYPE_RISCOS_DISPLAY, NULL);
  riscos_display = GDK_RISCOS_DISPLAY (display);

  /* initialize the display's screens */
  riscos_display->screens = g_new (GdkScreen *, 1);
  riscos_display->screens[0] = _gdk_riscos_screen_new (display, 0);

  /* We need to initialize events after we have the screen
   * structures in places
   */
  _gdk_riscos_screen_events_init (riscos_display->screens[0]);

  /*set the default screen */
  riscos_display->default_screen = riscos_display->screens[0];

  display->device_manager = _gdk_riscos_device_manager_new (display);

  gdk_event_init (display);

  _gdk_riscos_display_init_dnd (display);

  _gdk_riscos_screen_setup (riscos_display->screens[0]);

  if (display_name == NULL)
    display_name = g_getenv ("RISCOS_DISPLAY");

  riscos_display->server = _gdk_riscos_server_new (display_name, &error);
  if (riscos_display->server == NULL)
    {
      GDK_NOTE (MISC, g_message ("Unable to init Riscos server: %s\n", error->message));
      g_error_free (error);
      return NULL;
    }

  /* riscos: the screen's real size (the screen starts at 1024x768) */
  {
    RiscosInputScreenResizeNotify msg = { { 0 } };
    GdkWindow *root = gdk_screen_get_root_window (riscos_display->screens[0]);
    int w, h;

    _gdk_riscos_server_get_screen_size (riscos_display->server, &w, &h);
    msg.width = w;
    msg.height = h;
    root->width = w;
    root->height = h;
    _gdk_window_update_size (root);
    _gdk_riscos_screen_size_changed (riscos_display->screens[0], &msg);
  }

  g_signal_emit_by_name (display, "opened");

  return display;
}

static const gchar *
gdk_riscos_display_get_name (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return (gchar *) "Riscos";
}

static GdkScreen *
gdk_riscos_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_RISCOS_DISPLAY (display)->default_screen;
}

static void
gdk_riscos_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
}

static void
gdk_riscos_display_sync (GdkDisplay *display)
{
  GdkRiscosDisplay *riscos_display = GDK_RISCOS_DISPLAY (display);

  g_return_if_fail (GDK_IS_RISCOS_DISPLAY (display));

  _gdk_riscos_server_sync (riscos_display->server);
}

static void
gdk_riscos_display_flush (GdkDisplay *display)
{
  GdkRiscosDisplay *riscos_display = GDK_RISCOS_DISPLAY (display);

  g_return_if_fail (GDK_IS_RISCOS_DISPLAY (display));

  _gdk_riscos_server_flush (riscos_display->server);
}

static gboolean
gdk_riscos_display_has_pending (GdkDisplay *display)
{
  return FALSE;
}

static GdkWindow *
gdk_riscos_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return NULL;
}

static void
gdk_riscos_display_dispose (GObject *object)
{
  GdkRiscosDisplay *riscos_display = GDK_RISCOS_DISPLAY (object);

  _gdk_screen_close (riscos_display->screens[0]);

  if (riscos_display->event_source)
    {
      g_source_destroy (riscos_display->event_source);
      g_source_unref (riscos_display->event_source);
      riscos_display->event_source = NULL;
    }

  G_OBJECT_CLASS (gdk_riscos_display_parent_class)->dispose (object);
}

static void
gdk_riscos_display_finalize (GObject *object)
{
  GdkRiscosDisplay *riscos_display = GDK_RISCOS_DISPLAY (object);

  /* Keymap */
  if (riscos_display->keymap)
    g_object_unref (riscos_display->keymap);

  _gdk_riscos_cursor_display_finalize (GDK_DISPLAY_OBJECT(riscos_display));

  /* Free all GdkScreens */
  g_object_unref (riscos_display->screens[0]);
  g_free (riscos_display->screens);

  g_object_unref (riscos_display->monitor);

  G_OBJECT_CLASS (gdk_riscos_display_parent_class)->finalize (object);
}

static void
gdk_riscos_display_notify_startup_complete (GdkDisplay  *display,
					      const gchar *startup_id)
{
}

static gboolean
gdk_riscos_display_supports_selection_notification (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_riscos_display_request_selection_notification (GdkDisplay *display,
						     GdkAtom     selection)

{
    return FALSE;
}

static gboolean
gdk_riscos_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

static void
gdk_riscos_display_store_clipboard (GdkDisplay    *display,
				      GdkWindow     *clipboard_window,
				      guint32        time_,
				      const GdkAtom *targets,
				      gint           n_targets)
{
}

static gboolean
gdk_riscos_display_supports_shapes (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_riscos_display_supports_input_shapes (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_riscos_display_supports_composite (GdkDisplay *display)
{
  return FALSE;
}

static gulong
gdk_riscos_display_get_next_serial (GdkDisplay *display)
{
  GdkRiscosDisplay *riscos_display;
  riscos_display = GDK_RISCOS_DISPLAY (display);

  return _gdk_riscos_server_get_next_serial (riscos_display->server);
}

void
gdk_riscos_display_show_keyboard (GdkRiscosDisplay *display)
{
  g_return_if_fail (GDK_IS_RISCOS_DISPLAY (display));

  _gdk_riscos_server_set_show_keyboard (display->server, TRUE);
}

void
gdk_riscos_display_hide_keyboard (GdkRiscosDisplay *display)
{
  g_return_if_fail (GDK_IS_RISCOS_DISPLAY (display));

  _gdk_riscos_server_set_show_keyboard (display->server, FALSE);
}

static int
gdk_riscos_display_get_n_monitors (GdkDisplay *display)
{
  return 1;
}

static GdkMonitor *
gdk_riscos_display_get_monitor (GdkDisplay *display,
                                  int         monitor_num)
{
  GdkRiscosDisplay *riscos_display = GDK_RISCOS_DISPLAY (display);

  if (monitor_num == 0)
    return riscos_display->monitor;

  return NULL;
}

static GdkMonitor *
gdk_riscos_display_get_primary_monitor (GdkDisplay *display)
{
  GdkRiscosDisplay *riscos_display = GDK_RISCOS_DISPLAY (display);

  return riscos_display->monitor;
}

static void
gdk_riscos_display_class_init (GdkRiscosDisplayClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_riscos_display_dispose;
  object_class->finalize = gdk_riscos_display_finalize;

  display_class->window_type = GDK_TYPE_RISCOS_WINDOW;

  display_class->get_name = gdk_riscos_display_get_name;
  display_class->get_default_screen = gdk_riscos_display_get_default_screen;
  display_class->beep = gdk_riscos_display_beep;
  display_class->sync = gdk_riscos_display_sync;
  display_class->flush = gdk_riscos_display_flush;
  display_class->has_pending = gdk_riscos_display_has_pending;
  display_class->queue_events = _gdk_riscos_display_queue_events;
  display_class->get_default_group = gdk_riscos_display_get_default_group;
  display_class->supports_selection_notification = gdk_riscos_display_supports_selection_notification;
  display_class->request_selection_notification = gdk_riscos_display_request_selection_notification;
  display_class->supports_clipboard_persistence = gdk_riscos_display_supports_clipboard_persistence;
  display_class->store_clipboard = gdk_riscos_display_store_clipboard;
  display_class->supports_shapes = gdk_riscos_display_supports_shapes;
  display_class->supports_input_shapes = gdk_riscos_display_supports_input_shapes;
  display_class->supports_composite = gdk_riscos_display_supports_composite;
  display_class->get_cursor_for_type = _gdk_riscos_display_get_cursor_for_type;
  display_class->get_cursor_for_name = _gdk_riscos_display_get_cursor_for_name;
  display_class->get_cursor_for_surface = _gdk_riscos_display_get_cursor_for_surface;
  display_class->get_default_cursor_size = _gdk_riscos_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_riscos_display_get_maximal_cursor_size;
  display_class->supports_cursor_alpha = _gdk_riscos_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_riscos_display_supports_cursor_color;

  display_class->before_process_all_updates = _gdk_riscos_display_before_process_all_updates;
  display_class->after_process_all_updates = _gdk_riscos_display_after_process_all_updates;
  display_class->get_next_serial = gdk_riscos_display_get_next_serial;
  display_class->notify_startup_complete = gdk_riscos_display_notify_startup_complete;
  display_class->create_window_impl = _gdk_riscos_display_create_window_impl;
  display_class->get_keymap = _gdk_riscos_display_get_keymap;
  display_class->get_selection_owner = _gdk_riscos_display_get_selection_owner;
  display_class->set_selection_owner = _gdk_riscos_display_set_selection_owner;
  display_class->send_selection_notify = _gdk_riscos_display_send_selection_notify;
  display_class->get_selection_property = _gdk_riscos_display_get_selection_property;
  display_class->convert_selection = _gdk_riscos_display_convert_selection;
  display_class->text_property_to_utf8_list = _gdk_riscos_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_riscos_display_utf8_to_string_target;

  display_class->get_n_monitors = gdk_riscos_display_get_n_monitors;
  display_class->get_monitor = gdk_riscos_display_get_monitor;
  display_class->get_primary_monitor = gdk_riscos_display_get_primary_monitor;
}

