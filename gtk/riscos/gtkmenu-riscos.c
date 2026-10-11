/*
 * gtkmenu-riscos.c - riscos-gtk: GtkMenus shown as RISC OS Wimp menus.
 *
 * Copied into GTK's gtk/ folder by riscos-gtk's build; gtkmenu.c calls
 * _gtk_menu_riscos_popup () at the start of gtk_menu_popup_internal () and
 * _gtk_menu_riscos_popdown () from gtk_menu_popdown () when the display is
 * the RISC OS one (patches/gtk+3.0-3.24.41-riscos.diff).
 *
 * Instead of GTK's own pop-up window, the menu's items become a Wimp menu
 * block: text in the Wimp's alphabet, keyboard short-cuts in a column,
 * ticks for check and radio items, shaded items, dotted lines for
 * separators, submenus as Wimp submenus. It opens with Wimp_CreateMenu:
 * below a menu bar title, to the right of a pop-up (a combo box, as a
 * Toolbox pop-up does), or at the pointer (64 OS units to its left, the
 * RISC OS convention). The Wimp closes it on a click elsewhere or Escape.
 * A choice activates the GtkMenuItem (from an idle callback); a choice
 * with Adjust reopens the menu with the ticks brought up to date.
 *
 * Written for riscos-gtk; part of GTK, so under GTK's licence:
 * LGPL version 2.1 or later.
 */
#include "config.h"

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include "gtkmenu.h"
#include "gtkmenuprivate.h"
#include "gtkmenushellprivate.h"
#include "gtkmenuitem.h"
#include "gtkmenubar.h"
#include "gtkcheckmenuitem.h"
#include "gtkseparatormenuitem.h"
#include "deprecated/gtktearoffmenuitem.h"
#include "gtkaccellabel.h"
#include "gtkaccelgroup.h"
#include "gtklabel.h"
#include "gtkcombobox.h"
#include "gtktogglebutton.h"
#include "gtkbox.h"
#include "gtkmain.h"
#include "gtktextview.h"
#include "gtkcellview.h"
#include "gtkcomboboxtext.h"
#include "gtkentry.h"
#include "gtktreeview.h"
#include "gtkiconview.h"
#include "gtklistbox.h"
#include "gtkflowbox.h"
#include "gtkbutton.h"
#include "gtkdialog.h"
#include "gtkmenu-riscos.h"

#include "riscos/gdkriscos.h"

#include <string.h>

#define ITEM_HEIGHT 44            /* OS units, as the Wimp's own menus */
#define TEXT_MAX    256

typedef struct
{
  int *block;                     /* the Wimp menu block */
  int n;                          /* items */
  GtkWidget **items;              /* the GtkMenuItem for each Wimp item */
  int *sub;                       /* index into state->menus of its submenu, or -1 */
} WimpMenu;

typedef struct
{
  GtkMenuShell *menu;             /* a GtkMenu, or a menu bar for the window menu */
  GtkWidget *parent_shell;        /* a menu bar, or NULL */
  GtkWidget *combo;               /* a combo box the menu belongs to, or NULL */
  GPtrArray *menus;               /* WimpMenu *; [0] is the top */
  GPtrArray *mem;                 /* text buffers and blocks to free */
  int selection[16];
  gboolean adjust, closed, handling;
} MenuState;

static MenuState *current;
static guint menus_opened;           /* Wimp menus opened (event_handler) */

/* *Set RISCOS_GTK_MENU_DEBUG 1: trace menus to stderr (the program's log) */
static void
trace (const char *what, MenuState *st)
{
  static int on = -1;
  if (on < 0)
    on = g_getenv ("RISCOS_GTK_MENU_DEBUG") != NULL;
  if (on)
    {
      GdkEvent *ev = gtk_get_current_event ();
      g_printerr ("riscos-gtk menu: %s %p (menu %p%s%s; event %d; bar selection %p)\n", what, (void *) st,
                  st ? (void *) st->menu : NULL,
                  st && st->parent_shell ? ", menu bar" : "", st && st->combo ? ", pop-up" : "",
                  ev ? (int) ev->type : -1,
                  st && st->parent_shell ? (void *) gtk_menu_shell_get_selected_item (GTK_MENU_SHELL (st->parent_shell)) : NULL);
      if (ev)
        gdk_event_free (ev);
    }
}

/* ---- text ---- */

static GtkWidget *
find_label (GtkWidget *w)
{
  GtkWidget *found = NULL;
  GList *l, *kids;

  if (GTK_IS_LABEL (w))
    return w;
  if (!GTK_IS_CONTAINER (w))
    return NULL;
  kids = gtk_container_get_children (GTK_CONTAINER (w));
  for (l = kids; l && !found; l = l->next)
    found = find_label (l->data);
  g_list_free (kids);
  return found;
}

static GtkWidget *
find_cell_view (GtkWidget *w)
{
  GtkWidget *found = NULL;
  GList *l, *kids;

  if (GTK_IS_CELL_VIEW (w))
    return w;
  if (!GTK_IS_CONTAINER (w))
    return NULL;
  kids = gtk_container_get_children (GTK_CONTAINER (w));
  for (l = kids; l && !found; l = l->next)
    found = find_cell_view (l->data);
  g_list_free (kids);
  return found;
}

/* An item's text: its label, or for a combo box's items (a GtkCellView)
   the row's text column (GtkComboBoxText: column 0; else the first
   string column). Returned in UTF-8; free it. */
static gchar *
item_text (GtkWidget *item)
{
  GtkWidget *label = find_label (item), *cv;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gchar *text = NULL;
  int c;

  if (label)
    return g_strdup (gtk_label_get_text (GTK_LABEL (label)));
  if (!(cv = find_cell_view (item)) || !(model = gtk_cell_view_get_model (GTK_CELL_VIEW (cv))) ||
      !(path = gtk_cell_view_get_displayed_row (GTK_CELL_VIEW (cv))))
    return g_strdup ("");
  if (gtk_tree_model_get_iter (model, &iter, path))
    for (c = 0; c < gtk_tree_model_get_n_columns (model) && text == NULL; c++)
      if (gtk_tree_model_get_column_type (model, c) == G_TYPE_STRING)
        gtk_tree_model_get (model, &iter, c, &text, -1);
  gtk_tree_path_free (path);
  return text ? text : g_strdup ("");
}

static gboolean
find_closure (GtkAccelKey *key, GClosure *closure, gpointer data)
{
  return closure == data;
}

/* The item's keyboard short-cut in RISC OS style ("^O", "F3", "Shift-F3") */
static void
shortcut_text (GtkWidget *label, char *out, int size)
{
  guint key = 0;
  GdkModifierType mods = 0;
  gchar *name;

  out[0] = '\0';
  if (!GTK_IS_ACCEL_LABEL (label))
    return;
  gtk_accel_label_get_accel (GTK_ACCEL_LABEL (label), &key, &mods);
  if (key == 0)
    {
      GClosure *closure = NULL;
      g_object_get (label, "accel-closure", &closure, NULL);
      if (closure)
        {
          GtkAccelGroup *group = gtk_accel_group_from_accel_closure (closure);
          GtkAccelKey *k = group ? gtk_accel_group_find (group, find_closure, closure) : NULL;
          if (k)
            {
              key = k->accel_key;
              mods = k->accel_mods;
            }
          g_closure_unref (closure);
        }
    }
  if (key == 0)
    return;
  name = gdk_keyval_name (gdk_keyval_to_upper (key)) ? g_strdup (gdk_keyval_name (gdk_keyval_to_upper (key))) : NULL;
  if (name == NULL)
    return;
  g_snprintf (out, size, "%s%s%s%s",
              (mods & GDK_SHIFT_MASK) ? "Shift-" : "",
              (mods & GDK_MOD1_MASK) ? "Alt-" : "",
              (mods & GDK_CONTROL_MASK) ? "^" : "",
              name);
  g_free (name);
}

static gpointer
keep (MenuState *st, gpointer mem)
{
  g_ptr_array_add (st->mem, mem);
  return mem;
}

/* ---- building the Wimp menu ---- */

static gboolean
item_ticked (GtkWidget *item)
{
  return GTK_IS_CHECK_MENU_ITEM (item) &&
         gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item));
}

static int build (MenuState *st, GtkMenuShell *menu, const char *title_utf8);

/* menu's items as a Wimp menu; extra_menu (if not NULL) adds a first
   item, extra_label, leading to it as a submenu (the window menu with a
   context menu in it) */
static int
build_with (MenuState *st, GtkMenuShell *menu, const char *title_utf8,
            GtkMenuShell *extra_menu, const char *extra_label)
{
  GList *kids = gtk_container_get_children (GTK_CONTAINER (menu)), *l;
  GPtrArray *items = g_ptr_array_new ();
  GArray *dotted = g_array_new (FALSE, TRUE, sizeof (gboolean));
  WimpMenu *wm;
  char **label_text, **key_text;
  int i, n, index, label_w = 0, key_w = 0, space_w, width, title_w;
  char *title;
  int *block;

  for (l = kids; l; l = l->next)
    {
      GtkWidget *w = l->data;
      gboolean f = TRUE;
      if (!GTK_IS_MENU_ITEM (w) || !gtk_widget_get_visible (w) || GTK_IS_TEAROFF_MENU_ITEM (w))
        continue;
      if (GTK_IS_SEPARATOR_MENU_ITEM (w))
        {
          if (items->len > 0)
            g_array_index (dotted, gboolean, items->len - 1) = TRUE;
          continue;
        }
      g_ptr_array_add (items, w);
      g_array_append_val (dotted, f);
      g_array_index (dotted, gboolean, items->len - 1) = FALSE;
    }
  g_list_free (kids);
  if (extra_menu)
    {
      gboolean dot = TRUE;          /* a dotted line under it */
      g_ptr_array_insert (items, 0, extra_menu);
      g_array_insert_val (dotted, 0, dot);
    }
  n = items->len;
  if (n == 0)
    {
      g_ptr_array_free (items, TRUE);
      g_array_free (dotted, TRUE);
      return -1;
    }

  wm = g_new0 (WimpMenu, 1);
  wm->n = n;
  wm->items = g_new0 (GtkWidget *, n);
  wm->sub = g_new (int, n);
  index = st->menus->len;
  g_ptr_array_add (st->menus, wm);

  /* texts and widths */
  label_text = g_new0 (char *, n);
  key_text = g_new0 (char *, n);
  space_w = gdk_riscos_text_width (" ");
  if (space_w <= 0)
    space_w = 16;
  for (i = 0; i < n; i++)
    {
      GtkWidget *item = g_ptr_array_index (items, i);
      char buf[TEXT_MAX];
      wm->items[i] = g_object_ref (item);
      if (extra_menu && i == 0)
        {
          gdk_riscos_utf8_to_latin1 (extra_label, buf, sizeof buf);
          label_text[i] = g_strdup (buf);
          key_text[i] = g_strdup ("");
        }
      else
        {
          GtkWidget *label = find_label (item);
          gchar *utf8 = item_text (item);
          gdk_riscos_utf8_to_latin1 (utf8, buf, sizeof buf);
          g_free (utf8);
          label_text[i] = g_strdup (buf);
          shortcut_text (label, buf, sizeof buf);
          key_text[i] = g_strdup (buf);
        }
      label_w = MAX (label_w, gdk_riscos_text_width (label_text[i]));
      if (key_text[i][0])
        key_w = MAX (key_w, gdk_riscos_text_width (key_text[i]));
    }

  title = keep (st, g_malloc (TEXT_MAX));
  gdk_riscos_utf8_to_latin1 (title_utf8 ? title_utf8 : "", title, TEXT_MAX);
  title_w = gdk_riscos_text_width (title);

  /* short-cuts in a column two spaces after the longest label */
  width = label_w + (key_w ? 2 * space_w + key_w : 0) + 24;
  width = MAX (width, title_w + 32);

  block = keep (st, g_malloc0 (28 + 24 * n));
  wm->block = block;
  block[0] = (int) title;          /* indirected title (flag in item 0) */
  block[1] = -1;
  block[2] = TEXT_MAX;
  ((unsigned char *) block)[12] = 7;   /* title foreground */
  ((unsigned char *) block)[13] = 2;   /* title background */
  ((unsigned char *) block)[14] = 7;   /* work area foreground */
  ((unsigned char *) block)[15] = 0;   /* work area background */
  block[4] = width;
  block[5] = ITEM_HEIGHT;
  block[6] = 0;

  for (i = 0; i < n; i++)
    {
      GtkWidget *item = wm->items[i];
      GtkWidget *submenu = extra_menu && i == 0 ? GTK_WIDGET (extra_menu)
                                                : gtk_menu_item_get_submenu (GTK_MENU_ITEM (item));
      int *it = block + 7 + 6 * i;
      char *text;

      if (key_text[i][0])
        {
          int pad = label_w + 2 * space_w - gdk_riscos_text_width (label_text[i]);
          int spaces = MAX (2, (pad + space_w - 1) / space_w);
          text = g_strdup_printf ("%s%*s%s", label_text[i], spaces, "", key_text[i]);
        }
      else
        text = g_strdup (label_text[i]);
      keep (st, text);

      it[0] = (item_ticked (item) ? 1 : 0) |
              (g_array_index (dotted, gboolean, i) && i < n - 1 ? 2 : 0) |
              (i == n - 1 ? 0x80 : 0) | (i == 0 ? 0x100 : 0);
      it[1] = -1;
      it[2] = 0x07000121 | (gtk_widget_is_sensitive (item) ? 0 : (1 << 22));
      it[3] = (int) text;
      it[4] = -1;
      it[5] = strlen (text) + 1;
      wm->sub[i] = -1;
      if (GTK_IS_MENU (submenu))
        {
          gchar *sub_title = extra_menu && i == 0 ? g_strdup (extra_label) : item_text (item);
          int child = build (st, GTK_MENU_SHELL (submenu), sub_title);
          g_free (sub_title);
          if (child >= 0)
            {
              wm->sub[i] = child;
              it[1] = (int) ((WimpMenu *) g_ptr_array_index (st->menus, child))->block;
            }
        }
    }

  for (i = 0; i < n; i++)
    {
      g_free (label_text[i]);
      g_free (key_text[i]);
    }
  g_free (label_text);
  g_free (key_text);
  g_ptr_array_free (items, TRUE);
  g_array_free (dotted, TRUE);
  return index;
}

static int
build (MenuState *st, GtkMenuShell *menu, const char *title_utf8)
{
  return build_with (st, menu, title_utf8, NULL, NULL);
}

/* Bring ticks and shading up to date (before reopening after Adjust) */
static void
refresh (MenuState *st)
{
  guint m;
  int i;

  for (m = 0; m < st->menus->len; m++)
    {
      WimpMenu *wm = g_ptr_array_index (st->menus, m);
      for (i = 0; i < wm->n; i++)
        {
          int *it = wm->block + 7 + 6 * i;
          it[0] = (it[0] & ~1) | (item_ticked (wm->items[i]) ? 1 : 0);
          it[2] = (it[2] & ~(1 << 22)) | (gtk_widget_is_sensitive (wm->items[i]) ? 0 : (1 << 22));
        }
    }
}

static void state_free_now (MenuState *st);

/* Menu memory is freed late: the Wimp may still be drawing a menu tree we
   have replaced or closed (it acts on Wimp_CreateMenu when it next
   polls), and freed text showed as garbage on the Pi. Keep the last few. */
#define KEEP_MENUS 8
static MenuState *retired[KEEP_MENUS];
static int retired_next;

static void
state_free (MenuState *st)
{
  if (retired[retired_next])
    state_free_now (retired[retired_next]);
  retired[retired_next] = st;
  retired_next = (retired_next + 1) % KEEP_MENUS;
}

static void
state_free_now (MenuState *st)
{
  guint m;
  int i;

  for (m = 0; m < st->menus->len; m++)
    {
      WimpMenu *wm = g_ptr_array_index (st->menus, m);
      for (i = 0; i < wm->n; i++)
        g_object_unref (wm->items[i]);
      g_free (wm->items);
      g_free (wm->sub);
      g_free (wm);
    }
  g_ptr_array_free (st->menus, TRUE);
  g_ptr_array_free (st->mem, TRUE);
  g_object_unref (st->menu);
  if (st->parent_shell)
    g_object_unref (st->parent_shell);
  if (st->combo)
    g_object_unref (st->combo);
  g_free (st);
}

/* The menu has gone (chosen from, or closed): tell GTK as its own pop-up
   would have done. */
static void
finish (MenuState *st)
{
  trace ("finish", st);
  if (current == st)
    current = NULL;
  if (GTK_IS_MENU (st->menu))
    g_signal_emit_by_name (st->menu, "deactivate");
  if (st->parent_shell && GTK_IS_MENU_SHELL (st->parent_shell))
    gtk_menu_shell_deactivate (GTK_MENU_SHELL (st->parent_shell));
  trace ("finished", st);
  if (st->combo)
    {
      /* GtkComboBox pressed its button to open the menu; release it */
      GList *kids = gtk_container_get_children (GTK_CONTAINER (st->combo)), *l;
      for (l = kids; l; l = l->next)
        {
          GtkWidget *b = l->data;
          if (GTK_IS_BOX (b))
            {
              GList *k2 = gtk_container_get_children (GTK_CONTAINER (b)), *l2;
              for (l2 = k2; l2; l2 = l2->next)
                if (GTK_IS_TOGGLE_BUTTON (l2->data))
                  b = l2->data;
              g_list_free (k2);
            }
          if (GTK_IS_TOGGLE_BUTTON (b) && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (b)))
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (b), FALSE);
        }
      g_list_free (kids);
    }
  state_free (st);
}

static gboolean
handle (gpointer data)
{
  MenuState *st = data;
  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (st->menu));
  WimpMenu *wm;
  GtkWidget *chosen = NULL;
  int k;

  if (st->closed)
    {
      finish (st);
      gdk_riscos_display_menu_done (display);
      return FALSE;
    }
  /* follow the selection down the submenus */
  wm = g_ptr_array_index (st->menus, 0);
  for (k = 0; k < 15 && st->selection[k] >= 0; k++)
    {
      int i = st->selection[k];
      if (i >= wm->n)
        break;
      if (st->selection[k + 1] >= 0 && wm->sub[i] >= 0)
        wm = g_ptr_array_index (st->menus, wm->sub[i]);
      else
        {
          if (wm->sub[i] < 0)        /* an item with a submenu: ignored */
            chosen = wm->items[i];
          break;
        }
    }
  st->handling = TRUE;
  if (chosen && gtk_widget_is_sensitive (chosen))
    {
      g_object_ref (chosen);
      gtk_menu_item_activate (GTK_MENU_ITEM (chosen));
      g_object_unref (chosen);
    }
  st->handling = FALSE;
  if (current == st && st->adjust)
    {
      refresh (st);
      gdk_riscos_display_reopen_menu (display);
    }
  else if (current == st)
    {
      gdk_riscos_display_close_menu (display);
      finish (st);
    }
  else
    state_free (st);                 /* replaced or popped down meanwhile */
  gdk_riscos_display_menu_done (display);
  return FALSE;
}

/* From the backend's Wimp poll: no GTK calls here */
static void
menu_event (const int *selection, gboolean adjust, gpointer data)
{
  MenuState *st = data;
  int k;

  trace (selection ? (adjust ? "chosen (Adjust)" : "chosen") : "closed by the Wimp", st);
  if (selection == NULL)
    st->closed = TRUE;
  else
    {
      for (k = 0; k < 15 && selection[k] >= 0; k++)
        st->selection[k] = selection[k];
      st->selection[k] = -1;
      st->adjust = adjust;
    }
  g_idle_add_full (G_PRIORITY_HIGH, handle, st, NULL);
}

/* ---- where to open it ---- */

static gboolean
root_rect (GtkWidget *w, GdkRectangle *r)
{
  GtkAllocation a;
  GtkWidget *top = gtk_widget_get_toplevel (w);
  GdkWindow *win = gtk_widget_get_window (top);
  int ox = 0, oy = 0, x = 0, y = 0;

  if (win == NULL || !gtk_widget_translate_coordinates (w, top, 0, 0, &x, &y))
    return FALSE;
  gdk_window_get_origin (win, &ox, &oy);
  gtk_widget_get_allocation (w, &a);
  r->x = ox + x;
  r->y = oy + y;
  r->width = a.width;
  r->height = a.height;
  return TRUE;
}

static void
position (MenuState *st, GtkWidget *parent_menu_item,
          GtkMenuPositionFunc func, gpointer data, int *x, int *y)
{
  GtkMenuPrivate *priv = GTK_MENU (st->menu)->priv;
  GdkRectangle r;
  int px = 0, py = 0;
  GdkDevice *pointer = gdk_seat_get_pointer (gdk_display_get_default_seat (
                         gtk_widget_get_display (GTK_WIDGET (st->menu))));

  gdk_device_get_position (pointer, NULL, &px, &py);
  *x = px - 32;                      /* 64 OS units left of the pointer */
  *y = py;
  if (st->combo && root_rect (st->combo, &r))
    {
      *x = r.x + r.width;            /* a pop-up: to the right */
      *y = r.y;
    }
  else if (parent_menu_item && GTK_IS_MENU_BAR (gtk_widget_get_parent (parent_menu_item)) &&
           root_rect (parent_menu_item, &r))
    {
      *x = r.x;                      /* a menu bar title: below it */
      *y = r.y + r.height;
    }
  else if (func)
    {
      gboolean push_in = FALSE;
      func (GTK_MENU (st->menu), x, y, &push_in, data);
    }
  else if (priv->rect_window && (priv->rect.width > 1 || priv->rect.height > 1))
    {
      int ox = 0, oy = 0;
      gdk_window_get_origin (priv->rect_window, &ox, &oy);
      *x = ox + priv->rect.x;
      *y = oy + priv->rect.y + priv->rect.height;
    }
  else if (priv->widget && root_rect (priv->widget, &r))
    {
      *x = r.x;
      *y = r.y + r.height;
    }
}

/* The window menu (a hidden menu bar) of the window a Menu click is in,
   when the current event is that click; *under: the widget clicked */
static GtkWidget *
context_window_menu (GtkWidget **under)
{
  GdkEvent *event = gtk_get_current_event ();
  GtkWidget *w = NULL, *top, *bar = NULL;

  if (event && event->type == GDK_BUTTON_PRESS && event->button.button == 3 && event->any.window)
    {
      gdk_window_get_user_data (event->any.window, (gpointer *) &w);
      if (w && GTK_IS_WIDGET (w) && (top = gtk_widget_get_toplevel (w)) && GTK_IS_WINDOW (top))
        {
          bar = g_object_get_data (G_OBJECT (top), "riscos-gtk-window-menu");
          if (bar && (!gtk_widget_get_visible (bar) || !gtk_widget_is_sensitive (bar)))
            bar = NULL;
        }
    }
  if (event)
    gdk_event_free (event);
  *under = bar ? w : NULL;
  return bar;
}

/* The context submenu's name: "riscos-gtk-context-name" on the widget
   clicked or an ancestor (a program can say "Folder", "Message"...), else
   "Selection" */
static const char *
context_name (GtkWidget *w)
{
  for (; w; w = gtk_widget_get_parent (w))
    {
      const char *name = g_object_get_data (G_OBJECT (w), "riscos-gtk-context-name");
      if (name)
        return name;
    }
  return "Selection";
}

/* ---- the hooks ---- */

gboolean
_gtk_menu_riscos_popup (GtkMenu             *menu,
                        GtkWidget           *parent_menu_shell,
                        GtkWidget           *parent_menu_item,
                        GtkMenuPositionFunc  func,
                        gpointer             data)
{
  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (menu));
  GtkWidget *attach = gtk_menu_get_attach_widget (menu);
  const char *title = g_get_application_name ();
  MenuState *st;
  gchar *title_owned = NULL;
  int x, y, built;

  if (!GDK_IS_RISCOS_DISPLAY (display) || g_getenv ("RISCOS_GTK_MENUS"))
    return FALSE;                    /* RISCOS_GTK_MENUS set: GTK's own menus */

  if (current && current->menu == GTK_MENU_SHELL (menu) && !current->handling && !current->closed)
    {
      trace ("popup again (kept)", current);
      return TRUE;                   /* already open as a Wimp menu */
    }

  if (current)
    {
      /* one menu tree at a time (a menu bar switching titles) */
      MenuState *old = current;
      current = NULL;
      gdk_riscos_display_close_menu (display);
      if (!old->handling)
        state_free (old);
    }

  st = g_new0 (MenuState, 1);
  st->menu = g_object_ref (GTK_MENU_SHELL (menu));
  st->menus = g_ptr_array_new ();
  st->mem = g_ptr_array_new_with_free_func (g_free);
  if (parent_menu_shell && GTK_IS_MENU_BAR (parent_menu_shell))
    st->parent_shell = g_object_ref (parent_menu_shell);
  if (attach)
    {
      GtkWidget *combo = GTK_IS_COMBO_BOX (attach) ? attach : gtk_widget_get_ancestor (attach, GTK_TYPE_COMBO_BOX);
      if (combo)
        st->combo = g_object_ref (combo);
    }
  if (title == NULL)
    title = g_get_prgname ();
  if (parent_menu_item && GTK_IS_MENU_ITEM (parent_menu_item))
    title_owned = item_text (parent_menu_item);

  /* A widget's context menu from a Menu click, in a window with a menu:
     the window's menu, with the context menu as its first entry, so that
     Menu gives the same menu anywhere in the window (RISC OS: one menu
     per window), and what's under the pointer is in it */
  if (!parent_menu_shell && !parent_menu_item && !st->combo)
    {
      GtkWidget *under = NULL, *bar = context_window_menu (&under);
      if (bar && GTK_WIDGET (menu) != bar)
        {
          const char *name = context_name (under);
          built = build_with (st, GTK_MENU_SHELL (bar), title, GTK_MENU_SHELL (menu), name);
          g_free (title_owned);
          if (built >= 0)
            {
              GdkDevice *pointer = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
              gdk_device_get_position (pointer, NULL, &x, &y);
              current = st;
              menus_opened++;
              trace ("context menu in the window menu", st);
              gdk_riscos_display_open_menu (display, ((WimpMenu *) g_ptr_array_index (st->menus, 0))->block,
                                            x - 32, y, menu_event, st);
              return TRUE;
            }
          state_free (st);
          return TRUE;
        }
    }

  built = build (st, GTK_MENU_SHELL (menu), title_owned ? title_owned : title);
  g_free (title_owned);
  if (built < 0)
    {
      state_free (st);
      if (parent_menu_shell && GTK_IS_MENU_BAR (parent_menu_shell))
        gtk_menu_shell_deactivate (GTK_MENU_SHELL (parent_menu_shell));
      return TRUE;                   /* nothing to show */
    }
  position (st, parent_menu_item, func, data, &x, &y);
  current = st;
  menus_opened++;
  trace ("popup", st);
  gdk_riscos_display_open_menu (display, ((WimpMenu *) g_ptr_array_index (st->menus, 0))->block,
                                x, y, menu_event, st);
  return TRUE;
}

void
_gtk_menu_riscos_popdown (GtkMenu *menu)
{
  MenuState *st = current;

  if (st == NULL || st->menu != GTK_MENU_SHELL (menu) || st->handling)
    return;
  trace ("popdown", st);
  current = NULL;
  gdk_riscos_display_close_menu (gtk_widget_get_display (GTK_WIDGET (menu)));
  state_free (st);
}

gboolean
_gtk_menu_riscos_is_open (GtkMenu *menu)
{
  return current && current->menu == GTK_MENU_SHELL (menu);
}

/* ---- the window menu: RISC OS style menus from a GtkMenuBar ---- */

/* RISC OS programs have no menu bar: Menu (the middle button) anywhere in
   a window opens the program's menu at the pointer. So a GtkMenuBar takes
   no space and draws nothing (gtkmenubar.c asks
   _gtk_menu_riscos_hide_menubar ()), and a Menu click in its window that no
   widget used opens one Wimp menu, titled with the program's name, whose
   entries are the menu bar's titles leading to their menus as submenus.
   The menu bar still exists, so its keyboard short-cuts still work.
   *Set RISCOS_GTK_MENUBAR 1 keeps GTK's menu bar instead. */

gboolean
_gtk_menu_riscos_hide_menubar (void)
{
  static int hide = -1;

  if (hide < 0)
    hide = g_getenv ("RISCOS_GTK_MENUBAR") == NULL && g_getenv ("RISCOS_GTK_MENUS") == NULL;
  return hide;
}

static gboolean
open_window_menu (GtkMenuShell *bar, GdkEventButton *event)
{
  GdkDisplay *display = gtk_widget_get_display (GTK_WIDGET (bar));
  const char *title = g_get_application_name ();
  MenuState *st;

  if (!GDK_IS_RISCOS_DISPLAY (display))
    return FALSE;
  if (current)
    {
      MenuState *old = current;
      current = NULL;
      gdk_riscos_display_close_menu (display);
      if (!old->handling)
        state_free (old);
    }
  if (title == NULL)
    title = g_get_prgname ();
  st = g_new0 (MenuState, 1);
  st->menu = g_object_ref (bar);
  st->menus = g_ptr_array_new ();
  st->mem = g_ptr_array_new_with_free_func (g_free);
  if (build (st, bar, title) < 0)
    {
      state_free (st);
      return FALSE;
    }
  current = st;
  menus_opened++;
  trace ("window menu", st);
  /* at the pointer, 64 OS units to its left, as RISC OS programs do */
  gdk_riscos_display_open_menu (display, ((WimpMenu *) g_ptr_array_index (st->menus, 0))->block,
                                (int) event->x_root - 32, (int) event->y_root, menu_event, st);
  return TRUE;
}

static gboolean
window_button_press (GtkWidget *window, GdkEventButton *event, gpointer data)
{
  GtkWidget *bar = g_object_get_data (G_OBJECT (window), "riscos-gtk-window-menu");

  /* GDK button 3 is RISC OS's Menu button. Only a click no widget took
     (a widget's own context menu comes first) reaches the window. */
  if (event->type != GDK_BUTTON_PRESS || event->button != 3 || bar == NULL ||
      !gtk_widget_get_visible (bar) || !gtk_widget_is_sensitive (bar))
    return FALSE;
  return open_window_menu (GTK_MENU_SHELL (bar), event);
}

static gboolean menu_click_taken;

/* The menu for a Menu click in top: its own menu bar's, else (a tool
   window, a palette: no menu of its own, not a dialogue box) that of the
   window it belongs to (transient for), else that of the program's only
   window with a menu, as a RISC OS program's tool windows give its menu */
static GtkWidget *
window_menu_for (GtkWidget *top)
{
  GtkWidget *bar = g_object_get_data (G_OBJECT (top), "riscos-gtk-window-menu");
  GtkWindow *p;
  GList *tops, *l;
  int n = 0;

  if (bar || GTK_IS_DIALOG (top) || gtk_window_get_modal (GTK_WINDOW (top)) ||
      gtk_window_get_window_type (GTK_WINDOW (top)) != GTK_WINDOW_TOPLEVEL)
    return bar;
  for (p = gtk_window_get_transient_for (GTK_WINDOW (top)); p && !bar; p = gtk_window_get_transient_for (p))
    bar = g_object_get_data (G_OBJECT (p), "riscos-gtk-window-menu");
  if (bar)
    return bar;
  tops = gtk_window_list_toplevels ();
  for (l = tops; l; l = l->next)
    if (gtk_widget_get_mapped (l->data) && g_object_get_data (l->data, "riscos-gtk-window-menu"))
      {
        bar = g_object_get_data (l->data, "riscos-gtk-window-menu");
        n++;
      }
  g_list_free (tops);
  return n == 1 ? bar : NULL;
}

/* A Menu click gives a menu, wherever it is, as RISC OS's Menu button
   always does. GTK hands every event to this first (gdk_event_handler_set):
   - over a multi-line text view in a window with a menu: the program's
     menu, as in a RISC OS editor's document window (Edit, StrongED), not
     GTK's Cut/Copy/Paste (the program's own Edit menu has those); the view
     never sees the click;
   - anywhere else the widget under the pointer has the click first. If it
     opened a menu of its own (a list's, an attachment's, a link's), that
     menu is shown inside the window's (_gtk_menu_riscos_popup). If it
     opened none - it ignored the click, or used the right button for
     something else (a toolbar, a paint program's canvas, an insensitive
     button) - the window's menu opens all the same. Before, only a click
     no widget took reached the window, so Menu worked over parts of a
     window only. A tool window with no menu of its own gives the menu of
     the window it belongs to (window_menu_for). */
static void
event_handler (GdkEvent *event, gpointer data)
{
  GtkWidget *w = NULL, *top = NULL, *bar = NULL;
  guint before;

  if (!event->any.window || event->button.button != 3 ||
      (event->type != GDK_BUTTON_PRESS && event->type != GDK_2BUTTON_PRESS &&
       event->type != GDK_3BUTTON_PRESS && event->type != GDK_BUTTON_RELEASE))
    {
      gtk_main_do_event (event);
      return;
    }
  if (event->type != GDK_BUTTON_PRESS)
    {
      /* the rest of a click a text view didn't get */
      if (menu_click_taken)
        {
          if (event->type == GDK_BUTTON_RELEASE)
            menu_click_taken = FALSE;
          return;
        }
      gtk_main_do_event (event);
      return;
    }
  menu_click_taken = FALSE;
  gdk_window_get_user_data (event->any.window, (gpointer *) &w);
  if (w && GTK_IS_WIDGET (w))
    {
      top = gtk_widget_get_toplevel (w);
      bar = top && GTK_IS_WINDOW (top) ? window_menu_for (top) : NULL;
      if (bar && (!gtk_widget_get_visible (bar) || !gtk_widget_is_sensitive (bar)))
        bar = NULL;
    }
  if (g_getenv ("RISCOS_GTK_MENU_DEBUG"))
    g_printerr ("riscos-gtk menu: Menu click over %s, window menu %p\n",
                w ? G_OBJECT_TYPE_NAME (w) : "nothing", (void *) bar);
  if (bar && gtk_widget_get_ancestor (w, GTK_TYPE_TEXT_VIEW) &&
      open_window_menu (GTK_MENU_SHELL (bar), &event->button))
    {
      menu_click_taken = TRUE;
      return;
    }
  before = menus_opened;
  if (bar)
    g_object_ref (bar);
  gtk_main_do_event (event);
  if (bar && menus_opened == before && gtk_widget_get_visible (bar) && gtk_widget_is_sensitive (bar))
    open_window_menu (GTK_MENU_SHELL (bar), &event->button);
  if (bar)
    g_object_unref (bar);
}

void
_gtk_menu_riscos_menubar_attach (GtkWidget *bar, GtkWidget *toplevel)
{
  static gboolean handler_set;

  if (!_gtk_menu_riscos_hide_menubar () || !GTK_IS_WINDOW (toplevel) ||
      !GDK_IS_RISCOS_DISPLAY (gtk_widget_get_display (toplevel)))
    return;
  if (!handler_set)
    {
      handler_set = TRUE;
      gdk_event_handler_set (event_handler, NULL, NULL);
    }
  /* the window's first menu bar is its menu */
  if (g_object_get_data (G_OBJECT (toplevel), "riscos-gtk-window-menu") == NULL)
    {
      g_object_set_data (G_OBJECT (toplevel), "riscos-gtk-window-menu", bar);
      if (!g_object_get_data (G_OBJECT (toplevel), "riscos-gtk-window-menu-hooked"))
        {
          g_object_set_data (G_OBJECT (toplevel), "riscos-gtk-window-menu-hooked", GINT_TO_POINTER (1));
          gtk_widget_add_events (toplevel, GDK_BUTTON_PRESS_MASK);
          g_signal_connect_after (toplevel, "button-press-event", G_CALLBACK (window_button_press), NULL);
        }
    }
}

void
_gtk_menu_riscos_menubar_detach (GtkWidget *bar, GtkWidget *toplevel)
{
  if (GTK_IS_WINDOW (toplevel) &&
      g_object_get_data (G_OBJECT (toplevel), "riscos-gtk-window-menu") == bar)
    g_object_set_data (G_OBJECT (toplevel), "riscos-gtk-window-menu", NULL);
}
