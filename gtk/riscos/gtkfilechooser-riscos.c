/*
 * gtkfilechooser-riscos.c - riscos-gtk: GtkFileChooserNative and
 * GtkFileChooserDialog as RISC OS Save boxes and Open boxes.
 *
 * Save (GTK_FILE_CHOOSER_ACTION_SAVE): a Save box, as RISC OS programs
 * have. Its file icon (the Wimp's sprite for the file's type, file_xxx)
 * is dragged to a Filer window, or to another program, and dropped:
 * Message_DataSave goes to the window under the pointer and its reply
 * (Message_DataSaveAck) names the file to save to, which is what the
 * program gets (gtk_file_chooser_get_file). After it has saved, the
 * receiver is sent Message_DataLoad (the data transfer protocol; another
 * program then loads <Wimp$Scrap>). OK saves to a full name typed in the
 * field (ADFS::HD4.$.Docs.Report/txt), or into the dialogue's current
 * folder if the program set one.
 *
 * Open, Select folder: an Open box. A file (or a directory) dragged here
 * from a Filer window is chosen; a full name can also be typed.
 *
 * GtkFileChooserDialog (a program's own file dialogue) is shown the same
 * way: the dialogue itself stays hidden while the box is open, and gets
 * its response (the program's own Save/Open button's id, or Cancel) when
 * the box closes; gtk_file_chooser_get_file() and friends then give the
 * chosen file (gtkfilechooser.c, "riscos-gtk-files").
 *
 * Names in the field are RISC OS names ("Report/txt"); GTK gets UnixLib
 * names ("/ADFS::HD4/$/Docs/Report.txt").
 *
 * A name without an extension is Data (&FFD), unless the program sets
 * RISCOS_GTK_DEFAULT_FILETYPE (hex, e.g. "fff" for a text editor).
 *
 * *Set RISCOS_GTK_NATIVE_CHOOSER 0 uses GTK's own file chooser instead
 * (for both).
 *
 * Part of GTK: LGPL version 2.1 or later. Copyright (C) 2026 Andrew Youll.
 */
#include "config.h"

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include "gtkfilechoosernativeprivate.h"
#include "gtkfilechooserdialog.h"
#include "gtkfilechooser-riscos.h"
#include "gtknativedialogprivate.h"
#include "gtkfilechooser.h"
#include "gtkwindow.h"
#include "gtkdialog.h"
#include "gtkmain.h"
#include "gtkbox.h"
#include "gtkbutton.h"
#include "gtkentry.h"
#include "gtklabel.h"
#include "gtkimage.h"
#include "gtkeventbox.h"
#include "gtkcheckbutton.h"
#include "gtkintl.h"
#include "gtkiconview.h"
#include "gtktreeview.h"
#include "gtktreeselection.h"
#include "gtktreesortable.h"
#include "gtkliststore.h"
#include "gtkscrolledwindow.h"
#include "gtkmenu.h"
#include "gtkmenuitem.h"
#include "gtkradiomenuitem.h"
#include "gtkseparatormenuitem.h"
#include "gtkcomboboxtext.h"
#include "gtkicontheme.h"
#include "gtkcellrenderertext.h"
#include "gtkcellrendererpixbuf.h"
#include "gtkcelllayout.h"
#include "gtkstylecontext.h"
#include "gtkfilefilter.h"
#include "deprecated/gtkstock.h"
#include "gtklistbox.h"
#include "gtkpaned.h"

#include "riscos/gdkriscos.h"
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>

#define FILETYPE_DIR   0x1000
#define FILETYPE_APP   0x2000

typedef struct _RiscosBox RiscosBox;
typedef struct _Browser Browser;
/* Answer the owner: response GTK_RESPONSE_ACCEPT (file set), _CANCEL or
   _DELETE_EVENT */
typedef void (*RiscosBoxDone) (GObject *owner, int response, GSList *files);

struct _RiscosBox
{
  GObject *owner;               /* the GtkFileChooserNative or GtkFileChooserDialog */
  RiscosBoxDone done;
  GFile *folder;                /* the program's current folder, or NULL */
  GFile *dropped;               /* a file dropped on an Open box */
  GtkFileChooserAction action;
  GtkWidget *window, *entry, *icon;
  char *sprite;                 /* the icon's sprite (file_xxx) */
  char *accept_label, *cancel_label;   /* the program's, or NULL */
  gboolean pressed, dragging;
  double press_x, press_y;

  /* what the program asked for, to make the box again as the other kind */
  char *title, *initial_name;
  GtkWindow *parent;
  gboolean modal, multiple;
  GSList *filters;              /* the program's GtkFileFilters (refs) */
  GtkFileFilter *filter;        /* the one chosen */

  /* the Filer-style directory browser (below) */
  gboolean browser;
  Browser *b;
};

/* After a drag save: waiting for the program to write the file, then
   Message_DataLoad to the receiver */
typedef struct
{
  char *riscos_path, *unix_path;
  gboolean existed;
  GStatBuf before;
  goffset last_size;
  int polls, stable;
} SaveWait;

static gboolean
debug (void)
{
  return g_getenv ("RISCOS_GTK_DEBUG") != NULL;
}

static void
riscos_files_free (GSList *files)
{
  g_slist_free_full (files, g_object_unref);
}

/* ---- file types ---- */

static const struct { const char *ext; int type; } types[] = {
  { "txt", 0xFFF }, { "text", 0xFFF }, { "c", 0xFFF }, { "h", 0xFFF }, { "md", 0xFFF },
  { "csv", 0xDFE }, { "html", 0xFAF }, { "htm", 0xFAF }, { "png", 0xB60 },
  { "jpg", 0xC85 }, { "jpeg", 0xC85 }, { "gif", 0x695 }, { "pdf", 0xADF },
  { "zip", 0xA91 }, { "svg", 0xAAD }, { "xml", 0xF80 }, { "json", 0xF13 },
  { "css", 0xF79 }, { "js", 0xF81 }, { "bmp", 0x69C }, { "tif", 0xFF0 },
  { "tiff", 0xFF0 }, { "wav", 0xFB1 }, { "mp3", 0x1AD }, { "ogg", 0xF8F },
};

/* The type of a RISC OS leaf name, from its extension ("Report/txt") */
static int
leaf_type (const char *leaf)
{
  const char *ext = strrchr (leaf, '/');
  guint i;

  if (ext == NULL)
    {
      /* no extension: the program's own type if it says (a text editor:
         RISCOS_GTK_DEFAULT_FILETYPE=fff), else Data */
      const char *def = g_getenv ("RISCOS_GTK_DEFAULT_FILETYPE");
      char *end = NULL;
      long t = def ? strtol (def, &end, 16) : -1;
      return def && end && *end == '\0' && t >= 0 && t <= 0xFFF ? (int) t : 0xFFD;
    }
  for (i = 0; i < G_N_ELEMENTS (types); i++)
    if (g_ascii_strcasecmp (ext + 1, types[i].ext) == 0)
      return types[i].type;
  return 0xFFD;
}

static const char *
leaf_of (const char *riscos_name)
{
  const char *dot = strrchr (riscos_name, '.');
  const char *colon = strrchr (riscos_name, ':');

  if (colon && (!dot || colon > dot))
    dot = colon;
  return dot ? dot + 1 : riscos_name;
}

/* A full RISC OS name (with a file system, a disc or a root) */
static gboolean
is_full_name (const char *name)
{
  return strchr (name, ':') || strchr (name, '$') || strchr (name, '<')
         || name[0] == '@' || name[0] == '&' || name[0] == '%';
}

/* The Wimp sprite for a type, as an image */
static GtkWidget *
file_icon (int type, char **sprite)
{
  GdkPixbuf *pb;
  GtkWidget *image;

  *sprite = g_strdup_printf ("file_%03x", type);
  pb = gdk_riscos_wimp_sprite (*sprite);
  if (pb == NULL && type != 0xFFD)
    {
      g_free (*sprite);
      *sprite = g_strdup ("file_xxx");
      pb = gdk_riscos_wimp_sprite (*sprite);
    }
  if (pb)
    {
      image = gtk_image_new_from_pixbuf (pb);
      g_object_unref (pb);
    }
  else
    image = gtk_image_new_from_icon_name ("text-x-generic", GTK_ICON_SIZE_DIALOG);
  return image;
}

/* ---- the end of a box ---- */

static void box_free (RiscosBox *box);

static void
on_box_destroy (GtkWidget *w, RiscosBox *box)
{
  box->window = NULL;
}

/* Close the box and answer the program (files set, for ACCEPT; the
   list and its GFiles are taken) */
static void
finish_files (RiscosBox *box, int response, GSList *files)
{
  GObject *owner = g_object_ref (box->owner);
  RiscosBoxDone done = box->done;

  if (g_object_get_data (owner, "riscos-gtk-box") == box)
    g_object_set_data (owner, "riscos-gtk-box", NULL);
  box_free (box);
  done (owner, response, files);
  g_slist_free_full (files, g_object_unref);
  g_object_unref (owner);
}

static void
finish (RiscosBox *box, int response, GFile *file)
{
  finish_files (box, response, file ? g_slist_prepend (NULL, file) : NULL);
}

static void browser_free (RiscosBox *box);

/* Close the box's window (the box itself stays) */
static void
box_close_window (RiscosBox *box)
{
  g_idle_remove_by_data (box);
  browser_free (box);
  if (box->window)
    {
      g_signal_handlers_disconnect_by_func (box->window, on_box_destroy, box);
      gtk_widget_destroy (box->window);
    }
  box->window = box->entry = box->icon = NULL;
  g_clear_pointer (&box->sprite, g_free);
  box->pressed = box->dragging = FALSE;
}

static void
box_free (RiscosBox *box)
{
  box_close_window (box);
  g_clear_object (&box->dropped);
  g_clear_object (&box->folder);
  g_slist_free_full (box->filters, g_object_unref);
  g_clear_object (&box->filter);
  g_free (box->accept_label);
  g_free (box->cancel_label);
  g_free (box->title);
  g_free (box->initial_name);
  g_free (box);
}

static void
on_cancel (GtkButton *b, RiscosBox *box)
{
  finish (box, GTK_RESPONSE_CANCEL, NULL);
}

static gboolean
on_delete (GtkWidget *w, GdkEvent *ev, RiscosBox *box)
{
  finish (box, GTK_RESPONSE_DELETE_EVENT, NULL);
  return TRUE;
}

static gboolean
on_key (GtkWidget *w, GdkEventKey *ev, RiscosBox *box)
{
  if (ev->keyval == GDK_KEY_Escape)
    {
      finish (box, GTK_RESPONSE_CANCEL, NULL);
      return TRUE;
    }
  return FALSE;
}

/* A RISC OS name as a GFile */
static GFile *
file_for (const char *riscos_name)
{
  char *unix_path = gdk_riscos_path_to_unix (riscos_name);
  GFile *f = g_file_new_for_path (unix_path);

  g_free (unix_path);
  return f;
}

/* The RISC OS name of a GFile, or NULL */
static char *
riscos_name_of (GFile *file)
{
  char *path = file ? g_file_get_path (file) : NULL;
  char *name = path ? gdk_riscos_path_from_unix (path) : NULL;

  g_free (path);
  return name;
}

/* ---- Save box ---- */

static gboolean
save_wait_poll (gpointer data)
{
  SaveWait *w = data;
  GStatBuf st;
  gboolean exists = g_stat (w->unix_path, &st) == 0;

  w->polls++;
  if (exists)
    {
      gboolean changed = !w->existed || st.st_mtime != w->before.st_mtime
                         || st.st_size != w->before.st_size;
      if (st.st_size == w->last_size)
        w->stable++;
      else
        w->stable = 0;
      w->last_size = st.st_size;
      /* written (and no longer growing), or there a while (saved in the
         same second, same size) */
      if ((changed && w->stable >= 1) || w->polls >= 8)
        {
          if (debug ())
            g_printerr ("riscos-gtk: Save box: %s saved (%ld bytes)\n", w->riscos_path,
                        (long) st.st_size);
          gdk_riscos_save_done (w->riscos_path);
          goto done;
        }
    }
  if (w->polls < 240)           /* a minute */
    return G_SOURCE_CONTINUE;
  if (debug ())
    g_printerr ("riscos-gtk: Save box: %s never saved\n", w->riscos_path);
done:
  g_free (w->riscos_path);
  g_free (w->unix_path);
  g_free (w);
  return G_SOURCE_REMOVE;
}

/* The receiver answered the drop: save there */
static void
on_save_to (const char *riscos_path, gboolean safe, gpointer data)
{
  RiscosBox *box = data;
  SaveWait *w = g_new0 (SaveWait, 1);

  w->riscos_path = g_strdup (riscos_path);
  w->unix_path = gdk_riscos_path_to_unix (riscos_path);
  if (!safe)
    g_unlink (w->unix_path);    /* <Wimp$Scrap>: wait for the new one */
  w->existed = g_stat (w->unix_path, &w->before) == 0;
  w->last_size = -1;
  g_timeout_add (250, save_wait_poll, w);
  if (debug ())
    g_printerr ("riscos-gtk: Save box: save to %s (%s)\n", riscos_path, safe ? "safe" : "scrap");
  finish (box, GTK_RESPONSE_ACCEPT, g_file_new_for_path (w->unix_path));
}

static gboolean
on_icon_press (GtkWidget *w, GdkEventButton *ev, RiscosBox *box)
{
  if (ev->type != GDK_BUTTON_PRESS || ev->button != 1)
    return FALSE;
  box->pressed = TRUE;
  box->dragging = FALSE;
  box->press_x = ev->x_root;
  box->press_y = ev->y_root;
  return TRUE;
}

static gboolean
on_icon_motion (GtkWidget *w, GdkEventMotion *ev, RiscosBox *box)
{
  GtkWidget *top;
  GtkAllocation a;
  int x = 0, y = 0, ox = 0, oy = 0;

  if (!box->pressed || box->dragging)
    return FALSE;
  if (ABS (ev->x_root - box->press_x) < 4 && ABS (ev->y_root - box->press_y) < 4)
    return TRUE;
  box->dragging = TRUE;
  top = gtk_widget_get_toplevel (box->icon);
  gtk_widget_get_allocation (box->icon, &a);
  gtk_widget_translate_coordinates (box->icon, top, 0, 0, &x, &y);
  gdk_window_get_origin (gtk_widget_get_window (top), &ox, &oy);
  gdk_riscos_save_drag_begin (gtk_widget_get_display (w), box->sprite,
                              ox + x, oy + y, a.width, a.height);
  return TRUE;
}

static gboolean
on_icon_release (GtkWidget *w, GdkEventButton *ev, RiscosBox *box)
{
  char *name, *leaf;

  if (ev->button != 1 || !box->pressed)
    return FALSE;
  box->pressed = FALSE;
  if (!box->dragging)
    return TRUE;
  box->dragging = FALSE;
  name = g_strdup (gtk_entry_get_text (GTK_ENTRY (box->entry)));
  leaf = g_strdup (leaf_of (name));
  if (*leaf == '\0')
    {
      g_free (leaf);
      leaf = g_strdup ("Untitled");
    }
  /* dropped on us or the desktop background: nothing, as RISC OS */
  gdk_riscos_save_drop (gtk_widget_get_display (w), leaf, leaf_type (leaf), 0, on_save_to, box);
  g_free (leaf);
  g_free (name);
  return TRUE;
}

/* The field's name changed: the icon follows its type */
static void
on_name_changed (GtkEditable *e, RiscosBox *box)
{
  GtkWidget *parent = box->icon;
  GtkWidget *image = gtk_bin_get_child (GTK_BIN (parent));
  char *sprite = NULL;
  int type = leaf_type (leaf_of (gtk_entry_get_text (GTK_ENTRY (box->entry))));
  GtkWidget *fresh = file_icon (type, &sprite);

  if (box->sprite && strcmp (sprite, box->sprite) == 0)
    {
      g_free (sprite);
      g_object_ref_sink (fresh);
      g_object_unref (fresh);
      return;
    }
  g_free (box->sprite);
  box->sprite = sprite;
  gtk_container_remove (GTK_CONTAINER (parent), image);
  gtk_container_add (GTK_CONTAINER (parent), fresh);
  gtk_widget_show (fresh);
}

static void
on_save_ok (GtkWidget *w, RiscosBox *box)
{
  const char *name = gtk_entry_get_text (GTK_ENTRY (box->entry));
  char *folder;

  if (*name == '\0')
    return;
  if (is_full_name (name))
    {
      finish (box, GTK_RESPONSE_ACCEPT, file_for (name));
      return;
    }
  folder = riscos_name_of (box->folder);
  if (folder)
    {
      char *full = g_strconcat (folder, ".", name, NULL);
      g_free (folder);
      finish (box, GTK_RESPONSE_ACCEPT, file_for (full));
      g_free (full);
      return;
    }
  /* the Save As module's message */
  gdk_riscos_report_error (_("To save, drag the icon to a directory display"));
}

/* ---- Open box ---- */

static gboolean
open_idle (gpointer data)
{
  RiscosBox *box = data;
  GFile *file = box->dropped;

  box->dropped = NULL;
  finish (box, GTK_RESPONSE_ACCEPT, file);
  return G_SOURCE_REMOVE;
}

/* A file dropped on the box from the Filer */
static gboolean
on_load (const char *riscos_path, int filetype, gpointer data)
{
  RiscosBox *box = data;
  gboolean dir = filetype == FILETYPE_DIR || filetype == FILETYPE_APP;

  if (box->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ? !dir : dir)
    return FALSE;
  if (debug ())
    g_printerr ("riscos-gtk: Open box: %s (&%03X)\n", riscos_path, filetype);
  if (box->dropped)
    return TRUE;                /* already chosen */
  box->dropped = file_for (riscos_path);
  /* not from inside the Wimp message */
  g_idle_add (open_idle, box);
  return TRUE;
}

static void
on_open_ok (GtkWidget *w, RiscosBox *box)
{
  const char *name = gtk_entry_get_text (GTK_ENTRY (box->entry));

  if (is_full_name (name))
    finish (box, GTK_RESPONSE_ACCEPT, file_for (name));
  else
    gdk_riscos_report_error (box->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
                             ? _("To choose a directory, drag it to this box from a directory display")
                             : _("To open a file, drag it to this box from a directory display"));
}

static void
on_open_realize (GtkWidget *w, RiscosBox *box)
{
  gdk_riscos_window_set_load_func (gtk_widget_get_window (w), on_load, box);
}

/* ---- show, hide ---- */

/* label without GTK's mnemonic underscore (RISC OS buttons have none) */
static GtkWidget *
action_button (const char *label)
{
  GString *t = g_string_new (NULL);
  const char *p;
  char *text;
  GtkWidget *b;

  for (p = label; *p; p++)
    if (*p != '_')
      g_string_append_c (t, *p);
  text = g_string_free (t, FALSE);
  b = gtk_button_new_with_label (text);
  g_free (text);
  return b;
}

/* Cancel and the default action button, side by side, the same size,
   filling the box's width (the Save As dialogue's layout) */
static GtkWidget *
buttons (RiscosBox *box, GCallback ok, const char *ok_label)
{
  GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *cancel = action_button (box->cancel_label ? box->cancel_label : _("Cancel"));
  GtkWidget *okb = action_button (box->accept_label ? box->accept_label : ok_label);

  gtk_box_set_homogeneous (GTK_BOX (row), TRUE);
  g_signal_connect (cancel, "clicked", G_CALLBACK (on_cancel), box);
  g_signal_connect (okb, "clicked", ok, box);
  gtk_widget_set_can_default (okb, TRUE);
  gtk_box_pack_start (GTK_BOX (row), cancel, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (row), okb, TRUE, TRUE, 0);
  gtk_window_set_default (GTK_WINDOW (box->window), okb);
  return row;
}

/* A transient box (opened by a click, or from a menu) closes when the
   user clicks elsewhere, as the Save As module's does; one opened from
   the keyboard (F3, Ctrl-S) stays until Cancel or Escape */
static void
on_realize_transient (GtkWidget *w, gpointer data)
{
  gdk_riscos_window_set_transient_dbox (gtk_widget_get_window (w), TRUE);
}

/* Can this be a RISC OS box? */
static gboolean
usable (GtkFileChooserAction action)
{
  const char *env = g_getenv ("RISCOS_GTK_NATIVE_CHOOSER");

  if (env && strcmp (env, "0") == 0)
    return FALSE;
  if (action != GTK_FILE_CHOOSER_ACTION_SAVE && action != GTK_FILE_CHOOSER_ACTION_OPEN
      && action != GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    return FALSE;               /* create folder: GTK's own */
  return GDK_IS_RISCOS_DISPLAY (gdk_display_get_default ());
}

static void browser_build (RiscosBox *box);
static void box_show (RiscosBox *box);

/* A pop-up menu stays until the next one (or the window goes): its
   choice is made from an idle callback after it has closed */
static void
drop_menu (gpointer menu)
{
  gtk_widget_destroy (menu);
  g_object_unref (menu);
}

static void
keep_menu (GtkWidget *window, GtkWidget *menu, GtkWidget *attach)
{
  gtk_menu_attach_to_widget (GTK_MENU (menu), attach, NULL);
  g_object_set_data_full (G_OBJECT (window), "riscos-gtk-menu", g_object_ref_sink (menu), drop_menu);
}

static void
on_browse (GtkWidget *button, RiscosBox *box)
{
  box_close_window (box);
  box->browser = TRUE;
  box_show (box);
}

static void
on_use_browser (GtkMenuItem *item, RiscosBox *box)
{
  box_close_window (box);
  box->browser = TRUE;
  box_show (box);
}

static gboolean
on_box_menu (GtkWidget *w, GdkEventButton *ev, RiscosBox *box)
{
  GtkWidget *menu, *item;

  if (!gdk_event_triggers_context_menu ((GdkEvent *) ev))
    return FALSE;
  menu = gtk_menu_new ();
  item = gtk_menu_item_new_with_label (_("Directory browser..."));
  g_signal_connect (item, "activate", G_CALLBACK (on_use_browser), box);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show_all (menu);
  keep_menu (box->window, menu, w);
  gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) ev);
  return TRUE;
}

/* The Save box or Open box's window */
static void
box_build (RiscosBox *box)
{
  GtkWidget *vb, *w;
  gboolean save = box->action == GTK_FILE_CHOOSER_ACTION_SAVE;

  box->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (box->window), box->title ? box->title : save ? _("Save as") : _("Open"));
  /* a dialogue box: title bar only (no close icon, it has Cancel; no
     back or size icons); opened at the pointer */
  gtk_window_set_resizable (GTK_WINDOW (box->window), FALSE);
  gtk_window_set_deletable (GTK_WINDOW (box->window), FALSE);
  gtk_window_set_type_hint (GTK_WINDOW (box->window), GDK_WINDOW_TYPE_HINT_DIALOG);
  if (box->parent)
    gtk_window_set_transient_for (GTK_WINDOW (box->window), box->parent);
  gtk_window_set_position (GTK_WINDOW (box->window), GTK_WIN_POS_MOUSE);
  if (save)
    {
      /* transient unless opened from the keyboard (as the Toolbox's
         Save As box: from a menu it's transient, F3 opens it static).
         An Open box is never transient: the drag that fills it starts
         with a click in a Filer window, which would close it. */
      GdkEvent *ev = gtk_get_current_event ();
      gboolean from_key = ev && (ev->type == GDK_KEY_PRESS || ev->type == GDK_KEY_RELEASE);
      if (ev)
        gdk_event_free (ev);
      if (!from_key)
        g_signal_connect (box->window, "realize", G_CALLBACK (on_realize_transient), NULL);
    }
  gtk_window_set_modal (GTK_WINDOW (box->window), box->modal);
  g_signal_connect (box->window, "destroy", G_CALLBACK (on_box_destroy), box);
  g_signal_connect (box->window, "delete-event", G_CALLBACK (on_delete), box);
  g_signal_connect (box->window, "key-press-event", G_CALLBACK (on_key), box);
  /* Menu over the box: the directory browser instead */
  g_signal_connect (box->window, "button-press-event", G_CALLBACK (on_box_menu), box);

  /* the Save As dialogue's layout: the file icon, the name under it
     (centred), Cancel and Save across the bottom */
  vb = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vb), 8);
  gtk_container_add (GTK_CONTAINER (box->window), vb);
  box->entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (box->entry), TRUE);
  gtk_entry_set_width_chars (GTK_ENTRY (box->entry), save ? 16 : 24);
  gtk_entry_set_alignment (GTK_ENTRY (box->entry), 0.5);

  if (save)
    {
      char *name = g_strdup (box->initial_name ? box->initial_name : _("Untitled"));
      char *leaf;

      leaf = g_strdup (leaf_of (name));
      /* The Toolbox SaveAs window, measured from the Toolbox manual's
         picture (pixels at 1 pixel = 2 OS units): work area 155 x 148;
         the file icon (34 x 34) centred 6 from the top; the writable
         (26 high) 6 below it, 18 in from the left, 17 from the right;
         the Selection option 7 below that, 6 in; Cancel (64 x 28) 9
         below, 6 in; Save (the default, 56 x 28, with its 4-pixel
         channel round it) 11 in from the right; 10 below the buttons.
         Widths grow with the text if the desktop font needs it. */
      GtkWidget *col = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      GtkWidget *sel, *cancel, *okb;

      gtk_container_remove (GTK_CONTAINER (box->window), vb);
      gtk_widget_set_size_request (col, 155, 148);
      gtk_container_add (GTK_CONTAINER (box->window), col);

      /* the icon, to drag */
      box->icon = gtk_event_box_new ();
      gtk_event_box_set_visible_window (GTK_EVENT_BOX (box->icon), FALSE);
      gtk_widget_add_events (box->icon, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                             | GDK_BUTTON_MOTION_MASK);
      gtk_container_add (GTK_CONTAINER (box->icon), file_icon (leaf_type (leaf), &box->sprite));
      gtk_widget_set_size_request (box->icon, 34, 34);
      gtk_widget_set_halign (box->icon, GTK_ALIGN_CENTER);
      gtk_widget_set_margin_top (box->icon, 6);
      gtk_widget_set_tooltip_text (box->icon, _("Drag this icon to a directory display to save"));
      g_signal_connect (box->icon, "button-press-event", G_CALLBACK (on_icon_press), box);
      g_signal_connect (box->icon, "motion-notify-event", G_CALLBACK (on_icon_motion), box);
      g_signal_connect (box->icon, "button-release-event", G_CALLBACK (on_icon_release), box);
      gtk_box_pack_start (GTK_BOX (col), box->icon, FALSE, FALSE, 0);

      gtk_entry_set_text (GTK_ENTRY (box->entry), name);
      gtk_entry_set_width_chars (GTK_ENTRY (box->entry), 1);
      gtk_widget_set_size_request (box->entry, 120, 26);
      gtk_widget_set_margin_top (box->entry, 6);
      gtk_widget_set_margin_start (box->entry, 18);
      gtk_widget_set_margin_end (box->entry, 17);
      g_signal_connect (box->entry, "changed", G_CALLBACK (on_name_changed), box);
      gtk_box_pack_start (GTK_BOX (col), box->entry, FALSE, FALSE, 0);

      /* Selection: shaded, as the Toolbox shows it when the program
         has no selection to save (GTK has no such thing) */
      sel = gtk_check_button_new_with_label (_("Selection"));
      gtk_widget_set_sensitive (sel, FALSE);
      gtk_widget_set_can_focus (sel, FALSE);
      gtk_widget_set_halign (sel, GTK_ALIGN_START);
      {
        /* Selection, and Browse... on its right: the directory browser
           (the Toolbox's box has the line's right half empty) */
        GtkWidget *line = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *browse = action_button (_("Browse..."));
        gtk_widget_set_tooltip_text (browse, _("Save from a directory browser instead"));
        g_signal_connect (browse, "clicked", G_CALLBACK (on_browse), box);
        gtk_widget_set_margin_top (line, 7);
        gtk_widget_set_margin_start (line, 6);
        gtk_widget_set_margin_end (line, 11);
        gtk_box_pack_start (GTK_BOX (line), sel, FALSE, FALSE, 0);
        gtk_box_pack_end (GTK_BOX (line), browse, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (col), line, FALSE, FALSE, 0);
      }

      cancel = action_button (box->cancel_label ? box->cancel_label : _("Cancel"));
      okb = action_button (box->accept_label ? box->accept_label : _("Save"));
      gtk_widget_set_size_request (cancel, 64, 28);
      gtk_widget_set_size_request (okb, 56, 28);
      g_signal_connect (cancel, "clicked", G_CALLBACK (on_cancel), box);
      g_signal_connect (okb, "clicked", G_CALLBACK (on_save_ok), box);
      gtk_widget_set_can_default (okb, TRUE);
      gtk_widget_set_margin_top (row, 9);
      gtk_widget_set_margin_start (row, 6);
      gtk_widget_set_margin_end (row, 11);
      gtk_widget_set_margin_bottom (row, 10);
      gtk_box_pack_start (GTK_BOX (row), cancel, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (row), okb, FALSE, FALSE, 0);
      gtk_widget_set_margin_start (okb, 18);     /* at least the picture's gap */
      gtk_box_pack_end (GTK_BOX (col), row, FALSE, FALSE, 0);
      gtk_window_set_default (GTK_WINDOW (box->window), okb);
      g_free (leaf);
      g_free (name);
    }
  else
    {
      gboolean dirs = box->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
      char *name = riscos_name_of (box->folder);

      w = gtk_label_new (dirs ? _("Drag a directory here from a Filer window,\n"
                                    "or type its full name.")
                                : _("Drag a file here from a Filer window,\n"
                                    "or type its full name."));
      gtk_label_set_justify (GTK_LABEL (w), GTK_JUSTIFY_CENTER);
      gtk_box_pack_start (GTK_BOX (vb), w, FALSE, FALSE, 0);
      if (name)
        {
          char *text = g_strconcat (name, ".", NULL);
          gtk_entry_set_text (GTK_ENTRY (box->entry), text);
          g_free (text);
        }
      gtk_box_pack_start (GTK_BOX (vb), box->entry, FALSE, FALSE, 0);
      {
        /* Browse..., Cancel, Open: Browse... gives the directory browser */
        GtkWidget *row = buttons (box, G_CALLBACK (on_open_ok), _("Open"));
        GtkWidget *browse = action_button (_("Browse..."));
        gtk_widget_set_tooltip_text (browse, _("Choose in a directory browser instead"));
        g_signal_connect (browse, "clicked", G_CALLBACK (on_browse), box);
        gtk_box_pack_start (GTK_BOX (row), browse, TRUE, TRUE, 0);
        gtk_box_reorder_child (GTK_BOX (row), browse, 0);
        gtk_box_pack_start (GTK_BOX (vb), row, FALSE, FALSE, 0);
      }
      g_signal_connect (box->window, "realize", G_CALLBACK (on_open_realize), box);
      g_free (name);
    }

}

static void
box_show (RiscosBox *box)
{
  if (box->browser)
    browser_build (box);
  else
    box_build (box);
  gtk_widget_show_all (box->window);
  /* the caret at the end of the name, as in a Save As box */
  gtk_widget_grab_focus (box->entry);
  gtk_editable_set_position (GTK_EDITABLE (box->entry), -1);
  if (debug ())
    g_printerr ("riscos-gtk: %s %s shown\n", box->action == GTK_FILE_CHOOSER_ACTION_SAVE ? "Save" : "Open",
                box->browser ? "browser" : "box");
}

/* Which kind first: *Set RISCOS_GTK_CHOOSER filer for the directory
   browser (a program's !Run can set it, or the user, for all programs);
   "box" (the default) for the Save and Open boxes */
static gboolean
want_browser (void)
{
  const char *env = g_getenv ("RISCOS_GTK_CHOOSER");

  return env && (g_ascii_strcasecmp (env, "filer") == 0 || g_ascii_strcasecmp (env, "browser") == 0);
}

/* Open a Save box or an Open box (or the directory browser) for owner.
   name: the RISC OS name to start with (Save; NULL: "Untitled"); folder:
   the program's folder; filters: the program's GtkFileFilters. */
static RiscosBox *
box_open (GObject *owner, RiscosBoxDone done, GtkFileChooserAction action,
          const char *title, GtkWindow *parent, gboolean modal,
          GFile *folder, const char *initial_name,
          const char *accept_label, const char *cancel_label,
          gboolean multiple, GSList *filters, GtkFileFilter *filter)
{
  RiscosBox *box = g_new0 (RiscosBox, 1);

  box->owner = owner;
  box->done = done;
  box->folder = folder ? g_object_ref (folder) : NULL;
  box->accept_label = g_strdup (accept_label);
  box->cancel_label = g_strdup (cancel_label);
  box->title = g_strdup (title);
  box->initial_name = g_strdup (initial_name);
  box->parent = parent;
  box->modal = modal;
  box->multiple = multiple && action == GTK_FILE_CHOOSER_ACTION_OPEN;
  box->filters = g_slist_copy_deep (filters, (GCopyFunc) g_object_ref, NULL);
  box->filter = filter ? g_object_ref (filter) : NULL;
  box->action = action;
  box->browser = want_browser ();
  g_object_set_data (owner, "riscos-gtk-box", box);
  box_show (box);
  return box;
}

/* ---- The directory browser: a Filer window to choose in ----

   *Set RISCOS_GTK_CHOOSER filer (or Menu > Directory browser... over a
   Save or Open box) shows this instead of the box: a Filer-like window
   of the folder's objects with their Wimp sprites, as large icons, small
   icons or full info (Menu > Display, as the Filer's), sorted by name,
   type, size or date. Along the top: Parent and the folder's full name
   (type one and Return to go there). Down the left, as the Finder's
   sidebar: Discs (the discs and mounts on the icon bar, with their
   sprites: gdk_riscos_discs), Places (the program's folder, Boot, Choices, Scrap) and
   Recent (the last folders used); a click shows one. Along the
   bottom: the file's name (Save: with its icon, which can still be
   dragged to a Filer window or another program), the program's filters,
   Cancel and Save/Open.

   Double-click a directory (or an application) to go into it; a file
   (Open) to open it. A directory dragged here from the Filer is shown; a
   file dragged here is chosen (Open) or its name taken (Save). The view
   and sort, and the last ten folders used, are kept in
   <Choices$Write>.GTK.Browser. */

enum
{
  COL_LARGE, COL_SMALL, COL_NAME, COL_TYPE, COL_OBJECT, COL_SIZE, COL_DATE,
  COL_SIZE_TEXT, COL_TYPE_TEXT, COL_DATE_TEXT, N_COLS
};

enum { VIEW_LARGE, VIEW_SMALL, VIEW_FULL };
enum { SORT_NAME, SORT_TYPE, SORT_SIZE, SORT_DATE };

#define RECENT_MAX 10

struct _Browser
{
  char *dir;                    /* the folder shown (RISC OS, canonical) */
  GtkWidget *path, *scroll, *icons, *tree, *side, *filter_combo, *status;
  GtkListStore *store;
  GHashTable *sprites;          /* sprite name -> GdkPixbuf (or a NULL value) */
  int view, sort;
  char **recent;
  gboolean filling;
};

static char *
browser_choices (void)
{
#ifdef __riscos
  if (g_getenv ("Choices$Write"))
    return g_strdup ("/<Choices$Write>/GTK/Browser");
#endif
  return g_build_filename (g_get_user_config_dir (), "riscos-gtk", "browser.ini", NULL);
}

static void
browser_load_choices (Browser *b)
{
  GKeyFile *kf = g_key_file_new ();
  char *file = browser_choices ();

  b->view = VIEW_LARGE;
  b->sort = SORT_NAME;
  if (g_key_file_load_from_file (kf, file, G_KEY_FILE_NONE, NULL))
    {
      b->view = CLAMP (g_key_file_get_integer (kf, "Browser", "View", NULL), VIEW_LARGE, VIEW_FULL);
      b->sort = CLAMP (g_key_file_get_integer (kf, "Browser", "Sort", NULL), SORT_NAME, SORT_DATE);
      b->recent = g_key_file_get_string_list (kf, "Browser", "Recent", NULL, NULL);
    }
  g_key_file_unref (kf);
  g_free (file);
}

static void
browser_save_choices (Browser *b)
{
  GKeyFile *kf = g_key_file_new ();
  char *file = browser_choices (), *dir = g_path_get_dirname (file);

  g_key_file_set_integer (kf, "Browser", "View", b->view);
  g_key_file_set_integer (kf, "Browser", "Sort", b->sort);
  if (b->recent && b->recent[0])
    g_key_file_set_string_list (kf, "Browser", "Recent", (const char * const *) b->recent,
                                g_strv_length (b->recent));
  g_mkdir_with_parents (dir, 0755);
  g_key_file_save_to_file (kf, file, NULL);
  g_key_file_unref (kf);
  g_free (dir);
  g_free (file);
}

/* dir to the front of the recent folders */
static void
browser_add_recent (Browser *b, const char *dir)
{
  GPtrArray *a = g_ptr_array_new ();
  int i;

  g_ptr_array_add (a, g_strdup (dir));
  for (i = 0; b->recent && b->recent[i] && a->len < RECENT_MAX; i++)
    if (strcmp (b->recent[i], dir) != 0)
      g_ptr_array_add (a, g_strdup (b->recent[i]));
  g_ptr_array_add (a, NULL);
  g_strfreev (b->recent);
  b->recent = (char **) g_ptr_array_free (a, FALSE);
}

static void browser_side_sync (RiscosBox *box);

static void
browser_free (RiscosBox *box)
{
  Browser *b = box->b;

  if (b == NULL)
    return;
  box->b = NULL;
  g_free (b->dir);
  g_clear_object (&b->store);
  g_clear_object (&b->icons);
  g_clear_object (&b->tree);
  g_hash_table_destroy (b->sprites);
  g_strfreev (b->recent);
  g_free (b);
}

/* ---- sprites ---- */

static void
unref_or_null (gpointer p)
{
  if (p)
    g_object_unref (p);
}

static GdkPixbuf *
browser_sprite (Browser *b, const char *name)
{
  GdkPixbuf *pb;

  if (g_hash_table_lookup_extended (b->sprites, name, NULL, (gpointer *) &pb))
    return pb;
  pb = gdk_riscos_wimp_sprite (name);
  g_hash_table_insert (b->sprites, g_strdup (name), pb);
  return pb;
}

static GdkPixbuf *
browser_theme_icon (const char *icon, int size)
{
  return gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), icon, size,
                                   GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
}

/* The Filer's sprites for an object: large (file_xxx, directory, !app)
   and small (small_xxx, small_dir, sm!app); new references */
static void
browser_icons (Browser *b, const char *leaf, int type, GdkPixbuf **large, GdkPixbuf **small)
{
  char *l = NULL, *s = NULL, *lf = NULL, *sf = NULL;
  const char *theme = "text-x-generic";
  GdkPixbuf *pl, *ps;

  if (type == FILETYPE_DIR)
    {
      l = g_strdup ("directory");
      s = g_strdup ("small_dir");
      theme = "folder";
    }
  else if (type == FILETYPE_APP)
    {
      char *low = g_ascii_strdown (leaf, -1);
      l = g_strdup (low);
      s = g_strconcat ("sm", low, NULL);
      lf = g_strdup ("application");
      sf = g_strdup ("small_app");
      theme = "application-x-executable";
      g_free (low);
    }
  else if (type < 0)
    {
      l = g_strdup ("file_lxa");
      s = g_strdup ("small_lxa");
    }
  else
    {
      l = g_strdup_printf ("file_%03x", type);
      s = g_strdup_printf ("small_%03x", type);
      lf = g_strdup ("file_xxx");
      sf = g_strdup ("small_xxx");
    }
  pl = browser_sprite (b, l);
  if (!pl && lf)
    pl = browser_sprite (b, lf);
  ps = browser_sprite (b, s);
  if (!ps && sf)
    ps = browser_sprite (b, sf);
  *large = pl ? g_object_ref (pl) : browser_theme_icon (theme, 34);
  if (ps)
    *small = g_object_ref (ps);
  else if (pl)
    *small = gdk_pixbuf_scale_simple (pl, MAX (1, gdk_pixbuf_get_width (pl) / 2),
                                      MAX (1, gdk_pixbuf_get_height (pl) / 2), GDK_INTERP_BILINEAR);
  else
    *small = browser_theme_icon (theme, 17);
  g_free (l);
  g_free (s);
  g_free (lf);
  g_free (sf);
}

/* ---- the listing ---- */

static char *
browser_size_text (int type, guint32 length)
{
  if (type == FILETYPE_DIR || type == FILETYPE_APP)
    return g_strdup ("");
  if (length < 4096)
    return g_strdup_printf ("%u bytes", length);
  if (length < 4096u * 1024u)
    return g_strdup_printf ("%uK", (length + 1023) / 1024);
  return g_strdup_printf ("%uM", (length + 1024 * 1024 - 1) / (1024 * 1024));
}

static char *
browser_date_text (gint64 cs)
{
  GDateTime *dt;
  char *text;

  if (cs < 0)
    return g_strdup ("");
  dt = g_date_time_new_from_unix_local (cs / 100 - G_GINT64_CONSTANT (2208988800));
  if (dt == NULL)
    return g_strdup ("");
  text = g_date_time_format (dt, "%H:%M:%S %d-%b-%Y");
  g_date_time_unref (dt);
  return text;
}

static char *
browser_type_text (int type)
{
  if (type == FILETYPE_DIR)
    return g_strdup (_("Directory"));
  if (type == FILETYPE_APP)
    return g_strdup (_("Application"));
  if (type < 0)
    return g_strdup (_("Untyped"));
  return gdk_riscos_filetype_name (type);
}

/* MIME types of the common RISC OS file types (GTK's filters often ask
   for one; a RISC OS program seldom ships GLib's MIME database) */
static const struct { int type; const char *mime; } mime_types[] = {
  { 0xFFF, "text/plain" }, { 0xFAF, "text/html" }, { 0xDFE, "text/csv" },
  { 0xF79, "text/css" }, { 0xF81, "application/javascript" }, { 0xF80, "application/xml" },
  { 0xF13, "application/json" }, { 0xB60, "image/png" }, { 0xC85, "image/jpeg" },
  { 0x695, "image/gif" }, { 0x69C, "image/bmp" }, { 0xFF0, "image/tiff" },
  { 0xAAD, "image/svg+xml" }, { 0xFF9, "image/x-riscos-sprite" }, { 0xADF, "application/pdf" },
  { 0xA91, "application/zip" }, { 0xFB1, "audio/x-wav" }, { 0x1AD, "audio/mpeg" },
  { 0xAFF, "image/x-drawfile" }, { 0xFFD, "application/octet-stream" },
};

/* The program's filter lets the file be seen? (GTK's filters take Unix
   names: "Report.txt", and a MIME type: from the file type, else guessed
   from the name) */
static gboolean
browser_filter_ok (RiscosBox *box, Browser *b, const char *leaf, int type)
{
  guint i;
  GtkFileFilterInfo info;
  char *full, *unix_path, *unix_leaf, *mime;
  gboolean ok;

  if (box->filter == NULL)
    return TRUE;
  full = g_strconcat (b->dir, ".", leaf, NULL);
  unix_path = gdk_riscos_path_to_unix (full);
  unix_leaf = g_path_get_basename (unix_path);
  mime = NULL;
  for (i = 0; i < G_N_ELEMENTS (mime_types) && !mime; i++)
    if (mime_types[i].type == type)
      mime = g_strdup (mime_types[i].mime);
  if (mime == NULL)
    {
      char *ct = g_content_type_guess (unix_leaf, NULL, 0, NULL);
      mime = g_content_type_get_mime_type (ct);
      if (mime == NULL)
        mime = g_strdup ("application/octet-stream");
      g_free (ct);
    }
  info.contains = GTK_FILE_FILTER_FILENAME | GTK_FILE_FILTER_DISPLAY_NAME | GTK_FILE_FILTER_MIME_TYPE;
  info.filename = unix_path;
  info.display_name = unix_leaf;
  info.mime_type = mime;
  info.uri = NULL;
  ok = gtk_file_filter_filter (box->filter, &info);
  g_free (mime);
  g_free (unix_leaf);
  g_free (unix_path);
  g_free (full);
  return ok;
}

static int
browser_compare (GtkTreeModel *model, GtkTreeIter *x, GtkTreeIter *y, gpointer data)
{
  Browser *b = data;
  char *nx, *ny;
  int tx, ty, r = 0;
  gint64 sx, sy, dx, dy;

  gtk_tree_model_get (model, x, COL_NAME, &nx, COL_TYPE, &tx, COL_SIZE, &sx, COL_DATE, &dx, -1);
  gtk_tree_model_get (model, y, COL_NAME, &ny, COL_TYPE, &ty, COL_SIZE, &sy, COL_DATE, &dy, -1);
  switch (b->sort)
    {
    case SORT_TYPE:
      /* directories, applications, then files by type (as the Filer) */
      {
        int kx = tx == FILETYPE_DIR ? -3 : tx == FILETYPE_APP ? -2 : tx;
        int ky = ty == FILETYPE_DIR ? -3 : ty == FILETYPE_APP ? -2 : ty;
        r = kx < ky ? -1 : kx > ky;
      }
      break;
    case SORT_SIZE:
      r = sx > sy ? -1 : sx < sy;             /* largest first */
      break;
    case SORT_DATE:
      r = dx > dy ? -1 : dx < dy;             /* newest first */
      break;
    }
  if (r == 0 && nx && ny)
    r = g_ascii_strcasecmp (nx, ny);
  g_free (nx);
  g_free (ny);
  return r;
}

static void
browser_status (Browser *b, const char *text)
{
  gtk_label_set_text (GTK_LABEL (b->status), text ? text : "");
  gtk_widget_set_visible (b->status, text != NULL);
}

/* Show folder dir; FALSE (and the filing system's message) if it can't
   be read */
static gboolean
browser_go (RiscosBox *box, const char *dir, gboolean report)
{
  Browser *b = box->b;
  char *canon = gdk_riscos_canonicalise (dir);
  GError *error = NULL;
  GPtrArray *list = gdk_riscos_read_dir (canon, &error);
  guint i;

  if (list == NULL)
    {
      if (report)
        gdk_riscos_report_error (error->message);
      else
        browser_status (b, error->message);
      if (debug ())
        g_printerr ("riscos-gtk: browser: %s: %s\n", canon, error->message);
      g_error_free (error);
      g_free (canon);
      return FALSE;
    }
  g_free (b->dir);
  b->dir = canon;
  browser_status (b, NULL);
  gtk_entry_set_text (GTK_ENTRY (b->path), b->dir);
  gtk_editable_set_position (GTK_EDITABLE (b->path), -1);
  b->filling = TRUE;
  gtk_list_store_clear (b->store);
  for (i = 0; i < list->len; i++)
    {
      GdkRiscosDirEntry *e = g_ptr_array_index (list, i);
      int type = e->object_type == 2 ? (e->name[0] == '!' ? FILETYPE_APP : FILETYPE_DIR)
                                     : e->filetype;
      GdkPixbuf *large, *small;
      char *st, *tt, *dt;
      GtkTreeIter it;

      if (type == FILETYPE_DIR || type == FILETYPE_APP)
        ;                       /* always shown */
      else if (box->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER
               || !browser_filter_ok (box, b, e->name, type))
        continue;
      browser_icons (b, e->name, type, &large, &small);
      st = browser_size_text (type, e->length);
      tt = browser_type_text (type);
      dt = browser_date_text (e->date);
      gtk_list_store_insert_with_values (b->store, &it, -1,
                                         COL_LARGE, large, COL_SMALL, small,
                                         COL_NAME, e->name, COL_TYPE, type,
                                         COL_OBJECT, e->object_type,
                                         COL_SIZE, (gint64) e->length, COL_DATE, e->date,
                                         COL_SIZE_TEXT, st, COL_TYPE_TEXT, tt,
                                         COL_DATE_TEXT, dt, -1);
      g_clear_object (&large);
      g_clear_object (&small);
      g_free (st);
      g_free (tt);
      g_free (dt);
    }
  b->filling = FALSE;
  g_ptr_array_unref (list);
  browser_side_sync (box);
  if (debug ())
    g_printerr ("riscos-gtk: browser: %s, %d objects\n", b->dir,
                gtk_tree_model_iter_n_children (GTK_TREE_MODEL (b->store), NULL));
  return TRUE;
}

static void
browser_refresh (RiscosBox *box)
{
  Browser *b = box->b;
  char *dir = g_strdup (b->dir);

  browser_go (box, dir, FALSE);
  g_free (dir);
}

/* ---- selection ---- */

/* The selected objects' paths (the view's own) */
static GList *
browser_selected (Browser *b)
{
  if (b->view == VIEW_FULL)
    return gtk_tree_selection_get_selected_rows (gtk_tree_view_get_selection (GTK_TREE_VIEW (b->tree)), NULL);
  return gtk_icon_view_get_selected_items (GTK_ICON_VIEW (b->icons));
}

static void
browser_row (Browser *b, GtkTreePath *path, char **name, int *type)
{
  GtkTreeIter it;

  *name = NULL;
  *type = 0;
  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (b->store), &it, path))
    gtk_tree_model_get (GTK_TREE_MODEL (b->store), &it, COL_NAME, name, COL_TYPE, type, -1);
}

static void
on_browser_selection (gpointer w, RiscosBox *box)
{
  Browser *b = box->b;
  GList *sel;
  char *name;
  int type;

  if (b == NULL || b->filling)
    return;
  sel = browser_selected (b);
  if (sel && !sel->next)
    {
      gboolean dir;
      browser_row (b, sel->data, &name, &type);
      dir = type == FILETYPE_DIR || type == FILETYPE_APP;
      /* a file's name (Save: to save over it; Open: to open it), or a
         directory's (choosing a folder) */
      if (name && (box->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ? dir : !dir))
        {
          gtk_entry_set_text (GTK_ENTRY (box->entry), name);
          gtk_editable_set_position (GTK_EDITABLE (box->entry), -1);
        }
      g_free (name);
    }
  g_list_free_full (sel, (GDestroyNotify) gtk_tree_path_free);
}

static GFile *
browser_file (Browser *b, const char *leaf)
{
  char *full = is_full_name (leaf) ? g_strdup (leaf) : g_strconcat (b->dir, ".", leaf, NULL);
  GFile *f = file_for (full);

  g_free (full);
  return f;
}

static void
browser_accept (RiscosBox *box, GSList *files)
{
  Browser *b = box->b;

  browser_add_recent (b, b->dir);
  browser_save_choices (b);
  if (box->filter && GTK_IS_FILE_CHOOSER (box->owner))
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (box->owner), box->filter);
  finish_files (box, GTK_RESPONSE_ACCEPT, files);
}

static gboolean
browser_is_dir (GFile *f)
{
  return g_file_query_file_type (f, G_FILE_QUERY_INFO_NONE, NULL) == G_FILE_TYPE_DIRECTORY;
}

static void
on_browser_ok (GtkWidget *w, RiscosBox *box)
{
  Browser *b = box->b;
  const char *name = gtk_entry_get_text (GTK_ENTRY (box->entry));
  GFile *f;

  if (box->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      if (*name == '\0')
        return;
      browser_accept (box, g_slist_prepend (NULL, browser_file (b, name)));
      return;
    }
  if (box->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
    {
      f = *name ? browser_file (b, name) : file_for (b->dir);
      if (!browser_is_dir (f))
        {
          g_object_unref (f);
          f = file_for (b->dir);
        }
      browser_accept (box, g_slist_prepend (NULL, f));
      return;
    }
  /* Open: the files selected (more than one, if the program takes
     several), else the name typed */
  if (box->multiple)
    {
      GList *sel = browser_selected (b), *l;
      GSList *files = NULL;
      for (l = sel; l; l = l->next)
        {
          char *leaf;
          int type;
          browser_row (b, l->data, &leaf, &type);
          if (leaf && type != FILETYPE_DIR && type != FILETYPE_APP)
            files = g_slist_prepend (files, browser_file (b, leaf));
          g_free (leaf);
        }
      g_list_free_full (sel, (GDestroyNotify) gtk_tree_path_free);
      if (files && files->next)
        {
          browser_accept (box, g_slist_reverse (files));
          return;
        }
      g_slist_free_full (files, g_object_unref);
    }
  if (*name == '\0')
    return;
  f = browser_file (b, name);
  if (browser_is_dir (f))
    {
      /* a directory: go into it */
      char *ro = riscos_name_of (f);
      g_object_unref (f);
      if (ro && browser_go (box, ro, TRUE))
        gtk_entry_set_text (GTK_ENTRY (box->entry), "");
      g_free (ro);
      return;
    }
  if (!g_file_query_exists (f, NULL))
    {
      char *text = g_strdup_printf (_("File '%s' not found"), name);
      gdk_riscos_report_error (text);
      g_free (text);
      g_object_unref (f);
      return;
    }
  browser_accept (box, g_slist_prepend (NULL, f));
}

/* Double-click (or Return) on an object */
static void
browser_activate (RiscosBox *box, GtkTreePath *path)
{
  Browser *b = box->b;
  char *name, *full;
  int type;

  browser_row (b, path, &name, &type);
  if (name == NULL)
    return;
  if (type == FILETYPE_DIR || type == FILETYPE_APP)
    {
      full = g_strconcat (b->dir, ".", name, NULL);
      browser_go (box, full, TRUE);
      g_free (full);
    }
  else if (box->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    browser_accept (box, g_slist_prepend (NULL, browser_file (b, name)));
  else if (box->action == GTK_FILE_CHOOSER_ACTION_SAVE)
    gtk_entry_set_text (GTK_ENTRY (box->entry), name);
  g_free (name);
}

static void
on_icon_activated (GtkIconView *v, GtkTreePath *path, RiscosBox *box)
{
  browser_activate (box, path);
}

static void
on_row_activated (GtkTreeView *v, GtkTreePath *path, GtkTreeViewColumn *c, RiscosBox *box)
{
  browser_activate (box, path);
}

static void
on_browser_parent (GtkWidget *w, RiscosBox *box)
{
  Browser *b = box->b;
  char *dot = strrchr (b->dir, '.');
  char *parent;

  if (dot == NULL || dot[1] == '$' || g_str_has_suffix (b->dir, ":$"))
    return;                     /* the root */
  parent = g_strndup (b->dir, dot - b->dir);
  browser_go (box, parent, TRUE);
  g_free (parent);
}

static void
on_browser_path (GtkEntry *e, RiscosBox *box)
{
  browser_go (box, gtk_entry_get_text (e), TRUE);
}

/* A directory dragged from the Filer is shown; a file is chosen (Open)
   or its name taken (Save) */
static gboolean
on_browser_load (const char *riscos_path, int filetype, gpointer data)
{
  RiscosBox *box = data;
  Browser *b = box->b;
  const char *leaf = leaf_of (riscos_path);
  char *parent;

  if (b == NULL)
    return FALSE;
  if (filetype == FILETYPE_DIR || filetype == FILETYPE_APP)
    {
      browser_go (box, riscos_path, TRUE);
      return TRUE;
    }
  parent = leaf > riscos_path ? g_strndup (riscos_path, leaf - riscos_path - 1) : NULL;
  if (parent)
    browser_go (box, parent, TRUE);
  g_free (parent);
  if (box->action == GTK_FILE_CHOOSER_ACTION_OPEN)
    {
      box->dropped = file_for (riscos_path);
      g_idle_add (open_idle, box);
    }
  else
    gtk_entry_set_text (GTK_ENTRY (box->entry), leaf);
  return TRUE;
}

static void
on_browser_realize (GtkWidget *w, RiscosBox *box)
{
  gdk_riscos_window_set_load_func (gtk_widget_get_window (w), on_browser_load, box);
}

/* ---- views ---- */

static void
browser_set_view (RiscosBox *box, int view)
{
  Browser *b = box->b;
  GtkWidget *now = gtk_bin_get_child (GTK_BIN (b->scroll));
  GtkWidget *want = view == VIEW_FULL ? b->tree : b->icons;
  GList *sel = now ? browser_selected (b) : NULL, *l;

  b->view = view;
  if (view != VIEW_FULL)
    {
      GtkIconView *iv = GTK_ICON_VIEW (b->icons);
      gtk_icon_view_set_pixbuf_column (iv, view == VIEW_LARGE ? COL_LARGE : COL_SMALL);
      gtk_icon_view_set_item_orientation (iv, view == VIEW_LARGE ? GTK_ORIENTATION_VERTICAL
                                                                 : GTK_ORIENTATION_HORIZONTAL);
      gtk_icon_view_set_item_width (iv, view == VIEW_LARGE ? 100 : 160);
      gtk_icon_view_set_row_spacing (iv, view == VIEW_LARGE ? 6 : 0);
      gtk_icon_view_set_spacing (iv, view == VIEW_LARGE ? 0 : 4);
      g_object_set (g_object_get_data (G_OBJECT (b->icons), "riscos-gtk-text"),
                    "xalign", view == VIEW_LARGE ? 0.5 : 0.0,
                    "alignment", view == VIEW_LARGE ? PANGO_ALIGN_CENTER : PANGO_ALIGN_LEFT, NULL);
    }
  if (now != want)
    {
      if (now)
        gtk_container_remove (GTK_CONTAINER (b->scroll), now);
      gtk_container_add (GTK_CONTAINER (b->scroll), want);
      gtk_widget_show_all (want);
      /* the same objects selected in the new view */
      b->filling = TRUE;
      if (view == VIEW_FULL)
        gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (GTK_TREE_VIEW (b->tree)));
      else
        gtk_icon_view_unselect_all (GTK_ICON_VIEW (b->icons));
      for (l = sel; l; l = l->next)
        if (view == VIEW_FULL)
          gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW (b->tree)), l->data);
        else
          gtk_icon_view_select_path (GTK_ICON_VIEW (b->icons), l->data);
      b->filling = FALSE;
    }
  g_list_free_full (sel, (GDestroyNotify) gtk_tree_path_free);
}

static void
browser_resort (Browser *b)
{
  /* the sort function reads b->sort: set it again to re-sort */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (b->store),
                                        GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (b->store),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
}

static void
on_menu_view (GtkCheckMenuItem *item, RiscosBox *box)
{
  if (debug ())
    g_printerr ("riscos-gtk: browser: view %d %s\n",
                GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "riscos-gtk-n")),
                gtk_check_menu_item_get_active (item) ? "on" : "off");
  if (box->b && gtk_check_menu_item_get_active (item))
    {
      browser_set_view (box, GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "riscos-gtk-n")));
      browser_save_choices (box->b);
    }
}

static void
on_menu_sort (GtkCheckMenuItem *item, RiscosBox *box)
{
  if (box->b && gtk_check_menu_item_get_active (item))
    {
      box->b->sort = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "riscos-gtk-n"));
      browser_resort (box->b);
      browser_save_choices (box->b);
    }
}

static void
on_menu_refresh (GtkMenuItem *item, RiscosBox *box)
{
  if (box->b)
    browser_refresh (box);
}

static void
on_use_box (GtkMenuItem *item, RiscosBox *box)
{
  box_close_window (box);
  box->browser = FALSE;
  box_show (box);
}

/* New directory: a small dialogue box for its name */
static void
on_newdir_response (GtkDialog *d, int response, RiscosBox *box)
{
  GtkWidget *entry = g_object_get_data (G_OBJECT (d), "riscos-gtk-entry");
  char *name = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

  gtk_widget_destroy (GTK_WIDGET (d));
  if (response == GTK_RESPONSE_OK && *name && box->b)
    {
      GFile *f = browser_file (box->b, name);
      GError *error = NULL;
      if (!g_file_make_directory (f, NULL, &error))
        {
          gdk_riscos_report_error (error->message);
          g_error_free (error);
        }
      else
        browser_refresh (box);
      g_object_unref (f);
    }
  g_free (name);
}

static void
on_menu_newdir (GtkMenuItem *item, RiscosBox *box)
{
  GtkWidget *d = gtk_dialog_new_with_buttons (_("New directory"), GTK_WINDOW (box->window),
                                              GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                              _("Cancel"), GTK_RESPONSE_CANCEL,
                                              _("Create"), GTK_RESPONSE_OK, NULL);
  GtkWidget *entry = gtk_entry_new ();

  gtk_entry_set_text (GTK_ENTRY (entry), _("Directory"));
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  gtk_dialog_set_default_response (GTK_DIALOG (d), GTK_RESPONSE_OK);
  gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (d))), 8);
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (d))), entry);
  g_object_set_data (G_OBJECT (d), "riscos-gtk-entry", entry);
  g_signal_connect (d, "response", G_CALLBACK (on_newdir_response), box);
  gtk_widget_show_all (d);
}

static GtkWidget *
radio_item (GSList **group, const char *label, int n, int now, GCallback cb, RiscosBox *box)
{
  GtkWidget *item = gtk_radio_menu_item_new_with_label (*group, label);

  *group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
  g_object_set_data (G_OBJECT (item), "riscos-gtk-n", GINT_TO_POINTER (n));
  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), n == now);
  g_signal_connect (item, "toggled", cb, box);
  return item;
}

/* The browser's menu (Menu over the objects), as the Filer's:
   Display > Large icons / Small icons / Full info / Sort by ...;
   New directory...; Refresh; Save box / Open box */
static GtkWidget *
browser_menu (RiscosBox *box)
{
  Browser *b = box->b;
  GtkWidget *menu = gtk_menu_new (), *display = gtk_menu_new (), *item;
  GSList *views = NULL, *sorts = NULL;
  const char *views_text[] = { N_("Large icons"), N_("Small icons"), N_("Full info") };
  const char *sorts_text[] = { N_("Sort by name"), N_("Sort by type"), N_("Sort by size"), N_("Sort by date") };
  int i;

  for (i = 0; i < 3; i++)
    gtk_menu_shell_append (GTK_MENU_SHELL (display),
                           radio_item (&views, _(views_text[i]), i, b->view, G_CALLBACK (on_menu_view), box));
  gtk_menu_shell_append (GTK_MENU_SHELL (display), gtk_separator_menu_item_new ());
  for (i = 0; i < 4; i++)
    gtk_menu_shell_append (GTK_MENU_SHELL (display),
                           radio_item (&sorts, _(sorts_text[i]), i, b->sort, G_CALLBACK (on_menu_sort), box));
  gtk_widget_show_all (display);      /* a submenu isn't shown with its parent */
  item = gtk_menu_item_new_with_label (_("Display"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), display);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  if (box->action != GTK_FILE_CHOOSER_ACTION_OPEN)
    {
      item = gtk_menu_item_new_with_label (_("New directory..."));
      g_signal_connect (item, "activate", G_CALLBACK (on_menu_newdir), box);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    }
  item = gtk_menu_item_new_with_label (_("Refresh"));
  g_signal_connect (item, "activate", G_CALLBACK (on_menu_refresh), box);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
  item = gtk_menu_item_new_with_label (box->action == GTK_FILE_CHOOSER_ACTION_SAVE ? _("Save box")
                                                                                   : _("Open box"));
  g_signal_connect (item, "activate", G_CALLBACK (on_use_box), box);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show_all (menu);
  return menu;
}

static gboolean
on_browser_button (GtkWidget *w, GdkEventButton *ev, RiscosBox *box)
{
  GtkWidget *menu;

  if (!gdk_event_triggers_context_menu ((GdkEvent *) ev) || box->b == NULL)
    return FALSE;
  menu = browser_menu (box);
  keep_menu (box->window, menu, w);
  gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) ev);
  return TRUE;
}

/* ---- the sidebar (as the Finder's): Discs, Places, Recent ---- */

/* The icon bar sprite for a filing system's discs */
static const char *
fs_sprite (const char *fs)
{
  static const struct { const char *fs, *sprite; } map[] = {
    { "RAM", "ramfs" }, { "Resources", "romapps" }, { "CDFS", "cddisc" },
    { "Share", "fileserver" }, { "ShareFS", "fileserver" }, { "Net", "fileserver" },
    { "LanMan", "fileserver" }, { "LanManFS", "fileserver" }, { "LanMan98", "fileserver" }, { "Sunfish", "fileserver" },
    { "NFS", "fileserver" }, { "OmniClient", "fileserver" },
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (map); i++)
    if (g_ascii_strcasecmp (fs, map[i].fs) == 0)
      return map[i].sprite;
  return "harddisc";
}

/* A sprite at small icon size (17 pixels at 90 dpi), or a theme icon */
static GdkPixbuf *
side_icon (Browser *b, const char *sprite, const char *theme)
{
  GdkPixbuf *pb = browser_sprite (b, sprite);

  if (pb == NULL)
    return theme ? browser_theme_icon (theme, 17) : NULL;
  if (gdk_pixbuf_get_height (pb) > 20)
    {
      int h = gdk_pixbuf_get_height (pb), w = gdk_pixbuf_get_width (pb);
      return gdk_pixbuf_scale_simple (pb, MAX (1, w * 17 / h), 17, GDK_INTERP_BILINEAR);
    }
  return g_object_ref (pb);
}

static void
side_heading (GtkWidget *list, const char *text)
{
  GtkWidget *row = gtk_list_box_row_new (), *label = gtk_label_new (text);

  gtk_style_context_add_class (gtk_widget_get_style_context (label), "riscos-sidebar-heading");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_widget_set_margin_start (label, 8);
  gtk_widget_set_margin_top (label, 8);
  gtk_widget_set_margin_bottom (label, 2);
  gtk_container_add (GTK_CONTAINER (row), label);
  gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  gtk_container_add (GTK_CONTAINER (list), row);
}

static void
side_item (Browser *b, const char *label, const char *dir, GdkPixbuf *icon)
{
  GtkWidget *row = gtk_list_box_row_new ();
  GtkWidget *hb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *text = gtk_label_new (label);

  gtk_widget_set_margin_start (hb, 14);
  gtk_widget_set_margin_end (hb, 6);
  gtk_widget_set_margin_top (hb, 2);
  gtk_widget_set_margin_bottom (hb, 2);
  if (icon)
    gtk_box_pack_start (GTK_BOX (hb), gtk_image_new_from_pixbuf (icon), FALSE, FALSE, 0);
  gtk_label_set_xalign (GTK_LABEL (text), 0);
  gtk_label_set_ellipsize (GTK_LABEL (text), PANGO_ELLIPSIZE_MIDDLE);
  gtk_box_pack_start (GTK_BOX (hb), text, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (row), hb);
  gtk_widget_set_tooltip_text (row, dir);
  g_object_set_data_full (G_OBJECT (row), "riscos-gtk-dir", g_strdup (dir), g_free);
  gtk_container_add (GTK_CONTAINER (b->side), row);
  g_clear_object (&icon);
}

/* The leaf of a folder's name, for the sidebar ("Work", "SDFS:$") */
static char *
dir_label (const char *dir)
{
  const char *leaf = leaf_of (dir);

  return g_strdup (*leaf && strcmp (leaf, "$") != 0 ? leaf : dir);
}

static void
on_side_selected (GtkListBox *list, GtkListBoxRow *row, RiscosBox *box)
{
  const char *dir = row ? g_object_get_data (G_OBJECT (row), "riscos-gtk-dir") : NULL;

  char *canon;

  if (box->b == NULL || box->b->filling || dir == NULL)
    return;
  canon = gdk_riscos_canonicalise (dir);
  if (box->b->dir && g_ascii_strcasecmp (canon, box->b->dir) == 0)
    {
      g_free (canon);
      return;                   /* shown already */
    }
  g_free (canon);
  if (!browser_go (box, dir, TRUE))
    browser_side_sync (box);
}

/* Select the sidebar's row for the folder shown, if it has one */
static void
browser_side_sync (RiscosBox *box)
{
  Browser *b = box->b;
  GList *rows, *l;
  GtkListBoxRow *found = NULL;
  char *canon;

  if (b == NULL || b->side == NULL || b->dir == NULL)
    return;
  rows = gtk_container_get_children (GTK_CONTAINER (b->side));
  for (l = rows; l && !found; l = l->next)
    {
      const char *dir = g_object_get_data (l->data, "riscos-gtk-dir");
      if (dir == NULL)
        continue;
      canon = gdk_riscos_canonicalise (dir);
      if (g_ascii_strcasecmp (canon, b->dir) == 0)
        found = l->data;
      g_free (canon);
    }
  g_list_free (rows);
  b->filling = TRUE;
  if (found)
    gtk_list_box_select_row (GTK_LIST_BOX (b->side), found);
  else
    gtk_list_box_unselect_all (GTK_LIST_BOX (b->side));
  b->filling = FALSE;
}

static GtkWidget *
browser_sidebar (RiscosBox *box)
{
  Browser *b = box->b;
  GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);
  GPtrArray *discs = gdk_riscos_discs ();
  static const struct { const char *label, *var, *sprite; } vars[] = {
    { N_("Boot"), "Boot$Dir", "sm!boot" }, { N_("Choices"), "Choices$Write", "small_dir" },
    { N_("Scrap"), "Wimp$ScrapDir", "small_dir" },
  };
  int i;

  b->side = gtk_list_box_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (b->side), "riscos-sidebar");
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (b->side), GTK_SELECTION_SINGLE);
  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (b->side), TRUE);

  side_heading (b->side, _("Discs"));
  for (i = 0; i < (int) discs->len; i++)
    {
      GdkRiscosDisc *d = discs->pdata[i];
      GdkPixbuf *icon = d->sprite ? side_icon (b, d->sprite, NULL) : NULL;
      if (icon == NULL)
        icon = side_icon (b, fs_sprite (d->fs), "drive-harddisk");
      side_item (b, d->name, d->root, icon);
    }
  g_ptr_array_unref (discs);

  side_heading (b->side, _("Places"));
  if (box->folder)
    {
      char *dir = riscos_name_of (box->folder), *label = dir ? dir_label (dir) : NULL;
      if (dir)
        side_item (b, label, dir, side_icon (b, "small_dir", "folder"));
      g_free (label);
      g_free (dir);
    }
  for (i = 0; i < (int) G_N_ELEMENTS (vars); i++)
    if (g_getenv (vars[i].var))
      {
        char *dir = g_strdup_printf ("<%s>", vars[i].var);
        side_item (b, _(vars[i].label), dir, side_icon (b, vars[i].sprite, "folder"));
        g_free (dir);
      }

  if (b->recent && b->recent[0])
    {
      side_heading (b->side, _("Recent"));
      for (i = 0; b->recent[i] && i < RECENT_MAX; i++)
        {
          char *label = dir_label (b->recent[i]);
          side_item (b, label, b->recent[i], side_icon (b, "small_dir", "folder"));
          g_free (label);
        }
    }
  g_signal_connect (b->side, "row-selected", G_CALLBACK (on_side_selected), box);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scroll), b->side);
  gtk_widget_set_size_request (scroll, 150, -1);
  return scroll;
}

static void
on_filter_changed (GtkComboBox *combo, RiscosBox *box)
{
  int i = gtk_combo_box_get_active (combo);
  GtkFileFilter *f = i >= 0 ? g_slist_nth_data (box->filters, i) : NULL;

  g_clear_object (&box->filter);
  box->filter = f ? g_object_ref (f) : NULL;
  if (box->b)
    browser_refresh (box);
}

/* ---- the window ---- */

static GtkWidget *
browser_icon_view (RiscosBox *box)
{
  Browser *b = box->b;
  GtkWidget *iv = gtk_icon_view_new_with_model (GTK_TREE_MODEL (b->store));
  GtkCellRenderer *text;

  gtk_icon_view_set_text_column (GTK_ICON_VIEW (iv), -1);
  gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (iv), COL_LARGE);
  /* the name: ellipsized in the middle, as the Filer truncates */
  text = gtk_cell_renderer_text_new ();
  g_object_set (text, "ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (iv), text, FALSE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (iv), text, "text", COL_NAME);
  g_object_set_data (G_OBJECT (iv), "riscos-gtk-text", text);
  gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (iv), box->multiple ? GTK_SELECTION_MULTIPLE
                                                                      : GTK_SELECTION_SINGLE);
  gtk_icon_view_set_activate_on_single_click (GTK_ICON_VIEW (iv), FALSE);
  gtk_icon_view_set_margin (GTK_ICON_VIEW (iv), 6);
  gtk_icon_view_set_column_spacing (GTK_ICON_VIEW (iv), 4);
  g_signal_connect (iv, "item-activated", G_CALLBACK (on_icon_activated), box);
  g_signal_connect (iv, "selection-changed", G_CALLBACK (on_browser_selection), box);
  g_signal_connect (iv, "button-press-event", G_CALLBACK (on_browser_button), box);
  return g_object_ref_sink (iv);
}

static GtkWidget *
browser_tree_view (RiscosBox *box)
{
  Browser *b = box->b;
  GtkWidget *tv = gtk_tree_view_new_with_model (GTK_TREE_MODEL (b->store));
  GtkTreeViewColumn *c;
  GtkCellRenderer *r;

  c = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (c, _("Name"));
  r = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (c, r, FALSE);
  gtk_tree_view_column_add_attribute (c, r, "pixbuf", COL_SMALL);
  r = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (c, r, TRUE);
  gtk_tree_view_column_add_attribute (c, r, "text", COL_NAME);
  gtk_tree_view_column_set_expand (c, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), c);
  r = gtk_cell_renderer_text_new ();
  c = gtk_tree_view_column_new_with_attributes (_("Type"), r, "text", COL_TYPE_TEXT, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), c);
  r = gtk_cell_renderer_text_new ();
  g_object_set (r, "xalign", 1.0, NULL);
  c = gtk_tree_view_column_new_with_attributes (_("Size"), r, "text", COL_SIZE_TEXT, NULL);
  gtk_tree_view_column_set_alignment (c, 1.0);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), c);
  r = gtk_cell_renderer_text_new ();
  c = gtk_tree_view_column_new_with_attributes (_("Date"), r, "text", COL_DATE_TEXT, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), c);
  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (tv)),
                               box->multiple ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_SINGLE);
  g_signal_connect (tv, "row-activated", G_CALLBACK (on_row_activated), box);
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (tv)), "changed",
                    G_CALLBACK (on_browser_selection), box);
  g_signal_connect (tv, "button-press-event", G_CALLBACK (on_browser_button), box);
  return g_object_ref_sink (tv);
}

/* The first folder to show: the program's, else the last one used,
   else the current directory */
static void
browser_start (RiscosBox *box)
{
  Browser *b = box->b;
  char *dir = riscos_name_of (box->folder);
  int i;

  if (dir && browser_go (box, dir, FALSE))
    goto done;
  for (i = 0; b->recent && b->recent[i]; i++)
    if (browser_go (box, b->recent[i], FALSE))
      goto done;
  if (browser_go (box, "@", FALSE))
    goto done;
  if (!b->dir)
    {
      b->dir = g_strdup ("$");
      gtk_entry_set_text (GTK_ENTRY (b->path), b->dir);
    }
done:
  g_free (dir);
}

static void
browser_build (RiscosBox *box)
{
  Browser *b = g_new0 (Browser, 1);
  gboolean save = box->action == GTK_FILE_CHOOSER_ACTION_SAVE;
  GtkWidget *vb, *bar, *w, *bottom;
  GtkStyleContext *sc;

  box->b = b;
  b->sprites = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, unref_or_null);
  browser_load_choices (b);
  b->store = gtk_list_store_new (N_COLS, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT,
                                 G_TYPE_INT, G_TYPE_INT64, G_TYPE_INT64,
                                 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (b->store), browser_compare, b, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (b->store),
                                        GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);

  box->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (box->window), box->title ? box->title : save ? _("Save as") : _("Open"));
  gtk_window_set_type_hint (GTK_WINDOW (box->window), GDK_WINDOW_TYPE_HINT_DIALOG);
  if (box->parent)
    gtk_window_set_transient_for (GTK_WINDOW (box->window), box->parent);
  gtk_window_set_position (GTK_WINDOW (box->window), GTK_WIN_POS_MOUSE);
  gtk_window_set_default_size (GTK_WINDOW (box->window), 720, 420);
  gtk_window_set_modal (GTK_WINDOW (box->window), box->modal);
  sc = gtk_widget_get_style_context (box->window);
  gtk_style_context_add_class (sc, "riscos-filer");
  g_signal_connect (box->window, "destroy", G_CALLBACK (on_box_destroy), box);
  g_signal_connect (box->window, "delete-event", G_CALLBACK (on_delete), box);
  g_signal_connect (box->window, "key-press-event", G_CALLBACK (on_key), box);
  g_signal_connect (box->window, "realize", G_CALLBACK (on_browser_realize), box);

  vb = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (box->window), vb);

  /* Parent, the folder's name */
  bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_container_set_border_width (GTK_CONTAINER (bar), 4);
  w = action_button (_("Parent"));
  gtk_widget_set_tooltip_text (w, _("The directory this one is in"));
  g_signal_connect (w, "clicked", G_CALLBACK (on_browser_parent), box);
  gtk_box_pack_start (GTK_BOX (bar), w, FALSE, FALSE, 0);
  b->path = gtk_entry_new ();
  gtk_widget_set_tooltip_text (b->path, _("Type a directory's full name and press Return"));
  g_signal_connect (b->path, "activate", G_CALLBACK (on_browser_path), box);
  gtk_box_pack_start (GTK_BOX (bar), b->path, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vb), bar, FALSE, FALSE, 0);

  b->status = gtk_label_new (NULL);
  gtk_widget_set_no_show_all (b->status, TRUE);
  gtk_box_pack_start (GTK_BOX (vb), b->status, FALSE, FALSE, 2);

  /* the sidebar, then the objects */
  {
    GtkWidget *paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_pack1 (GTK_PANED (paned), browser_sidebar (box), FALSE, FALSE);
    b->scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (b->scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_paned_pack2 (GTK_PANED (paned), b->scroll, TRUE, FALSE);
    gtk_box_pack_start (GTK_BOX (vb), paned, TRUE, TRUE, 0);
  }
  b->icons = browser_icon_view (box);
  b->tree = browser_tree_view (box);
  browser_set_view (box, b->view);

  /* the name, the filters, Cancel, Save/Open */
  bottom = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (bottom), 6);
  box->entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (box->entry), TRUE);
  if (save)
    {
      char *leaf = g_strdup (leaf_of (box->initial_name ? box->initial_name : _("Untitled")));
      box->icon = gtk_event_box_new ();
      gtk_event_box_set_visible_window (GTK_EVENT_BOX (box->icon), FALSE);
      gtk_widget_add_events (box->icon, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                             | GDK_BUTTON_MOTION_MASK);
      gtk_container_add (GTK_CONTAINER (box->icon), file_icon (leaf_type (leaf), &box->sprite));
      gtk_widget_set_size_request (box->icon, 34, 34);
      gtk_widget_set_tooltip_text (box->icon, _("Drag this icon to a directory display to save there"));
      g_signal_connect (box->icon, "button-press-event", G_CALLBACK (on_icon_press), box);
      g_signal_connect (box->icon, "motion-notify-event", G_CALLBACK (on_icon_motion), box);
      g_signal_connect (box->icon, "button-release-event", G_CALLBACK (on_icon_release), box);
      gtk_box_pack_start (GTK_BOX (bottom), box->icon, FALSE, FALSE, 0);
      gtk_entry_set_text (GTK_ENTRY (box->entry), leaf);
      g_signal_connect (box->entry, "changed", G_CALLBACK (on_name_changed), box);
      g_free (leaf);
    }
  else
    {
      w = gtk_label_new (box->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ? _("Directory") : _("Name"));
      gtk_box_pack_start (GTK_BOX (bottom), w, FALSE, FALSE, 0);
    }
  gtk_widget_set_valign (box->entry, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (bottom), box->entry, TRUE, TRUE, 0);
  if (box->filters)
    {
      GSList *l;
      int i, active = 0;
      b->filter_combo = gtk_combo_box_text_new ();
      for (l = box->filters, i = 0; l; l = l->next, i++)
        {
          const char *name = gtk_file_filter_get_name (l->data);
          gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (b->filter_combo), name ? name : "*");
          if (l->data == box->filter)
            active = i;
        }
      if (box->filter == NULL)
        box->filter = g_object_ref (box->filters->data);
      gtk_combo_box_set_active (GTK_COMBO_BOX (b->filter_combo), active);
      g_signal_connect (b->filter_combo, "changed", G_CALLBACK (on_filter_changed), box);
      gtk_widget_set_valign (b->filter_combo, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (bottom), b->filter_combo, FALSE, FALSE, 0);
    }
  w = buttons (box, G_CALLBACK (on_browser_ok),
               save ? _("Save") : box->action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ? _("Choose") : _("Open"));
  gtk_box_set_homogeneous (GTK_BOX (w), FALSE);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_box_pack_end (GTK_BOX (bottom), w, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (vb), bottom, FALSE, FALSE, 0);

  browser_start (box);
}

/* ---- GtkFileChooserNative ---- */

static void
native_done (GObject *owner, int response, GSList *files)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (owner);

  self->mode_data = NULL;
  if (files)
    {
      g_slist_free_full (self->custom_files, g_object_unref);
      self->custom_files = g_slist_copy_deep (files, (GCopyFunc) g_object_ref, NULL);
    }
  _gtk_native_dialog_emit_response (GTK_NATIVE_DIALOG (self), response);
}

gboolean
gtk_file_chooser_native_riscos_show (GtkFileChooserNative *self)
{
  GtkFileChooserAction action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self));
  char *name = NULL;
  GSList *filters;

  if (!usable (action))
    return FALSE;
  if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      name = riscos_name_of (self->current_file);
      if (name == NULL && self->current_name)
        name = gdk_riscos_path_from_unix (self->current_name);
    }
  filters = gtk_file_chooser_list_filters (GTK_FILE_CHOOSER (self));
  self->mode_data = box_open (G_OBJECT (self), native_done, action,
                              gtk_native_dialog_get_title (GTK_NATIVE_DIALOG (self)),
                              gtk_native_dialog_get_transient_for (GTK_NATIVE_DIALOG (self)),
                              gtk_native_dialog_get_modal (GTK_NATIVE_DIALOG (self)),
                              self->current_folder, name,
                              self->accept_label, self->cancel_label,
                              gtk_file_chooser_get_select_multiple (GTK_FILE_CHOOSER (self)),
                              filters, gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (self)));
  g_slist_free (filters);
  g_free (name);
  return TRUE;
}



void
gtk_file_chooser_native_riscos_hide (GtkFileChooserNative *self)
{
  RiscosBox *box = self->mode_data;

  if (box == NULL)
    return;
  self->mode_data = NULL;
  g_object_set_data (G_OBJECT (self), "riscos-gtk-box", NULL);
  box_free (box);
}

/* ---- GtkFileChooserDialog (a program's own file dialogue) ---- */

/* The label of the dialogue's button for response id, or NULL */
static const char *
button_label (GtkDialog *dialog, int id)
{
  GtkWidget *b = gtk_dialog_get_widget_for_response (dialog, id);
  const char *label = b && GTK_IS_BUTTON (b) ? gtk_button_get_label (GTK_BUTTON (b)) : NULL;
  GtkStockItem item;

  /* a stock button (Geany's "gtk-open"): the stock item's label */
  if (label && gtk_button_get_use_stock (GTK_BUTTON (b)) && gtk_stock_lookup (label, &item))
    return item.label;
  return label;
}

static void
dialog_done (GObject *owner, int response, GSList *files)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (owner);

  g_object_set_data_full (owner, "riscos-gtk-files",
                          files ? g_slist_copy_deep (files, (GCopyFunc) g_object_ref, NULL) : NULL,
                          files ? (GDestroyNotify) riscos_files_free : NULL);
  _gtk_file_chooser_dialog_riscos_respond (dialog, response == GTK_RESPONSE_ACCEPT,
                                           response == GTK_RESPONSE_DELETE_EVENT);
}

static void
on_dialog_destroy (GtkWidget *dialog, gpointer data)
{
  RiscosBox *box = g_object_get_data (G_OBJECT (dialog), "riscos-gtk-box");

  if (box)
    {
      g_object_set_data (G_OBJECT (dialog), "riscos-gtk-box", NULL);
      box_free (box);
    }
}

gboolean
_gtk_file_chooser_riscos_dialog_show (GtkFileChooserDialog *dialog)
{
  GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
  GtkFileChooserAction action = gtk_file_chooser_get_action (chooser);
  GFile *folder;
  GSList *filters;
  char *name = NULL;
  int accept = _gtk_file_chooser_dialog_riscos_accept_id (dialog);

  if (!usable (action))
    return FALSE;
  if (g_object_get_data (G_OBJECT (dialog), "riscos-gtk-box"))
    return TRUE;                /* already open */
  g_object_set_data (G_OBJECT (dialog), "riscos-gtk-files", NULL);
  folder = gtk_file_chooser_get_current_folder_file (chooser);
  if (folder == NULL)
    {
      /* set, but not read yet by the hidden widget (gtkfilechooser.c) */
      GFile *set = g_object_get_data (G_OBJECT (dialog), "riscos-gtk-folder");
      folder = set ? g_object_ref (set) : NULL;
    }
  filters = gtk_file_chooser_list_filters (chooser);
  if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      char *leaf = gtk_file_chooser_get_current_name (chooser);
      if (leaf && *leaf)
        {
          char *ro_leaf = gdk_riscos_path_from_unix (leaf);
          GFile *existing = folder ? g_file_get_child (folder, leaf) : NULL;
          /* a file saved before: its full name, so Save saves it again */
          if (existing && g_file_query_exists (existing, NULL))
            name = riscos_name_of (existing);
          if (name == NULL)
            name = g_strdup (ro_leaf);
          g_free (ro_leaf);
          g_clear_object (&existing);
        }
      g_free (leaf);
    }
  box_open (G_OBJECT (dialog), dialog_done, action,
            gtk_window_get_title (GTK_WINDOW (dialog)),
            gtk_window_get_transient_for (GTK_WINDOW (dialog)),
            gtk_window_get_modal (GTK_WINDOW (dialog)),
            folder, name, button_label (GTK_DIALOG (dialog), accept),
            button_label (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL),
            gtk_file_chooser_get_select_multiple (chooser), filters,
            gtk_file_chooser_get_filter (chooser));
  g_slist_free (filters);
  if (!g_signal_handler_find (dialog, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_dialog_destroy, NULL))
    g_signal_connect (dialog, "destroy", G_CALLBACK (on_dialog_destroy), NULL);
  g_clear_object (&folder);
  g_free (name);
  return TRUE;
}
