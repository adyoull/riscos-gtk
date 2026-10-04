/* themetest.h - the theme test's widgets, for its drivers. MIT. */
#include <gtk/gtk.h>
#include <stdarg.h>
#include <stdlib.h>
extern GtkWidget *tt_window, *tt_notebook, *tt_status, *tt_combo, *tt_menubar, *tt_context_area;
extern int tt_events;
int themetest_main (int argc, char **argv);
