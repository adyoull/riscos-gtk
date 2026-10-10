/*
 * glarea.c - riscos-gtk devkit example: OpenGL in a GTK program on RISC
 * OS. A GtkGLArea draws a spinning triangle with a vertex buffer and a
 * pair of GLSL shaders; the frame rate is shown underneath.
 *
 * OpenGL on RISC OS is riscos-mesa (Mesa's software renderer: OpenGL 2.1,
 * GLSL 1.20), linked into the program: build with $RISCOS_GTK_GL_LIBS
 * (the Makefile does). GTK asks for OpenGL 3.2 core by default and gets
 * a 2.1 compatibility ("legacy") context here, as on any system without
 * core profiles, so the shaders are GLSL 1.20 and the code checks
 * gdk_gl_context_is_legacy() if it wants to know. GL calls go through
 * libepoxy, as in any GTK program.
 *
 * MIT licence: copy and change it freely.
 */
#include <gtk/gtk.h>
#include <epoxy/gl.h>
#include <riscos-gtk/app.h>

#define APP_NAME    "GLArea"
#define APP_VERSION "1.00"

static GtkWidget *area, *info, *spin;
static GLuint program, buffer;
static GLint u_angle, u_aspect, a_pos, a_colour;
static float angle;
static gint64 fps_start;
static int fps_frames;
static char renderer[160];

static const char *vertex_shader =
  "#version 120\n"
  "uniform float angle;\n"
  "uniform float aspect;\n"
  "attribute vec2 pos;\n"
  "attribute vec3 colour;\n"
  "varying vec3 v_colour;\n"
  "void main() {\n"
  "  float c = cos(angle), s = sin(angle);\n"
  "  vec2 p = vec2(c * pos.x - s * pos.y, s * pos.x + c * pos.y);\n"
  "  gl_Position = vec4(p.x / aspect, p.y, 0.0, 1.0);\n"
  "  v_colour = colour;\n"
  "}\n";

static const char *fragment_shader =
  "#version 120\n"
  "varying vec3 v_colour;\n"
  "void main() {\n"
  "  gl_FragColor = vec4(v_colour, 1.0);\n"
  "}\n";

/* x, y, r, g, b */
static const float triangle[] = {
   0.0f,  0.7f,  1.0f, 0.2f, 0.2f,
  -0.6f, -0.45f, 0.2f, 1.0f, 0.2f,
   0.6f, -0.45f, 0.3f, 0.4f, 1.0f,
};

static GLuint
compile (GLenum type, const char *source)
{
  GLuint shader = glCreateShader (type);
  GLint ok;
  char log[512];

  glShaderSource (shader, 1, &source, NULL);
  glCompileShader (shader);
  glGetShaderiv (shader, GL_COMPILE_STATUS, &ok);
  if (!ok)
    {
      glGetShaderInfoLog (shader, sizeof log, NULL, log);
      g_printerr ("glarea: shader: %s\n", log);
    }
  return shader;
}

static void
on_realize (GtkGLArea *gl_area, gpointer data)
{
  GLuint vs, fs;

  gtk_gl_area_make_current (gl_area);
  if (gtk_gl_area_get_error (gl_area) != NULL)
    {
      /* e.g. built without $RISCOS_GTK_GL_LIBS */
      gtk_label_set_text (GTK_LABEL (info), gtk_gl_area_get_error (gl_area)->message);
      return;
    }
  g_snprintf (renderer, sizeof renderer, "%s, OpenGL %s",
              (const char *) glGetString (GL_RENDERER), (const char *) glGetString (GL_VERSION));
  gtk_label_set_text (GTK_LABEL (info), renderer);
  g_print ("%s\n", renderer);           /* to the Log */

  vs = compile (GL_VERTEX_SHADER, vertex_shader);
  fs = compile (GL_FRAGMENT_SHADER, fragment_shader);
  program = glCreateProgram ();
  glAttachShader (program, vs);
  glAttachShader (program, fs);
  glLinkProgram (program);
  glDeleteShader (vs);
  glDeleteShader (fs);
  u_angle = glGetUniformLocation (program, "angle");
  u_aspect = glGetUniformLocation (program, "aspect");
  a_pos = glGetAttribLocation (program, "pos");
  a_colour = glGetAttribLocation (program, "colour");

  glGenBuffers (1, &buffer);
  glBindBuffer (GL_ARRAY_BUFFER, buffer);
  glBufferData (GL_ARRAY_BUFFER, sizeof triangle, triangle, GL_STATIC_DRAW);
}

static void
on_unrealize (GtkGLArea *gl_area, gpointer data)
{
  gtk_gl_area_make_current (gl_area);
  if (gtk_gl_area_get_error (gl_area) != NULL)
    return;
  glDeleteBuffers (1, &buffer);
  glDeleteProgram (program);
}

static gboolean
on_render (GtkGLArea *gl_area, GdkGLContext *context, gpointer data)
{
  int w = gtk_widget_get_allocated_width (GTK_WIDGET (gl_area));
  int h = gtk_widget_get_allocated_height (GTK_WIDGET (gl_area));

  /* the area's framebuffer is bound, at the window's scale (2 in an EX0
     EY0 mode): GtkGLArea sets the viewport to its size */
  glClearColor (0.1f, 0.2f, 0.4f, 1.0f);
  glClear (GL_COLOR_BUFFER_BIT);

  glUseProgram (program);
  glUniform1f (u_angle, angle);
  glUniform1f (u_aspect, h > 0 ? (float) w / h : 1.0f);
  glBindBuffer (GL_ARRAY_BUFFER, buffer);
  glEnableVertexAttribArray (a_pos);
  glEnableVertexAttribArray (a_colour);
  glVertexAttribPointer (a_pos, 2, GL_FLOAT, GL_FALSE, 5 * sizeof (float), (void *) 0);
  glVertexAttribPointer (a_colour, 3, GL_FLOAT, GL_FALSE, 5 * sizeof (float), (void *) (2 * sizeof (float)));
  glDrawArrays (GL_TRIANGLES, 0, 3);
  glDisableVertexAttribArray (a_pos);
  glDisableVertexAttribArray (a_colour);
  glUseProgram (0);

  fps_frames++;
  return TRUE;
}

/* each frame of the window's frame clock: turn a little and draw again */
static gboolean
tick (GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
  gint64 now = gdk_frame_clock_get_frame_time (clock);

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (spin)) || !renderer[0])
    return G_SOURCE_CONTINUE;
  angle += 0.03f;
  gtk_gl_area_queue_render (GTK_GL_AREA (area));
  if (fps_start == 0)
    fps_start = now;
  else if (now - fps_start >= G_USEC_PER_SEC)
    {
      char *s = g_strdup_printf ("%s - %.1f frames a second", renderer,
                                 fps_frames * (double) G_USEC_PER_SEC / (now - fps_start));
      gtk_label_set_text (GTK_LABEL (info), s);
      g_free (s);
      fps_start = now;
      fps_frames = 0;
    }
  return G_SOURCE_CONTINUE;
}

static void
on_activate (GApplication *app, gpointer data)
{
  GtkWidget *window, *box, *row;
  GList *windows = gtk_application_get_windows (GTK_APPLICATION (app));

  if (windows)
    {
      gtk_window_present (windows->data);
      return;
    }
  window = gtk_application_window_new (GTK_APPLICATION (app));
  gtk_window_set_title (GTK_WINDOW (window), "OpenGL in GTK");
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);

  area = gtk_gl_area_new ();
  gtk_widget_set_size_request (area, 400, 300);
  gtk_widget_set_vexpand (area, TRUE);
  g_signal_connect (area, "realize", G_CALLBACK (on_realize), NULL);
  g_signal_connect (area, "unrealize", G_CALLBACK (on_unrealize), NULL);
  g_signal_connect (area, "render", G_CALLBACK (on_render), NULL);
  gtk_box_pack_start (GTK_BOX (box), area, TRUE, TRUE, 0);

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (row), 8);
  spin = gtk_check_button_new_with_label ("Spin");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (spin), TRUE);
  gtk_box_pack_start (GTK_BOX (row), spin, FALSE, FALSE, 0);
  info = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (info), 0);
  gtk_label_set_ellipsize (GTK_LABEL (info), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start (GTK_BOX (row), info, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), row, FALSE, FALSE, 0);

  gtk_widget_add_tick_callback (area, tick, NULL, NULL);
  gtk_widget_show_all (window);
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  int status;

  riscos_gtk_app_init (APP_NAME, &argc, &argv);
#ifdef GDK_WINDOWING_RISCOS
  gdk_riscos_iconbar_set_info (APP_NAME, "OpenGL in GTK", "\xc2\xa9 Your name", APP_VERSION);
#endif
  app = gtk_application_new ("org.example.GLArea", G_APPLICATION_NON_UNIQUE);
  g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);
  return status;
}
