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

static int n_tests, n_failed;
static char title_seen[256];
static int stage, plots_at_click;

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

  fake_set_screen (1280, 720, 0, 5);     /* TBGR desktop: the TRGB sprites get swapped */
  fake_wimp_nulls = 10;
  fake_wimp_hook = hook;
  hello_main (argc > 1 ? 2 : 1, argv);

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
    save_ppm (argv[2], 180, 120, 380, 240);
  printf ("1..%d\n# hellotest: %d checks, %d failed\n", n_tests, n_tests, n_failed);
  return n_failed;
}
