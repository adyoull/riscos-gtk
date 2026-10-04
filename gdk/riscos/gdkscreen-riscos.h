/* riscos-gtk: from GTK 3.24.41 gdk/broadway (client side), renamed
   broadway -> riscos; see riscos-gtk LICENCES.md. LGPL-2.1-or-later. */
/*
 * gdkscreen-riscos.h
 * 
 * Copyright 2001 Sun Microsystems Inc. 
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

#ifndef __GDK_RISCOS_SCREEN_H__
#define __GDK_RISCOS_SCREEN_H__

#include <gdk/gdkscreenprivate.h>
#include <gdk/gdkvisual.h>
#include "gdkprivate-riscos.h"

G_BEGIN_DECLS

typedef struct _GdkRiscosScreen GdkRiscosScreen;
typedef struct _GdkRiscosScreenClass GdkRiscosScreenClass;

#define GDK_TYPE_RISCOS_SCREEN              (gdk_riscos_screen_get_type ())
#define GDK_RISCOS_SCREEN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_RISCOS_SCREEN, GdkRiscosScreen))
#define GDK_RISCOS_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_RISCOS_SCREEN, GdkRiscosScreenClass))
#define GDK_IS_RISCOS_SCREEN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_RISCOS_SCREEN))
#define GDK_IS_RISCOS_SCREEN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_RISCOS_SCREEN))
#define GDK_RISCOS_SCREEN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_RISCOS_SCREEN, GdkRiscosScreenClass))

typedef struct _GdkRiscosMonitor GdkRiscosMonitor;

struct _GdkRiscosScreen
{
  GdkScreen parent_instance;

  GdkDisplay *display;
  GdkWindow *root_window;

  int width;
  int height;

  /* Visual Part */
  GdkVisual **visuals;
  gint nvisuals;
  GdkVisual *system_visual;
  GdkVisual *rgba_visual;
  gint available_depths[7];
  gint navailable_depths;
  GdkVisualType available_types[6];
  gint navailable_types;
};

struct _GdkRiscosScreenClass
{
  GdkScreenClass parent_class;

  void (* window_manager_changed) (GdkRiscosScreen *screen);
};

GType       gdk_riscos_screen_get_type (void);
GdkScreen * _gdk_riscos_screen_new      (GdkDisplay *display,
					   gint	  screen_number);
void _gdk_riscos_screen_setup           (GdkScreen *screen);

G_END_DECLS

#endif /* __GDK_RISCOS_SCREEN_H__ */
