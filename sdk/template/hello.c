/*
 * hello.c - riscos-gtk devkit template: a small GTK 3 program that runs
 * as a RISC OS Wimp task. Copy this folder to start your own.
 *
 * What it shows:
 *   - riscos_gtk_app_init(): fonts, theme, the Log and start-up files;
 *   - a GtkApplication: Select on the icon bar icon is "activate", a Text
 *     file double-clicked or dropped on the icon is "open", Quit on the
 *     icon bar menu is the "quit" action;
 *   - the icon bar's Info box and the file types the program loads;
 *   - a menu bar, which riscos-gtk turns into a Wimp menu on Menu;
 *   - GtkFileChooserDialog, which riscos-gtk shows as the RISC OS Save
 *     and Open boxes;
 *   - unsaved work: Quit and desktop shutdown ask first;
 *   - a web address opened in the user's browser (Help menu).
 * Nothing in it is RISC OS-only except the #ifdef GDK_WINDOWING_RISCOS
 * part: the same source builds for Linux.
 *
 * MIT licence: copy and change it freely.
 */
#include <gtk/gtk.h>
#include <riscos-gtk/app.h>

#define APP_NAME    "Hello"
#define APP_VERSION "1.00"

typedef struct {
  GtkWidget *window;
  GtkTextBuffer *buffer;
  GtkWidget *status;
  char *path;                      /* the file shown (UnixLib name), or NULL */
} HelloWindow;

static GtkApplication *app;
static HelloWindow *current;

static void on_quit (GtkWidget *item, gpointer data);

static void
set_status (HelloWindow *hw, const char *text)
{
  gtk_label_set_text (GTK_LABEL (hw->status), text);
  g_print ("%s\n", text);          /* goes to <Wimp$ScrapDir>.Hello.Log */
}

static void
set_title (HelloWindow *hw)
{
  char *base = hw->path ? g_path_get_basename (hw->path) : g_strdup ("<Untitled>");
  char *title = g_strdup_printf ("%s%s - " APP_NAME, base,
                                 gtk_text_buffer_get_modified (hw->buffer) ? " *" : "");
  gtk_window_set_title (GTK_WINDOW (hw->window), title);
  g_free (title);
  g_free (base);
}

static gboolean
load_file (HelloWindow *hw, const char *path)
{
  char *text;
  gsize len;
  GError *error = NULL;

  if (!g_file_get_contents (path, &text, &len, &error))
    {
      set_status (hw, error->message);
      g_error_free (error);
      return FALSE;
    }
  if (!g_utf8_validate (text, len, NULL))
    {
      /* RISC OS text is usually Latin-1 */
      char *utf8 = g_convert (text, len, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
      g_free (text);
      text = utf8 ? utf8 : g_strdup ("");
    }
  gtk_text_buffer_set_text (hw->buffer, text, -1);
  gtk_text_buffer_set_modified (hw->buffer, FALSE);
  g_free (text);
  g_free (hw->path);
  hw->path = g_strdup (path);
  set_title (hw);
  set_status (hw, "Loaded");
  return TRUE;
}

static gboolean
save_file (HelloWindow *hw, const char *path)
{
  GtkTextIter a, b;
  char *text;
  GError *error = NULL;
  gboolean ok;

  gtk_text_buffer_get_bounds (hw->buffer, &a, &b);
  text = gtk_text_buffer_get_text (hw->buffer, &a, &b, FALSE);
  ok = g_file_set_contents (path, text, -1, &error);
  g_free (text);
  if (!ok)
    {
      set_status (hw, error->message);
      g_error_free (error);
      return FALSE;
    }
  /* riscos-gtk gives a saved file the type for its extension (none: the
     program's default, here Text, set in main()) */
  gtk_text_buffer_set_modified (hw->buffer, FALSE);
  g_free (hw->path);
  hw->path = g_strdup (path);
  set_title (hw);
  set_status (hw, "Saved");
  return TRUE;
}

static void
on_modified (GtkTextBuffer *buffer, HelloWindow *hw)
{
  set_title (hw);
}

static void
on_hello (GtkButton *button, HelloWindow *hw)
{
  GtkTextIter end;

  gtk_text_buffer_get_end_iter (hw->buffer, &end);
  gtk_text_buffer_insert (hw->buffer, &end, "Hello from GTK on RISC OS!\n", -1);
  set_status (hw, "Said hello");
}

static void
on_open (GtkWidget *item, HelloWindow *hw)
{
  GtkWidget *d = gtk_file_chooser_dialog_new ("Open", GTK_WINDOW (hw->window),
                                              GTK_FILE_CHOOSER_ACTION_OPEN,
                                              "_Cancel", GTK_RESPONSE_CANCEL,
                                              "_Open", GTK_RESPONSE_ACCEPT, NULL);
  if (gtk_dialog_run (GTK_DIALOG (d)) == GTK_RESPONSE_ACCEPT)
    {
      char *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (d));
      load_file (hw, path);
      g_free (path);
    }
  gtk_widget_destroy (d);
}

static void
on_save_as (GtkWidget *item, HelloWindow *hw)
{
  /* On RISC OS this is a Save box: drag its icon to a Filer window */
  GtkWidget *d = gtk_file_chooser_dialog_new ("Save as", GTK_WINDOW (hw->window),
                                              GTK_FILE_CHOOSER_ACTION_SAVE,
                                              "_Cancel", GTK_RESPONSE_CANCEL,
                                              "_Save", GTK_RESPONSE_ACCEPT, NULL);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (d), "Hello");
  if (gtk_dialog_run (GTK_DIALOG (d)) == GTK_RESPONSE_ACCEPT)
    {
      char *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (d));
      save_file (hw, path);
      g_free (path);
    }
  gtk_widget_destroy (d);
}


/* Help > riscos-gtk web site: the user's browser, through RISC OS's URI
   handler (riscos-gtk/app.h) */
static void
on_web_site (GtkWidget *item, HelloWindow *hw)
{
  riscos_gtk_open_uri ("https://github.com/adyoull/riscos-gtk");
}

static void
on_destroy (GtkWidget *w, HelloWindow *hw)
{
  if (current == hw)
    current = NULL;
  g_free (hw->path);
  g_free (hw);
}

static GtkWidget *
menu_item (GtkWidget *menu, const char *label, GCallback cb, gpointer data)
{
  GtkWidget *item = gtk_menu_item_new_with_mnemonic (label);
  g_signal_connect (item, "activate", cb, data);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  return item;
}

static HelloWindow *
hello_window_new (void)
{
  HelloWindow *hw = g_new0 (HelloWindow, 1);
  GtkWidget *box, *bar, *file, *menu, *scroll, *view, *row, *button;

  hw->window = gtk_application_window_new (app);
  gtk_window_set_default_size (GTK_WINDOW (hw->window), 480, 320);
  g_signal_connect (hw->window, "destroy", G_CALLBACK (on_destroy), hw);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (hw->window), box);

  /* The menu bar: on RISC OS it isn't drawn; Menu over the window opens
     it as a Wimp menu */
  bar = gtk_menu_bar_new ();
  file = gtk_menu_item_new_with_mnemonic ("_File");
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (file), menu);
  menu_item (menu, "_Open...", G_CALLBACK (on_open), hw);
  menu_item (menu, "Save _as...", G_CALLBACK (on_save_as), hw);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
  menu_item (menu, "_Quit", G_CALLBACK (on_quit), NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (bar), file);
  file = gtk_menu_item_new_with_mnemonic ("_Help");
  menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (file), menu);
  menu_item (menu, "riscos-gtk _web site", G_CALLBACK (on_web_site), hw);
  gtk_menu_shell_append (GTK_MENU_SHELL (bar), file);
  gtk_box_pack_start (GTK_BOX (box), bar, FALSE, FALSE, 0);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  view = gtk_text_view_new ();
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD_CHAR);
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 6);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 6);
  gtk_text_view_set_top_margin (GTK_TEXT_VIEW (view), 4);
  hw->buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  g_signal_connect (hw->buffer, "modified-changed", G_CALLBACK (on_modified), hw);
  gtk_container_add (GTK_CONTAINER (scroll), view);
  gtk_box_pack_start (GTK_BOX (box), scroll, TRUE, TRUE, 0);

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (row), 8);
  button = gtk_button_new_with_label ("Say hello");
  g_signal_connect (button, "clicked", G_CALLBACK (on_hello), hw);
  gtk_box_pack_start (GTK_BOX (row), button, FALSE, FALSE, 0);
  hw->status = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (hw->status), 0);
  gtk_box_pack_start (GTK_BOX (row), hw->status, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), row, FALSE, FALSE, 0);

  set_title (hw);
  gtk_widget_show_all (hw->window);
  current = hw;
  return hw;
}

/* Select on the icon bar icon (and starting with no file) */
static void
on_activate (GApplication *application, gpointer data)
{
  if (current)
    gtk_window_present (GTK_WINDOW (current->window));
  else
    hello_window_new ();
}

/* Files: on the command line, double-clicked, or dropped on the icon */
static void
on_open_files (GApplication *application, GFile **files, int n, const char *hint, gpointer data)
{
  int i;

  for (i = 0; i < n; i++)
    {
      char *path = g_file_get_path (files[i]);
      HelloWindow *hw = current && current->path == NULL &&
                        !gtk_text_buffer_get_modified (current->buffer)
                        ? current : hello_window_new ();
      if (path)
        load_file (hw, path);
      gtk_window_present (GTK_WINDOW (hw->window));
      g_free (path);
    }
}

static gboolean
any_unsaved (void)
{
  GList *l;

  for (l = gtk_application_get_windows (app); l; l = l->next)
    {
      GtkWidget *view = NULL;
      /* each window's buffer: walk to the text view */
      GList *kids = gtk_container_get_children (GTK_CONTAINER (gtk_bin_get_child (GTK_BIN (l->data))));
      GList *k;
      for (k = kids; k; k = k->next)
        if (GTK_IS_SCROLLED_WINDOW (k->data))
          view = gtk_bin_get_child (GTK_BIN (k->data));
      g_list_free (kids);
      if (view && gtk_text_buffer_get_modified (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view))))
        return TRUE;
    }
  return FALSE;
}

/* The "quit" action: Quit on the icon bar menu (riscos-gtk has already
   asked about unsaved work, through iconbar_unsaved below) */
static void
on_quit_action (GSimpleAction *action, GVariant *param, gpointer data)
{
  g_application_quit (G_APPLICATION (app));
}

/* File > Quit: ask first, as RISC OS programs do */
static void
on_quit (GtkWidget *item, gpointer data)
{
#ifdef GDK_WINDOWING_RISCOS
  if (any_unsaved () && !gdk_riscos_ask ("There is unsaved work. Discard it and quit?", "Discard"))
    return;
#endif
  g_application_quit (G_APPLICATION (app));
}

#ifdef GDK_WINDOWING_RISCOS
/* Quit on the icon bar menu and PreQuit (desktop shutdown) ask this
   first: riscos-gtk asks the user when it says TRUE */
static gboolean iconbar_unsaved (gpointer data) { return any_unsaved (); }
#endif

int
main (int argc, char **argv)
{
  static const GActionEntry actions[] = { { "quit", on_quit_action } };
  int status;

  riscos_gtk_app_init (APP_NAME, &argc, &argv);

#ifdef GDK_WINDOWING_RISCOS
  {
    /* Only "unsaved" is ours: Select, files and Quit come from the
       GtkApplication (activate, open, the "quit" action) */
    static GdkRiscosIconbarHandlers handlers;
    handlers.unsaved = iconbar_unsaved;
    gdk_riscos_iconbar_set_handlers (&handlers, NULL);
  }
  gdk_riscos_iconbar_set_info (APP_NAME, "riscos-gtk template",
                               "\xc2\xa9 Your name", APP_VERSION);
  gdk_riscos_iconbar_add_filetype (0xFFF);           /* Text */
  g_setenv ("RISCOS_GTK_DEFAULT_FILETYPE", "fff", FALSE);
#endif

  /* NON_UNIQUE: no D-Bus on RISC OS; riscos-gtk's icon bar code makes
     sure only one copy runs */
  app = gtk_application_new ("org.example.Hello",
                             G_APPLICATION_NON_UNIQUE | G_APPLICATION_HANDLES_OPEN);
  g_action_map_add_action_entries (G_ACTION_MAP (app), actions, G_N_ELEMENTS (actions), NULL);
  g_signal_connect (app, "activate", G_CALLBACK (on_activate), NULL);
  g_signal_connect (app, "open", G_CALLBACK (on_open_files), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);
  return status;
}
