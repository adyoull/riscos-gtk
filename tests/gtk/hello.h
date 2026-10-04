/* hello.h - the M2 test program's widgets, for the QEMU driver. MIT. */
#include <gtk/gtk.h>
extern GtkWidget *hello_window, *hello_button, *hello_label, *hello_entry, *hello_check;
extern int hello_clicks, hello_option;
extern char hello_entry_text[64];   /* saved when the window closes */
int hello_main (int argc, char **argv);
