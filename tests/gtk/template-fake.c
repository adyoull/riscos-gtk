/*
 * template-fake.c - runs the devkit's template program (sdk/template/
 * hello.c, built with sdk/include/riscos-gtk/app.h) on the fake RISC OS
 * under qemu-arm-aligntrap, as the devkit's users will first run it:
 * started with a Text file, it shows it; the icon bar icon is the
 * template's; Select on "Say hello" adds a line; Menu over the window
 * opens the window menu; Quit (the GtkApplication's "quit" action, as the
 * icon bar menu sends it) ends it.
 *
 *   template-fake APPDIR OUTDIR
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#define main hello_template_main
#include "../../sdk/template/hello.c"
#undef main

#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static int n_tests, n_failed;
static int stage, menus_before = -1, menus_after = -1, quit_sent;
static char title_start[256], title_after[256], text_after[512];

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

static GtkWidget *
find_type (GtkWidget *w, GType type)
{
  GtkWidget *found = NULL;
  GList *l, *kids;

  if (G_TYPE_CHECK_INSTANCE_TYPE (w, type))
    return w;
  if (!GTK_IS_CONTAINER (w))
    return NULL;
  kids = gtk_container_get_children (GTK_CONTAINER (w));
  for (l = kids; l && !found; l = l->next)
    found = find_type (l->data, type);
  g_list_free (kids);
  return found;
}

static void
centre_in_window (GtkWidget *w, int *x, int *y)
{
  GtkAllocation a;
  gtk_widget_get_allocation (w, &a);
  gtk_widget_translate_coordinates (w, current->window, a.width / 2, a.height / 2, x, y);
}

static char *
buffer_text (void)
{
  GtkTextIter a, b;
  gtk_text_buffer_get_bounds (current->buffer, &a, &b);
  return gtk_text_buffer_get_text (current->buffer, &a, &b, FALSE);
}

static const char *shot;

/* the whole fake screen, as a PPM, at the screen's own pixels */
static void
save_screen (const char *path)
{
  FILE *f = fopen (path, "wb");
  int x, y, w = fake_screen.w, h = fake_screen.h;
  if (!f)
    return;
  fprintf (f, "P6\n%d %d\n255\n", w, h);
  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      {
        unsigned int p = fake_bank_pixel (fake_screen.display_bank, x, y);
        fputc (p & 0xFF, f);
        fputc ((p >> 8) & 0xFF, f);
        fputc ((p >> 16) & 0xFF, f);
      }
  fclose (f);
}

static void
hook (int reason)
{
  int x, y, i;

  if (stage == 0 && current && gtk_widget_get_mapped (current->window) && fake_plots > 0 &&
      current->path != NULL)
    {
      GtkWidget *button = find_type (current->window, GTK_TYPE_BUTTON);
      GtkWidget *view = find_type (current->window, GTK_TYPE_TEXT_VIEW);
      stage = 1;
      if (fake_wimp_title)
        g_strlcpy (title_start, fake_wimp_title, sizeof title_start);
      centre_in_window (button, &x, &y);
      add (FAKE_MOVE, x, y, 0);
      add (6, x, y, 4);                 /* Select on "Say hello" */
      add (FAKE_NULL, 0, 0, 0);
      add (FAKE_RELEASE, 0, 0, 0);
      for (i = 0; i < 10; i++)
        add (FAKE_NULL, 0, 0, 0);
      centre_in_window (view, &x, &y);
      add (FAKE_MOVE, x, y, 0);
      add (FAKE_BUTTONS, 2, 0, 0);
      add (6, x, y, 2);                 /* Menu over the text */
      add (FAKE_RELEASE, 0, 0, 0);
      for (i = 0; i < 10; i++)
        add (FAKE_NULL, 0, 0, 0);
      for (i = 0; i < 5; i++)
        add (FAKE_SLEEP, 20, 0, 0);
      for (i = 0; i < 400; i++)         /* room for the quit below */
        add (FAKE_NULL, 0, 0, 0);
    }
  else if (stage == 1 && menus_before < 0)
    menus_before = fake_menus_opened;
  else if (stage == 1 && fake_wimp_script_pos () > 25)
    {
      char *t = buffer_text ();
      stage = 2;
      menus_after = fake_menus_opened;
      g_strlcpy (text_after, t, sizeof text_after);
      g_free (t);
      if (fake_wimp_title)
        g_strlcpy (title_after, fake_wimp_title, sizeof title_after);
      if (shot)
        save_screen (shot);
      /* the icon bar menu's Quit: the GtkApplication's "quit" action
         (nothing unsaved is asked about here: iconbar_unsaved is TRUE, but
         the action itself doesn't ask) */
      g_action_group_activate_action (G_ACTION_GROUP (app), "quit", NULL);
      quit_sent = 1;
    }
}

/* the Wimp pool has the template's sprite (its !Boot loads !Sprites) */
static const int *
pool_lookup (const char *name)
{
  static int spr[12] = { 48, 0, 0, 0, 0, 0, 0, 31, 44, 44, 0x301680B5, 0 };
  if (strcmp (name, "!hello") != 0)
    return NULL;
  memcpy (&spr[1], "!hello\0\0\0\0\0\0", 12);
  return spr;
}

int
main (int argc, char **argv)
{
  const char *appdir = argc > 1 ? argv[1] : ".";
  const char *outdir = argc > 2 ? argv[2] : ".";
  char *note = g_build_filename (outdir, "note.txt", NULL);
  char *conf = g_build_filename (appdir, "fonts", "fonts.conf", NULL);
  char *share = g_build_filename (appdir, "share", NULL);
  char *hargv[3];
  int status;

  shot = g_build_filename (outdir, g_getenv ("FAKE_EIG") && atoi (g_getenv ("FAKE_EIG")) == 0
                                   ? "template-eig0.ppm" : "template.ppm", NULL);
  g_file_set_contents (note, "A note from the Filer.\n", -1, NULL);
  /* as the application's own folder would give */
  g_setenv ("FONTCONFIG_FILE", conf, TRUE);
  g_setenv ("XDG_DATA_DIRS", share, TRUE);
  g_setenv ("RISCOS_GTK_NO_LOG", "1", TRUE);
  fake_set_screen (1280, 720, 0, 5);
  fake_sprite_plot_converts = 0;
  fake_wimp_nulls = 10;
  fake_wimp_hook = hook;
  fake_pool_lookup = pool_lookup;

  hargv[0] = argv[0]; hargv[1] = note; hargv[2] = NULL;
  status = hello_template_main (2, hargv);

  printf ("# title at start \"%s\", after \"%s\"\n", title_start, title_after);
  check (stage >= 1, "started with a file: its window shown and drawn");
  check (strcmp (title_start, "note.txt - Hello") == 0, "the window's title names the file");
  check (strncmp (text_after, "A note from the Filer.\n", 23) == 0, "the file was loaded (GtkApplication open)");
  check (strstr (text_after, "Hello from GTK on RISC OS!") != NULL, "Select on \"Say hello\" added its line");
  check (strcmp (title_after, "note.txt * - Hello") == 0, "the title shows unsaved changes");
  printf ("# icon bar: %d icon(s), sprite \"%s\"\n", fake_iconbar_icons, fake_iconbar_sprite);
  check (fake_iconbar_icons == 1 && strcmp (fake_iconbar_sprite, "!hello") == 0,
         "an icon bar icon with the template's sprite (!hello)");
  printf ("# menus opened: %d before, %d after\n", menus_before, menus_after);
  check (menus_after > menus_before && menus_before >= 0, "Menu over the window opened a Wimp menu");
  check (quit_sent && status == 0, "the \"quit\" action ended the program (status 0)");
  check (g_strcmp0 (g_get_prgname (), "Hello") == 0, "riscos_gtk_app_init named the program");
  printf ("1..%d\n", n_tests);
  g_free (note); g_free (conf); g_free (share);
  return n_failed;
}
