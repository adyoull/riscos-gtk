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
 * *Set RISCOS_GTK_NATIVE_CHOOSER 0 uses GTK's own file chooser instead
 * (for both).
 *
 * Part of GTK: LGPL version 2.1 or later. Copyright (C) 2026 Andrew Youll.
 */
#include "config.h"

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

#include "riscos/gdkriscos.h"
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>

#define FILETYPE_DIR   0x1000
#define FILETYPE_APP   0x2000

typedef struct _RiscosBox RiscosBox;
/* Answer the owner: response GTK_RESPONSE_ACCEPT (file set), _CANCEL or
   _DELETE_EVENT */
typedef void (*RiscosBoxDone) (GObject *owner, int response, GFile *file);

struct _RiscosBox
{
  GObject *owner;               /* the GtkFileChooserNative or GtkFileChooserDialog */
  RiscosBoxDone done;
  GFile *folder;                /* the program's current folder, or NULL */
  GFile *dropped;               /* a file dropped on an Open box */
  GtkFileChooserAction action;
  GtkWidget *window, *entry, *icon;
  char *sprite;                 /* the icon's sprite (file_xxx) */
  const char *accept_label, *cancel_label;   /* the program's, or NULL (only while opening) */
  gboolean pressed, dragging;
  double press_x, press_y;
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
    return 0xFFD;               /* Data */
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

/* Close the box and answer the program (file set, for ACCEPT) */
static void
finish (RiscosBox *box, int response, GFile *file)
{
  GObject *owner = g_object_ref (box->owner);
  RiscosBoxDone done = box->done;

  if (g_object_get_data (owner, "riscos-gtk-box") == box)
    g_object_set_data (owner, "riscos-gtk-box", NULL);
  box_free (box);
  done (owner, response, file);
  g_clear_object (&file);
  g_object_unref (owner);
}

static void
box_free (RiscosBox *box)
{
  g_idle_remove_by_data (box);
  g_clear_object (&box->dropped);
  g_clear_object (&box->folder);
  if (box->window)
    {
      g_signal_handlers_disconnect_by_func (box->window, on_box_destroy, box);
      gtk_widget_destroy (box->window);
    }
  g_free (box->sprite);
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

/* Open a Save box or an Open box for owner. name: the RISC OS name to
   start with (Save; NULL: "Untitled"); folder: the program's folder. */
static RiscosBox *
box_open (GObject *owner, RiscosBoxDone done, GtkFileChooserAction action,
          const char *title, GtkWindow *parent, gboolean modal,
          GFile *folder, const char *initial_name,
          const char *accept_label, const char *cancel_label)
{
  RiscosBox *box;
  GtkWidget *vb, *w;
  gboolean save = action == GTK_FILE_CHOOSER_ACTION_SAVE;

  box = g_new0 (RiscosBox, 1);
  box->owner = owner;
  box->done = done;
  box->folder = folder ? g_object_ref (folder) : NULL;
  box->accept_label = accept_label;
  box->cancel_label = cancel_label;
  box->action = action;
  box->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (box->window), title ? title : save ? _("Save as") : _("Open"));
  /* a dialogue box: title bar only (no close icon, it has Cancel; no
     back or size icons); opened at the pointer */
  gtk_window_set_resizable (GTK_WINDOW (box->window), FALSE);
  gtk_window_set_deletable (GTK_WINDOW (box->window), FALSE);
  gtk_window_set_type_hint (GTK_WINDOW (box->window), GDK_WINDOW_TYPE_HINT_DIALOG);
  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (box->window), parent);
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
  gtk_window_set_modal (GTK_WINDOW (box->window), modal);
  g_signal_connect (box->window, "destroy", G_CALLBACK (on_box_destroy), box);
  g_signal_connect (box->window, "delete-event", G_CALLBACK (on_delete), box);
  g_signal_connect (box->window, "key-press-event", G_CALLBACK (on_key), box);

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
      char *name = g_strdup (initial_name ? initial_name : _("Untitled"));
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
      gtk_widget_set_margin_top (sel, 7);
      gtk_widget_set_margin_start (sel, 6);
      gtk_box_pack_start (GTK_BOX (col), sel, FALSE, FALSE, 0);

      cancel = action_button (cancel_label ? cancel_label : _("Cancel"));
      okb = action_button (accept_label ? accept_label : _("Save"));
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
      gboolean dirs = action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
      char *name = riscos_name_of (folder);

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
      gtk_box_pack_start (GTK_BOX (vb), buttons (box, G_CALLBACK (on_open_ok), _("Open")),
                          FALSE, FALSE, 0);
      g_signal_connect (box->window, "realize", G_CALLBACK (on_open_realize), box);
      g_free (name);
    }

  g_object_set_data (owner, "riscos-gtk-box", box);
  gtk_widget_show_all (box->window);
  /* the caret at the end of the name, as in a Save As box */
  gtk_widget_grab_focus (box->entry);
  gtk_editable_set_position (GTK_EDITABLE (box->entry), -1);
  if (debug ())
    g_printerr ("riscos-gtk: %s box shown\n", save ? "Save" : "Open");
  box->accept_label = box->cancel_label = NULL;
  return box;
}

/* ---- GtkFileChooserNative ---- */

static void
native_done (GObject *owner, int response, GFile *file)
{
  GtkFileChooserNative *self = GTK_FILE_CHOOSER_NATIVE (owner);

  self->mode_data = NULL;
  if (file)
    {
      g_slist_free_full (self->custom_files, g_object_unref);
      self->custom_files = g_slist_prepend (NULL, g_object_ref (file));
    }
  _gtk_native_dialog_emit_response (GTK_NATIVE_DIALOG (self), response);
}

gboolean
gtk_file_chooser_native_riscos_show (GtkFileChooserNative *self)
{
  GtkFileChooserAction action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self));
  char *name = NULL;

  if (!usable (action))
    return FALSE;
  if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
    {
      name = riscos_name_of (self->current_file);
      if (name == NULL && self->current_name)
        name = gdk_riscos_path_from_unix (self->current_name);
    }
  self->mode_data = box_open (G_OBJECT (self), native_done, action,
                              gtk_native_dialog_get_title (GTK_NATIVE_DIALOG (self)),
                              gtk_native_dialog_get_transient_for (GTK_NATIVE_DIALOG (self)),
                              gtk_native_dialog_get_modal (GTK_NATIVE_DIALOG (self)),
                              self->current_folder, name,
                              self->accept_label, self->cancel_label);
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

  return b && GTK_IS_BUTTON (b) ? gtk_button_get_label (GTK_BUTTON (b)) : NULL;
}

static void
dialog_done (GObject *owner, int response, GFile *file)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (owner);

  g_object_set_data_full (owner, "riscos-gtk-files",
                          file ? g_slist_prepend (NULL, g_object_ref (file)) : NULL,
                          file ? (GDestroyNotify) riscos_files_free : NULL);
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
  char *name = NULL;
  int accept = _gtk_file_chooser_dialog_riscos_accept_id (dialog);

  if (!usable (action))
    return FALSE;
  if (g_object_get_data (G_OBJECT (dialog), "riscos-gtk-box"))
    return TRUE;                /* already open */
  g_object_set_data (G_OBJECT (dialog), "riscos-gtk-files", NULL);
  folder = gtk_file_chooser_get_current_folder_file (chooser);
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
            button_label (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL));
  if (!g_signal_handler_find (dialog, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_dialog_destroy, NULL))
    g_signal_connect (dialog, "destroy", G_CALLBACK (on_dialog_destroy), NULL);
  g_clear_object (&folder);
  g_free (name);
  return TRUE;
}
