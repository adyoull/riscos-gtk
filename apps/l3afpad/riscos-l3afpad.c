/*
 * riscos-l3afpad.c - L3afpad's start on RISC OS (riscos-gtk).
 *
 *   - fonts and GTK's data (theme, icons, settings schemas) from the
 *     application directory: FONTCONFIG_FILE and XDG_DATA_DIRS for this
 *     program only (UnixLib keeps names without '$' to the program);
 *   - messages to <Wimp$ScrapDir>.L3afpad.Log, opened once (RISC OS allows
 *     one write-open of a file) and shared by stdout and stderr;
 *   - a file given by !Run (a RISC OS name, e.g. a text file dropped on
 *     the application) becomes a UnixLib name, as L3afpad expects;
 *   - files saved without an extension are text (&FFF), not data;
 *   - the icon bar (riscos-gtk's gdk_riscos_iconbar_*), as riscos-gtk's
 *     other RISC OS programs: L3afpad starts on the icon bar; Select opens
 *     a new window; Menu gives Info, Choices..., Quit; each document has
 *     its own window (see "the windows" below); closing a window asks to
 *     save first; text files double-clicked in the Filer, dropped on the
 *     icon or saved to it from another program open in a window of their
 *     own; a second copy hands its file to the first; Quit and PreQuit with unsaved text ask (Cancel /
 *     Discard); Choices... opens a Choices window, kept in
 *     <Choices$Write>.L3afpad.Choices.
 *
 * GPL version 2 or later, as L3afpad. Copyright (C) 2026 Andrew Youll.
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <gdk/gdkriscos.h>
#include "l3afpad.h"
#include "riscos-windows.h"

#ifndef RISCOS_PORT_VERSION
#define RISCOS_PORT_VERSION "dev"
#endif

void riscos_l3afpad_init (gint *argc, gchar ***argv);
gboolean riscos_l3afpad_close (GtkWidget *w, GdkEvent *e, gpointer d);

/* ---- the windows ---------------------------------------------------
   One task, a window per document, as RISC OS's own editors: Select on the
   icon bar, File > New, and a file opened from the Filer or another
   program each open a window (a hidden empty one is used first; a file
   already open has its window brought to the front). Closing a window
   asks to save; the last one is emptied and hidden, so L3afpad stays on
   the icon bar.
   L3afpad keeps a window's state in pub and in file-level variables
   (each file lists them: riscos-windows.h); they're swapped when an input
   event (a click, a key, the input focus, a drop, the close icon) goes to
   another window or one of its dialogue boxes. */

typedef struct {
  PublicData *pub;
  guchar *state;                /* the variables while another window is used */
} Doc;

extern const L3State l3_state_callback[], l3_state_hlight[], l3_state_indent[],
  l3_state_linenum[], l3_state_menu[], l3_state_undo[], l3_state_view[];
static const L3State *const tables[] = {
  l3_state_callback, l3_state_hlight, l3_state_indent, l3_state_linenum,
  l3_state_menu, l3_state_undo, l3_state_view
};

void l3afpad_setup_window (void);

static GList *docs;
static Doc *current;
static guchar *pristine;        /* the variables as the program started */

static gsize
state_size (void)
{
  gsize n = 0, t;
  const L3State *v;

  for (t = 0; t < G_N_ELEMENTS (tables); t++)
    for (v = tables[t]; v->address; v++)
      n += v->size;
  return n;
}

static void
state_save (guchar *to)
{
  gsize t;
  const L3State *v;

  for (t = 0; t < G_N_ELEMENTS (tables); t++)
    for (v = tables[t]; v->address; v++)
      {
        memcpy (to, v->address, v->size);
        to += v->size;
      }
}

static void
state_load (const guchar *from)
{
  gsize t;
  const L3State *v;

  for (t = 0; t < G_N_ELEMENTS (tables); t++)
    for (v = tables[t]; v->address; v++)
      {
        memcpy (v->address, from, v->size);
        from += v->size;
      }
}

static void
use (Doc *d)
{
  if (d == current || d == NULL)
    return;
  if (current)
    state_save (current->state);
  state_load (d->state);
  pub = d->pub;
  current = d;
}

static Doc *
doc_of_widget (GtkWidget *w)
{
  GList *l;
  int depth;

  for (depth = 0; w && depth < 8; depth++)
    {
      w = gtk_widget_get_toplevel (w);
      for (l = docs; l; l = l->next)
        if (((Doc *) l->data)->pub->mw->window == w)
          return l->data;
      /* a dialogue box: its document window; a menu: what it's attached to */
      if (GTK_IS_WINDOW (w) && gtk_window_get_transient_for (GTK_WINDOW (w)))
        w = GTK_WIDGET (gtk_window_get_transient_for (GTK_WINDOW (w)));
      else if (GTK_IS_WINDOW (w) && GTK_IS_MENU (gtk_bin_get_child (GTK_BIN (w))))
        w = gtk_menu_get_attach_widget (GTK_MENU (gtk_bin_get_child (GTK_BIN (w))));
      else
        return NULL;
    }
  return NULL;
}

static Doc *
doc_of_gdk_window (GdkWindow *gw)
{
  gpointer data = NULL;

  if (gw == NULL)
    return NULL;
  gdk_window_get_user_data (gdk_window_get_toplevel (gw), &data);
  return data && GTK_IS_WIDGET (data) ? doc_of_widget (data) : NULL;
}

/* before GTK sees an input event: that window's state. Not while a
   dialogue box holds the input (a modal grab: "Save changes?"), whose
   code goes on with the window it was asked for. */
static void
event_watch (GdkEvent *event, gpointer data)
{
  if (gtk_grab_get_current ())
    return;
  switch (event->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_KEY_PRESS:
    case GDK_DELETE:
    case GDK_DRAG_ENTER:
    case GDK_DROP_START:
      use (doc_of_gdk_window (event->any.window));
      break;
    case GDK_FOCUS_CHANGE:
      if (event->focus_change.in)
        use (doc_of_gdk_window (event->any.window));
      break;
    default:
      break;
    }
}

static gboolean
doc_empty (Doc *d)
{
  return d->pub->fi->filename == NULL && !gtk_text_buffer_get_modified (d->pub->mw->buffer) &&
         gtk_text_buffer_get_char_count (d->pub->mw->buffer) == 0;
}

/* l3afpad_setup_window () has made a window: it's the one in use */
void
riscos_l3afpad_window_created (void)
{
  Doc *d = g_new0 (Doc, 1);

  if (docs == NULL)
    gdk_riscos_set_event_watch (event_watch, NULL);
  d->pub = pub;
  d->state = g_malloc (state_size ());
  docs = g_list_append (docs, d);
  current = d;
}

/* A new, empty window, in use; offset from the last one shown, as RISC
   OS opens a program's next window (48 OS units down and right) */
static Doc *
new_doc (void)
{
  GtkWindow *last = NULL;
  GList *l;

  for (l = docs; l; l = l->next)
    if (gtk_widget_get_visible (((Doc *) l->data)->pub->mw->window))
      last = GTK_WINDOW (((Doc *) l->data)->pub->mw->window);
  if (current)
    state_save (current->state);
  state_load (pristine);
  current = NULL;
  pub = g_malloc (sizeof (PublicData));
  pub->fi = g_malloc (sizeof (FileInfo));
  pub->fi->filename = NULL;
  pub->fi->charset = NULL;
  pub->fi->charset_flag = FALSE;
  pub->fi->lineend = LF;
  l3afpad_setup_window ();
  set_main_window_title ();
  indent_refresh_tab_width (pub->mw->view);
  if (last)
    {
      gint x, y;
      GdkWindow *gw = gtk_widget_get_window (GTK_WIDGET (last));
      if (gw)
        {
          gdk_window_get_root_origin (gw, &x, &y);
          gtk_window_move (GTK_WINDOW (pub->mw->window), x + 24, y + 24);
        }
    }
  return current;
}

/* a window to put a document in: a hidden empty one, else a new one */
static Doc *
spare_doc (void)
{
  GList *l;

  for (l = docs; l; l = l->next)
    {
      Doc *d = l->data;
      if (!gtk_widget_get_visible (d->pub->mw->window) && doc_empty (d))
        {
          use (d);
          return d;
        }
    }
  return new_doc ();
}

void
riscos_l3afpad_new_window (void)
{
  spare_doc ();
  gtk_window_present (GTK_WINDOW (pub->mw->window));
}

/* Close: ask to save; then the window goes (the last one is emptied and
   hidden, and L3afpad stays on the icon bar) */
void
riscos_l3afpad_close_window (void)
{
  Doc *d = current;

  if (d == NULL || check_text_modification ())
    return;
  save_config_file ();
  if (g_list_length (docs) == 1)
    {
      riscos_l3afpad_empty ();
      gtk_widget_hide (pub->mw->window);
      return;
    }
  undo_clear_all (pub->mw->buffer);
  docs = g_list_remove (docs, d);
  current = NULL;
  gtk_widget_destroy (d->pub->mw->window);
  g_free (d->pub->fi->filename);
  g_free (d->pub->fi->charset);
  g_free (d->pub->fi);
  g_free (d->pub->mw);
  g_free (d->pub);
  g_free (d->state);
  g_free (d);
  /* another window's state, so that pub is never left dangling */
  state_load (((Doc *) docs->data)->state);
  pub = ((Doc *) docs->data)->pub;
  current = docs->data;
}

/* The close icon */
gboolean
riscos_l3afpad_close (GtkWidget *w, GdkEvent *e, gpointer d)
{
  riscos_l3afpad_close_window ();
  return TRUE;
}

/* File > Quit: each window with unsaved text asks; Cancel stops */
void
riscos_l3afpad_quit (void)
{
  GList *l;

  for (l = docs; l; l = l->next)
    {
      use (l->data);
      if (gtk_text_buffer_get_modified (pub->mw->buffer))
        {
          gtk_window_present (GTK_WINDOW (pub->mw->window));
          if (check_text_modification ())
            return;
        }
    }
  save_config_file ();
  gtk_main_quit ();
}

static void
show_window (gpointer d)
{
  riscos_l3afpad_new_window ();
}

static gboolean
same_file (const char *a, const char *b)
{
  return a && b && g_ascii_strcasecmp (a, b) == 0;     /* RISC OS names: case doesn't matter */
}

static gboolean
open_file (const char *path, int filetype, gboolean scrap, gpointer d)
{
  FileInfo *fi;
  GList *l;
  Doc *doc;

  /* already open: its window to the front */
  if (!scrap)
    for (l = docs; l; l = l->next)
      if (same_file (((Doc *) l->data)->pub->fi->filename, path))
        {
          use (l->data);
          gtk_window_present (GTK_WINDOW (pub->mw->window));
          return TRUE;
        }
  doc = spare_doc ();
  fi = g_malloc (sizeof (FileInfo));
  fi->filename = g_strdup (path);
  fi->charset = NULL;
  fi->charset_flag = FALSE;
  fi->lineend = LF;
  if (file_open_real (pub->mw->view, fi))
    {
      g_free (fi->filename);
      g_free (fi);
      if (!gtk_widget_get_visible (pub->mw->window) && g_list_length (docs) > 1)
        {
          /* not opened: no empty window left over */
          riscos_l3afpad_close_window ();
        }
      return FALSE;
    }
  g_free (pub->fi->filename);
  g_free (pub->fi->charset);
  g_free (pub->fi);
  pub->fi = fi;
  undo_clear_all (pub->mw->buffer);
  if (scrap)
    {
      /* from another program: untitled, and not saved anywhere yet */
      g_free (pub->fi->filename);
      pub->fi->filename = NULL;
      gtk_text_buffer_set_modified (pub->mw->buffer, TRUE);
    }
  force_call_cb_modified_changed (pub->mw->view);
  gtk_window_present (GTK_WINDOW (pub->mw->window));
  (void) doc;
  return TRUE;
}

/* Quit (the icon bar menu): riscos-gtk has asked about unsaved text */
static void
quit (gpointer d)
{
  save_config_file ();
  gtk_main_quit ();
}

static gboolean
unsaved (gpointer d)
{
  GList *l;

  for (l = docs; l; l = l->next)
    if (gtk_text_buffer_get_modified (((Doc *) l->data)->pub->mw->buffer))
      return TRUE;
  return FALSE;
}

/* ---- Choices ------------------------------------------------------- */

static GtkWidget *choices_box, *c_font, *c_tab, *c_wrap, *c_lines, *c_indent;

static GtkToggleAction *
option (const char *path)
{
  return GTK_TOGGLE_ACTION (gtk_ui_manager_get_action (pub->mw->menubar, path));
}

static void
choices_fill (void)
{
  gchar *font = pango_font_description_to_string (
    gtk_style_context_get_font (gtk_widget_get_style_context (pub->mw->view), 0));

  gtk_font_chooser_set_font (GTK_FONT_CHOOSER (c_font), font);
  g_free (font);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (c_tab), get_current_tab_width ());
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (c_wrap),
                                gtk_toggle_action_get_active (option ("/M/Options/WordWrap")));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (c_lines),
                                gtk_toggle_action_get_active (option ("/M/Options/LineNumbers")));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (c_indent),
                                gtk_toggle_action_get_active (option ("/M/Options/AutoIndent")));
}

/* Set: the choices for every window, and saved */
static void
choices_response (GtkDialog *dialog, gint response, gpointer d)
{
  if (response == GTK_RESPONSE_OK)
    {
      Doc *was = current;
      GList *l;
      for (l = docs; l; l = l->next)
        {
          gchar *font = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (c_font));
          use (l->data);
          if (font)
            set_text_font_by_name (pub->mw->view, font);
          g_free (font);
      gtk_toggle_action_set_active (option ("/M/Options/WordWrap"),
                                    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (c_wrap)));
      gtk_toggle_action_set_active (option ("/M/Options/LineNumbers"),
                                    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (c_lines)));
      gtk_toggle_action_set_active (option ("/M/Options/AutoIndent"),
                                    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (c_indent)));
          indent_set_default_tab_width (gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (c_tab)));
          indent_refresh_tab_width (pub->mw->view);
        }
      use (was);
      save_config_file ();
    }
  gtk_widget_hide (GTK_WIDGET (dialog));
}

static GtkWidget *
row_label (const char *text)
{
  GtkWidget *l = gtk_label_new (text);
  gtk_widget_set_halign (l, GTK_ALIGN_END);
  return l;
}

static void
choices (gpointer d)
{
  if (choices_box == NULL)
    {
      GtkWidget *grid, *area;

      choices_box = gtk_dialog_new_with_buttons ("L3afpad choices", NULL, 0,
                                                 "Cancel", GTK_RESPONSE_CANCEL,
                                                 "Set", GTK_RESPONSE_OK, NULL);
      gtk_dialog_set_default_response (GTK_DIALOG (choices_box), GTK_RESPONSE_OK);
      gtk_window_set_resizable (GTK_WINDOW (choices_box), FALSE);
      grid = gtk_grid_new ();
      gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
      gtk_grid_set_column_spacing (GTK_GRID (grid), 8);
      gtk_container_set_border_width (GTK_CONTAINER (grid), 8);
      c_font = gtk_font_button_new ();
      c_tab = gtk_spin_button_new_with_range (1, 16, 1);
      c_wrap = gtk_check_button_new_with_label ("Word wrap");
      c_lines = gtk_check_button_new_with_label ("Line numbers");
      c_indent = gtk_check_button_new_with_label ("Auto indent");
      gtk_grid_attach (GTK_GRID (grid), row_label ("Font"), 0, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), c_font, 1, 0, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), row_label ("Tab width"), 0, 1, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), c_tab, 1, 1, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), c_wrap, 1, 2, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), c_lines, 1, 3, 1, 1);
      gtk_grid_attach (GTK_GRID (grid), c_indent, 1, 4, 1, 1);
      area = gtk_dialog_get_content_area (GTK_DIALOG (choices_box));
      gtk_box_pack_start (GTK_BOX (area), grid, TRUE, TRUE, 0);
      gtk_widget_show_all (grid);
      g_signal_connect (choices_box, "response", G_CALLBACK (choices_response), NULL);
      g_signal_connect (choices_box, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    }
  choices_fill ();
  gtk_window_present (GTK_WINDOW (choices_box));
}

static const GdkRiscosIconbarHandlers handlers = {
  show_window, open_file, choices, quit, unsaved
};

static gboolean
is_riscos_name (const char *name)
{
  return name[0] != '/' && (strchr (name, ':') || name[0] == '$' || name[0] == '<'
                            || name[0] == '@' || name[0] == '&');
}

void
riscos_l3afpad_init (gint *argc, gchar ***argv)
{
  int fd, i;

  g_setenv ("FONTCONFIG_FILE", "/<L3afpad$Dir>/fonts/fonts.conf", FALSE);
  g_setenv ("XDG_DATA_DIRS", "/<L3afpad$Dir>/share", FALSE);
  g_setenv ("RISCOS_GTK_DEFAULT_FILETYPE", "fff", FALSE);
  g_setenv ("L3AFPAD_CHOICES", "/<Choices$Write>/L3afpad", FALSE);

  /* the per-window variables as they start, for each new window */
  pristine = g_malloc (state_size ());
  state_save (pristine);

  gdk_riscos_iconbar_set_handlers (&handlers, NULL);
  gdk_riscos_iconbar_set_info ("L3afpad", "Text editor",
                               "\xc2\xa9 Tarot Osuji and others; RISC OS port Andrew Youll",
                               RISCOS_PORT_VERSION);
  gdk_riscos_iconbar_add_filetype (0xFFF);

  if (!g_getenv ("L3AFPAD_NO_LOG"))
    {
      fd = open ("/<Wimp$ScrapDir>/L3afpad/Log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd >= 0)
        {
          dup2 (fd, 1);
          dup2 (fd, 2);
          if (fd > 2)
            close (fd);
          setvbuf (stdout, NULL, _IOLBF, 0);
          setvbuf (stderr, NULL, _IONBF, 0);
        }
    }

  for (i = 1; i < *argc; i++)
    if ((*argv)[i][0] != '-' && is_riscos_name ((*argv)[i]))
      {
        /* if L3afpad is already running, it gets this file instead */
        gdk_riscos_iconbar_set_start_file ((*argv)[i]);
        (*argv)[i] = gdk_riscos_path_to_unix ((*argv)[i]);
      }
}
