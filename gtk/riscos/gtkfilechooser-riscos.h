/* gtkfilechooser-riscos.h - riscos-gtk: GtkFileChooserDialog as RISC OS
 * Save and Open boxes (gtkfilechooser-riscos.c, gtkfilechooserdialog.c).
 * Part of GTK: LGPL version 2.1 or later. */
#ifndef __GTK_FILE_CHOOSER_RISCOS_H__
#define __GTK_FILE_CHOOSER_RISCOS_H__

#include "gtkfilechooserdialog.h"

G_BEGIN_DECLS

/* gtkfilechooser-riscos.c: show dialog as a Save or Open box instead of
   itself; FALSE if it can't be (GTK's own dialogue then). */
gboolean _gtk_file_chooser_riscos_dialog_show (GtkFileChooserDialog *dialog);

/* gtkfilechooserdialog.c: the dialogue's accept response id (its Save or
   Open button: ACCEPT, OK, YES or APPLY), and its response when the box
   closed (accept, or cancelled; deleted: closed without Cancel) */
int  _gtk_file_chooser_dialog_riscos_accept_id (GtkFileChooserDialog *dialog);
void _gtk_file_chooser_dialog_riscos_respond (GtkFileChooserDialog *dialog,
                                              gboolean accept, gboolean deleted);

G_END_DECLS

#endif
