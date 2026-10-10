/* gdkglcontext-riscos.h - riscos-gtk: OpenGL contexts (GtkGLArea) through
 * riscos-mesa's EGL. LGPL-2.1-or-later, as GTK. Copyright (C) 2026 Andrew
 * Youll. */
#ifndef __GDK_RISCOS_GL_CONTEXT__
#define __GDK_RISCOS_GL_CONTEXT__

#include "gdkglcontextprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkwindow.h"

G_BEGIN_DECLS

#define GDK_TYPE_RISCOS_GL_CONTEXT  (gdk_riscos_gl_context_get_type ())
#define GDK_RISCOS_GL_CONTEXT(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_RISCOS_GL_CONTEXT, GdkRiscosGLContext))
#define GDK_IS_RISCOS_GL_CONTEXT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_RISCOS_GL_CONTEXT))

typedef struct _GdkRiscosGLContext      GdkRiscosGLContext;
typedef struct _GdkRiscosGLContextClass GdkRiscosGLContextClass;

GType         gdk_riscos_gl_context_get_type (void) G_GNUC_CONST;

GdkGLContext *_gdk_riscos_window_create_gl_context        (GdkWindow    *window,
                                                           gboolean      attached,
                                                           GdkGLContext *share,
                                                           GError      **error);
gboolean      _gdk_riscos_display_make_gl_context_current (GdkDisplay   *display,
                                                           GdkGLContext *context);

G_END_DECLS

#endif
