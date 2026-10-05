/* riscos-gtk: from GTK 3.24.41 gdk/broadway (client side), renamed
   broadway -> riscos; see riscos-gtk LICENCES.md. LGPL-2.1-or-later. */
/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/* needs to be first because any header might include gdk-pixbuf.h otherwise */
#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "gdkcursor.h"
#include "gdkcursorprivate.h"

#include "gdkprivate-riscos.h"
#include "gdkdisplay-riscos.h"

#include <string.h>
#include <errno.h>

struct _GdkRiscosCursor
{
  GdkCursor cursor;
  int shape;                  /* riscos-gtk: RISCOS_POINTER_* */
};

/* riscos-gtk: the RISC OS pointer for a GTK cursor name (GTK 3 uses CSS
   names) or an old X cursor type */
static const struct { const char *name; int shape; } pointer_names[] = {
  { "text", RISCOS_POINTER_TEXT }, { "vertical-text", RISCOS_POINTER_TEXT },
  { "xterm", RISCOS_POINTER_TEXT },
  { "pointer", RISCOS_POINTER_HAND }, { "hand1", RISCOS_POINTER_HAND },
  { "hand2", RISCOS_POINTER_HAND },
  { "ew-resize", RISCOS_POINTER_H_RESIZE }, { "col-resize", RISCOS_POINTER_H_RESIZE },
  { "e-resize", RISCOS_POINTER_H_RESIZE }, { "w-resize", RISCOS_POINTER_H_RESIZE },
  { "sb_h_double_arrow", RISCOS_POINTER_H_RESIZE },
  { "ns-resize", RISCOS_POINTER_V_RESIZE }, { "row-resize", RISCOS_POINTER_V_RESIZE },
  { "n-resize", RISCOS_POINTER_V_RESIZE }, { "s-resize", RISCOS_POINTER_V_RESIZE },
  { "sb_v_double_arrow", RISCOS_POINTER_V_RESIZE },
  { "nwse-resize", RISCOS_POINTER_NWSE_RESIZE }, { "nw-resize", RISCOS_POINTER_NWSE_RESIZE },
  { "se-resize", RISCOS_POINTER_NWSE_RESIZE },
  { "nesw-resize", RISCOS_POINTER_NESW_RESIZE }, { "ne-resize", RISCOS_POINTER_NESW_RESIZE },
  { "sw-resize", RISCOS_POINTER_NESW_RESIZE },
  { "crosshair", RISCOS_POINTER_CROSS }, { "cell", RISCOS_POINTER_CROSS },
  { "cross", RISCOS_POINTER_CROSS }, { "tcross", RISCOS_POINTER_CROSS },
  { "move", RISCOS_POINTER_MOVE }, { "all-scroll", RISCOS_POINTER_MOVE },
  { "grab", RISCOS_POINTER_MOVE }, { "grabbing", RISCOS_POINTER_MOVE },
  { "fleur", RISCOS_POINTER_MOVE },
};

static int
shape_for_name (const char *name)
{
  guint i;
  for (i = 0; name && i < G_N_ELEMENTS (pointer_names); i++)
    if (strcmp (name, pointer_names[i].name) == 0)
      return pointer_names[i].shape;
  return RISCOS_POINTER_DEFAULT;
}

static int
shape_for_type (GdkCursorType type)
{
  switch (type)
    {
    case GDK_XTERM: return RISCOS_POINTER_TEXT;
    case GDK_HAND1: case GDK_HAND2: return RISCOS_POINTER_HAND;
    case GDK_SB_H_DOUBLE_ARROW: case GDK_LEFT_SIDE: case GDK_RIGHT_SIDE: return RISCOS_POINTER_H_RESIZE;
    case GDK_SB_V_DOUBLE_ARROW: case GDK_TOP_SIDE: case GDK_BOTTOM_SIDE: return RISCOS_POINTER_V_RESIZE;
    case GDK_TOP_LEFT_CORNER: case GDK_BOTTOM_RIGHT_CORNER: return RISCOS_POINTER_NWSE_RESIZE;
    case GDK_TOP_RIGHT_CORNER: case GDK_BOTTOM_LEFT_CORNER: return RISCOS_POINTER_NESW_RESIZE;
    case GDK_CROSSHAIR: case GDK_CROSS: case GDK_TCROSS: return RISCOS_POINTER_CROSS;
    case GDK_FLEUR: return RISCOS_POINTER_MOVE;
    default: return RISCOS_POINTER_DEFAULT;
    }
}

int
_gdk_riscos_cursor_get_shape (GdkCursor *cursor)
{
  return cursor && GDK_IS_RISCOS_CURSOR (cursor) ? ((GdkRiscosCursor *) cursor)->shape
                                                 : RISCOS_POINTER_DEFAULT;
}

struct _GdkRiscosCursorClass
{
  GdkCursorClass cursor_class;
};

/*** GdkRiscosCursor ***/

G_DEFINE_TYPE (GdkRiscosCursor, gdk_riscos_cursor, GDK_TYPE_CURSOR)

static cairo_surface_t * gdk_riscos_cursor_get_surface (GdkCursor *cursor,
							  gdouble   *x_hot,
							  gdouble   *y_hot);

static void
gdk_riscos_cursor_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_riscos_cursor_parent_class)->finalize (object);
}

static void
gdk_riscos_cursor_class_init (GdkRiscosCursorClass *xcursor_class)
{
  GdkCursorClass *cursor_class = GDK_CURSOR_CLASS (xcursor_class);
  GObjectClass *object_class = G_OBJECT_CLASS (xcursor_class);

  object_class->finalize = gdk_riscos_cursor_finalize;

  cursor_class->get_surface = gdk_riscos_cursor_get_surface;
}

static void
gdk_riscos_cursor_init (GdkRiscosCursor *cursor)
{
}

/* Called by gdk_display_riscos_finalize to flush any cached cursors
 * for a dead display.
 */
void
_gdk_riscos_cursor_display_finalize (GdkDisplay *display)
{
}

GdkCursor*
_gdk_riscos_display_get_cursor_for_type (GdkDisplay    *display,
					   GdkCursorType  cursor_type)
{
  GdkRiscosCursor *private;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  private = g_object_new (GDK_TYPE_RISCOS_CURSOR,
                          "cursor-type", cursor_type,
                          "display", display,
			  NULL);
  private->shape = shape_for_type (cursor_type);

  return GDK_CURSOR (private);
}

static cairo_surface_t *
gdk_riscos_cursor_get_surface (GdkCursor *cursor,
				 gdouble *x_hot,
				 gdouble *y_hot)
{
  g_return_val_if_fail (cursor != NULL, NULL);

  return NULL;
}

void
_gdk_riscos_cursor_update_theme (GdkCursor *cursor)
{
  g_return_if_fail (cursor != NULL);
}

GdkCursor *
_gdk_riscos_display_get_cursor_for_surface (GdkDisplay *display,
					      cairo_surface_t *surface,
					      gdouble     x,
					      gdouble     y)
{
  GdkRiscosCursor *private;
  GdkCursor *cursor;

  private = g_object_new (GDK_TYPE_RISCOS_CURSOR, 
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);
  cursor = (GdkCursor *) private;

  return cursor;
}

GdkCursor*
_gdk_riscos_display_get_cursor_for_name (GdkDisplay  *display,
					   const gchar *name)
{
  GdkRiscosCursor *private;

  private = g_object_new (GDK_TYPE_RISCOS_CURSOR,
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);
  private->shape = shape_for_name (name);

  return GDK_CURSOR (private);
}

gboolean
_gdk_riscos_display_supports_cursor_alpha (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

gboolean
_gdk_riscos_display_supports_cursor_color (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return TRUE;
}

void
_gdk_riscos_display_get_default_cursor_size (GdkDisplay *display,
					       guint      *width,
					       guint      *height)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  *width = *height = 20;
}

void
_gdk_riscos_display_get_maximal_cursor_size (GdkDisplay *display,
					       guint       *width,
					       guint       *height)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  *width = 128;
  *height = 128;
}
