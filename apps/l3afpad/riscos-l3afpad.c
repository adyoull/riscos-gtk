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
 *     its window; Menu gives Info, Choices..., Quit; closing the window
 *     empties it (asking to save first) and leaves L3afpad on the icon bar;
 *     text files double-clicked in the Filer, dropped on the icon or saved
 *     to it from another program are opened; a second copy hands its file
 *     to the first; Quit and PreQuit with unsaved text ask (Cancel /
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

#ifndef RISCOS_PORT_VERSION
#define RISCOS_PORT_VERSION "dev"
#endif

void riscos_l3afpad_init (gint *argc, gchar ***argv);
gboolean riscos_l3afpad_close (GtkWidget *w, GdkEvent *e, gpointer d);

/* ---- the window ---------------------------------------------------- */

/* The close icon: empty the window (asking to save first) and hide it;
   L3afpad stays on the icon bar */
gboolean
riscos_l3afpad_close (GtkWidget *w, GdkEvent *e, gpointer d)
{
  on_file_close ();
  if (!gtk_text_buffer_get_modified (pub->mw->buffer))
    {
      save_config_file ();
      gtk_widget_hide (pub->mw->window);
    }
  return TRUE;
}

static void
show_window (gpointer d)
{
  gtk_window_present (GTK_WINDOW (pub->mw->window));
}

static gboolean
open_file (const char *path, int filetype, gboolean scrap, gpointer d)
{
  FileInfo *fi;

  show_window (NULL);
  if (check_text_modification ())
    return FALSE;
  fi = g_malloc (sizeof (FileInfo));
  fi->filename = g_strdup (path);
  fi->charset = pub->fi->charset_flag ? g_strdup (pub->fi->charset) : NULL;
  fi->charset_flag = pub->fi->charset_flag;
  fi->lineend = LF;
  if (file_open_real (pub->mw->view, fi))
    {
      g_free (fi->filename);
      g_free (fi);
      return FALSE;
    }
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
  return gtk_text_buffer_get_modified (pub->mw->buffer);
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

static void
choices_response (GtkDialog *dialog, gint response, gpointer d)
{
  if (response == GTK_RESPONSE_OK)
    {
      gchar *font = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (c_font));
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

  gdk_riscos_iconbar_set_handlers (&handlers, NULL);
  gdk_riscos_iconbar_set_info ("L3afpad", "Text editor",
                               "\xc2\xa9 Tarot Osuji and others; RISC OS port Andrew Youll",
                               PACKAGE_VERSION " (RISC OS " RISCOS_PORT_VERSION ")");
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
