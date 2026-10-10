/*
 * gltest-fake.c - runs tests/gtk/gltest.c (!GTKGLTest) on the fake RISC
 * OS under qemu-arm-aligntrap, with riscos-mesa's real RISC OS libEGL.a
 * and libOSMesa.a linked in (tests/fake/mesa-shim.c): GtkGLArea through
 * riscos-gtk's GDK GL context.
 *
 * It checks that GDK made an OpenGL context (2.1 compatibility, so
 * "legacy" to GTK) from Mesa, that the area rendered, and what reached
 * the screen: the blue background, the triangle in the middle and the
 * white square at the top left (so the picture is the right way up).
 * Then it resizes the window and checks the picture follows.
 * Built without riscos-mesa (gltest-nogl, GLTEST_NO_MESA=1), it checks
 * that GtkGLArea says OpenGL isn't available and the program carries on.
 *
 *   gltest-fake APPDIR OUTDIR
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include "gltest.h"
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int n_tests, n_failed;
static int stage, frames_at_resize, area_w1, area_h1;
static unsigned int px_bg, px_mid, px_square, px_bg2, px_square2;
static int ax, ay, aw, ah;                 /* the area, root GDK pixels */
static const char *outdir;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
  fflush (stdout);
}

static void
add (int reason, int a, int b, int c)
{
  int *e = fake_wimp_script[fake_wimp_script_len++];
  e[0] = reason; e[1] = a; e[2] = b; e[3] = c;
}

static void
area_rect (void)
{
  int ox, oy, x, y;
  gdk_window_get_origin (gtk_widget_get_window (gl_window), &ox, &oy);
  gtk_widget_translate_coordinates (gl_area, gl_window, 0, 0, &x, &y);
  ax = ox + x;
  ay = oy + y;
  aw = gtk_widget_get_allocated_width (gl_area);
  ah = gtk_widget_get_allocated_height (gl_area);
}

static unsigned int
px (int x, int y)
{
  return fake_screen_pixel (ax + x, ay + y) & 0xFFFFFF;   /* 0x00BBGGRR */
}

static void
save_ppm (const char *name)
{
  char *path = g_build_filename (outdir, name, NULL);
  FILE *f = fopen (path, "wb");
  int x, y, w = aw + 40, h = ah + 80;
  g_free (path);
  if (!f)
    return;
  fprintf (f, "P6\n%d %d\n255\n", w, h);
  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      {
        unsigned int p = fake_screen_pixel (ax - 20 + x, ay - 20 + y);
        fputc (p & 0xFF, f);
        fputc ((p >> 8) & 0xFF, f);
        fputc ((p >> 16) & 0xFF, f);
      }
  fclose (f);
}

static void
hook (int reason)
{
  int i;

  if (stage == 0 && gl_window && gtk_widget_get_mapped (gl_window) && (gl_frames >= 1 || gl_error[0]) && fake_plots > 0)
    {
      stage = 1;
      for (i = 0; i < 10; i++)
        add (FAKE_NULL, 0, 0, 0);
    }
  else if (stage == 1 && fake_wimp_script_pos () >= 8)
    {
      stage = 2;
      area_rect ();
      area_w1 = aw;
      area_h1 = ah;
      px_bg = px (aw - 6, ah - 6);           /* bottom right: background */
      px_mid = px (aw / 2, ah / 2);          /* the triangle's middle */
      px_square = px (aw / 40, ah / 40);     /* inside the white square */
      save_ppm (getenv ("FAKE_EIG") && atoi (getenv ("FAKE_EIG")) == 0 ? "gltest-eig0.ppm" : "gltest.ppm");
      frames_at_resize = gl_frames;
      /* the Wimp asks for a bigger window: 600 x 450 pixels (OS units at EIG 1) */
      add (2, (600 * 2) | ((450 * 2) << 16), 0, 0);
      for (i = 0; i < 30; i++)
        add (FAKE_NULL, 0, 0, 0);
      for (i = 0; i < 5; i++)
        add (FAKE_SLEEP, 20, 0, 0);
      for (i = 0; i < 10; i++)
        add (FAKE_NULL, 0, 0, 0);
    }
  else if (stage == 2 && fake_wimp_script_pos () >= fake_wimp_script_len - 2)
    {
      stage = 3;
      area_rect ();
      px_bg2 = px (aw - 6, ah - 6);
      px_square2 = px (aw / 40, ah / 40);
      /* then the fake sends Close_Window_Request */
    }
}

static int
near (unsigned int p, int r, int g, int b)
{
  int pr = p & 0xFF, pg = (p >> 8) & 0xFF, pb = (p >> 16) & 0xFF;
  return abs (pr - r) <= 3 && abs (pg - g) <= 3 && abs (pb - b) <= 3;
}

int
main (int argc, char **argv)
{
  char *gargv[2];

  outdir = argc > 2 ? argv[2] : ".";
  if (argc > 1)
    {
      char *conf = g_build_filename (argv[1], "fonts", "fonts.conf", NULL);
      char *share = g_build_filename (argv[1], "share", NULL);
      g_setenv ("FONTCONFIG_FILE", conf, TRUE);
      g_setenv ("XDG_DATA_DIRS", share, TRUE);
    }
  g_setenv ("GLTEST_STILL", "1", TRUE);
  g_setenv ("MESA_NO_JIT", "1", TRUE);       /* the interpreter: no JIT memory games under qemu */
  fake_set_screen (1280, 720, 0, 5);
  fake_sprite_plot_converts = 0;
  fake_wimp_nulls = 10;
  fake_wimp_hook = hook;

  gargv[0] = argv[0]; gargv[1] = NULL;
  gltest_main (1, gargv);

  printf ("# renderer \"%s\", legacy %d, es %d, error \"%s\", frames %d\n",
          gl_renderer, gl_legacy, gl_es, gl_error, gl_frames);
  if (g_getenv ("GLTEST_NO_MESA"))
    {
      /* built without riscos-mesa (gltest-nogl): GtkGLArea says why,
         and the program carries on */
      check (strstr (gl_error, "OpenGL is not available") != NULL &&
             strstr (gl_error, "riscos-mesa") != NULL,
             "without riscos-mesa linked in, GtkGLArea reports that OpenGL isn't available");
      check (gl_frames == 0 && gl_window != NULL, "and the program runs on without GL");
      printf ("1..%d\n", n_tests);
      return n_failed;
    }
  check (gl_error[0] == 0, "GtkGLArea got a GL context (no error)");
  check (strstr (gl_renderer, "Mesa") != NULL || strstr (gl_renderer, "OSMesa") != NULL,
         "the renderer is riscos-mesa's OSMesa");
  check (strstr (gl_renderer, "2.1") != NULL && gl_legacy == 1 && gl_es == 0,
         "OpenGL 2.1 compatibility (legacy to GTK), not ES");
  check (gl_frames >= 1, "the area rendered");
  printf ("# area %dx%d at %d,%d; pixels (0x00BBGGRR): background %06X, middle %06X, square %06X\n",
          area_w1, area_h1, ax, ay, px_bg, px_mid, px_square);
  check (near (px_bg, 26, 51, 102), "the background reached the screen (glClearColor 0.1, 0.2, 0.4)");
  check (!near (px_mid, 26, 51, 102) && px_mid != 0, "the triangle is in the middle");
  check (near (px_square, 255, 255, 255), "the white square is at the top left: the picture is the right way up");
  printf ("# after the resize: area %dx%d; background %06X, square %06X; frames %d -> %d\n",
          aw, ah, px_bg2, px_square2, frames_at_resize, gl_frames);
  check (aw > area_w1 && ah > area_h1 && gl_frames > frames_at_resize,
         "resized: the area grew and rendered again");
  check (near (px_bg2, 26, 51, 102) && near (px_square2, 255, 255, 255),
         "after the resize, the picture fills the new area, the right way up");
  printf ("1..%d\n", n_tests);
  return n_failed;
}
