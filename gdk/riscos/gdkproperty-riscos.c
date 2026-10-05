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

#include "gdkproperty.h"

#include "gdkmain.h"
#include "gdkprivate.h"
#include "gdkinternals.h"
#include "gdkdisplay-riscos.h"
#include "gdkscreen-riscos.h"
#include "gdkselection.h"

#include <string.h>

/* riscos-gtk: window properties, kept in memory. There's no X server
   here: properties exist only to carry selection data (the clipboard and
   drag and drop) from GTK's owner side to its requesting side, as on X. */
typedef struct
{
  GdkWindow *window;
  GdkAtom    property;
  GdkAtom    type;
  gint       format;
  guchar    *data;
  gint       length;          /* bytes */
} RiscosProperty;

static GSList *properties;

static RiscosProperty *
find_property (GdkWindow *window, GdkAtom property)
{
  GSList *l;

  for (l = properties; l; l = l->next)
    {
      RiscosProperty *p = l->data;
      if (p->window == window && p->property == property)
        return p;
    }
  return NULL;
}

static void
free_property (RiscosProperty *p)
{
  properties = g_slist_remove (properties, p);
  g_free (p->data);
  g_free (p);
}

/* format 32 items are longs, as on X */
static gint
bytes_per_item (gint format)
{
  return format == 32 ? (gint) sizeof (long) : format == 16 ? (gint) sizeof (short) : 1;
}

void
_gdk_riscos_property_set (GdkWindow    *window,
                          GdkAtom       property,
                          GdkAtom       type,
                          gint          format,
                          const guchar *data,
                          gint          length,
                          gboolean      append)
{
  RiscosProperty *p = find_property (window, property);

  if (p && !append)
    {
      free_property (p);
      p = NULL;
    }
  if (p == NULL)
    {
      p = g_new0 (RiscosProperty, 1);
      p->window = window;
      p->property = property;
      p->type = type;
      p->format = format;
      p->data = g_malloc (1);
      p->data[0] = 0;
      properties = g_slist_prepend (properties, p);
    }
  p->data = g_realloc (p->data, p->length + length + 1);
  if (length)
    memcpy (p->data + p->length, data, length);
  p->length += length;
  p->data[p->length] = 0;         /* text ends in a NUL, as on X */
}

/* The property's value (data to g_free); deleted if delete. */
gboolean
_gdk_riscos_property_take (GdkWindow  *window,
                           GdkAtom     property,
                           gboolean    delete,
                           GdkAtom    *type,
                           gint       *format,
                           guchar    **data,
                           gint       *length)
{
  RiscosProperty *p = find_property (window, property);

  if (p == NULL)
    return FALSE;
  if (type)
    *type = p->type;
  if (format)
    *format = p->format;
  if (length)
    *length = p->length;
  if (data)
    *data = g_memdup2 (p->data, p->length + 1);
  if (delete)
    free_property (p);
  return TRUE;
}

void
_gdk_riscos_properties_window_destroyed (GdkWindow *window)
{
  GSList *l = properties;

  while (l)
    {
      RiscosProperty *p = l->data;
      l = l->next;
      if (p->window == window)
        free_property (p);
    }
}

gboolean
_gdk_riscos_window_get_property (GdkWindow   *window,
				   GdkAtom      property,
				   GdkAtom      type,
				   gulong       offset,
				   gulong       length,
				   gint         pdelete,
				   GdkAtom     *actual_property_type,
				   gint        *actual_format_type,
				   gint        *actual_length,
				   guchar     **data)
{
  RiscosProperty *p = find_property (window, property);
  gulong start, n;

  if (p == NULL || (type != GDK_NONE && type != p->type))
    return FALSE;
  /* offset and length are in 32-bit units, as on X */
  start = MIN ((gulong) p->length, offset * 4);
  n = MIN ((gulong) p->length - start, length * 4);
  if (actual_property_type)
    *actual_property_type = p->type;
  if (actual_format_type)
    *actual_format_type = p->format;
  if (actual_length)
    *actual_length = n;
  if (data)
    {
      *data = g_malloc (n + 1);
      memcpy (*data, p->data + start, n);
      (*data)[n] = 0;
    }
  if (pdelete && start + n == (gulong) p->length)
    free_property (p);
  return TRUE;
}

void
_gdk_riscos_window_change_property (GdkWindow    *window,
				      GdkAtom       property,
				      GdkAtom       type,
				      gint          format,
				      GdkPropMode   mode,
				      const guchar *data,
				      gint          nelements)
{
  g_return_if_fail (!window || GDK_WINDOW_IS_RISCOS (window));

  if (window == NULL)
    return;
  if (mode == GDK_PROP_MODE_PREPEND)
    {
      /* rare: rebuild the value */
      guchar *old = NULL;
      gint oldlen = 0;
      if (_gdk_riscos_property_take (window, property, TRUE, NULL, NULL, &old, &oldlen))
        {
          _gdk_riscos_property_set (window, property, type, format, data,
                                    nelements * bytes_per_item (format), FALSE);
          _gdk_riscos_property_set (window, property, type, format, old, oldlen, TRUE);
          g_free (old);
          return;
        }
    }
  _gdk_riscos_property_set (window, property, type, format, data,
                            nelements * bytes_per_item (format),
                            mode == GDK_PROP_MODE_APPEND);
}

void
_gdk_riscos_window_delete_property (GdkWindow *window,
				      GdkAtom    property)
{
  RiscosProperty *p;

  g_return_if_fail (!window || GDK_WINDOW_IS_RISCOS (window));
  if (window && (p = find_property (window, property)))
    free_property (p);
}
