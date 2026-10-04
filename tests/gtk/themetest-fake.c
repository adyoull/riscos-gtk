/*
 * themetest-fake.c - runs tests/gtk/themetest.c on the fake RISC OS
 * (tests/fake/mesa-egl/fake_riscos.c) under qemu-arm-aligntrap: clicks each
 * notebook tab, opens the File menu from the menu bar, closes it with
 * Escape, and saves a picture of each page and of the open menu.
 *
 *   themetest-fake APPDIR OUTDIR
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include "themetest.h"
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int n_tests, n_failed;
static int stage, last_page = -1, polls_on_page, saved[4], menu_seen, max_windows;
static const char *outdir;
static int win_w, win_h, main_handle, menu_label_dark = -1;

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

/* Centre of a widget, in pixels from the toplevel's top left */
static void
centre (GtkWidget *w, int *x, int *y)
{
  GtkAllocation a;
  gtk_widget_get_allocation (w, &a);
  gtk_widget_translate_coordinates (w, tt_window, a.width / 2, a.height / 2, x, y);
}

static void
save_ppm (const char *name, int x0, int y0, int w, int h)
{
  char path[512];
  FILE *f;
  int x, y;

  g_snprintf (path, sizeof path, "%s/%s", outdir, name);
  if (!(f = fopen (path, "wb")))
    return;
  fprintf (f, "P6\n%d %d\n255\n", w, h);
  for (y = y0; y < y0 + h; y++)
    for (x = x0; x < x0 + w; x++)
      {
        unsigned int p = fake_screen_pixel (x, y);   /* TBGR */
        fputc (p & 0xFF, f);
        fputc ((p >> 8) & 0xFF, f);
        fputc ((p >> 16) & 0xFF, f);
      }
  fclose (f);
}

static int
open_windows (void)
{
  int i, n = 0;
  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle)
      n++;
  return n;
}

static void
hook (int reason)
{
  int x, y, i, n;

  n = open_windows ();
  if (n > max_windows)
    max_windows = n;
  if (stage == 0 && tt_window && gtk_widget_get_mapped (tt_window) && fake_plots > 0 &&
      gtk_widget_get_allocated_width (tt_notebook) > 1)
    {
      stage = 1;
      for (i = 0; i < FAKE_MAX_WINDOWS; i++)
        if (fake_windows[i].handle)
          main_handle = fake_windows[i].handle;
      win_w = gtk_widget_get_allocated_width (tt_window);
      win_h = gtk_widget_get_allocated_height (tt_window);
      for (i = 1; i < 4; i++)
        {
          GtkWidget *tab = gtk_notebook_get_tab_label (GTK_NOTEBOOK (tt_notebook),
                             gtk_notebook_get_nth_page (GTK_NOTEBOOK (tt_notebook), i));
          int k;
          centre (tab, &x, &y);
          add (FAKE_MOVE, x, y, 0);
          add (6, x, y, 4);                     /* Select click on the tab */
          add (FAKE_RELEASE, 0, 0, 0);
          for (k = 0; k < 15; k++)
            add (FAKE_NULL, 0, 0, 0);
        }
      /* the File menu from the menu bar */
      centre (GTK_WIDGET (gtk_container_get_children (GTK_CONTAINER (tt_menubar))->data), &x, &y);
      add (FAKE_MOVE, x, y, 0);
      add (6, x, y, 4);
      add (FAKE_RELEASE, 0, 0, 0);
      for (i = 0; i < 20; i++)
        add (FAKE_NULL, 0, 0, 0);
      add (8, 0x1B, 0, 0);                      /* Escape closes it */
      for (i = 0; i < 20; i++)
        add (FAKE_NULL, 0, 0, 0);
      /* then the fake sends Close_Window_Request */
    }
  if (stage == 1 && tt_notebook)
    {
      int cur = gtk_notebook_get_current_page (GTK_NOTEBOOK (tt_notebook));
      if (cur != last_page)
        {
          last_page = cur;
          polls_on_page = 0;
        }
      else if (++polls_on_page == 14 && cur >= 0 && cur < 4 && !saved[cur])
        {
          char name[32];
          saved[cur] = 1;
          g_snprintf (name, sizeof name, "themetest-page%d.ppm", cur);
          save_ppm (name, 100, 60, 760, 800);
        }
      if (n >= 2 && !menu_seen && polls_on_page > 25)
        {
          /* a pop-up window is open (the menu): picture it after a while */
          static int menu_polls;
          if (++menu_polls == 10)
            {
              menu_seen = 1;
              save_ppm ("themetest-menu.ppm", 100, 60, 760, 400);
              /* dark pixels in the left 45% of the menu: the item names
                 (the right holds the short-cuts) */
              for (i = 0; i < FAKE_MAX_WINDOWS; i++)
                {
                  fake_window_t *fw = &fake_windows[i];
                  if (fw->handle && fw->handle != main_handle)
                    {
                      int px0 = fw->x0 / 2, px1 = fw->x0 / 2 + (fw->x1 - fw->x0) / 2 * 45 / 100;
                      int py0 = 1024 - fw->y1 / 2, py1 = 1024 - fw->y0 / 2;   /* 1024 high, EIG 1 */
                      menu_label_dark = 0;
                      for (y = py0 + 2; y < py1 - 2; y++)
                        for (x = px0 + 2; x < px1; x++)
                          {
                            unsigned int p = fake_screen_pixel (x, y);
                            if ((p & 0xFF) + ((p >> 8) & 0xFF) + ((p >> 16) & 0xFF) < 200)
                              menu_label_dark++;
                          }
                    }
                }
            }
        }
    }
}

int
main (int argc, char **argv)
{
  char logpath[600], logbuf[16384] = "";
  char *targv[4];
  int saved_out = dup (1), saved_err = dup (2);
  FILE *lf;
  int pages = 0;

  if (argc < 3)
    return 2;
  outdir = argv[2];
  fake_set_screen (1280, 1024, 0, 5);
  fake_sprite_plot_converts = 0;
  fake_wimp_nulls = 400;             /* the hook scripts the rest once drawn */
  fake_wimp_hook = hook;
  g_snprintf (logpath, sizeof logpath, "%s/GTKThemeTest.Log", outdir);
  targv[0] = argv[0]; targv[1] = argv[1]; targv[2] = logpath; targv[3] = NULL;
  themetest_main (3, targv);
  fflush (stdout); fflush (stderr);
  dup2 (saved_out, 1); dup2 (saved_err, 2);
  if ((lf = fopen (logpath, "r")))
    {
      size_t k = fread (logbuf, 1, sizeof logbuf - 1, lf);
      logbuf[k] = 0;
      fclose (lf);
    }
  pages = (strstr (logbuf, "page 1: Options") != NULL) + (strstr (logbuf, "page 2: Lists") != NULL)
          + (strstr (logbuf, "page 3: Dialogues") != NULL);

  check (stage == 1, "theme test window shown and drawn");
  check (win_w > 500 && win_w < 1000 && win_h > 500 && win_h < 950,
         "the window has a sensible size (labels measured with real font metrics)");
  printf ("# window %dx%d\n", win_w, win_h);
  check (pages == 3, "the three other tabs open when clicked");
  check (saved[0] && saved[1] && saved[2] && saved[3], "each page drawn and pictured");
  check (max_windows >= 2 && menu_seen, "the File menu opens as a pop-up window");
  check (menu_label_dark > 100, "the menu's item names are drawn (not white on white)");
  printf ("# %d dark pixels where the menu's item names are\n", menu_label_dark);
  check (strstr (logbuf, "window closed") != NULL, "the window closes (after Escape closed the menu)");
  check (strstr (logbuf, "CRITICAL") == NULL && strstr (logbuf, "WARNING") == NULL &&
         strstr (logbuf, "Gtk-") == NULL, "no GTK warnings or criticals in the log");
  printf ("# log:\n");
  {
    char *line = strtok (logbuf, "\n");
    while (line)
      {
        printf ("#   %s\n", line);
        line = strtok (NULL, "\n");
      }
  }
  printf ("1..%d\n# themetest-fake: %d checks, %d failed\n", n_tests, n_tests, n_failed);
  return n_failed;
}
