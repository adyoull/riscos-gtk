/*
 * hellotest.c - runs tests/gtk/hello.c on the fake RISC OS
 * (tests/fake/mesa-egl/fake_riscos.c) as an arm-linux program under
 * qemu-arm-aligntrap: GTK, riscos-gtk's GDK backend and the RISCOS theme
 * draw into the fake screen, and the fake Wimp feeds it a click on the
 * button, typing in the entry, a click on the check box and a close.
 *
 *   hellotest APPDIR [OUT.ppm]
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include "hello.h"
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static int n_tests, n_failed;
static char title_seen[256];
static int stage, plots_at_click;
static int big_width = -1, big_height = -1, red_x = -1, blue_x = -1;
static unsigned int big_corner;

/* Leftmost strongly red and strongly blue pixels in the label row area
   (fake screen pixels are TBGR, 0x00BBGGRR). */
static void
find_colours (int x0, int y0, int x1, int y1)
{
  int x, y;
  red_x = blue_x = -1;
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++)
      {
        unsigned int p = fake_screen_pixel (x, y);
        int r = p & 0xFF, g = (p >> 8) & 0xFF, b = (p >> 16) & 0xFF;
        if (r > 150 && g < 90 && b < 90 && (red_x < 0 || x < red_x))
          red_x = x;
        if (b > 150 && r < 90 && g < 90 && (blue_x < 0 || x < blue_x))
          blue_x = x;
      }
}

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
centre (GtkWidget *w, int *x, int *y)
{
  GtkAllocation a;
  gtk_widget_get_allocation (w, &a);
  *x = a.x + a.width / 2;
  *y = a.y + a.height / 2;
}

/* Called before each Wimp_Poll returns: once the window is drawn, script
   the user's actions. */
static void
hook (int reason)
{
  int x, y;

  if (fake_wimp_title)
    g_strlcpy (title_seen, fake_wimp_title, sizeof title_seen);
  if (reason == 3 && stage == 1 && big_width < 0 && hello_window)
    {
      /* About to close: after the toggle-size request */
      big_width = gtk_widget_get_allocated_width (hello_window);
      big_height = gtk_widget_get_allocated_height (hello_window);
      big_corner = fake_screen_pixel (200 + 880, 150 + 480) & 0xFFFFFF;
      find_colours (200, 150, 1100, 650);
    }
  if (stage == 0 && hello_window && gtk_widget_get_mapped (hello_window) &&
      gtk_widget_get_allocated_width (hello_button) > 1 && fake_plots > 0)
    {
      stage = 1;
      plots_at_click = fake_plots;
      centre (hello_button, &x, &y);
      add (FAKE_MOVE, x, y, 0);
      add (6, x, y, 4);                 /* Select click on the button */
      add (FAKE_NULL, 0, 0, 0);
      add (FAKE_RELEASE, 0, 0, 0);
      add (FAKE_NULL, 0, 0, 0);
      centre (hello_entry, &x, &y);
      add (FAKE_MOVE, x, y, 0);
      add (6, x, y, 4);                 /* click in the entry */
      add (FAKE_RELEASE, 0, 0, 0);
      add (8, 'H', 0, 0);
      add (8, 'i', 0, 0);
      add (8, 0x18C, 0, 0);             /* Left */
      add (8, 0x18D, 0, 0);             /* Right */
      centre (hello_check, &x, &y);
      add (FAKE_MOVE, x, y, 0);
      add (6, x, y, 4);                 /* click the check box */
      add (FAKE_RELEASE, 0, 0, 0);
      for (x = 0; x < 20; x++)
        add (FAKE_NULL, 0, 0, 0);
      /* Toggle size / corner drag: the Wimp asks for 900x500 pixels
         (OS units at EIG 1); GTK must lay out and paint the new size. */
      add (2, (900 * 2) | ((500 * 2) << 16), 0, 0);
      for (x = 0; x < 30; x++)
        add (FAKE_NULL, 0, 0, 0);
      /* GTK's frame clock runs on real time: give it some */
      for (x = 0; x < 10; x++)
        add (FAKE_SLEEP, 20, 0, 0);
      /* then the fake sends Close_Window_Request */
    }
}

static void
save_ppm (const char *path, int x0, int y0, int w, int h)
{
  FILE *f = fopen (path, "wb");
  int x, y;
  if (!f)
    return;
  fprintf (f, "P6\n%d %d\n255\n", w, h);
  for (y = y0; y < y0 + h; y++)
    for (x = x0; x < x0 + w; x++)
      {
        unsigned int p = fake_screen_pixel (x, y);   /* TBGR: 0x00BBGGRR */
        fputc (p & 0xFF, f);
        fputc ((p >> 8) & 0xFF, f);
        fputc ((p >> 16) & 0xFF, f);
      }
  fclose (f);
}

int
main (int argc, char **argv)
{
  int x, y, colours = 0, dark = 0;
  unsigned int seen[16];
  int nseen = 0;
  char logpath[600], logbuf[4096] = "";
  char *hargv[4];
  int saved_out = dup (1), saved_err = dup (2);
  FILE *lf;

  fake_set_screen (1280, 720, 0, 5);     /* TBGR desktop, as on the Pi */
  fake_sprite_plot_converts = 0;         /* SpriteOp copies pixels as they are, as on the Pi */
  fake_wimp_nulls = 10;
  fake_wimp_hook = hook;
  /* As !Run does: hello APPDIR LOG. The log takes stdout and stderr, so
     hellotest's own output is put back afterwards. */
  g_snprintf (logpath, sizeof logpath, "%s/GTKHello.Log", argc > 1 ? argv[1] : ".");
  hargv[0] = argv[0]; hargv[1] = argc > 1 ? argv[1] : (char *) "."; hargv[2] = logpath; hargv[3] = NULL;
  hello_main (3, hargv);
  fflush (stdout); fflush (stderr);
  dup2 (saved_out, 1); dup2 (saved_err, 2);
  if ((lf = fopen (logpath, "r")))
    {
      size_t n = fread (logbuf, 1, sizeof logbuf - 1, lf);
      logbuf[n] = 0;
      fclose (lf);
    }

  check (strstr (logbuf, "riscos-gtk: task") && strstr (logbuf, "hello: window closed"),
         "the log has stderr and stdout (one write-open, as RISC OS allows)");
  check (stage == 1, "window shown and drawn (fake_plots > 0)");
  check (strcmp (title_seen, "GTK on RISC OS ? Gr\xfc\xdf" "e") == 0,
         "title in the Wimp's alphabet (Latin-1; the em dash has none)");
  printf ("# title \"%s\"\n", title_seen);
  check (hello_clicks == 1, "the button's clicked signal (Select press + release)");
  check (fake_plots > plots_at_click, "the window redrew after the click");
  check (strcmp (hello_entry_text, "Hi") == 0, "typing into the entry (Key_Pressed, caret)");
  printf ("# entry text \"%s\"\n", hello_entry_text);
  check (hello_option == 1, "the check box toggled by a click");
  check (fake_wimp_keys_passed == 0, "no keys handed back to the Wimp");
  check (red_x >= 0 && blue_x >= 0 && red_x < blue_x,
         "\"Red\" is red and \"Blue\" is blue on a TBGR screen");
  printf ("# leftmost red pixel x=%d, blue x=%d\n", red_x, blue_x);
  check (big_width >= 890 && big_height >= 490,
         "toggle size: GTK lays the window out at the Wimp's new size");
  printf ("# after the open request: %dx%d\n", big_width, big_height);
  check (big_corner == 0xDDDDDD, "toggle size: the new area is painted (window grey)");
  printf ("# pixel near the new bottom-right corner: %06X\n", big_corner);
  {
    gchar *font = NULL, *theme = NULL;
    gint dpi = 0;
    gboolean anim = TRUE;
    g_object_get (gtk_settings_get_default (), "gtk-font-name", &font,
                  "gtk-theme-name", &theme, "gtk-xft-dpi", &dpi,
                  "gtk-enable-animations", &anim, NULL);
    check (font && strcmp (font, "DejaVu Sans 12") == 0 && theme &&
           strcmp (theme, "RISCOS") == 0 && dpi == 90 * 1024 && !anim,
           "desktop defaults from the backend (RISCOS, DejaVu Sans 12, 90 dpi)");
    check (gdk_screen_get_resolution (gdk_screen_get_default ()) == 90.0,
           "the screen resolution is 90 dpi (what Pango lays out with)");
    printf ("# settings: font \"%s\", theme \"%s\", dpi %d, animations %d\n",
            font, theme, dpi / 1024, anim);
    g_free (font);
    g_free (theme);
  }
  /* The window was at pixel 200,150, 320x180 (gtk_window_move/default size) */
  for (y = 150; y < 330; y++)
    for (x = 200; x < 520; x++)
      {
        unsigned int p = fake_screen_pixel (x, y) & 0xFFFFFF;
        int i;
        if (((p & 0xFF) + ((p >> 8) & 0xFF) + (p >> 16)) < 150)
          dark++;
        for (i = 0; i < nseen && seen[i] != p; i++)
          ;
        if (i == nseen && nseen < 16)
          seen[nseen++] = p;
      }
  colours = nseen;
  check (colours >= 6, "the window has the theme's colours and text");
  check (dark > 200, "dark text pixels drawn");
  printf ("# %d+ colours, %d dark pixels in the window\n", colours, dark);
  if (argc > 2)
    save_ppm (argv[2], 180, 120, 960, 560);
  printf ("1..%d\n# hellotest: %d checks, %d failed\n", n_tests, n_tests, n_failed);
  return n_failed;
}
