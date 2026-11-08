/* gtkriscostheme.h - riscos-gtk: the theme takes the desktop's Wimp colours
 * and sprites (gtkriscostheme.c). Part of GTK: LGPL version 2.1 or later. */
#ifndef __GTK_RISCOS_THEME_H__
#define __GTK_RISCOS_THEME_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

void _gtk_riscos_theme_init (GdkDisplay *display);
/* gtkiconbar-riscos.c: TRUE if the program should stay on the icon bar
   when its last window closes (it has an icon there), instead of quitting */
gboolean _gtk_riscos_iconbar_keep_running (void);

G_END_DECLS

#endif
