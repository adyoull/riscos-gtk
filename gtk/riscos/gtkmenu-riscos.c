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
#include "gtkcellview.h"
#include "gtkcomboboxtext.h"
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
  GtkMenu *menu;
  GtkWidget *parent_shell;        /* a menu bar, or NULL */
  GtkWidget *combo;               /* a combo box the menu belongs to, or NULL */
  GPtrArray *menus;               /* WimpMenu *; [0] is the top */
  GPtrArray *mem;                 /* text buffers and blocks to free */
  int selection[16];
  gboolean adjust, closed, handling;
} MenuState;

static MenuState *current;

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

static int
build (MenuState *st, GtkMenu *menu, const char *title_utf8)
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
      GtkWidget *label = find_label (item);
      gchar *utf8 = item_text (item);
      char buf[TEXT_MAX];
      wm->items[i] = g_object_ref (item);
      gdk_riscos_utf8_to_latin1 (utf8, buf, sizeof buf);
      g_free (utf8);
      label_text[i] = g_strdup (buf);
      shortcut_text (label, buf, sizeof buf);
      key_text[i] = g_strdup (buf);
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
      GtkWidget *submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (item));
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
          gchar *sub_title = item_text (item);
          int child = build (st, GTK_MENU (submenu), sub_title);
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
  GtkMenuPrivate *priv = st->menu->priv;
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
      func (st->menu, x, y, &push_in, data);
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

  if (current && current->menu == menu && !current->handling && !current->closed)
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
  st->menu = g_object_ref (menu);
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

  built = build (st, menu, title_owned ? title_owned : title);
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
  trace ("popup", st);
  gdk_riscos_display_open_menu (display, ((WimpMenu *) g_ptr_array_index (st->menus, 0))->block,
                                x, y, menu_event, st);
  return TRUE;
}

void
_gtk_menu_riscos_popdown (GtkMenu *menu)
{
  MenuState *st = current;

  if (st == NULL || st->menu != menu || st->handling)
    return;
  trace ("popdown", st);
  current = NULL;
  gdk_riscos_display_close_menu (gtk_widget_get_display (GTK_WIDGET (menu)));
  state_free (st);
}

gboolean
_gtk_menu_riscos_is_open (GtkMenu *menu)
{
  return current && current->menu == menu;
}
