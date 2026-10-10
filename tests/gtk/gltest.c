/*
 * gltest.c - riscos-gtk: !GTKGLTest. A GtkGLArea drawing with OpenGL on
 * RISC OS, through riscos-mesa's EGL and OSMesa (software rendering):
 * a spinning triangle on a blue background, a white square in the top
 * left corner (to check the picture's way up), the renderer's name and
 * the frame rate.
 *
 * Built with riscos-mesa's devkit linked in:
 *   -Wl,-u,eglGetProcAddress -lEGL -lOSMesa -lstdc++ -lz -lm
 * (build/build-gltest.sh). Without it, the area shows GTK's "OpenGL
 * isn't available" message instead.
 *
 * GL calls go through libepoxy, as in any GTK program.
 *
 *   gltest [APPDIR [LOG]]       (as !Run starts it)
 * GLTEST_STILL=1: no spinning (the fake-desktop test).
 *
 * MIT licence.
 */
#include "gltest.h"
#include <epoxy/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

GtkWidget *gl_window, *gl_area, *gl_info, *gl_spin;
int gl_frames;
char gl_renderer[128];
char gl_error[256];
int gl_legacy = -1, gl_es = -1;

static double angle;
static gint64 fps_start;
static int fps_frames;

static void
on_realize (GtkGLArea *area, gpointer data)
{
  GdkGLContext *context;
  GError *error;

  gtk_gl_area_make_current (area);
  error = gtk_gl_area_get_error (area);
  if (error != NULL)
    {
      g_strlcpy (gl_error, error->message, sizeof gl_error);
      gtk_label_set_text (GTK_LABEL (gl_info), error->message);
      g_printerr ("gltest: %s\n", error->message);
      return;
    }
  context = gtk_gl_area_get_context (area);
  gl_legacy = gdk_gl_context_is_legacy (context);
  gl_es = gdk_gl_context_get_use_es (context);
  g_snprintf (gl_renderer, sizeof gl_renderer, "%s, %s",
              (const char *) glGetString (GL_RENDERER), (const char *) glGetString (GL_VERSION));
  gtk_label_set_text (GTK_LABEL (gl_info), gl_renderer);
  g_print ("gltest: %s (%s%s)\n", gl_renderer, gl_es ? "OpenGL ES" : "OpenGL",
           gl_legacy ? ", legacy" : "");
}

static gboolean
on_render (GtkGLArea *area, GdkGLContext *context, gpointer data)
{
  int w = gtk_widget_get_allocated_width (GTK_WIDGET (area));
  int h = gtk_widget_get_allocated_height (GTK_WIDGET (area));
  int scale = gtk_widget_get_scale_factor (GTK_WIDGET (area));
  double aspect = h > 0 ? (double) w / h : 1.0;

  glViewport (0, 0, w * scale, h * scale);
  glClearColor (0.1f, 0.2f, 0.4f, 1.0f);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();
  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  /* the white square, top left: the picture's way up */
  glColor3f (1.0f, 1.0f, 1.0f);
  glBegin (GL_QUADS);
  glVertex2f (-1.0f, 1.0f);
  glVertex2f (-0.8f, 1.0f);
  glVertex2f (-0.8f, 0.8f);
  glVertex2f (-1.0f, 0.8f);
  glEnd ();

  /* the triangle */
  glScalef (1.0f / (float) aspect, 1.0f, 1.0f);
  glRotatef ((float) angle, 0.0f, 0.0f, 1.0f);
  glBegin (GL_TRIANGLES);
  glColor3f (1.0f, 0.2f, 0.2f);
  glVertex2f (0.0f, 0.7f);
  glColor3f (0.2f, 1.0f, 0.2f);
  glVertex2f (-0.6f, -0.45f);
  glColor3f (0.3f, 0.4f, 1.0f);
  glVertex2f (0.6f, -0.45f);
  glEnd ();
  glFlush ();

  gl_frames++;
  fps_frames++;
  return TRUE;
}

static gboolean
tick (GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
  gint64 now = g_get_monotonic_time ();

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gl_spin)) || gl_error[0])
    return G_SOURCE_CONTINUE;
  angle += 2.0;
  if (angle >= 360.0)
    angle -= 360.0;
  gtk_gl_area_queue_render (GTK_GL_AREA (gl_area));

  if (fps_start == 0)
    fps_start = now;
  else if (now - fps_start >= G_USEC_PER_SEC)
    {
      char *s = g_strdup_printf ("%s - %.1f frames a second", gl_renderer,
                                 fps_frames * (double) G_USEC_PER_SEC / (now - fps_start));
      gtk_label_set_text (GTK_LABEL (gl_info), s);
      g_free (s);
      fps_start = now;
      fps_frames = 0;
    }
  return G_SOURCE_CONTINUE;
}

int
gltest_main (int argc, char **argv)
{
  GtkWidget *box, *row;

  if (argc > 2)
    {
      /* one write-open per file on RISC OS: open the log once */
      if (freopen (argv[2], "w", stderr))
        {
          setvbuf (stderr, NULL, _IONBF, 0);
          fflush (stdout);
          dup2 (fileno (stderr), fileno (stdout));
          setvbuf (stdout, NULL, _IOLBF, 0);
        }
    }
  if (argc > 1 && argv[1][0])
    {
      char *conf = g_build_filename (argv[1], "fonts", "fonts.conf", NULL);
      char *share = g_build_filename (argv[1], "share", NULL);
      g_setenv ("FONTCONFIG_FILE", conf, TRUE);
      g_setenv ("XDG_DATA_DIRS", share, TRUE);
      g_free (conf);
      g_free (share);
    }
  g_set_prgname ("GTKGLTest");
  gtk_init (&argc, &argv);

  gl_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (gl_window), "GTK OpenGL test");
  g_signal_connect (gl_window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (gl_window), box);

  gl_area = gtk_gl_area_new ();
  gtk_gl_area_set_has_depth_buffer (GTK_GL_AREA (gl_area), TRUE);
  gtk_widget_set_size_request (gl_area, 400, 300);
  gtk_widget_set_hexpand (gl_area, TRUE);
  gtk_widget_set_vexpand (gl_area, TRUE);
  g_signal_connect (gl_area, "realize", G_CALLBACK (on_realize), NULL);
  g_signal_connect (gl_area, "render", G_CALLBACK (on_render), NULL);
  gtk_box_pack_start (GTK_BOX (box), gl_area, TRUE, TRUE, 0);

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (row), 8);
  gl_spin = gtk_check_button_new_with_label ("Spin");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gl_spin), !g_getenv ("GLTEST_STILL"));
  gtk_box_pack_start (GTK_BOX (row), gl_spin, FALSE, FALSE, 0);
  gl_info = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (gl_info), 0);
  gtk_label_set_ellipsize (GTK_LABEL (gl_info), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (row), gl_info, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), row, FALSE, FALSE, 0);

  gtk_widget_add_tick_callback (gl_area, tick, NULL, NULL);
  gtk_widget_show_all (gl_window);
  gtk_main ();
  g_print ("gltest: %d frames\n", gl_frames);
  return 0;
}

#ifndef GLTEST_NO_MAIN
int
main (int argc, char **argv)
{
  return gltest_main (argc, argv);
}
#endif
