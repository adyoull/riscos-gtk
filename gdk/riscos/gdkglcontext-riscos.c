/*
 * gdkglcontext-riscos.c - riscos-gtk: OpenGL for GtkGLArea, through
 * riscos-mesa's EGL and OSMesa (software rendering: OpenGL 2.1 and
 * OpenGL ES 2.0).
 *
 * How it fits together:
 *   - Every GdkGLContext is an EGL context made current on one small
 *     pbuffer, shared by all of them. GtkGLArea draws into its own
 *     framebuffer object, so the pbuffer itself is never drawn in.
 *   - GDK doesn't paint windows with GL on RISC OS (the gdkwindow.c patch
 *     keeps current_paint.use_gl FALSE). gdk_cairo_draw_from_gl() then
 *     takes GDK's software path: it reads the GL area's pixels back with
 *     glReadPixels into a cairo image and paints that into the window's
 *     backing sprite, with everything else in the window. Only the GL
 *     area's pixels are copied, once a frame.
 *   - The window's "paint context" (made by GDK for that read-back) and
 *     each GtkGLArea's context share objects, so the area's renderbuffer
 *     can be read from the paint context.
 *   - libepoxy finds GL functions with eglGetProcAddress (patches/
 *     libepoxy-1.5.10-riscos.diff).
 *
 * The EGL functions are referenced weakly: a program that doesn't use GL
 * links no Mesa, and gets "OpenGL is not available" (GtkGLArea shows
 * it). A GL program links riscos-mesa's devkit with
 *   -Wl,-u,eglGetProcAddress -lEGL -lOSMesa -lstdc++ -lz -lm
 * (the -u pulls libEGL in, which resolves these references).
 *
 * The EGL types and constants are declared here rather than included,
 * so GTK builds without riscos-mesa's headers. Values from Khronos's
 * egl.h (riscos-mesa 20.3.5-16): display 0 is the screen.
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as GTK. Based on
 * GTK's gdk/wayland/gdkglcontext-wayland.c (Emmanuele Bassi, Alexander
 * Larsson; LGPL-2.1-or-later).
 */
#include "config.h"

#include "gdkglcontext-riscos.h"
#include "gdkdisplay-riscos.h"
#include "gdkinternals.h"
#include "gdkintl.h"

#include <glib.h>

/* ---- EGL, declared weakly ------------------------------------------- */

typedef void *EGLDisplay, *EGLConfig, *EGLContext, *EGLSurface;
typedef int EGLint;
typedef unsigned int EGLBoolean, EGLenum;

#define EGL_NONE                    0x3038
#define EGL_SURFACE_TYPE            0x3033
#define EGL_PBUFFER_BIT             0x0001
#define EGL_RED_SIZE                0x3024
#define EGL_GREEN_SIZE              0x3023
#define EGL_BLUE_SIZE               0x3022
#define EGL_ALPHA_SIZE              0x3021
#define EGL_DEPTH_SIZE              0x3025
#define EGL_STENCIL_SIZE            0x3026
#define EGL_RENDERABLE_TYPE         0x3040
#define EGL_OPENGL_BIT              0x0008
#define EGL_OPENGL_ES2_BIT          0x0004
#define EGL_WIDTH                   0x3057
#define EGL_HEIGHT                  0x3056
#define EGL_OPENGL_API              0x30A2
#define EGL_OPENGL_ES_API           0x30A0
#define EGL_CONTEXT_CLIENT_VERSION  0x3098
#define EGL_VENDOR                  0x3053
#define EGL_VERSION                 0x3054
#define EGL_EXTENSIONS              0x3055
#define EGL_CLIENT_APIS             0x308D

#define WEAK __attribute__ ((weak))
extern void      *eglGetProcAddress (const char *procname) WEAK;
extern EGLDisplay eglGetDisplay (int display_id) WEAK;
extern EGLBoolean eglInitialize (EGLDisplay dpy, EGLint *major, EGLint *minor) WEAK;
extern EGLBoolean eglBindAPI (EGLenum api) WEAK;
extern EGLBoolean eglChooseConfig (EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs,
                                   EGLint config_size, EGLint *num_config) WEAK;
extern EGLContext eglCreateContext (EGLDisplay dpy, EGLConfig config, EGLContext share_context,
                                    const EGLint *attrib_list) WEAK;
extern EGLBoolean eglDestroyContext (EGLDisplay dpy, EGLContext ctx) WEAK;
extern EGLSurface eglCreatePbufferSurface (EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list) WEAK;
extern EGLBoolean eglMakeCurrent (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) WEAK;
extern EGLContext eglGetCurrentContext (void) WEAK;
extern EGLint     eglGetError (void) WEAK;
extern const char *eglQueryString (EGLDisplay dpy, EGLint name) WEAK;

/* ---- the display's EGL state ------------------------------------------ */

static struct {
  gboolean tried, ok;
  EGLDisplay dpy;
  EGLConfig config;
  EGLSurface pbuffer;         /* 1x1: every context is current on it */
  char why[128];              /* why GL isn't available */
} egl;

static gboolean
init_egl (void)
{
  static const EGLint config_attrs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT | EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
    EGL_NONE
  };
  static const EGLint pbuffer_attrs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
  EGLint major = 0, minor = 0, n = 0;

  if (egl.tried)
    return egl.ok;
  egl.tried = TRUE;

  if (eglGetProcAddress == NULL || eglGetDisplay == NULL)
    {
      g_strlcpy (egl.why, "this program wasn't linked with riscos-mesa's EGL and OSMesa",
                 sizeof egl.why);
      return FALSE;
    }
  egl.dpy = eglGetDisplay (0);
  if (egl.dpy == NULL || !eglInitialize (egl.dpy, &major, &minor))
    {
      g_snprintf (egl.why, sizeof egl.why, "eglInitialize failed (EGL error 0x%x)", eglGetError ());
      return FALSE;
    }
  if (!eglChooseConfig (egl.dpy, config_attrs, &egl.config, 1, &n) || n < 1)
    {
      g_snprintf (egl.why, sizeof egl.why, "no EGL config for a pbuffer (EGL error 0x%x)", eglGetError ());
      return FALSE;
    }
  egl.pbuffer = eglCreatePbufferSurface (egl.dpy, egl.config, pbuffer_attrs);
  if (egl.pbuffer == NULL)
    {
      g_snprintf (egl.why, sizeof egl.why, "eglCreatePbufferSurface failed (EGL error 0x%x)", eglGetError ());
      return FALSE;
    }
  egl.ok = TRUE;
  g_printerr ("riscos-gtk: OpenGL: EGL %d.%d, %s %s (%s)\n", major, minor,
              eglQueryString (egl.dpy, EGL_VENDOR), eglQueryString (egl.dpy, EGL_VERSION),
              eglQueryString (egl.dpy, EGL_CLIENT_APIS));
  GDK_NOTE (OPENGL, g_message ("EGL extensions: %s", eglQueryString (egl.dpy, EGL_EXTENSIONS)));
  return TRUE;
}

/* ---- the context ---------------------------------------------------------- */

struct _GdkRiscosGLContext
{
  GdkGLContext parent_instance;
  EGLContext egl_context;
  gboolean is_attached;
};

struct _GdkRiscosGLContextClass
{
  GdkGLContextClass parent_class;
};

G_DEFINE_TYPE (GdkRiscosGLContext, gdk_riscos_gl_context, GDK_TYPE_GL_CONTEXT)

static gboolean
gdk_riscos_gl_context_realize (GdkGLContext *context,
                               GError      **error)
{
  GdkRiscosGLContext *self = GDK_RISCOS_GL_CONTEXT (context);
  GdkGLContext *share = gdk_gl_context_get_shared_context (context);
  EGLint attrs[4];
  int major, minor, i = 0;
  gboolean use_es;

  /* OpenGL ES with GDK_GL=gles (*Set GDK_GL gles), or if the shared
     context is ES: GL and ES contexts can't share objects. As in GTK 3's
     other backends, a GtkGLArea's own use-es doesn't change this: its
     context shares with the window's paint context. */
  use_es = (_gdk_gl_flags & GDK_GL_GLES) != 0 ||
           (share != NULL && gdk_gl_context_get_use_es (share));

  gdk_gl_context_get_required_version (context, &major, &minor);

  if (use_es)
    {
      /* OpenGL ES 2.0 (riscos-mesa has no ES 3) */
      eglBindAPI (EGL_OPENGL_ES_API);
      attrs[i++] = EGL_CONTEXT_CLIENT_VERSION;
      attrs[i++] = 2;
    }
  else
    {
      /* OpenGL 2.1 compatibility: riscos-mesa has no core profiles, so
         GTK's request for 3.2 core becomes a legacy context, as it does
         on other platforms without one */
      eglBindAPI (EGL_OPENGL_API);
    }
  attrs[i++] = EGL_NONE;

  GDK_NOTE (OPENGL, g_message ("Creating EGL context (%s; %d.%d asked for)",
                               use_es ? "OpenGL ES 2.0" : "OpenGL 2.1 compatibility",
                               major, minor));

  self->egl_context = eglCreateContext (egl.dpy, egl.config,
                                        share != NULL ? GDK_RISCOS_GL_CONTEXT (share)->egl_context : NULL,
                                        attrs);
  if (self->egl_context == NULL)
    {
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                   _("Unable to create a GL context") /* EGL error */);
      g_printerr ("riscos-gtk: OpenGL: eglCreateContext failed (EGL error 0x%x)\n", eglGetError ());
      return FALSE;
    }

  gdk_gl_context_set_is_legacy (context, !use_es);
  gdk_gl_context_set_use_es (context, use_es);
  return TRUE;
}

static void
gdk_riscos_gl_context_end_frame (GdkGLContext   *context,
                                 cairo_region_t *painted,
                                 cairo_region_t *damage)
{
  /* GDK never paints a RISC OS window with GL (see the top), so there is
     no frame to show: the window's backing sprite is plotted as usual */
}

static void
gdk_riscos_gl_context_dispose (GObject *gobject)
{
  GdkRiscosGLContext *self = GDK_RISCOS_GL_CONTEXT (gobject);

  if (self->egl_context != NULL)
    {
      if (eglGetCurrentContext () == self->egl_context)
        eglMakeCurrent (egl.dpy, NULL, NULL, NULL);
      eglDestroyContext (egl.dpy, self->egl_context);
      self->egl_context = NULL;
    }

  G_OBJECT_CLASS (gdk_riscos_gl_context_parent_class)->dispose (gobject);
}

static void
gdk_riscos_gl_context_class_init (GdkRiscosGLContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkGLContextClass *context_class = GDK_GL_CONTEXT_CLASS (klass);

  gobject_class->dispose = gdk_riscos_gl_context_dispose;
  context_class->realize = gdk_riscos_gl_context_realize;
  context_class->end_frame = gdk_riscos_gl_context_end_frame;
}

static void
gdk_riscos_gl_context_init (GdkRiscosGLContext *self)
{
}

/* ---- the backend's hooks ----------------------------------------------------- */

GdkGLContext *
_gdk_riscos_window_create_gl_context (GdkWindow    *window,
                                      gboolean      attached,
                                      GdkGLContext *share,
                                      GError      **error)
{
  GdkRiscosGLContext *context;

  if (!init_egl ())
    {
      g_set_error (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                   "OpenGL is not available: %s", egl.why);
      return NULL;
    }

  context = g_object_new (GDK_TYPE_RISCOS_GL_CONTEXT,
                          "display", gdk_window_get_display (window),
                          "window", window,
                          "shared-context", share,
                          NULL);
  context->is_attached = attached;
  return GDK_GL_CONTEXT (context);
}

gboolean
_gdk_riscos_display_make_gl_context_current (GdkDisplay   *display,
                                             GdkGLContext *context)
{
  GdkRiscosGLContext *self;

  if (!egl.ok)
    return FALSE;

  if (context == NULL)
    {
      eglMakeCurrent (egl.dpy, NULL, NULL, NULL);
      return TRUE;
    }

  self = GDK_RISCOS_GL_CONTEXT (context);
  /* EGL keeps a current context per API: GL calls go to the bound one */
  eglBindAPI (gdk_gl_context_get_use_es (context) ? EGL_OPENGL_ES_API : EGL_OPENGL_API);
  if (!eglMakeCurrent (egl.dpy, egl.pbuffer, egl.pbuffer, self->egl_context))
    {
      g_warning ("eglMakeCurrent failed (EGL error 0x%x)", eglGetError ());
      return FALSE;
    }
  return TRUE;
}
