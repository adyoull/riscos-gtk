/*
 * gnumeric-fake.c - Gnumeric (1.12.57, the port) on the fake RISC OS, with
 * its plugins built in (GLib's static module registry, tools/
 * mkstaticmodules.py) and apps/gnumeric/riscos-gnumeric.c's start:
 *   - it starts with a workbook window;
 *   - cells using plugin functions (fn-stat's AVERAGE, fn-string's
 *     CONCATENATE, fn-math's ROUND) work: the modules load on first use;
 *   - the workbook saves as .xlsx (the excel plugin) and .ods
 *     (openoffice), and the .xlsx opens again with its values;
 *   - a bar chart plot comes from GOffice's plot_barcol plugin;
 *   - a sheet with a check box on it scrolls; file types are known from
 *     names; a file dialogue with a grab on it (File > Open) leaves the
 *     grab with the RISC OS Open box;
 *   - Format Cells' font chooser (GOFontSel): picking Serif or Monospace
 *     from its family menu changes the preview's face, not only its size;
 *   - closing the window quits.
 *   gnumeric-fake APPDIR OUTDIR
 * APPDIR is the packaged !Gnumeric's folders (lib/plugins, goffice/lib/
 * plugins, share, fonts) for the linuxarm build. GN_SHOT=FILE.ppm: a
 * picture of the screen after the cells are filled; GN_TREE=1: the window's
 * widgets then ("#T" lines). Prints TAP. MIT licence.
 */
#include <gnumeric-config.h>
#include <gnumeric.h>
#include <application.h>
#include <workbook-priv.h>
#include <workbook-view.h>
#include <workbook-control.h>
#include <wbc-gtk.h>
#include <sheet.h>
#include <sheet-object.h>
#include <sheet-object-widget.h>
#include <sheet-control-gui.h>
#include <ranges.h>
#include <cell.h>
#include <value.h>
#include <command-context-stderr.h>
#include <goffice/goffice.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

int riscos_gnumeric_main (int argc, char **argv);

static int n_tests, n_failed, stage, waits;
static const char *outdir;
static int started, avg_ok, concat_ok, round_ok, xlsx_saved, ods_saved, reopened_ok, plot_ok;
static int scrolled_ok, mime_ok, grab_ok, files_done, icons_ok, fontsel_ok;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
  fflush (stdout);
}

static void
add (int reason, int a, int b, int c)
{
  int *e = fake_wimp_script[fake_wimp_script_len++];
  e[0] = reason; e[1] = a; e[2] = b; e[3] = c;
}

static void
nulls (int n)
{
  while (n--)
    add (FAKE_NULL, 0, 0, 0);
}

static WBCGtk *
first_wbcg (void)
{
  GList *l;
  WBCGtk *found = NULL;

  for (l = gnm_app_workbook_list (); l && !found; l = l->next)
    {
      Workbook *wb = l->data;
      WORKBOOK_FOREACH_CONTROL (wb, view, control,
        if (!found && GNM_IS_WBC_GTK (control))
          found = WBC_GTK (control););
    }
  return found;
}

static GnmValue const *
cell_value (Sheet *sheet, int col, int row)
{
  GnmCell *cell = sheet_cell_get (sheet, col, row);
  return cell ? cell->value : NULL;
}

static gboolean
do_cells (gpointer d)
{
  WBCGtk *wbcg = first_wbcg ();
  Workbook *wb = wb_control_get_workbook (GNM_WBC (wbcg));
  Sheet *sheet = workbook_sheet_by_index (wb, 0);
  GnmValue const *v;

  sheet_cell_set_text (sheet_cell_fetch (sheet, 0, 0), "1", NULL);
  sheet_cell_set_text (sheet_cell_fetch (sheet, 0, 1), "2", NULL);
  sheet_cell_set_text (sheet_cell_fetch (sheet, 0, 2), "6", NULL);
  sheet_cell_set_text (sheet_cell_fetch (sheet, 0, 3), "=AVERAGE(A1:A3)", NULL);
  sheet_cell_set_text (sheet_cell_fetch (sheet, 1, 0), "=CONCATENATE(\"Risc\",\"OS\")", NULL);
  sheet_cell_set_text (sheet_cell_fetch (sheet, 1, 1), "=ROUND(2.675,1)", NULL);
  workbook_recalc (wb);
  {
    /* a check box on the sheet: GOffice draws sheet widgets through an
       offscreen box, which asked GDK for a native child window when the
       sheet scrolled (an assertion in the RISC OS GDK) */
    SheetObject *so = g_object_new (sheet_widget_checkbox_get_type (), NULL);
    SheetObjectAnchor anchor;
    GnmRange r;
    range_init (&r, 2, 30, 3, 31);
    sheet_object_anchor_init (&anchor, &r, NULL, GOD_ANCHOR_DIR_DOWN_RIGHT, GNM_SO_ANCHOR_TWO_CELLS);
    sheet_object_set_anchor (so, &anchor);
    sheet_object_set_sheet (so, sheet);
    g_object_unref (so);
  }
  v = cell_value (sheet, 0, 3);
  avg_ok = v && VALUE_IS_FLOAT (v) && value_get_as_float (v) == 3;
  v = cell_value (sheet, 1, 0);
  concat_ok = v && VALUE_IS_STRING (v) && strcmp (value_peek_string (v), "RiscOS") == 0;
  v = cell_value (sheet, 1, 1);
  round_ok = v && VALUE_IS_FLOAT (v) && value_get_as_float (v) > 2.65 && value_get_as_float (v) < 2.75;
  printf ("# A4 %s, B1 %s, B2 %s\n", cell_value (sheet, 0, 3) ? value_peek_string (cell_value (sheet, 0, 3)) : "-",
          cell_value (sheet, 1, 0) ? value_peek_string (cell_value (sheet, 1, 0)) : "-",
          cell_value (sheet, 1, 1) ? value_peek_string (cell_value (sheet, 1, 1)) : "-");
  return G_SOURCE_REMOVE;
}

static gboolean
save_as (WorkbookView *wbv, const char *saver, const char *leaf)
{
  GOFileSaver *fs = go_file_saver_for_id (saver);
  char *path = g_build_filename (outdir, leaf, NULL), *uri = go_filename_to_uri (path);
  GOCmdContext *cc = gnm_cmd_context_stderr_new ();
  struct stat st;
  gboolean ok;

  printf ("# saver %s: %s\n", saver, fs ? "found" : "not found");
  ok = fs && workbook_view_save_as (wbv, fs, uri, cc) && stat (path, &st) == 0 && st.st_size > 0;
  g_object_unref (cc);
  g_free (uri);
  g_free (path);
  return ok;
}

/* GOFontSel's pieces: its family GOOptionMenu (the one whose menu has
   "All fonts...") and its preview label (the sample text) */
static GOOptionMenu *fs_family;
static GtkLabel *fs_preview;

static void
find_fontsel_parts (GtkWidget *w, const char *sample)
{
  if (GO_IS_OPTION_MENU (w) && !fs_family)
    {
      GtkWidget *m = go_option_menu_get_menu (GO_OPTION_MENU (w));
      GList *l, *items = m ? gtk_container_get_children (GTK_CONTAINER (m)) : NULL;
      for (l = items; l; l = l->next)
        if (GTK_IS_MENU_ITEM (l->data) &&
            g_strcmp0 (gtk_menu_item_get_label (l->data), "All fonts...") == 0)
          fs_family = GO_OPTION_MENU (w);
      g_list_free (items);
    }
  if (GTK_IS_LABEL (w) && g_strcmp0 (gtk_label_get_text (GTK_LABEL (w)), sample) == 0)
    fs_preview = GTK_LABEL (w);
  if (GTK_IS_CONTAINER (w))
    {
      GList *l, *c = gtk_container_get_children (GTK_CONTAINER (w));
      for (l = c; l; l = l->next)
        find_fontsel_parts (l->data, sample);
      g_list_free (c);
    }
}

/* choose NAME in the family menu, as a click on its item does */
static gboolean
pick_family (const char *name)
{
  GtkWidget *m = go_option_menu_get_menu (fs_family);
  GList *l, *items = gtk_container_get_children (GTK_CONTAINER (m));
  gboolean found = FALSE;

  for (l = items; l && !found; l = l->next)
    if (GTK_IS_MENU_ITEM (l->data) &&
        g_strcmp0 (gtk_menu_item_get_label (l->data), name) == 0)
      {
        gtk_menu_item_activate (l->data);
        found = TRUE;
      }
  g_list_free (items);
  return found;
}

/* the family of the face the preview is drawn with */
static char *
preview_face (void)
{
  PangoLayout *layout = gtk_label_get_layout (fs_preview);
  PangoLayoutLine *line = pango_layout_get_line_readonly (layout, 0);
  char *family = NULL;

  if (line && line->runs)
    {
      PangoGlyphItem *run = line->runs->data;
      PangoFontDescription *d = pango_font_describe (run->item->analysis.font);
      family = g_strdup (pango_font_description_get_family (d));
      pango_font_description_free (d);
    }
  return family;
}

static void
do_fontsel (void)
{
  GtkWidget *win = gtk_offscreen_window_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *gfs = go_font_sel_new ();
  char *sans = NULL, *serif = NULL, *mono = NULL;

  /* in a box, as in Format Cells: GOffice's chooser can't be destroyed
     as a window's own child (its screen-changed handler runs after its
     dispose) */
  gtk_container_add (GTK_CONTAINER (box), gfs);
  gtk_container_add (GTK_CONTAINER (win), box);
  gtk_widget_show_all (win);
  find_fontsel_parts (gfs, pango_language_get_sample_string (NULL));
  if (fs_family && fs_preview)
    {
      if (pick_family ("Sans"))
        sans = preview_face ();
      if (pick_family ("Serif"))
        serif = preview_face ();
      if (pick_family ("Monospace"))
        mono = preview_face ();
    }
  printf ("# font chooser: family menu %d, preview %d; Sans %s, Serif %s, Monospace %s\n",
          fs_family != NULL, fs_preview != NULL, sans, serif, mono);
  fontsel_ok = g_strcmp0 (sans, "DejaVu Sans") == 0 && g_strcmp0 (serif, "DejaVu Serif") == 0 &&
               g_strcmp0 (mono, "DejaVu Sans Mono") == 0;
  g_free (sans); g_free (serif); g_free (mono);
  gtk_widget_destroy (win);
}

static gboolean
do_files (gpointer d)
{
  WBCGtk *wbcg = first_wbcg ();
  WorkbookView *wbv = wb_control_view (GNM_WBC (wbcg));
  char *path, *uri;
  GOCmdContext *cc;
  GOIOContext *io;
  WorkbookView *again;

  ods_saved = save_as (wbv, "Gnumeric_OpenCalc:openoffice", "test.ods");
  xlsx_saved = save_as (wbv, "Gnumeric_Excel:xlsx", "test.xlsx");
  path = g_build_filename (outdir, "test.xlsx", NULL);
  uri = go_filename_to_uri (path);
  cc = gnm_cmd_context_stderr_new ();
  io = go_io_context_new (cc);
  again = workbook_view_new_from_uri (uri, NULL, io, NULL);
  if (again)
    {
      Workbook *wb = wb_view_get_workbook (again);
      Sheet *sheet = workbook_sheet_by_index (wb, 0);
      GnmValue const *v;
      workbook_recalc (wb);
      v = cell_value (sheet, 0, 3);
      reopened_ok = v && value_get_as_float (v) == 3;
      g_object_unref (wb);
    }
  g_object_unref (io);
  g_object_unref (cc);
  g_free (uri);
  g_free (path);

  {
    /* scroll so the check box comes into view, then back */
    SheetControlGUI *scg = wbcg_cur_scg (wbcg);
    scg_set_top_row (scg, 20);
    while (gtk_events_pending ())
      gtk_main_iteration ();
    scg_set_top_row (scg, 0);
    while (gtk_events_pending ())
      gtk_main_iteration ();
    scrolled_ok = 1;          /* (an assertion would have stopped it) */
  }
  {
    /* GIO knows file types from names (the application's share/mime) */
    char *t1 = g_content_type_guess ("Job Tracker.xlsx", NULL, 0, NULL);
    char *t2 = g_content_type_guess ("x.ods", NULL, 0, NULL);
    printf ("# types: %s, %s\n", t1, t2);
    mime_ok = strcmp (t1, "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet") == 0 &&
              strcmp (t2, "application/vnd.oasis.opendocument.spreadsheet") == 0;
    g_free (t1);
    g_free (t2);
  }
  {
    /* File > Open's way (GOffice's go_gtk_file_sel_dialog): show the
       dialogue, then a grab on it. It is shown as a RISC OS Open box and
       stays hidden: the grab must not go to it, or the box gets no
       clicks */
    GtkWidget *d = gtk_file_chooser_dialog_new ("Open", wbcg_toplevel (wbcg),
                                                GTK_FILE_CHOOSER_ACTION_OPEN,
                                                "_Cancel", GTK_RESPONSE_CANCEL,
                                                "_Open", GTK_RESPONSE_OK, NULL);
    gtk_window_set_modal (GTK_WINDOW (d), TRUE);
    gtk_widget_show (d);
    gtk_grab_add (d);
    printf ("# grab: %s\n", gtk_grab_get_current () ? G_OBJECT_TYPE_NAME (gtk_grab_get_current ()) : "none");
    grab_ok = gtk_grab_get_current () != d && !gtk_widget_get_mapped (d);
    gtk_grab_remove (d);
    gtk_widget_destroy (d);
  }
  {
    /* Gnumeric's own icons in the RISC OS style (mkriscosicons.py --pack
       gnumeric) come before its built-in ones */
    GtkIconTheme *t = gtk_icon_theme_get_default ();
    const char *names[] = { "gnumeric-format-border-left", "gnumeric-autosum", "gnumeric-row-add" };
    int i;
    icons_ok = 1;
    for (i = 0; i < 3; i++)
      {
        GtkIconInfo *info = gtk_icon_theme_lookup_icon (t, names[i], 24, 0);
        const char *f = info ? gtk_icon_info_get_filename (info) : NULL;
        if (!f || !strstr (f, "/RISCOS/24x24/"))
          {
            icons_ok = 0;
            printf ("# icon %s: %s\n", names[i], f ? f : "(none or built in)");
          }
        if (info)
          g_object_unref (info);
      }
  }
  {
    GogPlot *plot = gog_plot_new_by_name ("GogBarColPlot");
    plot_ok = plot != NULL;
    if (plot)
      g_object_unref (plot);
  }
  do_fontsel ();
  files_done = 1;
  return G_SOURCE_REMOVE;
}

static void
shot (const char *path)
{
  FILE *f = fopen (path, "wb");
  int x, y;

  if (!f)
    return;
  fprintf (f, "P6\n1280 1024\n255\n");
  for (y = 0; y < 1024; y++)
    for (x = 0; x < 1280; x++)
      {
        unsigned int p = fake_screen_pixel (x, y);
        fputc (p & 0xFF, f);
        fputc ((p >> 8) & 0xFF, f);
        fputc ((p >> 16) & 0xFF, f);
      }
  fclose (f);
}

static void
tree (GtkWidget *w, int depth)
{
  GtkAllocation a;
  gtk_widget_get_allocation (w, &a);
  printf ("#T %*s%s %s %d,%d %dx%d %s\n", depth * 2, "", G_OBJECT_TYPE_NAME (w),
          gtk_widget_get_name (w), a.x, a.y, a.width, a.height,
          gtk_widget_get_mapped (w) ? "mapped" : "");
  if (GTK_IS_CONTAINER (w))
    {
      GList *l, *c = gtk_container_get_children (GTK_CONTAINER (w));
      for (l = c; l; l = l->next)
        tree (l->data, depth + 1);
      g_list_free (c);
    }
}

static void
hook (int reason)
{
  WBCGtk *wbcg;

  if (fake_wimp_script_pos () < fake_wimp_script_len - 1)
    return;
  switch (stage)
    {
    case 0:
      wbcg = first_wbcg ();
      if (wbcg && gtk_widget_get_mapped (GTK_WIDGET (wbcg_toplevel (wbcg))) && fake_plots > 0)
        {
          started = 1;
          stage = 1;
          g_idle_add (do_cells, NULL);
          g_main_context_wakeup (NULL);
        }
      else if (++waits > 400)
        stage = 99;
      nulls (5);
      break;
    case 1:
      if (getenv ("GN_SHOT") && waits < 1000)
        {
          int i;
          waits = 1000;
          for (i = 0; i < 12; i++) { add (FAKE_SLEEP, 50, 0, 0); add (FAKE_NULL, 0, 0, 0); }
          break;
        }
      if (getenv ("GN_SHOT"))
        shot (getenv ("GN_SHOT"));
      if (getenv ("GN_TREE"))
        tree (GTK_WIDGET (wbcg_toplevel (first_wbcg ())), 0);
      stage = 2;
      g_idle_add (do_files, NULL);
      g_main_context_wakeup (NULL);
      nulls (10);
      break;
    case 2:
      /* reading and saving files lets the desktop run (progress), so this
         comes back before do_files has finished */
      if (!files_done)
        {
          nulls (5);
          break;
        }
      {
        Workbook *wb = wb_control_get_workbook (GNM_WBC (first_wbcg ()));
        go_doc_set_dirty (GO_DOC (wb), FALSE);      /* no "save changes?" */
      }
      stage = 3;
      nulls (10);              /* then the fake closes the window */
      break;
    }
}

int
main (int argc, char **argv)
{
  char *a[] = { argv[0], NULL };
  char *conf = g_build_filename (argv[1], "fonts", "fonts.conf", NULL);
  char *share = g_build_filename (argv[1], "share", NULL);
  char *go = g_build_filename (argv[1], "goffice", NULL);
  int status;

  outdir = argv[2];
  g_setenv ("GNUMERIC_DIR", argv[1], TRUE);
  g_setenv ("GOFFICE_DIR", go, TRUE);
  g_setenv ("FONTCONFIG_FILE", conf, TRUE);
  g_setenv ("XDG_DATA_DIRS", share, TRUE);
  g_setenv ("XDG_CONFIG_HOME", outdir, TRUE);
  g_setenv ("GNUMERIC_NO_LOG", "1", TRUE);
  fake_set_screen (1280, 1024, 0, 5);
  fake_wimp_nulls = 40;
  fake_wimp_hook = hook;
  status = riscos_gnumeric_main (1, a);

  check (started, "Gnumeric starts with a workbook window");
  check (avg_ok, "=AVERAGE(A1:A3) (fn-stat, a built-in plugin) is 3");
  check (concat_ok, "=CONCATENATE(\"Risc\",\"OS\") (fn-string) is RiscOS");
  check (round_ok, "=ROUND(2.675,1) (fn-math) works");
  check (ods_saved, "saved as OpenDocument (the openoffice plugin)");
  check (xlsx_saved, "saved as .xlsx (the excel plugin)");
  check (reopened_ok, "the .xlsx opens again with A4 = 3");
  check (plot_ok, "a bar chart plot (GOffice's plot_barcol plugin)");
  check (scrolled_ok, "scrolling a sheet with a check box on it");
  check (mime_ok, "file types from names: .xlsx and .ods are spreadsheets");
  check (grab_ok, "File > Open: the hidden dialogue doesn't take the grab from the Open box");
  check (icons_ok, "Gnumeric's own toolbar icons in the RISC OS style");
  check (fontsel_ok, "the font chooser's preview changes face for Sans, Serif and Monospace");
  check (stage == 3, "closing the window quits Gnumeric");
  printf ("# exit status %d\n", status);
  printf ("1..%d\n", n_tests);
  return n_failed;
}
