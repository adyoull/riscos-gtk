/* gdkriscosmenu.h - riscos-gtk: Wimp menus for GTK (gtk/gtkmenu-riscos.c).
 * Part of riscos-gtk's GTK backend: LGPL version 2.1 or later. */
#ifndef __GDK_RISCOS_MENU_H__
#define __GDK_RISCOS_MENU_H__

#if !defined (__GDKRISCOS_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkriscos.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

/* Called from the backend's Wimp poll when the open menu is chosen from
 * (selection: Menu_Selection's list of item numbers, ended by -1; adjust:
 * chosen with Adjust) or closed without a choice (selection NULL). The
 * caller must not run GTK code here: queue an idle and call
 * gdk_riscos_display_menu_done() from it (after reopening for Adjust). */
typedef void (*GdkRiscosMenuFunc) (const int *selection, gboolean adjust, gpointer data);

/* Open a Wimp menu block (Wimp_CreateMenu) with its top left at root_x,
 * root_y (GDK root pixels). It must stay valid until closed. */
GDK_AVAILABLE_IN_ALL
void gdk_riscos_display_open_menu   (GdkDisplay *display, gpointer menu_block,
                                     gint root_x, gint root_y,
                                     GdkRiscosMenuFunc func, gpointer data);
/* Reopen it in the same place (after an Adjust choice). */
GDK_AVAILABLE_IN_ALL
void gdk_riscos_display_reopen_menu (GdkDisplay *display);
/* Close it (no callback). */
GDK_AVAILABLE_IN_ALL
void gdk_riscos_display_close_menu  (GdkDisplay *display);
/* A choice has been handled: the backend may poll the Wimp again. */
GDK_AVAILABLE_IN_ALL
void gdk_riscos_display_menu_done   (GdkDisplay *display);
/* Width of a Latin-1 string in the desktop font, OS units. */
GDK_AVAILABLE_IN_ALL
gint gdk_riscos_text_width          (const char *latin1);
/* UTF-8 to the Wimp's alphabet (Latin-1; other characters become '?'). */
GDK_AVAILABLE_IN_ALL
void gdk_riscos_utf8_to_latin1      (const char *utf8, char *out, gint size);

G_END_DECLS

#endif /* __GDK_RISCOS_MENU_H__ */
