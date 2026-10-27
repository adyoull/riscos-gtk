/*
 * gdkriscos-server.c - the RISC OS Wimp side of riscos-gtk's GDK backend.
 *
 * The rest of gdk/riscos is GTK's Broadway backend (client side), renamed.
 * Broadway's GDK code talks to a "server" (broadwayd, over a socket) through
 * the functions in gdkriscos-server.h, and gets input back as
 * RiscosInputMsg structures passed to _gdk_riscos_events_got_input(). This
 * file implements that same interface directly on the Wimp, in-process:
 *
 *   - each GDK toplevel is a Wimp window with native furniture (title bar,
 *     close, back, toggle size, adjust size); GDK_WINDOW_TEMP windows
 *     (popups) are plain windows without furniture;
 *   - a window's backing store is a cairo RGB24 image surface whose pixels
 *     are those of a 32bpp TRGB sprite (0x00RRGGBB, cairo's own layout), so
 *     redrawing is one OS_SpriteOp plot per Wimp rectangle and no copy
 *     (the same TRGB sprite technique as riscos-mesa's EGL);
 *   - GLib's main loop polls the Wimp: the default context's poll function
 *     checks GLib's fds without blocking and does all waiting in
 *     Wimp_PollIdle (UnixLib's select() busy-waits in a Wimp task);
 *   - the Wimp has no button-release, key-release or pointer-motion
 *     events: releases and motion come from polling Wimp_GetPointerInfo on
 *     null events while the pointer is over a window or a button is held;
 *     key releases are sent straight after the press.
 *
 * Coordinates: GDK uses pixels from the top left of the screen; the Wimp
 * uses OS units from the bottom left. A window's work area is (0, -h, w, 0)
 * in OS units, scroll offsets are kept at 0, so work area pixel (px, py) is
 * (px << xeig, -(py << yeig)).
 *
 * SWIs and Wimp messages as in the RISC OS PRMs (Wimp_Initialise 380 with a
 * message list, Wimp_PollIdle, Wimp_RedrawWindow/GetRectangle,
 * OS_SpriteOp 9/15/34/52, OS_ReadVduVariables 0/4/5/9/11/12).
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as the GTK code it
 * plugs into.
 */
#include "config.h"

#include "gdkriscos-server.h"
#include "gdkprivate-riscos.h"
#include "gdkriscosmenu.h"
#include "gdkriscoswimp.h"
#include <gdk/gdkcairo.h>
#include <gdk/gdkkeysyms.h>

#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <kernel.h>
#include <swis.h>

#ifndef Wimp_ForceRedraw
#define Wimp_ForceRedraw 0x400D1
#endif
#ifndef Wimp_GetWindowOutline
#define Wimp_GetWindowOutline 0x400E0
#endif
#ifndef Wimp_DragBox
#define Wimp_DragBox 0x400D0
#endif

/* Poll reason codes */
enum {
  NULL_REASON = 0, REDRAW_WINDOW_REQUEST = 1, OPEN_WINDOW_REQUEST = 2,
  CLOSE_WINDOW_REQUEST = 3, POINTER_LEAVING_WINDOW = 4,
  POINTER_ENTERING_WINDOW = 5, MOUSE_CLICK = 6, KEY_PRESSED = 8,
  MENU_SELECTION = 9, SCROLL_REQUEST = 10, LOSE_CARET = 11, GAIN_CARET = 12,
  USER_MESSAGE = 17, USER_MESSAGE_RECORDED = 18, USER_MESSAGE_ACKNOWLEDGE = 19
};
#define MESSAGE_QUIT        0
#define MESSAGE_PREQUIT     8
#define MESSAGE_MODECHANGE  0x400C1
#define MESSAGE_MENUSDELETED 0x400C9

#ifndef Wimp_CreateMenu
#define Wimp_CreateMenu 0x400D4
#endif
#ifndef Wimp_TextOp
#define Wimp_TextOp 0x400F9
#endif

/* Window flags (Wimp_CreateWindow +28) */
#define WF_MOVEABLE   (1u << 1)
#define WF_NO_BOUNDS  (1u << 6)
#define WF_BACK       (1u << 24)
#define WF_CLOSE      (1u << 25)
#define WF_TITLE      (1u << 26)
#define WF_TOGGLE     (1u << 27)
#define WF_ADJUST     (1u << 29)
#define WF_NEW_FORMAT (1u << 31)

#define MODEFLAG_TRGB 0x4000
#define TITLE_MAX 256

/* Mouse buttons as the Wimp reports them, and GDK's numbers for them:
   Select = 1 (left), Adjust = 2 (middle), Menu = 3 (right; GTK opens
   context menus on button 3). */
#define BUT_SELECT 4
#define BUT_MENU   2
#define BUT_ADJUST 1

typedef struct {
  guint32 id;                 /* GDK's id for the window */
  int handle;                 /* Wimp window handle */
  gboolean is_temp;
  gboolean visible;
  int x, y, width, height;    /* pixels; x, y = top left of the work area */
  cairo_surface_t *surface;   /* the backing store (a TRGB sprite) */
  char title[TITLE_MAX];      /* indirected title (Latin-1) */
  int pointer_shape;          /* RISCOS_POINTER_* while the pointer is over it */
  unsigned furniture;         /* RISCOS_FURNITURE_* the Wimp window was made with */
  unsigned furniture_wanted;  /* ... and what GDK asks for (applied when hidden) */
  int title_h;                /* its title bar's height (pixels; 0 = not known yet) */
  gboolean as_menu;           /* opened as a transient dialogue (Wimp_CreateMenu) */
  gboolean menu_open;
  int grip_x, grip_w;         /* the grip's column (GTK's vertical scroll bar), if set: w > 0 */
  int grip_top;               /* ... and its top + 1 (0: at the window's foot) */
  /* no scroll bar in the corner: an empty scroll bar down the right edge,
     outside GTK's window (as the Wimp's furniture is outside the work
     area), with the grip at its foot; GTK draws it (strip_image) */
  int strip;                  /* its width (0: none) */
  cairo_surface_t *strip_surface;  /* a sprite: GTK's image, the grip */
} RiscosWindow;

struct _GdkRiscosServer {
  GObject parent_instance;

  guint32 next_serial;
  int task;                   /* Wimp task handle */
  /* xeig, yeig: OS units per GDK pixel, as eigen factors. GDK pixels are
     the screen's pixels times scale (1, or 2 in an EX0 EY0 mode: see
     read_screen); dev_xeig, dev_yeig are the screen's own. */
  int xeig, yeig, dev_xeig, dev_yeig, scale, log2bpp, mode_flags;
  gboolean sprite_logged;
  /* the open Wimp menu (riscos-gtk's GtkMenu replacement, gtkmenu-riscos.c) */
  int *menu_block;
  int menu_x, menu_y;           /* where it opened, OS units */
  GdkRiscosMenuFunc menu_func;
  gpointer menu_data;
  int menu_hold;                /* polls to wait for GTK to handle a selection */
  int dbox_menu;                /* a window open as a transient dialogue (0 = none) */
  int screen_w, screen_h;     /* pixels */
  guint32 next_id;
  GHashTable *windows;        /* id -> RiscosWindow */
  GHashTable *handles;        /* Wimp handle -> RiscosWindow */

  guint32 pointer_window;     /* window the pointer is over (0 = none) */
  int shown_shape;            /* pointer shape shown (gdkpointer-riscos.c) */
  int last_x, last_y;         /* last pointer position reported (pixels) */
  int buttons;                /* Wimp buttons currently held */
  guint32 grab_window;        /* pointer grab (0 = none) */
  guint32 press_window;       /* implicit grab: where the held buttons went down */
  gboolean grab_owner_events;
  guint32 focus_window;       /* window with the caret (0 = none) */
  guint32 last_seen_time;
  /* the size grip drawn in resizable windows' bottom right corner */
  GdkPixbuf *grip;
  gboolean grip_read;
  int grip_w, grip_h;           /* pixels */
};

struct _GdkRiscosServerClass {
  GObjectClass parent_class;
};

static GType gdk_riscos_server_get_type (void);
G_DEFINE_TYPE (GdkRiscosServer, gdk_riscos_server, G_TYPE_OBJECT)

static GdkRiscosServer *the_server;   /* for the main loop's poll function */

static void
gdk_riscos_server_init (GdkRiscosServer *server)
{
  server->next_serial = 1;
  server->next_id = 1;
  server->windows = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  server->handles = g_hash_table_new (NULL, NULL);
}

static void
gdk_riscos_server_finalize (GObject *object)
{
  GdkRiscosServer *server = GDK_RISCOS_SERVER (object);
  g_hash_table_destroy (server->handles);
  g_hash_table_destroy (server->windows);
  g_clear_object (&server->grip);
  G_OBJECT_CLASS (gdk_riscos_server_parent_class)->finalize (object);
}

static void
gdk_riscos_server_class_init (GdkRiscosServerClass *class)
{
  G_OBJECT_CLASS (class)->finalize = gdk_riscos_server_finalize;
}

/* ---- SWI helpers ------------------------------------------------------ */

static _kernel_oserror *
swi (int no, _kernel_swi_regs *r)
{
  return _kernel_swi (no, r, r);
}

static guint32
monotonic_cs (void)
{
  _kernel_swi_regs r;
  swi (OS_ReadMonotonicTime, &r);
  return (guint32) r.r[0];
}

static guint32
now_ms (GdkRiscosServer *server)
{
  server->last_seen_time = monotonic_cs () * 10;
  return server->last_seen_time;
}

static void
read_screen (GdkRiscosServer *server)
{
  /* VDU variables: 0 ModeFlags, 4/5 X/YEigFactor, 9 Log2BPP,
     11/12 X/YWindLimit (pixels - 1). */
  static const int vars[] = { 0, 4, 5, 9, 11, 12, -1 };
  int vals[6] = { 0, 1, 1, 5, 1023, 767 };
  _kernel_swi_regs r;

  r.r[0] = (int) vars;
  r.r[1] = (int) vals;
  swi (OS_ReadVduVariables, &r);
  server->mode_flags = vals[0];
  server->dev_xeig = vals[1];
  server->dev_yeig = vals[2];
  server->log2bpp = vals[3];
  /* An EX0 EY0 mode (180 dpi: one OS unit a pixel) gets GDK scale 2, so
     GTK lays its windows out in 90 dpi pixels, as on the usual EX1 EY1
     desktop, and draws them at the screen's resolution: text and widgets
     are their usual size, and sharp. Decided once, when the program
     starts (GTK's layout follows it); a later mode change keeps it, and
     windows stay the right size because GDK pixels are converted to OS
     units with xeig and yeig. RISCOS_GTK_SCALE=1 or 2 overrides. */
  if (server->scale == 0)
    server->scale = _gdk_riscos_scale_factor ();
  server->xeig = server->dev_xeig + (server->scale == 2);
  server->yeig = server->dev_yeig + (server->scale == 2);
  server->screen_w = (vals[4] + 1) / server->scale;
  server->screen_h = (vals[5] + 1) / server->scale;
}

/* A backing surface's size in GDK pixels (its device scale undone) */
static int
surface_w (cairo_surface_t *s)
{
  double sx = 1, sy = 1;
  cairo_surface_get_device_scale (s, &sx, &sy);
  return (int) (cairo_image_surface_get_width (s) / sx + 0.5);
}

static int
surface_h (cairo_surface_t *s)
{
  double sx = 1, sy = 1;
  cairo_surface_get_device_scale (s, &sx, &sy);
  return (int) (cairo_image_surface_get_height (s) / sy + 0.5);
}

static int
key_down (int internal_key)
{
  /* OS_Byte 121 with X = internal key number EOR &80: X = &FF if down */
  return (_kernel_osbyte (121, internal_key ^ 0x80, 0) & 0xFF) == 0xFF;
}

static guint32
modifier_state (GdkRiscosServer *server)
{
  guint32 state = 0;

  if (key_down (0))
    state |= GDK_SHIFT_MASK;
  if (key_down (1))
    state |= GDK_CONTROL_MASK;
  if (key_down (2))
    state |= GDK_MOD1_MASK;
  if (server->buttons & BUT_SELECT)
    state |= GDK_BUTTON1_MASK;
  if (server->buttons & BUT_ADJUST)
    state |= GDK_BUTTON2_MASK;
  if (server->buttons & BUT_MENU)
    state |= GDK_BUTTON3_MASK;
  return state;
}

/* OS units on the screen <-> GDK root pixels */
static int
os_to_px_x (GdkRiscosServer *s, int x)
{
  return x >> s->xeig;
}

static int
os_to_px_y (GdkRiscosServer *s, int y)
{
  return s->screen_h - 1 - (y >> s->yeig);
}

/* ---- windows ----------------------------------------------------------- */

static RiscosWindow *
window_by_id (GdkRiscosServer *server, guint32 id)
{
  return g_hash_table_lookup (server->windows, GUINT_TO_POINTER (id));
}

static RiscosWindow *
window_by_handle (GdkRiscosServer *server, int handle)
{
  return g_hash_table_lookup (server->handles, GINT_TO_POINTER (handle));
}

/* Visible area of a window at GDK position (x, y) and size (w, h) */
static void
visible_area (GdkRiscosServer *s, int x, int y, int w, int h, int box[4])
{
  int screen_top = s->screen_h << s->yeig;

  box[0] = x << s->xeig;
  box[2] = (x + w) << s->xeig;
  box[3] = screen_top - (y << s->yeig);
  box[1] = box[3] - (h << s->yeig);
}

static gboolean create_wimp_window (GdkRiscosServer *server, RiscosWindow *w);
static void utf8_to_latin1 (const char *utf8, char *out, int size);

/* TRUE if it was moved to show its title bar (GDK is to be told) */
static gboolean
open_window (GdkRiscosServer *s, RiscosWindow *w, int behind)
{
  int block[8];
  _kernel_swi_regs r;

  if (w->as_menu && !w->visible)
    {
      /* a transient dialogue box (as a Save box opened from a menu):
         opened as a menu, so a click elsewhere closes it (the Wimp sends
         Message_MenusDeleted with its handle). It opens at its last
         size; Wimp_OpenWindow below gives it GTK's. */
      int area[4];
      visible_area (s, w->x, w->y, MAX (w->width, 1) + w->strip, MAX (w->height, 1), area);
      r.r[1] = w->handle;
      r.r[2] = area[0];
      r.r[3] = area[3];
      if (swi (Wimp_CreateMenu, &r) == NULL)
        {
          w->menu_open = TRUE;
          s->dbox_menu = w->handle;
        }
    }
  block[0] = w->handle;
  visible_area (s, w->x, w->y, MAX (w->width, 1) + w->strip, MAX (w->height, 1), &block[1]);
  block[5] = 0;
  block[6] = 0;
  block[7] = behind;
  r.r[1] = (int) block;
  if (swi (Wimp_OpenWindow, &r) == NULL)
    w->visible = TRUE;
  else
    return FALSE;
  /* GTK places a window by its frame (gtk_window_move (w, 0, 0) is the
     top-left corner of the screen, title bar and all), GDK here by its
     work area: a window the program puts at the top of the screen would
     have its title bar off the top, where it can't be moved or closed.
     It is moved down to show the title bar. */
  if ((w->furniture & RISCOS_FURNITURE_TITLE) && !w->is_temp && !w->as_menu)
    {
      int outline[5], screen_top = s->screen_h << s->yeig;
      outline[0] = w->handle;
      r.r[1] = (int) outline;
      if (swi (Wimp_GetWindowOutline, &r) == NULL)
        {
          w->title_h = (outline[4] - block[4] + (1 << s->yeig) - 1) >> s->yeig;
          if (outline[4] > screen_top)
            {
              w->y += (outline[4] - screen_top + (1 << s->yeig) - 1) >> s->yeig;
              block[0] = w->handle;
              visible_area (s, w->x, w->y, MAX (w->width, 1) + w->strip, MAX (w->height, 1), &block[1]);
              block[5] = 0;
              block[6] = 0;
              block[7] = behind;
              r.r[1] = (int) block;
              swi (Wimp_OpenWindow, &r);
              return TRUE;
            }
        }
    }
  return FALSE;
}

int
_gdk_riscos_server_window_title_height (GdkRiscosServer *server, gint id)
{
  RiscosWindow *w = window_by_id (server, id);

  if (w == NULL || !(w->furniture_wanted & RISCOS_FURNITURE_TITLE))
    return 0;
  if (w->title_h > 0)
    return w->title_h;
  return 44 >> server->yeig;   /* the usual title bar, before it has opened */
}

/* ---- input messages to GDK ------------------------------------------- */

static void
send (GdkRiscosServer *server, RiscosInputMsg *msg, char type)
{
  msg->base.type = type;
  msg->base.serial = server->next_serial++;
  msg->base.time = now_ms (server);
  _gdk_riscos_events_got_input (msg);
}

/* Fill in a pointer message for the window events should go to (the grab
   window, if any, else the window under the pointer). */
static gboolean
pointer_msg (GdkRiscosServer *server, RiscosInputPointerMsg *p,
             guint32 under, int root_x, int root_y)
{
  guint32 target = under;
  RiscosWindow *w;

  if (server->grab_window &&
      !(server->grab_owner_events && under != 0))
    target = server->grab_window;
  else if (server->buttons && server->press_window && window_by_id (server, server->press_window))
    /* While a button is held its events, the release too, go to the
       window it went down in, wherever the pointer is (as X's implicit
       grab). Else a release over another task's window, or over a Wimp
       menu just opened from a menu bar, was lost and GDK kept sending
       later clicks to the old widget. */
    target = server->press_window;
  w = window_by_id (server, target);
  if (w == NULL)
    return FALSE;
  p->mouse_window_id = under;
  p->event_window_id = target;
  p->root_x = root_x;
  p->root_y = root_y;
  p->win_x = root_x - w->x;
  p->win_y = root_y - w->y;
  p->state = modifier_state (server);
  return TRUE;
}

static void
send_configure (GdkRiscosServer *server, RiscosWindow *w)
{
  RiscosInputMsg msg;

  memset (&msg, 0, sizeof msg);
  msg.configure_notify.id = w->id;
  msg.configure_notify.x = w->x;
  msg.configure_notify.y = w->y;
  msg.configure_notify.width = w->width;
  msg.configure_notify.height = w->height;
  send (server, &msg, RISCOS_EVENT_CONFIGURE_NOTIFY);
}

static void
send_screen_size (GdkRiscosServer *server)
{
  RiscosInputMsg msg;

  memset (&msg, 0, sizeof msg);
  msg.screen_resize_notify.width = server->screen_w;
  msg.screen_resize_notify.height = server->screen_h;
  send (server, &msg, RISCOS_EVENT_SCREEN_SIZE_CHANGED);
}

static void
send_focus (GdkRiscosServer *server, guint32 new_id)
{
  RiscosInputMsg msg;

  if (new_id == server->focus_window)
    return;
  memset (&msg, 0, sizeof msg);
  msg.focus.old_id = server->focus_window;
  msg.focus.new_id = new_id;
  server->focus_window = new_id;
  send (server, &msg, RISCOS_EVENT_FOCUS);
}

static void
send_crossing (GdkRiscosServer *server, guint32 id, char type, int x, int y)
{
  RiscosInputMsg msg;

  memset (&msg, 0, sizeof msg);
  if (!pointer_msg (server, &msg.pointer, id, x, y))
    return;
  msg.pointer.event_window_id = id;     /* crossings go to the real window */
  msg.pointer.win_x = x - window_by_id (server, id)->x;
  msg.pointer.win_y = y - window_by_id (server, id)->y;
  msg.crossing.mode = GDK_CROSSING_NORMAL;
  send (server, &msg, type);
}

static void
send_button (GdkRiscosServer *server, guint32 under, int x, int y,
             int gdk_button, char type)
{
  RiscosInputMsg msg;

  memset (&msg, 0, sizeof msg);
  if (!pointer_msg (server, &msg.pointer, under, x, y))
    return;
  msg.button.button = gdk_button;
  if (g_getenv ("RISCOS_GTK_DEBUG"))
    g_printerr ("riscos-gtk: button %d %s: window %u (event window %u), root %d,%d, in window %d,%d, grab %u\n",
                gdk_button, type == RISCOS_EVENT_BUTTON_PRESS ? "press" : "release",
                msg.pointer.mouse_window_id, msg.pointer.event_window_id, x, y,
                msg.pointer.win_x, msg.pointer.win_y, server->grab_window);
  send (server, &msg, type);
}

static int
gdk_button_for (int wimp_button)
{
  return wimp_button == BUT_SELECT ? 1 : wimp_button == BUT_ADJUST ? 2 : 3;
}

/* Read the pointer and send motion, crossing and release events for what
   changed since the last look. TRUE if it sent any. */
static gboolean
track_pointer (GdkRiscosServer *server)
{
  guint32 serial = server->next_serial;
  int block[5];
  _kernel_swi_regs r;
  RiscosWindow *w;
  guint32 under = 0;
  int x, y, released, b;

  r.r[1] = (int) block;
  if (swi (Wimp_GetPointerInfo, &r) != NULL)
    return FALSE;
  x = os_to_px_x (server, block[0]);
  y = os_to_px_y (server, block[1]);
  w = window_by_handle (server, block[3]);
  if (w)
    under = w->id;

  /* our shape over our window, the normal pointer elsewhere */
  {
    int shape = w ? w->pointer_shape : RISCOS_POINTER_DEFAULT;
    if (shape != server->shown_shape)
      {
        _gdk_riscos_pointer_show (shape);
        server->shown_shape = shape;
      }
  }

  if (under != server->pointer_window)
    {
      if (server->pointer_window && window_by_id (server, server->pointer_window))
        send_crossing (server, server->pointer_window, RISCOS_EVENT_LEAVE, x, y);
      server->pointer_window = under;
      if (under)
        send_crossing (server, under, RISCOS_EVENT_ENTER, x, y);
    }

  if (x != server->last_x || y != server->last_y)
    {
      RiscosInputMsg msg;

      server->last_x = x;
      server->last_y = y;
      memset (&msg, 0, sizeof msg);
      if (pointer_msg (server, &msg.pointer, under, x, y))
        send (server, &msg, RISCOS_EVENT_POINTER_MOVE);
    }

  released = server->buttons & ~block[2];
  for (b = BUT_SELECT; b; b >>= 1)
    if (released & b)
      {
        send_button (server, under, x, y, gdk_button_for (b),
                     RISCOS_EVENT_BUTTON_RELEASE);
        server->buttons &= ~b;
      }
  if (server->buttons == 0)
    server->press_window = 0;
  return server->next_serial != serial;
}

/* RISC OS key code -> GDK keyval (0 = not one GDK should see) */
static guint
keyval_for (int code, guint32 *state)
{
  switch (code)
    {
    case 8:     return GDK_KEY_BackSpace;
    case 13:    return GDK_KEY_Return;
    case 27:    return GDK_KEY_Escape;
    case 0x7F:  return GDK_KEY_Delete;
    case 0x1E:  return GDK_KEY_Home;
    case 0x18B: return GDK_KEY_End;          /* Copy */
    case 0x18A: return GDK_KEY_Tab;
    case 0x19A: *state |= GDK_SHIFT_MASK; return GDK_KEY_ISO_Left_Tab;
    case 0x18C: return GDK_KEY_Left;
    case 0x18D: return GDK_KEY_Right;
    case 0x18E: return GDK_KEY_Down;
    case 0x18F: return GDK_KEY_Up;
    case 0x19E: return GDK_KEY_Page_Down;    /* Shift-Down */
    case 0x19F: return GDK_KEY_Page_Up;      /* Shift-Up */
    case 0x1CD: return GDK_KEY_Insert;
    case 0x1AC: *state |= GDK_CONTROL_MASK; return GDK_KEY_Left;
    case 0x1AD: *state |= GDK_CONTROL_MASK; return GDK_KEY_Right;
    case 0x1AE: *state |= GDK_CONTROL_MASK; return GDK_KEY_Down;
    case 0x1AF: *state |= GDK_CONTROL_MASK; return GDK_KEY_Up;
    case 0x19C: *state |= GDK_SHIFT_MASK; return GDK_KEY_Left;
    case 0x19D: *state |= GDK_SHIFT_MASK; return GDK_KEY_Right;
    case 0x1CA: return GDK_KEY_F10;
    case 0x1CB: return GDK_KEY_F11;
    default: break;
    }
  if (code >= 0x181 && code <= 0x189)
    return GDK_KEY_F1 + (code - 0x181);
  if (code >= 1 && code <= 26 && code != 8 && code != 13)
    {
      *state |= GDK_CONTROL_MASK;            /* Ctrl-A .. Ctrl-Z */
      return GDK_KEY_a + (code - 1);
    }
  if ((code >= 32 && code < 127) || (code >= 160 && code <= 255))
    return code;                             /* Latin-1 = keysym */
  return 0;
}

/* The letter or digit key held down with Left Alt, as GTK's mnemonics
   (Alt+F for a File menu) and Alt accelerators want it: the keyboard may
   have made another character of it (an accented one), or none GTK knows.
   Keyboard scan codes (PRMs, "Keyboard Scan Codes"); 0 if none is down. */
static guint
alt_letter (void)
{
  static const unsigned char letters[26] = {
    65, 100, 82, 50, 34, 67, 83, 84, 37, 69, 70, 86, 101,     /* A..M */
    85, 54, 55, 16, 51, 81, 35, 53, 99, 33, 66, 68, 97        /* N..Z */
  };
  static const unsigned char digits[10] = { 39, 48, 49, 17, 18, 19, 52, 36, 21, 38 };
  int i;

  for (i = 0; i < 26; i++)
    if (key_down (letters[i]))
      return GDK_KEY_a + i;
  for (i = 0; i < 10; i++)
    if (key_down (digits[i]))
      return GDK_KEY_0 + i;
  return 0;
}

static void
handle_key (GdkRiscosServer *server, int *block)
{
  RiscosWindow *w = window_by_handle (server, block[0]);
  RiscosInputMsg msg;
  guint32 state;
  guint keyval, typed;
  _kernel_swi_regs r;

  state = modifier_state (server) & ~(GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK);
  keyval = keyval_for (block[6], &state);
  typed = keyval;
  /* Left Alt (Right Alt is AltGr for typing): the key, not the character */
  if (key_down (5) && !(state & GDK_CONTROL_MASK))
    {
      guint letter = alt_letter ();
      if (letter)
        {
          keyval = letter;
          state |= GDK_MOD1_MASK;
        }
    }
  if (g_getenv ("RISCOS_GTK_KEY_DEBUG"))  /* the Log, for key problems */
    g_printerr ("riscos-gtk: key &%X in window &%X -> keyval %x (typed %x) state %x%s\n", block[6], block[0],
                keyval, typed, state, w ? "" : " (not ours)");
  if (w == NULL || keyval == 0)
    {
      /* Not ours (F12, hot keys...): give it back to the Wimp. */
      r.r[0] = block[6];
      swi (Wimp_ProcessKey, &r);
      return;
    }
  memset (&msg, 0, sizeof msg);
  msg.key.window_id = w->id;
  msg.key.state = state;
  msg.key.key = keyval;
  msg.key.typed = typed;
  send (server, &msg, RISCOS_EVENT_KEY_PRESS);
  /* The Wimp has no key-release events. */
  memset (&msg, 0, sizeof msg);
  msg.key.window_id = w->id;
  msg.key.state = state;
  msg.key.key = keyval;
  msg.key.typed = typed;
  send (server, &msg, RISCOS_EVENT_KEY_RELEASE);
}

static void
take_caret (RiscosWindow *w)
{
  _kernel_swi_regs r;

  r.r[0] = w->handle;
  r.r[1] = -1;
  r.r[2] = 0;
  r.r[3] = 0;
  r.r[4] = 1 << 25;          /* invisible caret */
  r.r[5] = -1;
  swi (Wimp_SetCaretPosition, &r);
}

/* The size grip. The Wimp puts its Adjust Size icon only where a window's
   scroll bars meet, and GTK windows have no Wimp scroll bars (GTK draws
   its own), so a resizable document window (title bar and back icon:
   not a dialogue box) gets the desktop's own
   size icon sprite ("sicon", from the window border sprites) drawn in the
   bottom right corner of its work area. Select or Adjust there starts the
   Wimp's own size drag (Wimp_DragBox type 2); Select also brings the
   window to the front, as the real icon does (PRMs, Adjust Size Icon). */
static gboolean
grip_for (GdkRiscosServer *server, RiscosWindow *w, unsigned furniture)
{
  /* document windows only: dialogue boxes (no back icon) have none, as
     on RISC OS, and keep the toggle size icon */
  if (!(furniture & RISCOS_FURNITURE_RESIZE) || !(furniture & RISCOS_FURNITURE_TITLE) ||
      !(furniture & RISCOS_FURNITURE_BACK) ||
      w->is_temp || w->as_menu)
    return FALSE;
  if (!server->grip_read)
    {
      server->grip_read = TRUE;
      if (g_strcmp0 (g_getenv ("RISCOS_GTK_SIZE_GRIP"), "0") == 0)
        server->grip_w = server->grip_h = 0;
      else if ((server->grip = gdk_riscos_wimp_tool_sprite ("sicon")))
        {
          server->grip_w = gdk_pixbuf_get_width (server->grip);
          server->grip_h = gdk_pixbuf_get_height (server->grip);
        }
      else
        {
          server->grip_w = 40 >> server->xeig;
          server->grip_h = 40 >> server->yeig;
        }
    }
  return server->grip_w > 0 && w->width > 2 * server->grip_w && w->height > 2 * server->grip_h;
}

static gboolean
has_grip (GdkRiscosServer *server, RiscosWindow *w)
{
  return grip_for (server, w, w->furniture);
}

/* For GTK (gtkriscostheme.c): the grip's size if the window has one (or
   will have, at its next showing), so scroll bars in that corner can stop
   short of it, as a Wimp scroll bar stops at the Adjust Size icon. */
gboolean
_gdk_riscos_server_window_get_grip (GdkRiscosServer *server, gint id, int *width, int *height)
{
  RiscosWindow *w = window_by_id (server, id);

  if (w == NULL || !grip_for (server, w, w->visible ? w->furniture : w->furniture_wanted))
    return FALSE;
  *width = server->grip_w;
  *height = server->grip_h;
  return TRUE;
}

/* the grip's column: under GTK's vertical scroll bar when GTK has said
   where that is, else the window's right edge */
static int
grip_width (GdkRiscosServer *server, RiscosWindow *w)
{
  return w->strip > 0 ? w->strip : w->grip_w > 0 ? w->grip_w : server->grip_w;
}

static int
grip_left (GdkRiscosServer *server, RiscosWindow *w)
{
  return w->strip > 0 ? w->width : w->grip_w > 0 ? w->grip_x : w->width - server->grip_w;
}

/* the grip's top in the window (pixels): at the foot of GTK's scroll bar
   when GTK has said where that is, else at the window's foot */
static int
grip_top (GdkRiscosServer *server, RiscosWindow *w)
{
  if (w->strip == 0 && w->grip_w > 0 && w->grip_top > 0)
    return w->grip_top - 1;
  return w->height - server->grip_h;
}

void
_gdk_riscos_server_window_set_grip_column (GdkRiscosServer *server, gint id, int x, int width, int y)
{
  RiscosWindow *w = window_by_id (server, id);

  if (w == NULL)
    return;
  w->grip_x = x;
  w->grip_w = width > 0 && width <= 2 * server->grip_w ? width : 0;
  w->grip_top = y >= 0 && y < w->height ? y + 1 : 0;
}

static gboolean
paint_grip_at (GdkRiscosServer *server, RiscosWindow *w, cairo_surface_t *surface, int x,
               cairo_rectangle_int_t *rect);

static gboolean
paint_grip (GdkRiscosServer *server, RiscosWindow *w, cairo_surface_t *surface,
            cairo_rectangle_int_t *rect)
{
  return paint_grip_at (server, w, surface, grip_left (server, w), rect);
}

/* rect (may be NULL): where it went, GDK pixels */
static gboolean
paint_grip_at (GdkRiscosServer *server, RiscosWindow *w, cairo_surface_t *surface, int x,
               cairo_rectangle_int_t *rect)
{
  int sw = surface_w (surface), sh = surface_h (surface);
  int gw = grip_width (server, w), gh = server->grip_h;
  int y = MIN (grip_top (server, w), sh - gh);
  cairo_t *cr;

  if (x < 0 || y < 0 || x + gw > sw)
    return FALSE;
  if (rect)
    {
      rect->x = x;
      rect->y = y;
      rect->width = gw;
      rect->height = gh;
    }
  cr = cairo_create (surface);
  if (server->grip)
    {
      /* in the scroll bar's column: as wide as the bar, as the Wimp's is */
      cairo_rectangle (cr, x, y, gw, gh);
      cairo_clip (cr);
      cairo_translate (cr, x, y);
      cairo_scale (cr, (double) gw / server->grip_w, 1.0);
      gdk_cairo_set_source_pixbuf (cr, server->grip, 0, 0);
      cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_NEAREST);
      cairo_paint (cr);
    }
  else
    {
      /* no sicon sprite: a plain ridged corner in the window greys */
      int i;
      cairo_rectangle (cr, x, y, gw, gh);
      cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);
      cairo_fill (cr);
      cairo_set_line_width (cr, 1);
      for (i = 1; i <= 3; i++)
        {
          double d = i * gw / 4.0;
          cairo_move_to (cr, x + gw - d, y + gh - 0.5);
          cairo_line_to (cr, x + gw - 0.5, y + gh - d);
          cairo_set_source_rgb (cr, 0.4, 0.4, 0.4);
          cairo_stroke (cr);
          cairo_move_to (cr, x + gw - d + 1.5, y + gh - 0.5);
          cairo_line_to (cr, x + gw - 0.5, y + gh - d + 1.5);
          cairo_set_source_rgb (cr, 1, 1, 1);
          cairo_stroke (cr);
        }
      cairo_rectangle (cr, x + 0.5, y + 0.5, gw - 1, gh - 1);
      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_stroke (cr);
    }
  cairo_destroy (cr);
  cairo_surface_flush (surface);
  return TRUE;
}

static void
start_size_drag (GdkRiscosServer *server, RiscosWindow *w, gboolean to_front)
{
  int block[10];
  _kernel_swi_regs r;

  if (to_front && open_window (server, w, -1))
    send_configure (server, w);
  memset (block, 0, sizeof block);
  block[0] = w->handle;
  block[1] = 2;                      /* drag window size: the Wimp does it */
  r.r[1] = (int) block;
  swi (Wimp_DragBox, &r);
}

static void
handle_click (GdkRiscosServer *server, int *block)
{
  RiscosWindow *w = window_by_handle (server, block[3]);
  int x = os_to_px_x (server, block[0]);
  int y = os_to_px_y (server, block[1]);
  int b, v = block[2], pressed;

  if (w == NULL)
    {
      _gdk_riscos_iconbar_click (block);
      return;
    }
  /* Button type 10: click = b << 8, drag = b << 4, double click = b.
     A drag is the same press still held: nothing new to send. */
  pressed = (v >> 8) & 7;
  pressed |= v & 7;
  if (pressed == 0)
    return;
  if (w->id != server->focus_window && !w->is_temp)
    take_caret (w);
  if ((pressed & (BUT_SELECT | BUT_ADJUST)) && server->buttons == 0 && has_grip (server, w) &&
      x >= w->x + grip_left (server, w) && x < w->x + grip_left (server, w) + grip_width (server, w) &&
      y >= w->y + grip_top (server, w) && y < w->y + grip_top (server, w) + server->grip_h)
    {
      start_size_drag (server, w, (pressed & BUT_SELECT) != 0);
      return;
    }
  /* the rest of the strip's scroll bar has nothing to scroll */
  if (w->strip > 0 && x >= w->x + w->width)
    return;
  if (server->pointer_window != w->id)
    track_pointer (server);
  if (server->buttons == 0)
    server->press_window = w->id;
  for (b = BUT_SELECT; b; b >>= 1)
    if (pressed & b)
      {
        server->buttons |= b;
        send_button (server, w->id, x, y, gdk_button_for (b),
                     RISCOS_EVENT_BUTTON_PRESS);
      }
}

/* Fill a rectangle (screen OS units, inclusive) in Wimp colour 1, the
   theme's window grey; the Wimp has clipped the graphics window to the
   rectangle being redrawn. */
static void
fill_grey (int x0, int y0, int x1, int y1)
{
  _kernel_swi_regs r;

  if (x1 < x0 || y1 < y0)
    return;
  r.r[0] = 1;
  swi (Wimp_SetColour, &r);
  r.r[0] = 4;                        /* move */
  r.r[1] = x0;
  r.r[2] = y0;
  swi (OS_Plot, &r);
  r.r[0] = 96 + 5;                   /* rectangle fill to */
  r.r[1] = x1;
  r.r[2] = y1;
  swi (OS_Plot, &r);
}

/* Plot a window's sprite for each rectangle of a redraw or update loop,
   and paint whatever the sprite doesn't cover (after the Wimp has made
   the window bigger and before GTK has drawn the new size): the work area
   has no background colour, so anything not painted would show the old
   screen. */
static void
plot_rectangles (GdkRiscosServer *server, RiscosWindow *w, int *block, int more)
{
  _kernel_swi_regs r;
  void *area = w->surface ? cairo_surface_get_user_data (w->surface, &_gdk_riscos_sprite_key) : NULL;
  /* the sprite's size on screen (OS units), and one screen pixel */
  int sw = w->surface ? cairo_image_surface_get_width (w->surface) << server->dev_xeig : 0;
  int sh = w->surface ? cairo_image_surface_get_height (w->surface) << server->dev_yeig : 0;
  int pxw = 1 << server->dev_xeig, pxh = 1 << server->dev_yeig;

  while (more)
    {
      int ox = block[1] - block[5];         /* work area origin on screen */
      int oy = block[4] - block[6];

      if (area)
        {
          int *a = area;
          char *sprite = (char *) area + a[2];

          r.r[0] = 512 + 34;                   /* PutSpriteUserCoords */
          r.r[1] = (int) area;
          r.r[2] = (int) sprite;
          r.r[3] = ox;
          r.r[4] = oy - sh;
          r.r[5] = 0;
          swi (OS_SpriteOp, &r);
        }
      else
        sw = sh = 0;
      /* block[7..10]: this rectangle, screen OS units, x1/y1 exclusive */
      fill_grey (MAX (block[7], ox + sw), block[8], block[9] - pxw, block[10] - pxh);
      fill_grey (block[7], block[8], MIN (block[9], ox + sw) - pxw, MIN (block[10], oy - sh) - pxh);
      if (w->strip_surface)
        {
          int *a = cairo_surface_get_user_data (w->strip_surface, &_gdk_riscos_sprite_key);
          if (a)
            {
              r.r[0] = 512 + 34;
              r.r[1] = (int) a;
              r.r[2] = (int) ((char *) a + a[2]);
              r.r[3] = ox + (w->width << server->xeig);
              r.r[4] = oy - (cairo_image_surface_get_height (w->strip_surface) << server->dev_yeig);
              r.r[5] = 0;
              swi (OS_SpriteOp, &r);
            }
        }
      r.r[1] = (int) block;
      if (swi (Wimp_GetRectangle, &r) != NULL)
        break;
      more = r.r[0];
    }
}

static void sync_sprite_if_stale (cairo_surface_t *surface);

static void
handle_redraw (GdkRiscosServer *server, int *block)
{
  RiscosWindow *w = window_by_handle (server, block[0]);
  _kernel_swi_regs r;

  r.r[1] = (int) block;
  if (swi (Wimp_RedrawWindow, &r) != NULL)
    return;
  if (w)
    {
      sync_sprite_if_stale (w->surface);
      plot_rectangles (server, w, block, r.r[0]);
    }
  else
    while (r.r[0])
      {
        r.r[1] = (int) block;
        if (swi (Wimp_GetRectangle, &r) != NULL)
          break;
      }
}

static void
handle_open_request (GdkRiscosServer *server, int *block)
{
  RiscosWindow *w = window_by_handle (server, block[0]);
  _kernel_swi_regs r;
  int x, y, width, height;

  block[5] = 0;              /* no scrolling: GTK draws the whole window */
  block[6] = 0;
  r.r[1] = (int) block;
  swi (Wimp_OpenWindow, &r);
  if (w == NULL)
    return;
  w->visible = TRUE;
  /* Wimp_OpenWindow updates the block to where the window really went. */
  x = os_to_px_x (server, block[1]);
  y = server->screen_h - (block[4] >> server->yeig);
  width = ((block[3] - block[1]) >> server->xeig) - w->strip;
  height = (block[4] - block[2]) >> server->yeig;
  if (width < 1)
    width = 1;
  if (x != w->x || y != w->y || width != w->width || height != w->height)
    {
      w->x = x;
      w->y = y;
      w->width = width;
      w->height = height;
      send_configure (server, w);
    }
}

static void
handle_message (GdkRiscosServer *server, int reason, int *block)
{
  /* printing (!Printers' answers), the icon bar icon: files opened from
     the Filer, PreQuit */
  if (_gdk_riscos_spawn_message (reason, block) ||
      _gdk_riscos_print_message (reason, block) || _gdk_riscos_iconbar_message (reason, block))
    return;
  switch (block[4])
    {
    case MESSAGE_QUIT:
      {
        _kernel_swi_regs r;
        if (server->shown_shape != RISCOS_POINTER_DEFAULT)
          _gdk_riscos_pointer_show (RISCOS_POINTER_DEFAULT);
        r.r[0] = server->task;
        r.r[1] = 0x4B534154;
        swi (Wimp_CloseDown, &r);
        exit (0);
      }
    case MESSAGE_MODECHANGE:
      /* the Wimp puts the normal pointer back; track_pointer sets ours */
      server->shown_shape = RISCOS_POINTER_DEFAULT;
      read_screen (server);
      send_screen_size (server);
      break;
    case MESSAGE_MENUSDELETED:
      /* a transient dialogue box closed by a click elsewhere: the
         Wimp has closed it; GTK gets a delete event (its Cancel) */
      if (server->dbox_menu && block[5] == server->dbox_menu)
        {
          RiscosWindow *w = window_by_handle (server, server->dbox_menu);
          server->dbox_menu = 0;
          if (w)
            {
              RiscosInputMsg msg;
              w->visible = FALSE;
              w->menu_open = FALSE;
              memset (&msg, 0, sizeof msg);
              msg.delete_notify.id = w->id;
              send (server, &msg, RISCOS_EVENT_DELETE_NOTIFY);
            }
          break;
        }
      /* closed without a choice (a click elsewhere, Escape) */
      if (server->menu_block && block[5] == (int) server->menu_block && server->menu_func)
        {
          GdkRiscosMenuFunc func = server->menu_func;
          server->menu_block = NULL;
          func (NULL, FALSE, server->menu_data);
        }
      break;
    default:
      /* the clipboard and data transfer (gdktransfer-riscos.c) */
      _gdk_riscos_transfer_message (reason, block);
      break;
    }
}

/* The scroll wheel: an "extended" Scroll_Request (the window asks for
   them with bit 1 of its extra window flags). +32, +36 = how far to
   scroll in x and y, OS units, multiples of 4 (bottom two bits 0: that's
   what marks an extended request); y positive = up. How far one wheel
   nudge goes is the user's *WimpScroll -Speed, so it becomes GTK scroll
   steps (each about a line), at least one, one per 64 OS units. */
#define SCROLL_STEP 64

static void
handle_scroll (GdkRiscosServer *server, int *block)
{
  RiscosWindow *w = window_by_handle (server, block[0]);
  int dx = block[8], dy = block[9], ptr[5] = { 0 };
  int axis;
  _kernel_swi_regs r;

  if (w == NULL || ((dx | dy) & 3) != 0)
    return;                        /* not ours, or a scroll bar's (we have none) */
  r.r[1] = (int) ptr;
  if (swi (Wimp_GetPointerInfo, &r) != NULL)
    return;
  if (g_getenv ("RISCOS_GTK_DEBUG"))
    g_printerr ("riscos-gtk: wheel: scroll request %d,%d\n", dx, dy);
  for (axis = 0; axis < 2; axis++)
    {
      int d = axis ? dy : dx, n, i;
      if (d == 0)
        continue;
      n = CLAMP ((ABS (d) + SCROLL_STEP / 2) / SCROLL_STEP, 1, 8);
      for (i = 0; i < n; i++)
        {
          RiscosInputMsg msg;
          int x = os_to_px_x (server, ptr[0]), y = os_to_px_y (server, ptr[1]);

          memset (&msg, 0, sizeof msg);
          if (!pointer_msg (server, &msg.pointer, w->id, x, y))
            return;
          msg.scroll.dir = axis ? (d > 0 ? 0 : 1) : (d > 0 ? 3 : 2);
          send (server, &msg, RISCOS_EVENT_SCROLL);
        }
    }
}

/* Is anything worth watching with null events: the pointer over one of our
   windows, or a button held? */
static gboolean
want_nulls (GdkRiscosServer *server)
{
  return server->pointer_window != 0 || server->buttons != 0;
}

/* One Wimp_PollIdle: returns TRUE for an event GDK may want to see now,
   FALSE for a null event. until = monotonic time (cs) to return by. */
static gboolean
poll_once (GdkRiscosServer *server, guint32 until, gboolean nulls)
{
  int block[64];
  _kernel_swi_regs r;
  int mask = (1 << 13);            /* no poll word events */

  if (!nulls)
    mask |= 1;                     /* no null events */
  r.r[0] = mask;
  r.r[1] = (int) block;
  r.r[2] = (int) until;
  r.r[3] = 0;
  if (swi (Wimp_PollIdle, &r) != NULL)
    return FALSE;

  switch (r.r[0])
    {
    case NULL_REASON:
      /* motion (each one: a drag needs to see the pointer move, not
         only where it ended up) */
      return want_nulls (server) && track_pointer (server);
    case REDRAW_WINDOW_REQUEST:
      handle_redraw (server, block);
      return FALSE;                /* drawn here: nothing for GDK */
    case OPEN_WINDOW_REQUEST:
      handle_open_request (server, block);
      return TRUE;
    case CLOSE_WINDOW_REQUEST:
      {
        RiscosWindow *w = window_by_handle (server, block[0]);
        if (w)
          {
            RiscosInputMsg msg;
            memset (&msg, 0, sizeof msg);
            msg.delete_notify.id = w->id;
            send (server, &msg, RISCOS_EVENT_DELETE_NOTIFY);
          }
      }
      return TRUE;
    case POINTER_LEAVING_WINDOW:
    case POINTER_ENTERING_WINDOW:
      track_pointer (server);
      return TRUE;
    case MOUSE_CLICK:
      handle_click (server, block);
      return TRUE;
    case KEY_PRESSED:
      handle_key (server, block);
      return TRUE;
    case LOSE_CARET:
      {
        RiscosWindow *w = window_by_handle (server, block[0]);
        if (w && w->id == server->focus_window)
          send_focus (server, 0);
      }
      return TRUE;
    case GAIN_CARET:
      {
        RiscosWindow *w = window_by_handle (server, block[0]);
        if (w)
          send_focus (server, w->id);
      }
      return TRUE;
    case USER_MESSAGE:
    case USER_MESSAGE_RECORDED:
      handle_message (server, r.r[0], block);
      return TRUE;
    case USER_MESSAGE_ACKNOWLEDGE:
      /* one of our recorded messages came back unanswered */
      if (!_gdk_riscos_print_message (r.r[0], block))
        _gdk_riscos_transfer_message (r.r[0], block);
      return TRUE;
    case SCROLL_REQUEST:
      handle_scroll (server, block);
      return TRUE;
    case MENU_SELECTION:
      if (server->menu_block && server->menu_func)
        {
          int ptr[5] = { 0 };
          _kernel_swi_regs pr;
          pr.r[1] = (int) ptr;
          swi (Wimp_GetPointerInfo, &pr);
          /* GTK handles the choice from an idle callback; the Wimp wants
             an Adjust choice's menu reopened before the next Wimp_Poll,
             so don't poll the Wimp until GTK says it's done */
          server->menu_hold = 8;
          server->menu_func (block, (ptr[2] & 1) != 0, server->menu_data);
        }
      return TRUE;
    default:
      return FALSE;
    }
}

/* GLib's main loop poll function for the default context (see the top of
   the file). timeout is in ms, -1 = for ever. */
static gint
riscos_poll (GPollFD *fds, guint nfds, gint timeout)
{
  GdkRiscosServer *server = the_server;
  guint32 start = monotonic_cs ();
  guint32 deadline = start + (timeout < 0 ? 0x10000000u : (guint32) (timeout + 9) / 10);
  gint n;

  if (server && server->menu_hold > 0)
    {
      /* A menu choice is being handled (see MENU_SELECTION) */
      server->menu_hold--;
      return nfds ? poll ((struct pollfd *) fds, nfds, 0) : 0;
    }

  for (;;)
    {
      guint32 now, until;
      gboolean nulls;

      /* GLib's fds (its wake-up eventfd, sockets, pipes) without blocking */
      n = nfds ? poll ((struct pollfd *) fds, nfds, 0) : 0;
      if (n != 0 || timeout == 0)
        {
          /* Still let the Wimp in now and then, so other tasks run and
             our windows redraw even when fds are always ready. */
          if (server && poll_once (server, monotonic_cs (), TRUE))
            return n > 0 ? n : 0;
          return n;
        }
      now = monotonic_cs ();
      if ((gint32) (now - deadline) >= 0)
        return 0;
      if (server == NULL)
        {
          /* No Wimp task yet: nothing to wait in. */
          return poll ((struct pollfd *) fds, nfds, timeout);
        }
      /* Wait in the Wimp. With fds to watch, or the pointer to track, come
         back every 2 cs; otherwise sleep until GLib's next deadline. */
      nulls = want_nulls (server) || nfds > 0;
      until = deadline;
      if (nulls && (gint32) (until - (now + 2)) > 0)
        until = now + 2;
      if (poll_once (server, until, TRUE))
        return 0;                  /* GDK has events: let GLib dispatch */
    }
}

/* At exit: give the normal pointer back if ours is showing */
static void
restore_pointer (void)
{
  if (the_server && the_server->shown_shape != RISCOS_POINTER_DEFAULT)
    _gdk_riscos_pointer_show (RISCOS_POINTER_DEFAULT);
}

/* ---- the interface for the Broadway-derived GDK code -------------------- */

cairo_user_data_key_t _gdk_riscos_sprite_key;
static cairo_user_data_key_t _gdk_riscos_sprite_data_key;

GdkRiscosServer *
_gdk_riscos_server_new (const char *display, GError **error)
{
  /* PreQuit, ModeChange, MenusDeleted; the data transfer protocol:
     DataSave, DataSaveAck, DataLoad, DataLoadAck, DataOpen (the icon bar),
     RAMFetch, RAMTransmit, ClaimEntity, DataRequest; printing: PrintFile,
     PrintError, PrintTypeOdd, SetPrinter; programs started with g_spawn
     (gdkspawn-riscos.c): TaskCloseDown, TaskWindow_Output, _Ego, _Morio,
     the DDE's throwback messages */
  static const int messages[] = { MESSAGE_PREQUIT, MESSAGE_MODECHANGE, MESSAGE_MENUSDELETED,
                                  1, 2, 3, 4, 5, 6, 7, 0xF, 0x10,
                                  0x80140, 0x80144, 0x80145, 0x80147,
                                  0x400C3, 0x808C1, 0x808C2, 0x808C3,
                                  0x42580, 0x42581, 0x42582, 0x42583, 0x42584, 0x42585, 0x42586, 0 };
  GdkRiscosServer *server;
  _kernel_swi_regs r;
  _kernel_oserror *e;
  const char *name = g_get_application_name ();

  if (name == NULL)
    name = g_get_prgname ();
  if (name == NULL)
    name = "GTK application";

  r.r[0] = 380;
  r.r[1] = 0x4B534154;             /* "TASK" */
  r.r[2] = (int) name;
  r.r[3] = (int) messages;
  e = swi (Wimp_Initialise, &r);
  if (e != NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Wimp_Initialise failed: %s", e->errmess);
      return NULL;
    }

  /* one copy of a program: a second hands its file to the first, and goes */
  _gdk_riscos_iconbar_startup (r.r[1], name);
  _gdk_riscos_spawn_startup (r.r[1]);
  server = g_object_new (GDK_TYPE_RISCOS_SERVER, NULL);
  server->task = r.r[1];
  read_screen (server);
  g_printerr ("riscos-gtk: task \"%s\", screen %dx%d, %d bpp %s, eig %d,%d\n",
              name, server->screen_w, server->screen_h, 1 << server->log2bpp,
              (server->mode_flags & MODEFLAG_TRGB) ? "TRGB" : "TBGR",
              server->dev_xeig, server->dev_yeig);
  if (server->scale != 1)
    g_printerr ("riscos-gtk: GDK scale %d (%d dpi screen: GTK lays out in 90 dpi pixels)\n",
                server->scale, 180 >> server->dev_xeig);
  if (server->log2bpp < 4)
    g_warning ("riscos-gtk: the desktop is %d bpp; GTK windows need a 16 or 32 bpp mode",
               1 << server->log2bpp);
  the_server = server;
  atexit (restore_pointer);
  g_main_context_set_poll_func (NULL, riscos_poll);
  return server;
}

void
_gdk_riscos_server_get_screen_size (GdkRiscosServer *server, int *width, int *height)
{
  *width = server->screen_w;
  *height = server->screen_h;
}

void
_gdk_riscos_server_flush (GdkRiscosServer *server)
{
}

void
_gdk_riscos_server_sync (GdkRiscosServer *server)
{
}

gulong
_gdk_riscos_server_get_next_serial (GdkRiscosServer *server)
{
  return server->next_serial;
}

guint32
_gdk_riscos_server_get_last_seen_time (GdkRiscosServer *server)
{
  return server->last_seen_time;
}

gboolean
_gdk_riscos_server_lookahead_event (GdkRiscosServer *server, const char *types)
{
  return FALSE;
}

void
_gdk_riscos_server_query_mouse (GdkRiscosServer *server, guint32 *toplevel,
                                gint32 *root_x, gint32 *root_y, guint32 *mask)
{
  int block[5];
  _kernel_swi_regs r;
  RiscosWindow *w;

  r.r[1] = (int) block;
  if (swi (Wimp_GetPointerInfo, &r) != NULL)
    memset (block, 0, sizeof block);
  w = window_by_handle (server, block[3]);
  if (toplevel)
    *toplevel = w ? w->id : 0;
  if (root_x)
    *root_x = os_to_px_x (server, block[0]);
  if (root_y)
    *root_y = os_to_px_y (server, block[1]);
  if (mask)
    *mask = modifier_state (server);
}

GdkGrabStatus
_gdk_riscos_server_grab_pointer (GdkRiscosServer *server, gint id,
                                 gboolean owner_events, guint32 event_mask,
                                 guint32 time_)
{
  server->grab_window = id;
  server->grab_owner_events = owner_events;
  return GDK_GRAB_SUCCESS;
}

guint32
_gdk_riscos_server_ungrab_pointer (GdkRiscosServer *server, guint32 time_)
{
  server->grab_window = 0;
  return server->next_serial;
}

gint32
_gdk_riscos_server_get_mouse_toplevel (GdkRiscosServer *server)
{
  return server->pointer_window;
}

guint32
_gdk_riscos_server_new_window (GdkRiscosServer *server, int x, int y,
                               int width, int height, gboolean is_temp)
{
  RiscosWindow *w;

  w = g_new0 (RiscosWindow, 1);
  w->id = server->next_id++;
  w->is_temp = is_temp;
  w->x = x;
  w->y = y;
  w->width = width;
  w->height = height;
  strcpy (w->title, "");

  w->furniture = w->furniture_wanted = is_temp ? 0 : RISCOS_FURNITURE_ALL;
  if (!create_wimp_window (server, w))
    {
      g_free (w);
      return 0;
    }
  g_hash_table_insert (server->windows, GUINT_TO_POINTER (w->id), w);
  g_hash_table_insert (server->handles, GINT_TO_POINTER (w->handle), w);
  return w->id;
}

/* The Wimp window for w (Wimp_CreateWindow), with w->furniture */
static gboolean
create_wimp_window (GdkRiscosServer *server, RiscosWindow *w)
{
  int block[22];
  _kernel_swi_regs r;
  unsigned char *colours = (unsigned char *) &block[8];
  int sw = server->screen_w << server->xeig, sh = server->screen_h << server->yeig;
  unsigned f = w->furniture;

  memset (block, 0, sizeof block);
  visible_area (server, w->x, w->y, MAX (w->width, 1) + w->strip, MAX (w->height, 1), &block[0]);
  block[6] = -1;                                  /* behind: top */
  block[7] = WF_NEW_FORMAT;
  if (w->is_temp || f == 0)
    block[7] |= WF_NO_BOUNDS;
  if (f & RISCOS_FURNITURE_TITLE)
    block[7] |= WF_TITLE | WF_MOVEABLE;
  if (f & RISCOS_FURNITURE_BACK)
    block[7] |= WF_BACK;
  if (f & RISCOS_FURNITURE_CLOSE)
    block[7] |= WF_CLOSE;
  if (f & RISCOS_FURNITURE_RESIZE)
    block[7] |= WF_TOGGLE | WF_ADJUST;
  colours[0] = 7;      /* title foreground */
  colours[1] = 2;      /* title background */
  colours[2] = 7;      /* work area foreground */
  colours[3] = 255;    /* work area background: none (we plot all of it) */
  colours[4] = 3;      /* scroll bar outer */
  colours[5] = 1;      /* scroll bar inner */
  colours[6] = 12;     /* title background with input focus */
  colours[7] = 2;      /* extra window flags: bit 1, extended scroll requests
                          (the scroll wheel; PRMs, Scroll_Request) */
  /* Work area extent: the whole screen, so the window can grow that big */
  block[10] = 0;
  block[11] = -sh;
  block[12] = sw;
  block[13] = 0;
  block[14] = 0x119;                              /* title: text, centred, indirected */
  block[15] = 10 << 12;                           /* buttons: double/click/drag */
  block[16] = 1;                                  /* Wimp sprite area */
  block[17] = (32 << 16) | 32;                    /* minimum height, width (OS units) */
  block[18] = (int) w->title;
  block[19] = -1;
  block[20] = TITLE_MAX;
  block[21] = 0;                                  /* no icons */
  r.r[1] = (int) block;
  if (swi (Wimp_CreateWindow, &r) != NULL)
    return FALSE;
  w->handle = r.r[0];
  return TRUE;
}

/* Title bar, close, back, toggle/adjust size: a Wimp window's furniture
   is fixed when it's made, so a window that's to change is made again
   (only while it isn't on the screen: else at its next showing). */
static void
apply_furniture (GdkRiscosServer *server, RiscosWindow *w)
{
  int block[1];
  _kernel_swi_regs r;
  int old = w->handle;

  if (w->visible || w->is_temp || w->furniture == w->furniture_wanted)
    return;
  w->furniture = w->furniture_wanted;
  if (!create_wimp_window (server, w))
    {
      w->handle = old;
      return;
    }
  block[0] = old;
  r.r[1] = (int) block;
  swi (Wimp_DeleteWindow, &r);
  g_hash_table_remove (server->handles, GINT_TO_POINTER (old));
  g_hash_table_insert (server->handles, GINT_TO_POINTER (w->handle), w);
}

void
_gdk_riscos_server_window_set_furniture (GdkRiscosServer *server, gint id, unsigned furniture)
{
  RiscosWindow *w = window_by_id (server, id);

  if (w == NULL || w->is_temp)
    return;
  w->furniture_wanted = furniture;
  apply_furniture (server, w);
}

void
_gdk_riscos_server_window_set_as_menu (GdkRiscosServer *server, gint id, gboolean as_menu)
{
  RiscosWindow *w = window_by_id (server, id);

  if (w)
    w->as_menu = as_menu;
}

void
_gdk_riscos_server_destroy_window (GdkRiscosServer *server, gint id)
{
  RiscosWindow *w = window_by_id (server, id);
  int block[1];
  _kernel_swi_regs r;

  if (w == NULL)
    return;
  block[0] = w->handle;
  r.r[1] = (int) block;
  swi (Wimp_DeleteWindow, &r);
  if (w->surface)
    cairo_surface_destroy (w->surface);
  if (w->strip_surface)
    cairo_surface_destroy (w->strip_surface);
  if (server->pointer_window == (guint32) id)
    server->pointer_window = 0;
  if (server->grab_window == (guint32) id)
    server->grab_window = 0;
  if (server->focus_window == (guint32) id)
    server->focus_window = 0;
  if (w->menu_open && server->dbox_menu == w->handle)
    {
      _kernel_swi_regs m;
      m.r[1] = -1;
      swi (Wimp_CreateMenu, &m);
      server->dbox_menu = 0;
    }
  g_hash_table_remove (server->handles, GINT_TO_POINTER (w->handle));
  g_hash_table_remove (server->windows, GINT_TO_POINTER (id));
}

gboolean
_gdk_riscos_server_window_show (GdkRiscosServer *server, gint id)
{
  RiscosWindow *w = window_by_id (server, id);

  if (w == NULL)
    return FALSE;
  /* a window with a title bar but no title (GTK's message dialogues):
     the program's name, as the Toolbox's Discard/Cancel/Save box has */
  if (!w->is_temp && w->title[0] == 0)
    {
      const char *name = g_get_application_name ();
      if (name == NULL)
        name = g_get_prgname ();
      if (name)
        utf8_to_latin1 (name, w->title, TITLE_MAX);
    }
  apply_furniture (server, w);
  if (open_window (server, w, -1))
    send_configure (server, w);
  if (!w->is_temp)
    take_caret (w);
  return TRUE;
}

gboolean
_gdk_riscos_server_window_hide (GdkRiscosServer *server, gint id)
{
  RiscosWindow *w = window_by_id (server, id);
  int block[1];
  _kernel_swi_regs r;

  if (w == NULL || !w->visible)
    return FALSE;
  if (w->menu_open && server->dbox_menu == w->handle)
    {
      r.r[1] = -1;                 /* close the menu tree it's in */
      swi (Wimp_CreateMenu, &r);
      server->dbox_menu = 0;
    }
  w->menu_open = FALSE;
  block[0] = w->handle;
  r.r[1] = (int) block;
  swi (Wimp_CloseWindow, &r);
  w->visible = FALSE;
  return TRUE;
}

void
_gdk_riscos_server_window_focus (GdkRiscosServer *server, gint id)
{
  RiscosWindow *w = window_by_id (server, id);

  if (w && w->visible)
    take_caret (w);
}

void
_gdk_riscos_server_window_set_transient_for (GdkRiscosServer *server,
                                             gint id, gint parent)
{
}

void
_gdk_riscos_server_set_show_keyboard (GdkRiscosServer *server,
                                      gboolean show_keyboard)
{
}

gboolean
_gdk_riscos_server_window_translate (GdkRiscosServer *server, gint id,
                                     cairo_region_t *area, gint dx, gint dy)
{
  return FALSE;                    /* GTK repaints instead */
}

/* The backing sprite is made in the screen's colour order, because
   OS_SpriteOp doesn't convert TRGB to TBGR when it plots a 32bpp sprite on
   a 32bpp screen (found on a Pi 4, RISC OS 5, TBGR desktop: red and blue
   came out swapped). On a TRGB screen cairo (0x00RRGGBB) draws straight
   into the sprite. On a TBGR screen (0x00BBGGRR, the usual RISC OS
   desktop) cairo draws into its own buffer and sync_sprite copies it into
   the sprite with red and blue swapped before each plot. */
typedef struct
{
  int *area;          /* sprite area holding the "gtk" sprite */
  guint32 *pixels;    /* the sprite's pixels */
  gboolean swap;      /* TBGR sprite: copy from cairo's buffer, swapping R/B */
  gboolean stale;     /* TBGR sprite: parts not copied yet (new, or updates
                         left while the window was closed) */
} RiscosSprite;

static void
riscos_sprite_free (void *data)
{
  RiscosSprite *s = data;
  g_free (s->area);
  g_free (s);
}

static void
sync_sprite (cairo_surface_t *surface)
{
  RiscosSprite *s = cairo_surface_get_user_data (surface, &_gdk_riscos_sprite_data_key);
  const guint32 *src;
  guint32 *dst;
  int n;

  if (s == NULL || !s->swap)
    return;
  s->stale = FALSE;
  src = (const guint32 *) cairo_image_surface_get_data (surface);
  dst = s->pixels;
  n = cairo_image_surface_get_width (surface) * cairo_image_surface_get_height (surface);
  while (n--)
    {
      guint32 p = *src++;
      *dst++ = (p & 0xFF00FF00u) | ((p >> 16) & 0xFF) | ((p & 0xFF) << 16);
    }
}

/* Copy only what GTK repainted (GDK pixels; NULL: all of it). A stale
   sprite is copied whole. */
static void
sync_sprite_region (cairo_surface_t *surface, const cairo_region_t *damage)
{
  RiscosSprite *s = cairo_surface_get_user_data (surface, &_gdk_riscos_sprite_data_key);
  const guint8 *data;
  double sx = 1, sy = 1;
  int sw, sh, stride, i, n;

  if (s == NULL || !s->swap)
    return;
  if (damage == NULL || s->stale)
    {
      sync_sprite (surface);
      return;
    }
  data = cairo_image_surface_get_data (surface);
  stride = cairo_image_surface_get_stride (surface);
  sw = cairo_image_surface_get_width (surface);
  sh = cairo_image_surface_get_height (surface);
  cairo_surface_get_device_scale (surface, &sx, &sy);
  n = cairo_region_num_rectangles (damage);
  for (i = 0; i < n; i++)
    {
      cairo_rectangle_int_t rc;
      int x0, y0, x1, y1, y;

      cairo_region_get_rectangle (damage, i, &rc);
      x0 = MAX (0, (int) (rc.x * sx));
      y0 = MAX (0, (int) (rc.y * sy));
      x1 = MIN (sw, (int) ((rc.x + rc.width) * sx + 0.999));
      y1 = MIN (sh, (int) ((rc.y + rc.height) * sy + 0.999));
      for (y = y0; y < y1; y++)
        {
          const guint32 *src = (const guint32 *) (data + y * stride) + x0;
          guint32 *dst = s->pixels + y * sw + x0;
          int k = x1 - x0;
          while (k-- > 0)
            {
              guint32 p = *src++;
              *dst++ = (p & 0xFF00FF00u) | ((p >> 16) & 0xFF) | ((p & 0xFF) << 16);
            }
        }
    }
}

/* Before the Wimp's redraw plots it: copy anything not copied yet */
static void
sync_sprite_if_stale (cairo_surface_t *surface)
{
  RiscosSprite *s = surface ? cairo_surface_get_user_data (surface, &_gdk_riscos_sprite_data_key) : NULL;

  if (s && s->stale)
    {
      cairo_surface_flush (surface);
      sync_sprite (surface);
    }
}

static void
mark_sprite_stale (cairo_surface_t *surface)
{
  RiscosSprite *s = cairo_surface_get_user_data (surface, &_gdk_riscos_sprite_data_key);

  if (s && s->swap)
    s->stale = TRUE;
}

static int
trgb_selector[] = {
  1, 0, 0, 5, -1,                  /* flags, x, y, log2bpp, frame rate */
  0, MODEFLAG_TRGB,                /* ModeFlags: TRGB (0x00RRGGBB) */
  3, -1,                           /* NColour: 16M */
  4, 1, 5, 1,                      /* X/YEigFactor (filled in) */
  -1
};

static cairo_surface_t *
create_surface_dev (int width, int height)
{
  GdkRiscosServer *server = the_server;
  int bytes = 16 + 44 + width * height * 4;
  int *area;
  char *sprite;
  _kernel_swi_regs r;
  cairo_surface_t *surface;
  RiscosSprite *rs;
  gboolean swap = server && !(server->mode_flags & MODEFLAG_TRGB);

  width = MAX (width, 1);
  height = MAX (height, 1);
  bytes = 16 + 44 + width * height * 4;
  area = g_try_malloc (bytes);
  if (area == NULL)
    return cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
  area[0] = bytes;
  area[1] = 0;
  area[2] = 16;
  area[3] = 16;
  r.r[0] = 256 + 9;                /* initialise sprite area */
  r.r[1] = (int) area;
  swi (OS_SpriteOp, &r);

  trgb_selector[6] = swap ? 0 : MODEFLAG_TRGB;
  trgb_selector[1] = width;
  trgb_selector[2] = height;
  trgb_selector[10] = server ? server->dev_xeig : 1;
  trgb_selector[12] = server ? server->dev_yeig : 1;
  r.r[0] = 256 + 15;               /* create sprite */
  r.r[1] = (int) area;
  r.r[2] = (int) "gtk";
  r.r[3] = 0;
  r.r[4] = width;
  r.r[5] = height;
  r.r[6] = (int) trgb_selector;
  if (swi (OS_SpriteOp, &r) != NULL)
    {
      g_warning ("riscos-gtk: can't make a 32bpp sprite (RISC OS 5.21 or later needed)");
      g_free (area);
      return cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
    }
  sprite = (char *) area + area[2];
  rs = g_new0 (RiscosSprite, 1);
  rs->area = area;
  rs->pixels = (guint32 *) (sprite + ((int *) sprite)[8]);
  rs->swap = swap;
  rs->stale = swap;
  if (swap)
    surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
  else
    surface = cairo_image_surface_create_for_data ((unsigned char *) rs->pixels,
                                                   CAIRO_FORMAT_RGB24, width, height, width * 4);
  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    {
      riscos_sprite_free (rs);
      return surface;
    }
  cairo_surface_set_user_data (surface, &_gdk_riscos_sprite_data_key, rs, riscos_sprite_free);
  cairo_surface_set_user_data (surface, &_gdk_riscos_sprite_key, area, NULL);
  if (server && !server->sprite_logged)
    {
      server->sprite_logged = TRUE;
      g_printerr ("riscos-gtk: backing sprites %s, mode word &%08X\n",
                  swap ? "TBGR (copied from cairo, R/B swapped)" : "TRGB (cairo draws in place)",
                  ((int *) sprite)[10]);
    }
  return surface;
}

/* A backing surface for a width x height (GDK pixels) window: a sprite at
   the screen's resolution, width x scale by height x scale pixels, with
   cairo's device scale set so that GTK draws in GDK pixels */
cairo_surface_t *
_gdk_riscos_server_create_surface (int width, int height)
{
  int scale = the_server ? the_server->scale : 1;
  cairo_surface_t *surface = create_surface_dev (MAX (width, 1) * scale, MAX (height, 1) * scale);

  if (scale != 1)
    cairo_surface_set_device_scale (surface, scale, scale);
  return surface;
}

/* GDK's scale factor for windows (gdkwindow-riscos.c, gdkscreen-riscos.c) */
int
_gdk_riscos_server_get_scale (void)
{
  return the_server && the_server->scale ? the_server->scale : _gdk_riscos_scale_factor ();
}

/* Show what GTK drew. damage (window coordinates, GDK pixels) is what was
   repainted since the last update: only that is copied into the sprite
   and plotted. NULL: all of it (first map, resize). */
void
_gdk_riscos_server_window_update (GdkRiscosServer *server, gint id,
                                  cairo_surface_t *surface,
                                  const cairo_region_t *damage)
{
  RiscosWindow *w = window_by_id (server, id);
  cairo_region_t *area = NULL;
  int block[11];
  _kernel_swi_regs r;
  int width, height, i, n;

  if (w == NULL || surface == NULL)
    return;
  if (w->surface != surface)
    {
      cairo_surface_reference (surface);
      if (w->surface)
        cairo_surface_destroy (w->surface);
      w->surface = surface;
      damage = NULL;
    }
  if (!w->visible)
    {
      /* the next update or redraw copies all of it */
      mark_sprite_stale (surface);
      return;
    }
  cairo_surface_flush (surface);
  width = surface_w (surface);
  height = surface_h (surface);
  if (damage)
    {
      cairo_rectangle_int_t all = { 0, 0, width, height };
      area = cairo_region_copy (damage);
      cairo_region_intersect_rectangle (area, &all);
    }
  if (has_grip (server, w) && w->strip == 0)
    {
      cairo_rectangle_int_t g;
      /* painted again where GTK painted over it */
      if (paint_grip (server, w, surface, &g) && area
          && cairo_region_contains_rectangle (area, &g) != CAIRO_REGION_OVERLAP_OUT)
        cairo_region_union_rectangle (area, &g);
    }
  if (area && cairo_region_is_empty (area))
    {
      cairo_region_destroy (area);
      return;
    }
  sync_sprite_region (surface, area);
  /* one Wimp update per area, or one round them all when there are many */
  n = area ? cairo_region_num_rectangles (area) : 1;
  if (n > 8)
    n = 1;
  for (i = 0; i < n; i++)
    {
      cairo_rectangle_int_t rc = { 0, 0, width, height };

      if (area && cairo_region_num_rectangles (area) > 8)
        cairo_region_get_extents (area, &rc);
      else if (area)
        cairo_region_get_rectangle (area, i, &rc);
      block[0] = w->handle;
      block[1] = rc.x << server->xeig;
      block[2] = -((rc.y + rc.height) << server->yeig);
      block[3] = (rc.x + rc.width) << server->xeig;
      block[4] = -(rc.y << server->yeig);
      r.r[1] = (int) block;
      if (swi (Wimp_UpdateWindow, &r) != NULL)
        break;
      plot_rectangles (server, w, block, r.r[0]);
    }
  if (area)
    cairo_region_destroy (area);
}

gboolean
_gdk_riscos_server_window_move_resize (GdkRiscosServer *server, gint id,
                                       gboolean with_move, int x, int y,
                                       int width, int height)
{
  RiscosWindow *w = window_by_id (server, id);

  if (w == NULL)
    return FALSE;
  if (with_move)
    {
      w->x = x;
      w->y = y;
    }
  w->width = width;
  w->height = height;
  if (w->visible)
    {
      /* Keep its place in the window stack */
      int state[9];
      _kernel_swi_regs r;
      state[0] = w->handle;
      r.r[1] = (int) state;
      open_window (server, w, swi (Wimp_GetWindowState, &r) == NULL ? state[7] : -1);
    }
  /* Confirm it, as Broadway's server does: GTK freezes a mapped toplevel's
     updates after asking for a new size until a configure event arrives,
     so without this a window that changes size after it opens never draws
     again (found with the theme test). */
  send_configure (server, w);
  return TRUE;
}

/* The strip (see RiscosWindow): GTK's image of an empty scroll bar, or
   NULL for none. The window's visible area grows or shrinks by its width;
   GTK's window keeps its size. */
void
_gdk_riscos_server_window_set_strip (GdkRiscosServer *server, gint id, cairo_surface_t *image)
{
  RiscosWindow *w = window_by_id (server, id);
  int width = image ? surface_w (image) : 0, old;
  _kernel_swi_regs r;

  if (w == NULL || w->is_temp)
    return;
  if (w->strip_surface)
    {
      cairo_surface_destroy (w->strip_surface);
      w->strip_surface = NULL;
    }
  old = w->strip;
  w->strip = width;
  if (width > 0)
    {
      cairo_surface_t *surface = _gdk_riscos_server_create_surface (width, MAX (w->height, 1));
      cairo_t *cr;

      if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
        {
          cairo_surface_destroy (surface);
          w->strip = 0;
          width = 0;
        }
      else
        {
          cr = cairo_create (surface);
          cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);
          cairo_paint (cr);
          cairo_set_source_surface (cr, image, 0, 0);
          cairo_paint (cr);
          cairo_destroy (cr);
          cairo_surface_flush (surface);
          if (has_grip (server, w))
            paint_grip_at (server, w, surface, 0, NULL);
          sync_sprite (surface);
          w->strip_surface = surface;
        }
    }
  if (!w->visible)
    return;
  if (old != width)
    {
      /* the visible area changes: open it again where it is in the stack */
      int state[9];
      state[0] = w->handle;
      r.r[1] = (int) state;
      if (open_window (server, w, swi (Wimp_GetWindowState, &r) == NULL ? state[7] : -1))
        send_configure (server, w);
    }
  if (width > 0)
    {
      /* plot it now (Wimp_UpdateWindow over the strip) */
      int block[11];
      block[0] = w->handle;
      block[1] = w->width << server->xeig;
      block[2] = -(w->height << server->yeig);
      block[3] = (w->width + width) << server->xeig;
      block[4] = 0;
      r.r[1] = (int) block;
      if (swi (Wimp_UpdateWindow, &r) == NULL)
        plot_rectangles (server, w, block, r.r[0]);
    }
}

void
_gdk_riscos_server_window_set_modal_hint (GdkRiscosServer *server, gint id,
                                          gboolean modal_hint)
{
}

/* UTF-8 -> the Wimp's alphabet (Latin-1), without iconv: characters
   beyond Latin-1 become '?'. */
static void
utf8_to_latin1 (const char *utf8, char *out, int size)
{
  int n = 0;
  const char *p = utf8 ? utf8 : "";

  if (!g_utf8_validate (p, -1, NULL))
    {
      g_strlcpy (out, p, size);
      return;
    }
  for (; *p && n < size - 1; p = g_utf8_next_char (p))
    {
      gunichar c = g_utf8_get_char (p);
      out[n++] = c < 256 ? (char) c : '?';
    }
  out[n] = '\0';
}

void
_gdk_riscos_server_window_set_title (GdkRiscosServer *server, gint id,
                                     const char *title)
{
  RiscosWindow *w = window_by_id (server, id);
  _kernel_swi_regs r;

  if (w == NULL)
    return;
  utf8_to_latin1 (title, w->title, TITLE_MAX);
  if (w->visible)
    {
      r.r[0] = w->handle;
      r.r[1] = 0x4B534154;         /* "TASK": R2 says which part */
      r.r[2] = 3;                  /* the title bar */
      swi (Wimp_ForceRedraw, &r);
    }
}

/* ---- Wimp menus (public: gdkriscosmenu.h, used by gtkmenu-riscos.c) ---- */

void
gdk_riscos_display_open_menu (GdkDisplay *display, gpointer menu_block,
                              gint root_x, gint root_y,
                              GdkRiscosMenuFunc func, gpointer data)
{
  GdkRiscosServer *server = the_server;
  _kernel_swi_regs r;

  if (server == NULL)
    return;
  /* another menu was open (a program's, the icon bar's): it closes */
  if (server->menu_block && server->menu_func &&
      (server->menu_func != func || server->menu_data != data))
    {
      GdkRiscosMenuFunc old = server->menu_func;
      gpointer old_data = server->menu_data;
      server->menu_block = NULL;
      server->menu_func = NULL;
      old (NULL, FALSE, old_data);
    }
  server->menu_block = menu_block;
  server->menu_func = func;
  server->menu_data = data;
  server->menu_x = root_x << server->xeig;
  server->menu_y = (server->screen_h - root_y) << server->yeig;
  r.r[1] = (int) menu_block;
  r.r[2] = server->menu_x;
  r.r[3] = server->menu_y;
  swi (Wimp_CreateMenu, &r);
}

void
gdk_riscos_display_reopen_menu (GdkDisplay *display)
{
  GdkRiscosServer *server = the_server;
  _kernel_swi_regs r;

  if (server == NULL || server->menu_block == NULL)
    return;
  r.r[1] = (int) server->menu_block;
  r.r[2] = server->menu_x;
  r.r[3] = server->menu_y;
  swi (Wimp_CreateMenu, &r);
}

void
gdk_riscos_display_close_menu (GdkDisplay *display)
{
  GdkRiscosServer *server = the_server;
  _kernel_swi_regs r;

  if (server == NULL)
    return;
  if (server->menu_block)
    {
      r.r[1] = -1;
      swi (Wimp_CreateMenu, &r);
    }
  server->menu_block = NULL;
  server->menu_func = NULL;
  server->menu_data = NULL;
}

void
gdk_riscos_display_menu_done (GdkDisplay *display)
{
  if (the_server)
    the_server->menu_hold = 0;
}

gint
gdk_riscos_text_width (const char *latin1)
{
  _kernel_swi_regs r;

  r.r[0] = 1;                      /* Wimp_TextOp 1: width in the desktop font */
  r.r[1] = (int) latin1;
  r.r[2] = 0;
  if (swi (Wimp_TextOp, &r) != NULL)
    return 16 * (gint) strlen (latin1);
  return r.r[0];
}

void
gdk_riscos_utf8_to_latin1 (const char *utf8, char *out, gint size)
{
  utf8_to_latin1 (utf8, out, size);
}

/* ---- for gdktransfer-riscos.c ------------------------------------------- */

int
_gdk_riscos_server_get_task (GdkRiscosServer *server)
{
  return server->task;
}

guint32
_gdk_riscos_server_id_for_handle (GdkRiscosServer *server, int handle)
{
  RiscosWindow *w = window_by_handle (server, handle);
  return w ? w->id : 0;
}

/* The Wimp handle of the window with the caret, else any of ours */
int
_gdk_riscos_server_focus_handle (GdkRiscosServer *server)
{
  RiscosWindow *w = window_by_id (server, server->focus_window);
  GHashTableIter iter;
  gpointer value;

  if (w)
    return w->handle;
  g_hash_table_iter_init (&iter, server->windows);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    if (((RiscosWindow *) value)->visible)
      return ((RiscosWindow *) value)->handle;
  return 0;
}

void
_gdk_riscos_server_os_to_px (GdkRiscosServer *server, int *x, int *y)
{
  *x = os_to_px_x (server, *x);
  *y = os_to_px_y (server, *y);
}

void
_gdk_riscos_server_screen_os_size (GdkRiscosServer *server, int *w, int *h)
{
  *w = server->screen_w << server->xeig;
  *h = server->screen_h << server->yeig;
}

/* GDK's cursor for window id (gdkwindow-riscos.c): shown while the
   pointer is over it */
void
_gdk_riscos_server_window_set_pointer (GdkRiscosServer *server, gint id, int shape)
{
  RiscosWindow *w = window_by_id (server, id);

  if (w == NULL)
    return;
  w->pointer_shape = shape;
  if (server->pointer_window == (guint32) id && shape != server->shown_shape)
    {
      _gdk_riscos_pointer_show (shape);
      server->shown_shape = shape;
    }
}
