/* gtkmenu-riscos.h - riscos-gtk: GtkMenus as Wimp menus.
 * Part of GTK: LGPL version 2.1 or later. */
#ifndef __GTK_MENU_RISCOS_H__
#define __GTK_MENU_RISCOS_H__

#include "gtkmenu.h"

G_BEGIN_DECLS

gboolean _gtk_menu_riscos_popup   (GtkMenu             *menu,
                                   GtkWidget           *parent_menu_shell,
                                   GtkWidget           *parent_menu_item,
                                   GtkMenuPositionFunc  func,
                                   gpointer             data);
void     _gtk_menu_riscos_popdown (GtkMenu *menu);
gboolean _gtk_menu_riscos_is_open (GtkMenu *menu);

G_END_DECLS

#endif
