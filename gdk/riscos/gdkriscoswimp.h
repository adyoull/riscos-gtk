/* gdkriscoswimp.h - riscos-gtk: the desktop's Wimp colours and sprites,
 * for the theme (gtk/gtkriscostheme.c). Part of riscos-gtk's GTK backend:
 * LGPL version 2.1 or later. */
#ifndef __GDK_RISCOS_WIMP_H__
#define __GDK_RISCOS_WIMP_H__

#if !defined (__GDKRISCOS_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkriscos.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

/* The 16 Wimp colours now, as 0xRRGGBB. FALSE if the Wimp can't say. */
GDK_AVAILABLE_IN_ALL
gboolean   gdk_riscos_wimp_palette (guint32 rgb[16]);

/* A sprite from the Wimp's sprite pool (e.g. "optoff") as an RGBA pixbuf
 * at screen pixel size, or NULL. */
GDK_AVAILABLE_IN_ALL
GdkPixbuf *gdk_riscos_wimp_sprite  (const char *name);

/* A window border sprite (the Wimp's tool sprites, e.g. "uicon",
 * "vbarmid"; names as in the PRMs) as an RGBA pixbuf, or NULL. */
GDK_AVAILABLE_IN_ALL
GdkPixbuf *gdk_riscos_wimp_tool_sprite (const char *name);

/* M4: RISC OS file transfer (Save boxes, files dropped on windows).
 * Paths: RISC OS form "ADFS::HD4.$.Docs.Report/txt" <-> UnixLib form
 * "/ADFS::HD4/$/Docs/Report.txt". Both return newly allocated strings. */
GDK_AVAILABLE_IN_ALL
char      *gdk_riscos_path_to_unix   (const char *riscos_path);
GDK_AVAILABLE_IN_ALL
char      *gdk_riscos_path_from_unix (const char *unix_path);

/* Called once the receiver of a save drag has replied (DataSaveAck): path
 * is where to save, in RISC OS form; safe is FALSE for <Wimp$Scrap> (on
 * its way to another program). Call gdk_riscos_save_done() after saving. */
typedef void     (*GdkRiscosSaveFunc) (const char *path, gboolean safe, gpointer data);
/* A file (RISC OS path) dropped from the Filer on a window. Return TRUE
 * to take it (the Filer gets DataLoadAck). */
typedef gboolean (*GdkRiscosLoadFunc) (const char *riscos_path, int filetype, gpointer data);

/* Start dragging a file icon: sprite from the Wimp pool (e.g. "file_fff"),
 * box at root pixel x, y, size w x h. */
GDK_AVAILABLE_IN_ALL
void       gdk_riscos_save_drag_begin (GdkDisplay *display, const char *sprite,
                                       int root_x, int root_y, int w, int h);
/* End the drag: offer leafname (RISC OS form) to what is under the
 * pointer. FALSE if nothing to save to (our own window, the background). */
GDK_AVAILABLE_IN_ALL
gboolean   gdk_riscos_save_drop  (GdkDisplay *display, const char *leafname,
                                  int filetype, int size,
                                  GdkRiscosSaveFunc func, gpointer data);
/* After saving: tell the receiver it's there (Message_DataLoad). */
GDK_AVAILABLE_IN_ALL
void       gdk_riscos_save_done  (const char *path);
/* Take files dropped from the Filer on window (NULL func to stop). */
GDK_AVAILABLE_IN_ALL
void       gdk_riscos_window_set_load_func (GdkWindow *window,
                                            GdkRiscosLoadFunc func, gpointer data);

/* Open window (a toplevel) as a transient dialogue box, as RISC OS opens
 * one from a menu (Wimp_CreateMenu): a click elsewhere closes it and GTK
 * gets a delete event. */
GDK_AVAILABLE_IN_ALL
void       gdk_riscos_window_set_transient_dbox (GdkWindow *window, gboolean transient);

/* A Wimp error box (Wimp_ReportError, OK button) saying message,
 * "Message from" the program. */
GDK_AVAILABLE_IN_ALL
void       gdk_riscos_report_error (const char *message);

G_END_DECLS

#endif
