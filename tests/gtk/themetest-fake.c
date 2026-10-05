/*
 * themetest-fake.c - runs tests/gtk/themetest.c on the fake RISC OS
 * (tests/fake/mesa-egl/fake_riscos.c) under qemu-arm-aligntrap.
 *
 * It uses the program as a person would: a pop-up (combo box), the File
 * and View menus from the menu bar (a choice, then an Adjust choice that
 * must keep the menu open, then closing it as a click elsewhere does),
 * every notebook tab, and a context menu on Menu. Menus are Wimp menus
 * (gtk/riscos/gtkmenu-riscos.c), so it checks the menu blocks the program
 * hands to Wimp_CreateMenu, where they open, and what each choice did
 * (the program logs every action). Each page is pictured.
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
static int stage, last_page = -1, polls_on_page, saved[4];
static int win_w, win_h, opened_seen;
static const char *outdir;

/* Each Wimp menu opened, decoded */
#define MAX_MENUS 16
static struct
{
  char title[64];
  char text[16][96];
  int flags[16], icon[16], n, x, y;
} menus[MAX_MENUS];
static int n_menus;
static int moved_x0 = -1, moved_y1 = -1, moved_y0, after_x0 = -1, after_y1 = -1, after_y0, tooltip_windows, main_handle, pos_stage;
static int edit_open_after_leave = -1, edit_title_ok;
static int nb_h0 = -1, nb_h1 = -1, def0 = -1, def1 = -1, apply_clicked_seen;
static int combo_right, combo_top, file_left, file_bottom, ctx_x, ctx_y;

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
nulls (int n)
{
  while (n--)
    add (FAKE_NULL, 0, 0, 0);
}

/* Widget rectangle in pixels from the toplevel's top left */
static void
rect (GtkWidget *w, int *x, int *y, int *wd, int *ht)
{
  GtkAllocation a;
  gtk_widget_get_allocation (w, &a);
  gtk_widget_translate_coordinates (w, tt_window, 0, 0, x, y);
  *wd = a.width;
  *ht = a.height;
}

static void
click (GtkWidget *w, int buttons)
{
  int x, y, wd, ht;
  rect (w, &x, &y, &wd, &ht);
  add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);
  add (6, x + wd / 2, y + ht / 2, buttons);
  add (FAKE_RELEASE, 0, 0, 0);
  nulls (6);
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

static void
record_menu (void)
{
  const int *m = fake_menu, *it = m + 7;
  int k = n_menus < MAX_MENUS ? n_menus++ : MAX_MENUS - 1, i;

  memset (&menus[k], 0, sizeof menus[k]);
  if (it[0] & 0x100)
    g_strlcpy (menus[k].title, (const char *) (long) m[0], sizeof menus[k].title);
  else
    memcpy (menus[k].title, m, 12);
  for (i = 0; i < 16; i++, it += 6)
    {
      if (it[2] & 0x100)
        g_strlcpy (menus[k].text[i], (const char *) (long) it[3], sizeof menus[k].text[i]);
      else
        memcpy (menus[k].text[i], it + 3, 12);
      menus[k].flags[i] = it[0];
      menus[k].icon[i] = it[2];
      menus[k].n = i + 1;
      if (it[0] & 0x80)
        break;
    }
  menus[k].x = fake_menu_x;
  menus[k].y = fake_menu_y;
}

static int
find_menu (const char *title, int from)
{
  int k;
  for (k = from; k < n_menus; k++)
    if (strcmp (menus[k].title, title) == 0)
      return k;
  return -1;
}

static void
hook (int reason)
{
  int x, y, wd, ht, i;

  if (stage == 1 && !apply_clicked_seen && gtk_widget_has_focus (tt_apply))
    apply_clicked_seen = 1;
  else if (apply_clicked_seen == 1 && fake_wimp_script_pos () > 0)
    {
      apply_clicked_seen = 2;   /* a poll later: laid out again */
    }
  else if (apply_clicked_seen == 2)
    {
      apply_clicked_seen = 3;
      nb_h1 = gtk_widget_get_allocated_height (tt_notebook);
      def1 = gtk_style_context_has_class (gtk_widget_get_style_context (tt_apply), "default");
    }
  if (main_handle)
    for (i = 0; i < FAKE_MAX_WINDOWS; i++)
      if (fake_windows[i].handle == main_handle)
        {
          if (pos_stage == 1 && moved_x0 < 0 && fake_windows[i].x0 != 240)
            {
              moved_x0 = fake_windows[i].x0;
              moved_y1 = fake_windows[i].y1;
              moved_y0 = fake_windows[i].y0;
            }
          if (pos_stage == 1 && moved_x0 >= 0)
            {
              after_x0 = fake_windows[i].x0;
              after_y1 = fake_windows[i].y1;
              after_y0 = fake_windows[i].y0;
            }
        }
  {
    int open = 0;
    for (i = 0; i < FAKE_MAX_WINDOWS; i++)
      if (fake_windows[i].handle && fake_windows[i].handle != main_handle && !fake_menu)
        open++;
    if (pos_stage == 1 && open > tooltip_windows)
      tooltip_windows = open;
  }
  {
    int pos = fake_wimp_script_pos ();
    if (pos >= 0 && fake_wimp_script[pos][0] == FAKE_NULL && fake_wimp_script[pos][3] == 77 &&
        edit_open_after_leave < 0)
      {
        edit_open_after_leave = fake_menu != NULL;
        if (fake_menu && (fake_menu[7] & 0x100))
          edit_title_ok = strcmp ((const char *) (long) fake_menu[0], "Edit") == 0 &&
                          strncmp ((const char *) (long) fake_menu[7 + 3], "Undo", 4) == 0;
      }
  }
  if (fake_menu && fake_menus_opened != opened_seen)
    {
      opened_seen = fake_menus_opened;
      record_menu ();
    }
  if (stage == 0 && tt_window && gtk_widget_get_mapped (tt_window) && fake_plots > 0 &&
      gtk_widget_get_allocated_width (tt_notebook) > 1)
    {
      GtkWidget *file, *view;
      GList *bar = gtk_container_get_children (GTK_CONTAINER (tt_menubar));

      stage = 1;
      nb_h0 = gtk_widget_get_allocated_height (tt_notebook);
      def0 = gtk_style_context_has_class (gtk_widget_get_style_context (tt_apply), "default");
      for (i = 0; i < FAKE_MAX_WINDOWS; i++)
        if (fake_windows[i].handle)
          main_handle = fake_windows[i].handle;
      fake_target_window = main_handle;   /* not its tooltips */
      win_w = gtk_widget_get_allocated_width (tt_window);
      win_h = gtk_widget_get_allocated_height (tt_window);
      file = g_list_nth_data (bar, 0);
      view = g_list_nth_data (bar, 2);
      g_list_free (bar);

      /* expected places, OS units (window top left at pixel 120,80;
         EIG 1; 1024-pixel-high screen) */
      rect (tt_combo, &x, &y, &wd, &ht);
      combo_right = (120 + x + wd) * 2;
      combo_top = (1024 - (80 + y)) * 2;
      rect (file, &x, &y, &wd, &ht);
      file_left = (120 + x) * 2;
      file_bottom = (1024 - (80 + y + ht)) * 2;

      /* 0. the window has the input focus (the Wimp sends Gain_Caret when
            the program takes the caret) */
      add (12, 0, 0, 0);
      nulls (4);
      /* 1. the pop-up: choose its second entry */
      click (tt_combo, 4);
      add (9, 1, -1, 0);
      nulls (8);
      /* 2. File, then Save */
      click (file, 4);
      add (9, 1, -1, 0);
      nulls (8);
      /* 3. View, Show grid with Adjust (stays open), then a click
            elsewhere closes it (Message_MenusDeleted) */
      click (view, 4);
      add (FAKE_BUTTONS, 1, 0, 0);
      add (9, 0, -1, 0);
      nulls (8);
      add (FAKE_BUTTONS, 0, 0, 0);
      add (17, 0x400C9, 0, 0);
      nulls (8);
      /* 3a. Edit from the menu bar, then the pointer goes off the window
            onto the menu (as on the Pi: leave events), then a click
            elsewhere closes it */
      {
        GList *bar2 = gtk_container_get_children (GTK_CONTAINER (tt_menubar));
        GtkWidget *edit = g_list_nth_data (bar2, 1);
        g_list_free (bar2);
        rect (edit, &x, &y, &wd, &ht);
        add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);
        add (6, x + wd / 2, y + ht / 2, 4);
        nulls (3);
        add (FAKE_MOVE, win_w + 40, y + ht + 30, 0);   /* off the window: over the menu */
        nulls (3);
        add (FAKE_RELEASE, 0, 0, 0);
        for (i = 0; i < 8; i++)
          add (FAKE_SLEEP, 100, 0, 0);
        add (FAKE_BUTTONS, 0, 0, 0);
        pos_stage = 0;
      }
      add (FAKE_NULL, 0, 0, 77);                      /* marker: check the menu here */
      add (17, 0x400C9, 0, 0);
      nulls (8);
      /* 3b. the Wimp moves the window (a title bar drag), then a button is
            clicked: the window must stay where it was put */
      add (2, 0, 200, -100);
      nulls (8);
      pos_stage = 1;
      rect (tt_apply, &x, &y, &wd, &ht);
      add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);   /* rest on Apply: its tooltip */
      for (i = 0; i < 12; i++)
        add (FAKE_SLEEP, 100, 0, 0);
      pos_stage = 1;
      add (6, x + wd / 2, y + ht / 2, 4);
      add (FAKE_RELEASE, 0, 0, 0);
      nulls (10);
      /* 4. every tab */
      for (i = 1; i < 4; i++)
        {
          GtkWidget *tab = gtk_notebook_get_tab_label (GTK_NOTEBOOK (tt_notebook),
                             gtk_notebook_get_nth_page (GTK_NOTEBOOK (tt_notebook), i));
          click (tab, 4);
          nulls (12);
        }
      nulls (30);                 /* 5. is added once page 4 is drawn */
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
      else if (++polls_on_page == 10 && cur >= 0 && cur < 4 && !saved[cur])
        {
          char name[32];
          saved[cur] = 1;
          g_snprintf (name, sizeof name, "themetest-page%d.ppm", cur);
          save_ppm (name, 100, 60, 760, 800);
          if (cur == 3)
            {
              /* 5. Menu click in the context area, then Info */
              rect (tt_context_area, &x, &y, &wd, &ht);
              for (i = 0; i < FAKE_MAX_WINDOWS; i++)
                if (fake_windows[i].handle == main_handle)
                  {
                    ctx_x = fake_windows[i].x0 + (x + wd / 2) * 2;
                    ctx_y = fake_windows[i].y1 - (y + ht / 2) * 2 - 1;
                  }
              add (FAKE_MOVE, x + wd / 2, y + ht / 2, 0);
              add (6, x + wd / 2, y + ht / 2, 2);   /* Menu button */
              add (FAKE_RELEASE, 0, 0, 0);
              nulls (8);
              add (9, 2, -1, 0);
              nulls (10);
            }
        }
    }
}

static void
critical_handler (const gchar *domain, GLogLevelFlags level, const gchar *message, gpointer data)
{
  GdkEvent *ev = gtk_get_current_event ();
  GtkWidget *w = ev ? gtk_get_event_widget (ev) : NULL;
  fprintf (stderr, "CRITICAL: %s (event %d, widget %s, window %p, grab %s)\n", message,
           ev ? (int) ev->type : -1, w ? G_OBJECT_TYPE_NAME (w) : "-",
           ev ? (void *) ev->any.window : NULL,
           gtk_grab_get_current () ? G_OBJECT_TYPE_NAME (gtk_grab_get_current ()) : "-");
  if (ev)
    gdk_event_free (ev);
}

int
main (int argc, char **argv)
{
  char logpath[600], logbuf[16384] = "", *line;
  char *targv[4];
  int saved_out = dup (1), saved_err = dup (2);
  FILE *lf;
  int pages, f, v, c, p, k, i;

  if (argc < 3)
    return 2;
  outdir = argv[2];
  fake_set_screen (1280, 1024, 0, 5);
  fake_sprite_plot_converts = 0;
  fake_wimp_nulls = 400;             /* the hook scripts the rest once drawn */
  fake_wimp_hook = hook;
  g_snprintf (logpath, sizeof logpath, "%s/GTKThemeTest.Log", outdir);
  targv[0] = argv[0]; targv[1] = argv[1]; targv[2] = logpath; targv[3] = NULL;
  g_setenv ("RISCOS_GTK_MENU_DEBUG", "1", TRUE);
  if (getenv ("TT_DEBUG"))
    g_setenv ("RISCOS_GTK_DEBUG", "1", TRUE);
  if (getenv ("TT_CRIT"))
    g_log_set_handler ("Gtk", G_LOG_LEVEL_CRITICAL, critical_handler, NULL);
  themetest_main (3, targv);
  fflush (stdout); fflush (stderr);
  dup2 (saved_out, 1); dup2 (saved_err, 2);
  if ((lf = fopen (logpath, "r")))
    {
      size_t n = fread (logbuf, 1, sizeof logbuf - 1, lf);
      logbuf[n] = 0;
      fclose (lf);
    }

  p = find_menu ("GTKThemeTest", 0);         /* the pop-up's menu comes first */
  f = find_menu ("File", 0);
  v = find_menu ("View", 0);
  c = p >= 0 ? find_menu ("GTKThemeTest", p + 1) : -1;
  pages = (strstr (logbuf, "page 1: Options") != NULL) + (strstr (logbuf, "page 2: Lists") != NULL)
          + (strstr (logbuf, "page 3: Dialogues") != NULL);

  check (stage == 1, "theme test window shown and drawn");
  check (win_w > 500 && win_w < 1000 && win_h > 500 && win_h < 950,
         "the window has a sensible size");
  printf ("# window %dx%d\n", win_w, win_h);

  check (p >= 0 && menus[p].n == 3 && strcmp (menus[p].text[1], "UTF-8") == 0 &&
         (menus[p].flags[0] & 1) == 0,
         "pop-up: a Wimp menu of its three entries");
  check (p >= 0 && abs (menus[p].x - combo_right) <= 4 && abs (menus[p].y - combo_top) <= 4,
         "pop-up: opens to the right of the field, level with its top");
  if (p >= 0)
    printf ("# pop-up menu at %d,%d (field's right/top %d,%d)\n", menus[p].x, menus[p].y,
            combo_right, combo_top);
  check (strstr (logbuf, "Pop-up: \"UTF-8\" chosen") != NULL, "pop-up: choosing UTF-8 sets it");

  check (f >= 0 && menus[f].n == 4 && strncmp (menus[f].text[0], "Open...", 7) == 0 &&
         strstr (menus[f].text[0], "^O") && strstr (menus[f].text[2], "Shift-F3") &&
         (menus[f].flags[2] & 2) && strcmp (menus[f].text[3], "Quit") == 0 &&
         (menus[f].flags[3] & 0x80),
         "File: Wimp menu with short-cuts, a dotted line before Quit, Quit last");
  check (f >= 0 && abs (menus[f].x - file_left) <= 4 && abs (menus[f].y - file_bottom) <= 4,
         "File: opens below its menu bar title");
  check (strstr (logbuf, "menu \"Save\"") != NULL, "File: choosing Save activates it");

  check (v >= 0 && (menus[v].flags[0] & 1) && (menus[v].flags[0] & 2) &&
         strcmp (menus[v].text[0], "Show grid") == 0 &&
         (menus[v].flags[1] & 1) && !(menus[v].flags[2] & 1),
         "View: ticks for Show grid and Portrait, none for Landscape");
  check (v >= 0 && menus[v].n == 4 && strcmp (menus[v].text[3], "Zoom") == 0,
         "View: Zoom (with its submenu) last");
  check (strstr (logbuf, "menu \"Show grid\" unticked") != NULL,
         "View: an Adjust choice activates the item");
  k = v >= 0 ? find_menu ("View", v + 1) : -1;
  check (k >= 0 && !(menus[k].flags[0] & 1),
         "View: Adjust reopens the menu, Show grid now unticked");

  check (edit_open_after_leave == 1 && edit_title_ok,
         "Edit stays open, its text intact, when the pointer moves off the window onto it");
  check (strstr (logbuf, "button \"Apply\" clicked") != NULL,
         "after a release off the window (over the menu), the next click reaches its button");
  check (nb_h0 > 0 && nb_h1 == nb_h0 && def0 == 1 && def1 == 1,
         "Apply is the default button from the start, and its focus moves nothing (the tabs)");
  printf ("# notebook height %d -> %d after Apply took focus; default style %d -> %d\n", nb_h0, nb_h1, def0, def1);
  check (moved_x0 == 240 + 200 && after_x0 == moved_x0 && after_y1 == moved_y1 && after_y0 == moved_y0,
         "the window stays where the Wimp moved it (Apply clicked, tabs, menus)");
  printf ("# moved to %d,%d (bottom %d); after the tooltip and clicking Apply %d,%d (bottom %d); %d tooltip window(s)\n",
          moved_x0, moved_y1, moved_y0, after_x0, after_y1, after_y0, tooltip_windows);
  check (pages == 3, "the three other tabs open when clicked");
  check (saved[0] && saved[1] && saved[2] && saved[3], "each page drawn and pictured");

  check (c >= 0 && menus[c].n == 4 && strcmp (menus[c].text[2], "Info") == 0 &&
         (menus[c].flags[1] & 2) && (menus[c].icon[3] & (1 << 22)),
         "context menu: dotted line after Clear selection, last entry shaded");
  check (c >= 0 && abs (menus[c].x - (ctx_x - 64)) <= 4 && abs (menus[c].y - ctx_y) <= 4,
         "context menu: opens 64 OS units left of the pointer");
  check (strstr (logbuf, "menu \"Info\"") != NULL, "context menu: choosing Info activates it");

  check (strstr (logbuf, "window closed") != NULL, "the window closes");
  check (strstr (logbuf, "CRITICAL") == NULL && strstr (logbuf, "WARNING") == NULL &&
         strstr (logbuf, "Gtk-") == NULL, "no GTK warnings or criticals in the log");

  printf ("# Wimp menus opened (%d):\n", n_menus);
  for (k = 0; k < n_menus; k++)
    {
      printf ("#   \"%s\" at %d,%d:", menus[k].title, menus[k].x, menus[k].y);
      for (i = 0; i < menus[k].n; i++)
        printf (" [%s%s%s%s]", menus[k].flags[i] & 1 ? "tick " : "", menus[k].text[i],
                menus[k].icon[i] & (1 << 22) ? " shaded" : "", menus[k].flags[i] & 2 ? " ---" : "");
      printf ("\n");
    }
  printf ("# log:\n");
  for (line = strtok (logbuf, "\n"); line; line = strtok (NULL, "\n"))
    printf ("#   %s\n", line);
  printf ("1..%d\n# themetest-fake: %d checks, %d failed\n", n_tests, n_tests, n_failed);
  return n_failed;
}
