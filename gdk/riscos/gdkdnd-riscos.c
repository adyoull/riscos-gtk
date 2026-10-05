/* riscos-gtk: from GTK 3.24.41 gdk/broadway (client side), renamed
   broadway -> riscos; see riscos-gtk LICENCES.md. LGPL-2.1-or-later. */
/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "gdkdndprivate.h"

#include "gdkinternals.h"
#include "gdkproperty.h"
#include "gdkprivate-riscos.h"
#include "gdkinternals.h"
#include "gdkscreen-riscos.h"
#include "gdkdisplay-riscos.h"

#include <string.h>

#define GDK_TYPE_RISCOS_DRAG_CONTEXT              (gdk_riscos_drag_context_get_type ())
#define GDK_RISCOS_DRAG_CONTEXT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_RISCOS_DRAG_CONTEXT, GdkRiscosDragContext))
#define GDK_RISCOS_DRAG_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_RISCOS_DRAG_CONTEXT, GdkRiscosDragContextClass))
#define GDK_IS_RISCOS_DRAG_CONTEXT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_RISCOS_DRAG_CONTEXT))
#define GDK_IS_RISCOS_DRAG_CONTEXT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_RISCOS_DRAG_CONTEXT))
#define GDK_RISCOS_DRAG_CONTEXT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_RISCOS_DRAG_CONTEXT, GdkRiscosDragContextClass))

#ifdef GDK_COMPILATION
typedef struct _GdkRiscosDragContext GdkRiscosDragContext;
#else
typedef GdkDragContext GdkRiscosDragContext;
#endif
typedef struct _GdkRiscosDragContextClass GdkRiscosDragContextClass;

GType     gdk_riscos_drag_context_get_type (void);

/* riscos-gtk: drag and drop.

   Between this program's own windows the drag works in-process, as GDK's
   Win32 backend does without OLE (its "local" protocol): GDK_DRAG_ENTER,
   _MOTION, _LEAVE and GDK_DROP_START go straight into the event queue for
   the window under the pointer, and the destination's replies
   (gdk_drag_status, gdk_drop_finish) go back to the source the same way;
   the data comes from the source's selection, owned in this program.

   Dropped on another task's window, the data is saved to it with the
   Wimp's data transfer protocol (Message_DataSave, then RAM transfer or
   <Wimp$Scrap>): gdktransfer-riscos.c. GDK sees that drop as one on the
   root window. Data saved into one of our windows by another task arrives
   as a drag that enters, moves and drops at once
   (_gdk_riscos_dnd_incoming).

   While dragging, the Wimp shows a dashed box at the pointer
   (Wimp_DragBox); GTK's own drag icon window isn't shown. */

typedef enum
{
  DRAG_STATUS_DRAG,
  DRAG_STATUS_MOTION_WAIT
} RiscosDragStatus;

struct _GdkRiscosDragContext {
  GdkDragContext context;
  RiscosDragStatus drag_status;
  gint last_x, last_y;          /* root pixels */
  gboolean foreign;             /* over another task's window */
  gint foreign_target[4];       /* its window, icon, x, y (OS units) */
  gboolean box;                 /* Wimp_DragBox started */
  gboolean incoming;            /* data dropped on us by another task */
  gboolean drop_failed;
};

struct _GdkRiscosDragContextClass
{
  GdkDragContextClass parent_class;
};

static void gdk_riscos_drag_context_finalize (GObject *object);

static GList *contexts;
static GdkDragContext *current_dest_drag;

#define TRACE(...) G_STMT_START { if (g_getenv ("RISCOS_GTK_DEBUG")) g_printerr ("riscos-gtk dnd: " __VA_ARGS__); } G_STMT_END

G_DEFINE_TYPE (GdkRiscosDragContext, gdk_riscos_drag_context, GDK_TYPE_DRAG_CONTEXT)

static void
gdk_riscos_drag_context_init (GdkRiscosDragContext *dragcontext)
{
  contexts = g_list_prepend (contexts, dragcontext);
}

static void
gdk_riscos_drag_context_finalize (GObject *object)
{
  GdkDragContext *context = GDK_DRAG_CONTEXT (object);
  GdkRiscosDragContext *rc = GDK_RISCOS_DRAG_CONTEXT (object);

  if (rc->box)
    _gdk_riscos_transfer_drag_box (FALSE);
  contexts = g_list_remove (contexts, context);
  if (current_dest_drag == context)
    current_dest_drag = NULL;

  G_OBJECT_CLASS (gdk_riscos_drag_context_parent_class)->finalize (object);
}

static GdkDragContext *
find_context (gboolean is_source, GdkWindow *source, GdkWindow *dest)
{
  GList *l;

  for (l = contexts; l; l = l->next)
    {
      GdkDragContext *c = l->data;
      if (c->is_source == is_source && c->source_window == source &&
          (dest == NULL || c->dest_window == dest))
        return c;
    }
  return NULL;
}

static void
put_dnd_event (GdkEventType type, GdkWindow *window, GdkDragContext *context,
               gint x_root, gint y_root, gboolean send_event, guint32 time)
{
  GdkEvent *event = gdk_event_new (type);
  GdkDevice *device = gdk_drag_context_get_device (context);

  event->dnd.window = g_object_ref (window);
  event->dnd.send_event = send_event;
  event->dnd.context = g_object_ref (context);
  event->dnd.time = time;
  event->dnd.x_root = x_root;
  event->dnd.y_root = y_root;
  if (device)
    {
      gdk_event_set_device (event, device);
      gdk_event_set_seat (event, gdk_device_get_seat (device));
    }
  gdk_event_put (event);
  gdk_event_free (event);
}

/* Source side */

static void
local_send_leave (GdkDragContext *context, guint32 time)
{
  if (current_dest_drag && current_dest_drag->source_window == context->source_window)
    {
      GdkDragContext *dest = current_dest_drag;
      current_dest_drag = NULL;
      put_dnd_event (GDK_DRAG_LEAVE, dest->dest_window, dest, 0, 0, FALSE, time);
      g_object_unref (dest);
    }
}

static void
local_send_enter (GdkDragContext *context, guint32 time)
{
  GdkDragContext *dest;

  if (current_dest_drag)
    {
      g_object_unref (current_dest_drag);
      current_dest_drag = NULL;
    }
  dest = g_object_new (GDK_TYPE_RISCOS_DRAG_CONTEXT, NULL);
  dest->display = context->display;
  dest->protocol = GDK_DRAG_PROTO_LOCAL;
  dest->is_source = FALSE;
  dest->source_window = g_object_ref (context->source_window);
  dest->dest_window = g_object_ref (context->dest_window);
  dest->targets = g_list_copy (context->targets);
  dest->actions = context->actions;
  gdk_drag_context_set_device (dest, gdk_drag_context_get_device (context));
  current_dest_drag = dest;
  put_dnd_event (GDK_DRAG_ENTER, context->dest_window, dest, 0, 0, FALSE, time);
}

static void
local_send_motion (GdkDragContext *context, gint x_root, gint y_root,
                   GdkDragAction action, guint32 time)
{
  if (current_dest_drag && current_dest_drag->source_window == context->source_window)
    {
      GdkRiscosDragContext *dest = GDK_RISCOS_DRAG_CONTEXT (current_dest_drag);

      current_dest_drag->suggested_action = action;
      dest->last_x = x_root;
      dest->last_y = y_root;
      GDK_RISCOS_DRAG_CONTEXT (context)->drag_status = DRAG_STATUS_MOTION_WAIT;
      put_dnd_event (GDK_DRAG_MOTION, current_dest_drag->dest_window, current_dest_drag,
                     x_root, y_root, FALSE, time);
    }
}

static void
local_send_drop (GdkDragContext *context, guint32 time)
{
  if (current_dest_drag && current_dest_drag->source_window == context->source_window)
    {
      GdkDragContext *dest = current_dest_drag;
      GdkRiscosDragContext *rd = GDK_RISCOS_DRAG_CONTEXT (dest);

      current_dest_drag = NULL;
      put_dnd_event (GDK_DROP_START, dest->dest_window, dest, rd->last_x, rd->last_y, FALSE, time);
      g_object_unref (dest);
    }
}

static void
do_leave (GdkDragContext *context, guint32 time)
{
  if (context->dest_window)
    {
      if (!GDK_RISCOS_DRAG_CONTEXT (context)->foreign)
        local_send_leave (context, time);
      g_clear_object (&context->dest_window);
    }
  GDK_RISCOS_DRAG_CONTEXT (context)->foreign = FALSE;
}

GdkDragContext *
_gdk_riscos_window_drag_begin (GdkWindow *window,
				 GdkDevice *device,
				 GList     *targets,
                                 gint       x_root,
                                 gint       y_root)
{
  GdkDragContext *new_context;

  g_return_val_if_fail (window != NULL, NULL);
  g_return_val_if_fail (GDK_WINDOW_IS_RISCOS (window), NULL);

  new_context = g_object_new (GDK_TYPE_RISCOS_DRAG_CONTEXT,
			      NULL);
  new_context->display = gdk_window_get_display (window);
  new_context->is_source = TRUE;
  new_context->protocol = GDK_DRAG_PROTO_LOCAL;
  new_context->source_window = g_object_ref (window);
  new_context->targets = g_list_copy (targets);
  gdk_drag_context_set_device (new_context, device);
  GDK_RISCOS_DRAG_CONTEXT (new_context)->last_x = x_root;
  GDK_RISCOS_DRAG_CONTEXT (new_context)->last_y = y_root;

  return new_context;
}

GdkDragProtocol
_gdk_riscos_window_get_drag_protocol (GdkWindow *window,
					GdkWindow **target)
{
  if (target)
    *target = NULL;
  return GDK_DRAG_PROTO_LOCAL;
}

static GdkWindow *
gdk_riscos_drag_context_find_window (GdkDragContext  *context,
				       GdkWindow       *drag_window,
				       GdkScreen       *screen,
				       gint             x_root,
				       gint             y_root,
				       GdkDragProtocol *protocol)
{
  GdkRiscosDragContext *rc = GDK_RISCOS_DRAG_CONTEXT (context);
  GdkWindow *window;
  int target[4];

  g_return_val_if_fail (context != NULL, NULL);

  window = _gdk_riscos_transfer_window_at (context->display, target);
  TRACE ("find_window %d,%d: %s (Wimp window %d)\n", x_root, y_root,
         window ? "ours" : target[0] == -1 ? "none" : "another task's", target[0]);
  if (window)
    {
      *protocol = GDK_DRAG_PROTO_LOCAL;
      return g_object_ref (window);
    }
  if (target[0] == -1)         /* the desktop background */
    {
      *protocol = GDK_DRAG_PROTO_NONE;
      return NULL;
    }
  /* another task's window (or the icon bar): saved to with DataSave */
  memcpy (rc->foreign_target, target, sizeof target);
  *protocol = GDK_DRAG_PROTO_LOCAL;
  return g_object_ref (gdk_screen_get_root_window (screen));
}

static gboolean
gdk_riscos_drag_context_drag_motion (GdkDragContext *context,
				       GdkWindow      *dest_window,
				       GdkDragProtocol protocol,
				       gint            x_root,
				       gint            y_root,
				       GdkDragAction   suggested_action,
				       GdkDragAction   possible_actions,
				       guint32         time)
{
  GdkRiscosDragContext *rc = GDK_RISCOS_DRAG_CONTEXT (context);

  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (dest_window == NULL || GDK_WINDOW_IS_RISCOS (dest_window), FALSE);

  TRACE ("motion %d,%d to %p (was %p), actions %x/%x\n", x_root, y_root, (void *) dest_window,
         (void *) context->dest_window, suggested_action, possible_actions);
  context->actions = possible_actions;
  if (!rc->box)
    rc->box = _gdk_riscos_transfer_drag_box (TRUE);

  if (context->dest_window == dest_window)
    {
      GdkDragContext *dest = rc->foreign ? NULL :
                             find_context (FALSE, context->source_window, dest_window);
      if (dest)
        dest->actions = context->actions;
      context->suggested_action = suggested_action;
    }
  else
    {
      /* a leave to the last destination, an enter to this one */
      do_leave (context, time);
      rc->drag_status = DRAG_STATUS_DRAG;
      if (dest_window)
        {
          context->dest_window = g_object_ref (dest_window);
          context->protocol = protocol;
          if (dest_window == gdk_screen_get_root_window (gdk_window_get_screen (dest_window)))
            {
              /* another task: it can have a copy */
              rc->foreign = TRUE;
              context->action = (possible_actions & GDK_ACTION_COPY) ? GDK_ACTION_COPY : suggested_action;
            }
          else
            local_send_enter (context, time);
          context->suggested_action = suggested_action;
        }
      else
        context->action = 0;

      /* a status event, so GTK knows the destination changed */
      put_dnd_event (GDK_DRAG_STATUS, context->source_window, context, 0, 0, TRUE, time);
    }

  rc->last_x = x_root;
  rc->last_y = y_root;
  if (context->dest_window && !rc->foreign)
    {
      if (rc->drag_status == DRAG_STATUS_DRAG)
        local_send_motion (context, x_root, y_root, suggested_action, time);
      else
        return TRUE;
    }
  return FALSE;
}

static void
gdk_riscos_drag_context_drag_drop (GdkDragContext *context,
				     guint32         time)
{
  GdkRiscosDragContext *rc = GDK_RISCOS_DRAG_CONTEXT (context);

  g_return_if_fail (context != NULL);

  TRACE ("drop (%s)\n", rc->foreign ? "another task" : "here");
  if (rc->box)
    {
      _gdk_riscos_transfer_drag_box (FALSE);
      rc->box = FALSE;
    }
  if (rc->foreign)
    {
      /* save the data to the other task; GTK can forget the drag */
      rc->drop_failed = !_gdk_riscos_transfer_drop_foreign (context->source_window,
                                                            gdk_drag_get_selection (context),
                                                            context->targets,
                                                            rc->foreign_target);
      put_dnd_event (GDK_DROP_FINISHED, context->source_window, context, 0, 0, FALSE, time);
    }
  else if (context->dest_window)
    local_send_drop (context, time);
}

static void
gdk_riscos_drag_context_drag_abort (GdkDragContext *context,
				      guint32         time)
{
  GdkRiscosDragContext *rc = GDK_RISCOS_DRAG_CONTEXT (context);

  g_return_if_fail (context != NULL);
  if (rc->box)
    {
      _gdk_riscos_transfer_drag_box (FALSE);
      rc->box = FALSE;
    }
  do_leave (context, time);
}

static void
gdk_riscos_drag_context_drop_done (GdkDragContext *context,
                                   gboolean        success)
{
  GdkRiscosDragContext *rc = GDK_RISCOS_DRAG_CONTEXT (context);

  if (rc->box)
    {
      _gdk_riscos_transfer_drag_box (FALSE);
      rc->box = FALSE;
    }
}

/* Destination side */

static void
gdk_riscos_drag_context_drag_status (GdkDragContext   *context,
				       GdkDragAction     action,
				       guint32           time)
{
  GdkDragContext *src;

  g_return_if_fail (context != NULL);

  context->action = action;
  TRACE ("status %x\n", action);
  if (GDK_RISCOS_DRAG_CONTEXT (context)->incoming)
    return;
  src = find_context (TRUE, context->source_window, context->dest_window);
  if (src)
    {
      GdkRiscosDragContext *rs = GDK_RISCOS_DRAG_CONTEXT (src);

      if (rs->drag_status == DRAG_STATUS_MOTION_WAIT)
        rs->drag_status = DRAG_STATUS_DRAG;
      src->action = action == GDK_ACTION_DEFAULT ? 0 : action;
      put_dnd_event (GDK_DRAG_STATUS, context->source_window, src, 0, 0, FALSE, time);
    }
}

static void
gdk_riscos_drag_context_drop_reply (GdkDragContext   *context,
				      gboolean          ok,
				      guint32           time)
{
  g_return_if_fail (context != NULL);
}

static void
gdk_riscos_drag_context_drop_finish (GdkDragContext   *context,
				       gboolean          success,
				       guint32           time)
{
  GdkDragContext *src;

  g_return_if_fail (context != NULL);

  if (GDK_RISCOS_DRAG_CONTEXT (context)->incoming)
    {
      _gdk_riscos_transfer_drop_done (success);
      return;
    }
  src = find_context (TRUE, context->source_window, context->dest_window);
  if (src)
    {
      GDK_RISCOS_DRAG_CONTEXT (src)->drop_failed = !success;
      put_dnd_event (GDK_DROP_FINISHED, src->source_window, src, 0, 0, FALSE, time);
    }
}

void
_gdk_riscos_window_register_dnd (GdkWindow      *window)
{
}

static GdkAtom
gdk_riscos_drag_context_get_selection (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, GDK_NONE);

  if (GDK_RISCOS_DRAG_CONTEXT (context)->incoming)
    return gdk_atom_intern_static_string ("RISCOS_DROP");
  return gdk_atom_intern_static_string ("RISCOS_DND");
}

static gboolean
gdk_riscos_drag_context_drop_status (GdkDragContext *context)
{
  g_return_val_if_fail (context != NULL, FALSE);

  return !GDK_RISCOS_DRAG_CONTEXT (context)->drop_failed;
}

/* riscos-gtk: another task saved data into our window (window) at root
   pixel x, y; the data waits in gdktransfer-riscos.c as the selection
   RISCOS_DROP. GTK sees a drag that enters, moves and drops there. */
void
_gdk_riscos_dnd_incoming (GdkDisplay *display, GdkWindow *window, gint x_root, gint y_root)
{
  GdkDragContext *dest;
  GdkSeat *seat = gdk_display_get_default_seat (display);
  static const char *targets[] = { "UTF8_STRING", "STRING", "TEXT", "COMPOUND_TEXT",
                                   "text/plain;charset=utf-8", "text/plain" };
  guint i;

  dest = g_object_new (GDK_TYPE_RISCOS_DRAG_CONTEXT, NULL);
  dest->display = display;
  dest->protocol = GDK_DRAG_PROTO_LOCAL;
  dest->is_source = FALSE;
  dest->source_window = g_object_ref (window);
  dest->dest_window = g_object_ref (window);
  for (i = 0; i < G_N_ELEMENTS (targets); i++)
    dest->targets = g_list_append (dest->targets, gdk_atom_intern_static_string (targets[i]));
  dest->actions = GDK_ACTION_COPY;
  dest->suggested_action = GDK_ACTION_COPY;
  if (seat)
    gdk_drag_context_set_device (dest, gdk_seat_get_pointer (seat));
  GDK_RISCOS_DRAG_CONTEXT (dest)->incoming = TRUE;
  put_dnd_event (GDK_DRAG_ENTER, window, dest, x_root, y_root, FALSE, GDK_CURRENT_TIME);
  put_dnd_event (GDK_DRAG_MOTION, window, dest, x_root, y_root, FALSE, GDK_CURRENT_TIME);
  put_dnd_event (GDK_DROP_START, window, dest, x_root, y_root, FALSE, GDK_CURRENT_TIME);
  g_object_unref (dest);
}

void
_gdk_riscos_display_init_dnd (GdkDisplay *display)
{
}

static void
gdk_riscos_drag_context_class_init (GdkRiscosDragContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDragContextClass *context_class = GDK_DRAG_CONTEXT_CLASS (klass);

  object_class->finalize = gdk_riscos_drag_context_finalize;

  context_class->find_window = gdk_riscos_drag_context_find_window;
  context_class->drag_status = gdk_riscos_drag_context_drag_status;
  context_class->drag_motion = gdk_riscos_drag_context_drag_motion;
  context_class->drag_abort = gdk_riscos_drag_context_drag_abort;
  context_class->drag_drop = gdk_riscos_drag_context_drag_drop;
  context_class->drop_reply = gdk_riscos_drag_context_drop_reply;
  context_class->drop_finish = gdk_riscos_drag_context_drop_finish;
  context_class->drop_status = gdk_riscos_drag_context_drop_status;
  context_class->get_selection = gdk_riscos_drag_context_get_selection;
  context_class->drop_done = gdk_riscos_drag_context_drop_done;
}
