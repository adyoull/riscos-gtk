/*
 * frontend.c - riscos-gtk: a native front end on RISC OS. A Wimp window
 * of the program's own (title, close, back, toggle size, adjust size)
 * with a toolbar strip at its top, drawn by the Wimp, and a GtkWindow
 * nested below it filling the rest (gdk_riscos_window_set_parent). The
 * GTK window follows the parent's moves and resizes (its nesting flags)
 * and is told where it went (gdk_riscos_window_parent_moved).
 *   frontend [test]  test: after 6 s it asks the Wimp to move its window
 *                    300 OS units right and 100 down and make it 200
 *                    wider (an Open_Window_Request to itself), as a drag
 *                    would. The GTK window's place goes to stderr.
 * MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include <stdio.h>
#include <string.h>

#define TOOLBAR 88                       /* OS units */
/* nesting flags (R4): the child's left and bottom edges follow the
   parent's visible area's left and bottom (1), its right and top edges
   the visible area's right and top (2) */
#define NEST ((1u << 16) | (1u << 18) | (2u << 20) | (2u << 22))

static int own;
static GtkWidget *win;
static GtkWidget *label;

static gboolean
moved (gpointer data)
{
  gint x, y;
  char text[96];

  gdk_riscos_window_parent_moved (gtk_widget_get_window (win));
  gdk_window_get_origin (gtk_widget_get_window (win), &x, &y);
  snprintf (text, sizeof text, "Nested in Wimp window &%X at %d, %d (%d x %d)", own, x, y,
            gtk_widget_get_allocated_width (win), gtk_widget_get_allocated_height (win));
  gtk_label_set_text (GTK_LABEL (label), text);
  g_printerr ("frontend: %s\n", text);
  return G_SOURCE_REMOVE;
}

/* the child: the parent's work area below the toolbar */
static void
child_box (const int *parent, int *box)
{
  box[0] = parent[1];
  box[1] = parent[2];
  box[2] = parent[3];
  box[3] = parent[4] - TOOLBAR;
}

static gboolean
filter (int reason, int *block, gpointer data)
{
  _kernel_swi_regs r;

  if (reason == 2 && block[0] == own)    /* Open_Window_Request */
    {
      r.r[1] = (int) block;
      _kernel_swi (0x400C5, &r, &r);     /* Wimp_OpenWindow: the child goes with it */
      g_idle_add (moved, NULL);
      return TRUE;
    }
  if (reason == 3 && block[0] == own)    /* Close_Window_Request */
    {
      g_idle_add ((GSourceFunc) gtk_main_quit, NULL);
      return TRUE;
    }
  return FALSE;
}

/* as if the user dragged it: an Open_Window_Request to itself */
static gboolean
drag (gpointer data)
{
  int b[9];
  _kernel_swi_regs r;

  b[0] = own;
  r.r[1] = (int) b;
  _kernel_swi (0x400CB, &r, &r);         /* Wimp_GetWindowState */
  b[1] += 300; b[3] += 500; b[2] -= 100; b[4] -= 100;
  r.r[0] = 2;
  r.r[1] = (int) b;
  r.r[2] = own;
  _kernel_swi (0x400E7, &r, &r);         /* Wimp_SendMessage */
  return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
  int w[23], state[9], box[4];
  _kernel_swi_regs r;
  GtkWidget *vbox, *entry, *button;
  GdkWindow *gw;

  gdk_riscos_set_wimp_filter (filter, NULL);
  gtk_init (&argc, &argv);

  /* the program's window: Wimp-drawn (auto-redraw), title "Frontend" */
  memset (w, 0, sizeof w);
  w[0] = 300; w[1] = 300; w[2] = 1300; w[3] = 1100;
  w[6] = -1;
  w[7] = (int) ((1u << 31) | (1u << 1) | (1u << 4) | (1u << 24) | (1u << 25) |
                (1u << 26) | (1u << 27) | (1u << 29));
  ((unsigned char *) &w[8])[0] = 7;      /* title foreground */
  ((unsigned char *) &w[8])[1] = 2;      /* title background */
  ((unsigned char *) &w[8])[2] = 7;      /* work area foreground */
  ((unsigned char *) &w[8])[3] = 1;      /* work area background */
  ((unsigned char *) &w[9])[0] = 3;
  ((unsigned char *) &w[9])[1] = 1;
  ((unsigned char *) &w[9])[2] = 12;
  w[10] = 0; w[11] = -4000; w[12] = 4000; w[13] = 0;   /* extent */
  w[14] = 0x3D;                          /* title: text, border, centred */
  w[15] = 0;
  w[16] = 1;                             /* the Wimp sprite area */
  w[17] = (200) | (200 << 16);           /* minimum size */
  strcpy ((char *) &w[18], "Frontend");
  w[21] = 0;                             /* no icons */
  r.r[1] = (int) w;
  if (_kernel_swi (0x400C1, &r, &r) != NULL)   /* Wimp_CreateWindow */
    return 1;
  own = r.r[0];
  state[0] = own;
  state[1] = 300; state[2] = 300; state[3] = 1300; state[4] = 1100;
  state[5] = 0; state[6] = 0; state[7] = -1;
  r.r[1] = (int) state;
  _kernel_swi (0x400C5, &r, &r);         /* Wimp_OpenWindow */

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_decorated (GTK_WINDOW (win), FALSE);
  child_box (state, box);
  /* GDK pixels: the desktop's OS units per pixel (2 in most modes) */
  gtk_window_set_default_size (GTK_WINDOW (win), (box[2] - box[0]) / 2, (box[3] - box[1]) / 2);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  label = gtk_label_new ("Nested");
  entry = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (entry), "A GtkEntry in a nested window");
  button = gtk_button_new_with_label ("A GtkButton");
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (win), vbox);
  gtk_window_move (GTK_WINDOW (win), box[0] / 2, gdk_screen_height () - box[3] / 2);
  gtk_widget_realize (win);
  gw = gtk_widget_get_window (win);
  gdk_riscos_window_set_parent (gw, own, NEST);
  gtk_widget_show_all (win);
  g_idle_add (moved, NULL);
  if (argc > 1 && strcmp (argv[1], "test") == 0)
    g_timeout_add (6000, drag, NULL);
  gtk_main ();
  return 0;
}
