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

#include "gdkselection.h"

#include "gdkproperty.h"
#include "gdkprivate.h"
#include "gdkprivate-riscos.h"
#include "gdkdisplay-riscos.h"

#include <string.h>


typedef struct _OwnerInfo OwnerInfo;

struct _OwnerInfo
{
  GdkAtom    selection;
  GdkWindow *owner;
  gulong     serial;
};

static GSList *owner_list;

/* When a window is destroyed we check if it is the owner
 * of any selections. This is somewhat inefficient, but
 * owner_list is typically short, and it is a low memory,
 * low code solution
 */
void
_gdk_riscos_selection_window_destroyed (GdkWindow *window)
{
  GSList *tmp_list = owner_list;
  while (tmp_list)
    {
      OwnerInfo *info = tmp_list->data;
      tmp_list = tmp_list->next;

      if (info->owner == window)
	{
	  owner_list = g_slist_remove (owner_list, info);
	  g_free (info);
	}
    }
  _gdk_riscos_properties_window_destroyed (window);
}

gboolean
_gdk_riscos_display_set_selection_owner (GdkDisplay *display,
					   GdkWindow  *owner,
					   GdkAtom     selection,
					   guint32     time,
					   gboolean    send_event)
{
  GSList *tmp_list;
  OwnerInfo *info;

  if (gdk_display_is_closed (display))
    return FALSE;

  tmp_list = owner_list;
  while (tmp_list)
    {
      info = tmp_list->data;
      if (info->selection == selection)
        {
          owner_list = g_slist_remove (owner_list, info);
          g_free (info);
          break;
        }
      tmp_list = tmp_list->next;
    }

  if (owner)
    {
      info = g_new (OwnerInfo, 1);
      info->owner = owner;
      info->serial = _gdk_display_get_next_serial (display);

      info->selection = selection;

      owner_list = g_slist_prepend (owner_list, info);

      /* riscos-gtk: the RISC OS clipboard is ours now */
      if (selection == GDK_SELECTION_CLIPBOARD)
        _gdk_riscos_clipboard_claimed ();
    }

  return TRUE;
}

/* riscos-gtk: another task claimed the clipboard (Message_ClaimEntity):
   GTK's owner loses it. */
void
_gdk_riscos_selection_lost (GdkDisplay *display, GdkAtom selection)
{
  GdkWindow *owner = _gdk_riscos_display_get_selection_owner (display, selection);
  GdkEvent *event;

  if (owner == NULL)
    return;
  _gdk_riscos_display_set_selection_owner (display, NULL, selection, GDK_CURRENT_TIME, FALSE);
  event = gdk_event_new (GDK_SELECTION_CLEAR);
  event->selection.window = g_object_ref (owner);
  event->selection.selection = selection;
  event->selection.time = GDK_CURRENT_TIME;
  gdk_event_put (event);
  gdk_event_free (event);
}

/* riscos-gtk: queue a selection event, as an X server would send it */
void
_gdk_riscos_selection_event (GdkEventType type,
                             GdkWindow   *window,
                             GdkWindow   *requestor,
                             GdkAtom      selection,
                             GdkAtom      target,
                             GdkAtom      property,
                             guint32      time)
{
  GdkEvent *event = gdk_event_new (type);

  event->selection.window = g_object_ref (window);
  event->selection.requestor = requestor ? g_object_ref (requestor) : NULL;
  event->selection.selection = selection;
  event->selection.target = target;
  event->selection.property = property;
  event->selection.time = time;
  gdk_event_put (event);
  gdk_event_free (event);
}

GdkWindow *
_gdk_riscos_display_get_selection_owner (GdkDisplay *display,
					   GdkAtom     selection)
{
  GSList *tmp_list;
  OwnerInfo *info;

  if (gdk_display_is_closed (display))
    return NULL;

  tmp_list = owner_list;
  while (tmp_list)
    {
      info = tmp_list->data;
      if (info->selection == selection)
	return info->owner;
      tmp_list = tmp_list->next;
    }

  return NULL;
}

/* riscos-gtk: GTK asks for a selection's value (as X's ConvertSelection).
   It answers with a GDK_SELECTION_NOTIFY to the requestor, the value in
   its "GDK_SELECTION" property. A selection owned in this program (drag
   and drop between its widgets) goes to the owner as a selection request;
   the clipboard and data dropped from another task come over the Wimp's
   data transfer protocol (gdktransfer-riscos.c). */
void
_gdk_riscos_display_convert_selection (GdkDisplay *display,
					 GdkWindow *requestor,
					 GdkAtom    selection,
					 GdkAtom    target,
					 guint32    time)
{
  GdkWindow *owner = _gdk_riscos_display_get_selection_owner (display, selection);
  GdkAtom property = gdk_atom_intern_static_string ("GDK_SELECTION");

  if (owner)
    _gdk_riscos_selection_event (GDK_SELECTION_REQUEST, owner, requestor,
                                 selection, target, property, time);
  else if (!_gdk_riscos_transfer_convert (requestor, selection, target, property))
    _gdk_riscos_selection_event (GDK_SELECTION_NOTIFY, requestor, NULL,
                                 selection, target, GDK_NONE, time);
}

gint
_gdk_riscos_display_get_selection_property (GdkDisplay *display,
					      GdkWindow  *requestor,
					      guchar    **data,
					      GdkAtom    *ret_type,
					      gint       *ret_format)
{
  gint length = 0;
  GdkAtom type = GDK_NONE;
  gint format = 0;
  guchar *value = NULL;

  if (!_gdk_riscos_property_take (requestor, gdk_atom_intern_static_string ("GDK_SELECTION"),
                                  TRUE, &type, &format, &value, &length))
    length = 0;
  if (ret_type)
    *ret_type = type;
  if (ret_format)
    *ret_format = format;
  if (data)
    *data = value;
  else
    g_free (value);

  return length;
}

void
_gdk_riscos_display_send_selection_notify (GdkDisplay      *display,
					     GdkWindow       *requestor,
					     GdkAtom          selection,
					     GdkAtom          target,
					     GdkAtom          property, 
					     guint32          time)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  /* the reply to a request made for another task? */
  if (_gdk_riscos_transfer_notify (requestor, selection, target, property))
    return;
  _gdk_riscos_selection_event (GDK_SELECTION_NOTIFY, requestor, NULL,
                               selection, target, property, time);
}


static gint
make_list (const gchar  *text,
           gint          length,
           gboolean      latin1,
           gchar      ***list)
{
  GSList *strings = NULL;
  gint n_strings = 0;
  gint i;
  const gchar *p = text;
  const gchar *q;
  GSList *tmp_list;
  GError *error = NULL;

  while (p < text + length)
    {
      gchar *str;

      q = p;
      while (*q && q < text + length)
        q++;

      if (latin1)
        {
          str = g_convert (p, q - p,
                           "UTF-8", "ISO-8859-1",
                           NULL, NULL, &error);

          if (!str)
            {
              g_warning ("Error converting selection from STRING: %s",
                         error->message);
              g_error_free (error);
            }
        }
      else
        {
          str = g_strndup (p, q - p);
          if (!g_utf8_validate (str, -1, NULL))
            {
              g_warning ("Error converting selection from UTF8_STRING");
              g_free (str);
              str = NULL;
            }
        }

      if (str)
        {
          strings = g_slist_prepend (strings, str);
          n_strings++;
        }

      p = q + 1;
    }

  if (list)
    {
      *list = g_new (gchar *, n_strings + 1);
      (*list)[n_strings] = NULL;
    }

  i = n_strings;
  tmp_list = strings;
  while (tmp_list)
    {
      if (list)
        (*list)[--i] = tmp_list->data;
      else
        g_free (tmp_list->data);

      tmp_list = tmp_list->next;
    }

  g_slist_free (strings);

  return n_strings;
}

gint 
_gdk_riscos_display_text_property_to_utf8_list (GdkDisplay    *display,
						  GdkAtom        encoding,
						  gint           format,
						  const guchar  *text,
						  gint           length,
						  gchar       ***list)
{
  g_return_val_if_fail (text != NULL, 0);
  g_return_val_if_fail (length >= 0, 0);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), 0);

  if (encoding == GDK_TARGET_STRING)
    {
      return make_list ((gchar *)text, length, TRUE, list);
    }
  else if (encoding == gdk_atom_intern_static_string ("UTF8_STRING"))
    {
      return make_list ((gchar *)text, length, FALSE, list);
    }
  
  if (list)
    *list = NULL;
  return 0;
}

gchar *
_gdk_riscos_display_utf8_to_string_target (GdkDisplay  *display,
					     const gchar *str)
{
  return g_strdup (str);
}
