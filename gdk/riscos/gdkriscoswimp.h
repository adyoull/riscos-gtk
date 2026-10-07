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
GDK_AVAILABLE_IN_ALL
gboolean   gdk_riscos_window_get_size_grip (GdkWindow *window, gint *width, gint *height);
/* Put the grip in the column of the window's vertical scroll bar (x, width
   in the window, pixels); width 0: at the window's right edge */
GDK_AVAILABLE_IN_ALL
void       gdk_riscos_window_set_size_grip_column (GdkWindow *window, gint x, gint width);
/* Called with each event before GTK has it (see gdkeventsource.c) */
typedef void (*GdkRiscosEventWatch) (GdkEvent *event, gpointer data);
GDK_AVAILABLE_IN_ALL
void       gdk_riscos_set_event_watch (GdkRiscosEventWatch watch, gpointer data);

/* A Wimp error box (Wimp_ReportError, OK button) saying message,
 * "Message from" the program. */
GDK_AVAILABLE_IN_ALL
void       gdk_riscos_report_error (const char *message);

/* ---- The icon bar (gdkiconbar-riscos.c, gtk/gtkiconbar-riscos.c) ------
 *
 * Every GTK program gets an icon on the icon bar, as riscos-gtk's other
 * RISC OS programs have (Reel, !FFmpeg, YTDL, NetStats):
 *   Select / Adjust   show the program's window (or make one);
 *   Menu              Info >  (About this program: Name, Purpose, Author,
 *                              Version)
 *                     Choices...  (only if the program has settings)
 *                     Quit
 * Files double-clicked in the Filer (Message_DataOpen, for the types
 * claimed), dropped on the icon, or saved to it by another program are
 * opened; Message_PreQuit with unsaved work asks (Cancel / Discard); a
 * second copy hands its file to the first and goes.
 *
 * By default GTK uses the program's GtkApplication (activate, open, the
 * "preferences" and "quit" actions) or its own menu items (Preferences/
 * Settings/Choices, Quit). A program can set its own handlers, the Info
 * text, the sprite, the file types and its start-up file before
 * gtk_init(). RISCOS_GTK_ICONBAR=0 (or gdk_riscos_iconbar_disable())
 * leaves the icon bar alone.
 */
typedef struct {
  /* Select on the icon: show the program's window (or make a new one) */
  void     (*show)      (gpointer data);
  /* open a file (UnixLib name); scrap: data from another program, saved
     in <Wimp$Scrap> (load it as untitled; it's deleted afterwards).
     Return FALSE if it couldn't be opened. */
  gboolean (*open_file) (const char *unix_path, int filetype, gboolean scrap, gpointer data);
  /* Choices...: the program's settings (NULL: no Choices item) */
  void     (*choices)   (gpointer data);
  /* Quit */
  void     (*quit)      (gpointer data);
  /* TRUE if there is unsaved work (Quit and PreQuit ask first) */
  gboolean (*unsaved)   (gpointer data);
} GdkRiscosIconbarHandlers;

GDK_AVAILABLE_IN_ALL
void     gdk_riscos_iconbar_set_handlers (const GdkRiscosIconbarHandlers *handlers, gpointer data);
GDK_AVAILABLE_IN_ALL
const GdkRiscosIconbarHandlers *gdk_riscos_iconbar_get_handlers (gpointer *data);
/* The Info box: Name, Purpose, Author, Version (UTF-8; NULL = leave) */
GDK_AVAILABLE_IN_ALL
void     gdk_riscos_iconbar_set_info     (const char *name, const char *purpose,
                                          const char *author, const char *version);
/* The icon's sprite in the Wimp pool (default "!" + the program name in
   lower case, else "application") */
GDK_AVAILABLE_IN_ALL
void     gdk_riscos_iconbar_set_sprite   (const char *sprite);
/* Load files of this type when they are double-clicked (Message_DataOpen) */
GDK_AVAILABLE_IN_ALL
void     gdk_riscos_iconbar_add_filetype (int filetype);
/* The file the program was started with (RISC OS name): if a copy is
   already running, it gets the file (Message_DataLoad) and this one goes */
GDK_AVAILABLE_IN_ALL
void     gdk_riscos_iconbar_set_start_file (const char *riscos_path);
/* No icon bar icon for this program (call before gtk_init) */
GDK_AVAILABLE_IN_ALL
void     gdk_riscos_iconbar_disable      (void);
/* A RISC OS question box (Wimp_ReportError, a question, new style): Cancel
   and button. TRUE if button was chosen. */
GDK_AVAILABLE_IN_ALL
gboolean gdk_riscos_ask                  (const char *message, const char *button);

/* For GTK's side (gtk/gtkiconbar-riscos.c). Events come from the Wimp
   poll: queue an idle, don't run GTK code there. */
typedef enum {
  GDK_RISCOS_ICONBAR_SELECT,
  GDK_RISCOS_ICONBAR_MENU,
  GDK_RISCOS_ICONBAR_ADJUST,
  GDK_RISCOS_ICONBAR_OPEN_FILE
} GdkRiscosIconbarEventType;

typedef struct {
  GdkRiscosIconbarEventType type;
  int x, y;                    /* clicks: the pointer, screen OS units */
  const char *path;            /* OPEN_FILE: UnixLib name */
  int filetype;
  gboolean scrap;
} GdkRiscosIconbarEvent;

typedef void (*GdkRiscosIconbarFunc) (GdkRiscosIconbarEvent *event, gpointer data);

GDK_AVAILABLE_IN_ALL
gboolean gdk_riscos_iconbar_create       (GdkDisplay *display, GdkRiscosIconbarFunc func,
                                          gpointer data);
GDK_AVAILABLE_IN_ALL
gboolean gdk_riscos_iconbar_enabled      (void);
/* The Info box, a Wimp window for the menu's Info item (0 if none) */
GDK_AVAILABLE_IN_ALL
gint     gdk_riscos_iconbar_info_window  (void);
/* Where an icon bar menu of n items opens (root pixels): 64 OS units left
   of the click, its top 96 + 44 n above the bottom of the screen */
GDK_AVAILABLE_IN_ALL
void     gdk_riscos_iconbar_menu_position (gint click_x, gint n_items,
                                           gint *root_x, gint *root_y);

/* ---- Printing (gdkprint-riscos.c; GTK's side is gtk/gtkprint-riscos.c) ---
 * Through the printer driver !Printers has set up (the PDriver module). */
typedef struct {
  char name[64];               /* the printer's name (UTF-8) */
  int dpi_x, dpi_y;            /* its resolution */
  gboolean colour;
  double paper_width, paper_height;     /* points */
  double left, bottom, right, top;      /* the margins it can't print in, points */
} GdkRiscosPrinter;

/* FALSE if no printer driver is loaded */
GDK_AVAILABLE_IN_ALL
gboolean         gdk_riscos_printer_info       (GdkRiscosPrinter *printer);
/* Ask !Printers, open the printer, start a job */
GDK_AVAILABLE_IN_ALL
gboolean         gdk_riscos_print_begin        (const char *title, GError **error);
/* Print a page GTK has drawn (@page: a recording surface, in points,
   0,0 the paper's top left) at @dpi, in strips, covering the paper */
GDK_AVAILABLE_IN_ALL
gboolean         gdk_riscos_print_page         (cairo_surface_t *page, double paper_width,
                                                double paper_height, int dpi, int copies,
                                                GError **error);
/* End the job (abort: abandon it) */
GDK_AVAILABLE_IN_ALL
void             gdk_riscos_print_end          (gboolean abort);

G_END_DECLS

#endif
