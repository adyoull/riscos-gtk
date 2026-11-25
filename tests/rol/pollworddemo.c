/*
 * pollworddemo.c - on RISC OS: the program's poll word
 * (gdk_riscos_set_poll_word). A word in the RMA (OS_Module 6) is given to
 * riscos-gtk; a 200 ms timer sets it (as another task would), so the next
 * Wimp poll returns PollWord_NonZero and the function clears it and counts.
 * The window shows the count; after 3 s the word is taken away again and
 * a set word must no longer be seen. Writes its messages to the Log.
 * MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include <stdio.h>

static volatile int *word;
static int wakes, sets, late;
static gint64 set_time, total_us;
static GtkWidget *label;

static void
on_word (volatile int *w, gpointer data)
{
  wakes++;
  total_us += g_get_monotonic_time () - set_time;
  *w = 0;
}

static gboolean
show (gpointer d)
{
  char *s = g_strdup_printf ("set %d, woken %d, after %d us on average, %s; seen after removal %d",
                             sets, wakes, wakes ? (int) (total_us / wakes) : 0,
                             word ? "word given" : "word removed", late);
  gtk_label_set_text (GTK_LABEL (label), s);
  printf ("pollworddemo: %s\n", s);
  g_free (s);
  return G_SOURCE_CONTINUE;
}

static gboolean
set_word (gpointer d)
{
  if (word == NULL)
    return G_SOURCE_REMOVE;
  sets++;
  set_time = g_get_monotonic_time ();
  *word = 1;
  return G_SOURCE_CONTINUE;
}

static gboolean
remove_word (gpointer d)
{
  volatile int *w = word;
  gdk_riscos_set_poll_word (NULL, FALSE, NULL, NULL);
  word = NULL;
  *w = 1;                          /* no longer looked at */
  late = wakes;
  show (NULL);
  return G_SOURCE_REMOVE;
}

static gboolean
check_late (gpointer d)
{
  late = wakes - late;             /* wakes since the removal: must be 0 */
  show (NULL);
  return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
  GtkWidget *win;
  _kernel_swi_regs r;

  r.r[0] = 6;                      /* OS_Module 6: claim RMA */
  r.r[3] = 4;
  if (_kernel_swi (0x1E, &r, &r) != NULL)
    return 1;
  word = (volatile int *) r.r[2];
  *word = 0;
  gdk_riscos_set_poll_word (word, FALSE, on_word, NULL);
  gtk_init (&argc, &argv);
  printf ("pollworddemo: word at %p\n", (void *) word);
  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "Poll word");
  gtk_window_set_default_size (GTK_WINDOW (win), 900, 120);
  label = gtk_label_new ("waiting");
  gtk_container_add (GTK_CONTAINER (win), label);
  g_signal_connect (win, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (win);
  g_timeout_add (200, set_word, NULL);
  g_timeout_add (500, show, NULL);
  g_timeout_add (3000, remove_word, NULL);
  g_timeout_add (4000, check_late, NULL);
  gtk_main ();
  return 0;
}
