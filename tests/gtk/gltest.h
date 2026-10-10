/* gltest.h - riscos-gtk: !GTKGLTest, GtkGLArea on RISC OS (riscos-mesa
 * EGL + OSMesa). What gltest-fake.c looks at. MIT licence. */
#ifndef GLTEST_H
#define GLTEST_H

#include <gtk/gtk.h>

extern GtkWidget *gl_window, *gl_area, *gl_info, *gl_spin;
extern int gl_frames;              /* frames rendered */
extern char gl_renderer[128];      /* GL_RENDERER, GL_VERSION */
extern char gl_error[256];         /* the area's error, if any */
extern int gl_legacy, gl_es;       /* what GDK made */

int gltest_main (int argc, char **argv);

#endif
