/*
 * pollword-fake.c - the program's poll word (gdk_riscos_set_poll_word):
 * given to every Wimp poll (R3, mask bit 22, bit 23 for high priority,
 * reason 13 not masked); when another task sets it, the poll returns
 * PollWord_NonZero and riscos-gtk calls the program's function, which
 * clears it; NULL gives polling as before.
 *
 *   pollword-fake APPDIR
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>

#ifndef GDK_RISCOS_POLL_WORD
#error "no GDK_RISCOS_POLL_WORD"
#endif

static int n_tests, n_failed;
static volatile int word;            /* the RMA word in a real program */
static int calls, seen_value, set_at = -1, polls;
static volatile int *seen_word;
static gpointer seen_data;

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
on_word (volatile int *w, gpointer data)
{
  calls++;
  seen_word = w;
  seen_data = data;
  seen_value = *w;
  *w = 0;
}

/* another task sets the word (called as each poll returns) */
static void
hook (int reason)
{
  polls++;
  if (polls == set_at)
    word = 0x55;
}

static void
iterate (int n)
{
  while (n-- > 0)
    g_main_context_iteration (NULL, FALSE);
}

int
main (int argc, char **argv)
{
  const char *appdir = argv[1];
  char *conf = g_build_filename (appdir, "fonts", "fonts.conf", NULL);
  char *share = g_build_filename (appdir, "share", NULL);
  int mask;

  g_setenv ("FONTCONFIG_FILE", conf, TRUE);
  g_setenv ("XDG_DATA_DIRS", share, TRUE);
  fake_set_screen (1280, 720, 0, 5);
  fake_wimp_nulls = 1;
  gdk_riscos_set_poll_word (&word, FALSE, on_word, (gpointer) 0x1234);
  gtk_init (&argc, &argv);
  mask = fake_poll_mask;
  check ((mask & (1 << 22)) && !(mask & (1 << 13)) && fake_poll_r3 == (int) (long) &word,
         "polls during gtk_init scan the word (bit 22, reason 13 allowed, R3 = the word)");

  fake_wimp_hook = hook;
  iterate (3);
  mask = fake_poll_mask;
  printf ("# mask %08X R3 %08X\n", mask, fake_poll_r3);
  check ((mask & (1 << 22)) && !(mask & (1 << 13)) && !(mask & (1 << 23)) &&
         fake_poll_r3 == (int) (long) &word,
         "the main loop's polls scan the word, normal priority");
  check (calls == 0 && fake_pollword_returns == 0, "no call while the word is 0");

  set_at = polls + 1;
  iterate (4);
  printf ("# calls %d returns %d value %X\n", calls, fake_pollword_returns, seen_value);
  check (fake_pollword_returns == 1 && calls == 1, "a set word: reason 13 once, the function called once");
  check (seen_word == &word && seen_data == (gpointer) 0x1234 && seen_value == 0x55,
         "the function gets the word (still set) and its data");
  check (word == 0, "the function cleared the word");

  gdk_riscos_set_poll_word (&word, TRUE, on_word, NULL);
  iterate (2);
  check ((fake_poll_mask & (1 << 23)) && (fake_poll_mask & (1 << 22)), "high priority: bit 23 too");

  gdk_riscos_set_poll_word (NULL, FALSE, NULL, NULL);
  word = 1;
  iterate (3);
  mask = fake_poll_mask;
  check (!(mask & (1 << 22)) && !(mask & (1 << 23)) && (mask & (1 << 13)) && fake_poll_r3 == 0,
         "NULL: no poll word (reason 13 masked, R3 0), as before");
  check (calls == 1 && fake_pollword_returns == 1, "and a set word is no longer seen");
  printf ("1..%d\n", n_tests);
  return n_failed;
}
