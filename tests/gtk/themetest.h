/* themetest.h - the theme test's widgets, for its drivers. MIT. */
#include <gtk/gtk.h>
#include <stdarg.h>
#include <stdlib.h>
extern GtkWidget *tt_window, *tt_notebook, *tt_status, *tt_combo, *tt_menubar, *tt_context_area, *tt_apply;
extern GtkWidget *tt_entry, *tt_entry_empty;     /* the Writable and Empty fields */
extern GtkWidget *tt_save_button, *tt_open_button, *tt_drop_area, *tt_file_drag;
#define TT_DRAG_FILE_TEXT "Item,Price\nTea,1.20\nCake,2.50\n"     /* tt_file_drag's file, Prices.csv */
extern GtkWidget *tt_save_dialog_button, *tt_open_dialog_button;  /* GtkFileChooserDialog */   /* Files (Dialogues page) */
extern char tt_last_file[256];                   /* the file last chosen or dropped */
extern int tt_events;
int themetest_main (int argc, char **argv);
