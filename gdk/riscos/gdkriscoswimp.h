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

G_END_DECLS

#endif
