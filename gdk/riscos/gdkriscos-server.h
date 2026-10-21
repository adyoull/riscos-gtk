/* riscos-gtk: from GTK 3.24.41 gdk/broadway (client side), renamed
   broadway -> riscos; see riscos-gtk LICENCES.md. LGPL-2.1-or-later. */
#ifndef __GDK_RISCOS_SERVER__
#define __GDK_RISCOS_SERVER__

#include <gdk/gdktypes.h>
#include "riscos-protocol.h"

typedef struct _GdkRiscosServer GdkRiscosServer;
typedef struct _GdkRiscosServerClass GdkRiscosServerClass;

#define GDK_TYPE_RISCOS_SERVER              (gdk_riscos_server_get_type())
#define GDK_RISCOS_SERVER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_RISCOS_SERVER, GdkRiscosServer))
#define GDK_RISCOS_SERVER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_RISCOS_SERVER, GdkRiscosServerClass))
#define GDK_IS_RISCOS_SERVER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_RISCOS_SERVER))
#define GDK_IS_RISCOS_SERVER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_RISCOS_SERVER))
#define GDK_RISCOS_SERVER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_RISCOS_SERVER, GdkRiscosServerClass))

GdkRiscosServer *_gdk_riscos_server_new                      (const char         *display,
								  GError            **error);
void               _gdk_riscos_server_flush                    (GdkRiscosServer  *server);
void               _gdk_riscos_server_sync                     (GdkRiscosServer  *server);
gulong             _gdk_riscos_server_get_next_serial          (GdkRiscosServer  *server);
guint32            _gdk_riscos_server_get_last_seen_time       (GdkRiscosServer  *server);
gboolean           _gdk_riscos_server_lookahead_event          (GdkRiscosServer  *server,
								  const char         *types);
void               _gdk_riscos_server_query_mouse              (GdkRiscosServer  *server,
								  guint32            *toplevel,
								  gint32             *root_x,
								  gint32             *root_y,
								  guint32            *mask);
GdkGrabStatus      _gdk_riscos_server_grab_pointer             (GdkRiscosServer  *server,
								  gint                id,
								  gboolean            owner_events,
								  guint32             event_mask,
								  guint32             time_);
guint32            _gdk_riscos_server_ungrab_pointer           (GdkRiscosServer  *server,
								  guint32             time_);
gint32             _gdk_riscos_server_get_mouse_toplevel       (GdkRiscosServer  *server);
guint32            _gdk_riscos_server_new_window               (GdkRiscosServer  *server,
								  int                 x,
								  int                 y,
								  int                 width,
								  int                 height,
								  gboolean            is_temp);
void               _gdk_riscos_server_destroy_window           (GdkRiscosServer  *server,
								  gint                id);
gboolean           _gdk_riscos_server_window_show              (GdkRiscosServer  *server,
								  gint                id);
gboolean           _gdk_riscos_server_window_hide              (GdkRiscosServer  *server,
								  gint                id);
void               _gdk_riscos_server_window_focus             (GdkRiscosServer  *server,
								  gint                id);
void               _gdk_riscos_server_window_set_transient_for (GdkRiscosServer  *server,
								  gint                id,
								  gint                parent);
void               _gdk_riscos_server_set_show_keyboard        (GdkRiscosServer  *server,
								  gboolean            show_keyboard);
gboolean           _gdk_riscos_server_window_translate         (GdkRiscosServer  *server,
								  gint                id,
								  cairo_region_t     *area,
								  gint                dx,
								  gint                dy);
int                _gdk_riscos_server_get_scale                (void);
cairo_surface_t   *_gdk_riscos_server_create_surface           (int                 width,
								  int                 height);
void               _gdk_riscos_server_window_update            (GdkRiscosServer  *server,
								  gint                id,
								  cairo_surface_t    *surface,
								  const cairo_region_t *damage);
gboolean           _gdk_riscos_server_window_move_resize       (GdkRiscosServer  *server,
								  gint                id,
								  gboolean            with_move,
								  int                 x,
								  int                 y,
								  int                 width,
								  int                 height);
void               _gdk_riscos_server_window_set_modal_hint    (GdkRiscosServer  *server,
								  gint                id,
								  gboolean            modal_hint);

void               _gdk_riscos_server_window_set_title         (GdkRiscosServer    *server,
								  gint                id,
								  const char         *title);
void               _gdk_riscos_server_get_screen_size          (GdkRiscosServer    *server,
								  int                *width,
								  int                *height);
void               _gdk_riscos_server_window_set_pointer       (GdkRiscosServer    *server,
                                                                gint                id,
                                                                int                 shape);
/* riscos-gtk: window furniture (Wimp window flags) */
#define RISCOS_FURNITURE_TITLE  1u
#define RISCOS_FURNITURE_CLOSE  2u
#define RISCOS_FURNITURE_BACK   4u
#define RISCOS_FURNITURE_RESIZE 8u     /* toggle size and adjust size */
#define RISCOS_FURNITURE_ALL    15u
int                _gdk_riscos_server_window_title_height      (GdkRiscosServer    *server,
                                                                gint                id);
void               _gdk_riscos_server_window_set_furniture     (GdkRiscosServer    *server,
                                                                gint                id,
                                                                unsigned            furniture);
gboolean           _gdk_riscos_server_window_get_grip          (GdkRiscosServer    *server,
                                                                 gint                id,
                                                                 int                *width,
                                                                 int                *height);
void               _gdk_riscos_server_window_set_grip_column   (GdkRiscosServer    *server,
                                                                 gint                id,
                                                                 int                 x,
                                                                 int                 width,
                                                                 int                 y);
void               _gdk_riscos_server_window_set_strip         (GdkRiscosServer    *server,
                                                                 gint                id,
                                                                 cairo_surface_t    *image);
void               _gdk_riscos_server_window_set_as_menu       (GdkRiscosServer    *server,
                                                                gint                id,
                                                                gboolean            as_menu);
/* riscos-gtk: for gdktransfer-riscos.c */
int                _gdk_riscos_server_get_task                 (GdkRiscosServer    *server);
guint32            _gdk_riscos_server_id_for_handle            (GdkRiscosServer    *server,
                                                                int                 handle);
int                _gdk_riscos_server_focus_handle             (GdkRiscosServer    *server);
void               _gdk_riscos_server_os_to_px                 (GdkRiscosServer    *server,
                                                                int                *x,
                                                                int                *y);
void               _gdk_riscos_server_screen_os_size           (GdkRiscosServer    *server,
                                                                int                *w,
                                                                int                *h);

/* The sprite area behind a surface from _gdk_riscos_server_create_surface */
extern cairo_user_data_key_t _gdk_riscos_sprite_key;

/* gdkiconbar-riscos.c: Mouse_Click on the icon bar, and messages; TRUE if
   they were the icon bar's */
gboolean           _gdk_riscos_iconbar_click                   (int                *block);
gboolean           _gdk_riscos_iconbar_message                 (int                 reason,
                                                                 int                *block);
void               _gdk_riscos_iconbar_startup                 (int                 task,
                                                                 const char         *task_name);

/* gdkprint-riscos.c: the printing messages (and our PrintSave bouncing) */
/* gdkspawn-riscos.c: g_spawn_async*() as RISC OS tasks */
void               _gdk_riscos_spawn_startup                   (int                 task);
gboolean           _gdk_riscos_spawn_message                   (int                 reason,
                                                                int                *block);
gboolean           _gdk_riscos_print_message                   (int                 reason,
                                                                 int                *block);

#endif /* __GDK_RISCOS_SERVER__ */
