/* gtkiconbar-riscos.h - riscos-gtk: the program's icon bar icon (GTK side).
 * LGPL-2.1-or-later. Copyright (C) 2026 Andrew Youll. */
#ifndef __GTK_ICONBAR_RISCOS_H__
#define __GTK_ICONBAR_RISCOS_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

/* From gtk_init (via _gtk_riscos_theme_init): put the icon on the icon bar */
void _gtk_riscos_iconbar_init (GdkDisplay *display);

G_END_DECLS

#endif
