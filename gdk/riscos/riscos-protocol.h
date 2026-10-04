/* riscos-gtk: from GTK 3.24.41 gdk/broadway (client side), renamed
   broadway -> riscos; see riscos-gtk LICENCES.md. LGPL-2.1-or-later. */
#ifndef __RISCOS_PROTOCOL_H__
#define __RISCOS_PROTOCOL_H__

#include <glib.h>

typedef struct  {
    gint32 x, y;
    gint32 width, height;
} RiscosRect;

typedef enum {
  RISCOS_EVENT_ENTER = 'e',
  RISCOS_EVENT_LEAVE = 'l',
  RISCOS_EVENT_POINTER_MOVE = 'm',
  RISCOS_EVENT_BUTTON_PRESS = 'b',
  RISCOS_EVENT_BUTTON_RELEASE = 'B',
  RISCOS_EVENT_TOUCH = 't',
  RISCOS_EVENT_SCROLL = 's',
  RISCOS_EVENT_KEY_PRESS = 'k',
  RISCOS_EVENT_KEY_RELEASE = 'K',
  RISCOS_EVENT_GRAB_NOTIFY = 'g',
  RISCOS_EVENT_UNGRAB_NOTIFY = 'u',
  RISCOS_EVENT_CONFIGURE_NOTIFY = 'w',
  RISCOS_EVENT_DELETE_NOTIFY = 'W',
  RISCOS_EVENT_SCREEN_SIZE_CHANGED = 'd',
  RISCOS_EVENT_FOCUS = 'f'
} RiscosEventType;

typedef enum {
  RISCOS_OP_GRAB_POINTER = 'g',
  RISCOS_OP_UNGRAB_POINTER = 'u',
  RISCOS_OP_NEW_SURFACE = 's',
  RISCOS_OP_SHOW_SURFACE = 'S',
  RISCOS_OP_HIDE_SURFACE = 'H',
  RISCOS_OP_RAISE_SURFACE = 'r',
  RISCOS_OP_LOWER_SURFACE = 'R',
  RISCOS_OP_DESTROY_SURFACE = 'd',
  RISCOS_OP_MOVE_RESIZE = 'm',
  RISCOS_OP_SET_TRANSIENT_FOR = 'p',
  RISCOS_OP_PUT_RGB = 'i',
  RISCOS_OP_REQUEST_AUTH = 'l',
  RISCOS_OP_AUTH_OK = 'L',
  RISCOS_OP_DISCONNECTED = 'D',
  RISCOS_OP_PUT_BUFFER = 'b',
  RISCOS_OP_SET_SHOW_KEYBOARD = 'k',
} RiscosOpType;

typedef struct {
  guint32 type;
  guint32 serial;
  guint64 time;
} RiscosInputBaseMsg;

typedef struct {
  RiscosInputBaseMsg base;
  guint32 mouse_window_id; /* The real window, not taking grabs into account */
  guint32 event_window_id;
  gint32 root_x;
  gint32 root_y;
  gint32 win_x;
  gint32 win_y;
  guint32 state;
} RiscosInputPointerMsg;

typedef struct {
  RiscosInputPointerMsg pointer;
  guint32 mode;
} RiscosInputCrossingMsg;

typedef struct {
  RiscosInputPointerMsg pointer;
  guint32 button;
} RiscosInputButtonMsg;

typedef struct {
  RiscosInputPointerMsg pointer;
  gint32 dir;
} RiscosInputScrollMsg;

typedef struct {
  RiscosInputBaseMsg base;
  guint32 touch_type;
  guint32 event_window_id;
  guint32 sequence_id;
  guint32 is_emulated;
  gint32 root_x;
  gint32 root_y;
  gint32 win_x;
  gint32 win_y;
  guint32 state;
} RiscosInputTouchMsg;

typedef struct {
  RiscosInputBaseMsg base;
  guint32 window_id;
  guint32 state;
  gint32 key;
} RiscosInputKeyMsg;

typedef struct {
  RiscosInputBaseMsg base;
  gint32 res;
} RiscosInputGrabReply;

typedef struct {
  RiscosInputBaseMsg base;
  gint32 id;
  gint32 x;
  gint32 y;
  gint32 width;
  gint32 height;
} RiscosInputConfigureNotify;

typedef struct {
  RiscosInputBaseMsg base;
  guint32 width;
  guint32 height;
} RiscosInputScreenResizeNotify;

typedef struct {
  RiscosInputBaseMsg base;
  gint32 id;
} RiscosInputDeleteNotify;

typedef struct {
  RiscosInputBaseMsg base;
  gint32 new_id;
  gint32 old_id;
} RiscosInputFocusMsg;

typedef union {
  RiscosInputBaseMsg base;
  RiscosInputPointerMsg pointer;
  RiscosInputCrossingMsg crossing;
  RiscosInputButtonMsg button;
  RiscosInputScrollMsg scroll;
  RiscosInputTouchMsg touch;
  RiscosInputKeyMsg key;
  RiscosInputGrabReply grab_reply;
  RiscosInputConfigureNotify configure_notify;
  RiscosInputDeleteNotify delete_notify;
  RiscosInputScreenResizeNotify screen_resize_notify;
  RiscosInputFocusMsg focus;
} RiscosInputMsg;

typedef enum {
  RISCOS_REQUEST_NEW_WINDOW,
  RISCOS_REQUEST_FLUSH,
  RISCOS_REQUEST_SYNC,
  RISCOS_REQUEST_QUERY_MOUSE,
  RISCOS_REQUEST_DESTROY_WINDOW,
  RISCOS_REQUEST_SHOW_WINDOW,
  RISCOS_REQUEST_HIDE_WINDOW,
  RISCOS_REQUEST_SET_TRANSIENT_FOR,
  RISCOS_REQUEST_UPDATE,
  RISCOS_REQUEST_MOVE_RESIZE,
  RISCOS_REQUEST_GRAB_POINTER,
  RISCOS_REQUEST_UNGRAB_POINTER,
  RISCOS_REQUEST_FOCUS_WINDOW,
  RISCOS_REQUEST_SET_SHOW_KEYBOARD,
  RISCOS_REQUEST_SET_MODAL_HINT
} RiscosRequestType;

typedef struct {
  guint32 size;
  guint32 serial;
  guint32 type;
} RiscosRequestBase, RiscosRequestFlush, RiscosRequestSync, RiscosRequestQueryMouse;

typedef struct {
  RiscosRequestBase base;
  guint32 id;
} RiscosRequestDestroyWindow, RiscosRequestShowWindow, RiscosRequestHideWindow, RiscosRequestFocusWindow;

typedef struct {
  RiscosRequestBase base;
  guint32 id;
  guint32 parent;
} RiscosRequestSetTransientFor;

typedef struct {
  RiscosRequestBase base;
  guint32 id;
  gint32 dx;
  gint32 dy;
  guint32 n_rects;
  RiscosRect rects[1];
} RiscosRequestTranslate;

typedef struct {
  RiscosRequestBase base;
  guint32 id;
  char name[36];
  guint32 width;
  guint32 height;
} RiscosRequestUpdate;

typedef struct {
  RiscosRequestBase base;
  guint32 id;
  guint32 owner_events;
  guint32 event_mask;
  guint32 time_;
} RiscosRequestGrabPointer;

typedef struct {
  RiscosRequestBase base;
  guint32 time_;
} RiscosRequestUngrabPointer;

typedef struct {
  RiscosRequestBase base;
  gint32 x;
  gint32 y;
  guint32 width;
  guint32 height;
  guint32 is_temp;
} RiscosRequestNewWindow;

typedef struct {
  RiscosRequestBase base;
  guint32 id;
  guint32 with_move;
  gint32 x;
  gint32 y;
  guint32 width;
  guint32 height;
} RiscosRequestMoveResize;

typedef struct {
  RiscosRequestBase base;
  guint32 show_keyboard;
} RiscosRequestSetShowKeyboard;

typedef struct {
  RiscosRequestBase base;
  guint32 id;
  gboolean modal_hint;
} RiscosRequestSetModalHint;

typedef union {
  RiscosRequestBase base;
  RiscosRequestNewWindow new_window;
  RiscosRequestFlush flush;
  RiscosRequestSync sync;
  RiscosRequestQueryMouse query_mouse;
  RiscosRequestDestroyWindow destroy_window;
  RiscosRequestShowWindow show_window;
  RiscosRequestHideWindow hide_window;
  RiscosRequestSetTransientFor set_transient_for;
  RiscosRequestUpdate update;
  RiscosRequestMoveResize move_resize;
  RiscosRequestGrabPointer grab_pointer;
  RiscosRequestUngrabPointer ungrab_pointer;
  RiscosRequestTranslate translate;
  RiscosRequestFocusWindow focus_window;
  RiscosRequestSetShowKeyboard set_show_keyboard;
  RiscosRequestSetModalHint set_modal_hint;
} RiscosRequest;

typedef enum {
  RISCOS_REPLY_EVENT,
  RISCOS_REPLY_SYNC,
  RISCOS_REPLY_QUERY_MOUSE,
  RISCOS_REPLY_NEW_WINDOW,
  RISCOS_REPLY_GRAB_POINTER,
  RISCOS_REPLY_UNGRAB_POINTER
} RiscosReplyType;

typedef struct {
  guint32 size;
  guint32 in_reply_to;
  guint32 type;
} RiscosReplyBase, RiscosReplySync;

typedef struct {
  RiscosReplyBase base;
  guint32 id;
} RiscosReplyNewWindow;

typedef struct {
  RiscosReplyBase base;
  guint32 status;
} RiscosReplyGrabPointer, RiscosReplyUngrabPointer;

typedef struct {
  RiscosReplyBase base;
  guint32 toplevel;
  gint32 root_x;
  gint32 root_y;
  guint32 mask;
} RiscosReplyQueryMouse;

typedef struct {
  RiscosReplyBase base;
  RiscosInputMsg msg;
} RiscosReplyEvent;

typedef union {
  RiscosReplyBase base;
  RiscosReplyEvent event;
  RiscosReplyQueryMouse query_mouse;
  RiscosReplyNewWindow new_window;
  RiscosReplyGrabPointer grab_pointer;
  RiscosReplyUngrabPointer ungrab_pointer;
} RiscosReply;

#endif /* __RISCOS_PROTOCOL_H__ */
