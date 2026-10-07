/*
 * gtkprint-riscos.c - riscos-gtk: GtkPrintOperation on RISC OS.
 *
 * GTK's "platform backend" for printing (as gtkprintoperation-win32.c is
 * Windows's), called from gtkprintoperation-unix.c (GTK patch):
 *
 *   - the Print dialogue box: the Toolbox's PrintDbox, laid out from the
 *     Toolbox manual's picture (print-2.png, 1 pixel = 2 OS units; work
 *     area 320 x 196): All / From [  ] to [  ]; Copies; Scale %; Upright /
 *     Sideways, Draft; Save, Cancel, Print. Its title is the printer's
 *     name ("Unknown printer" without one). Save keeps the settings for
 *     the next time without printing;
 *   - printing: each page drawn by GTK into a cairo recording surface in
 *     points, on the printer's paper (PDriver_PageSize), and printed by
 *     gdk/riscos/gdkprint-riscos.c (!Printers' protocol, PDriver) at the
 *     printer's resolution (at most 600 dpi; Draft 150) a strip at a time;
 *   - units: GTK's default unit (GTK_UNIT_NONE) is the surface's own, which
 *     programs take to be points (as GTK's PDF and PostScript surfaces on
 *     Unix are); so the contexts are 72 dpi to GTK, and the preview's
 *     bitmap gets a device scale;
 *     copies by the printer driver; landscape pages turned by GTK;
 *   - Print Preview: the pages drawn at 90 dpi (actual size on the RISC OS
 *     desktop) and shown in a window of our own: Page n of m, Back, Next,
 *     and Print (the program's own Print, when its menu has one).
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as the GTK code it
 * plugs into.
 */
#include "config.h"

#include "gtkprint-riscos.h"
#include "gtkprintoperation-private.h"
#include "gtkprintcontext.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkwindow.h"
#include "gtkfixed.h"
#include "gtkbox.h"
#include "gtklabel.h"
#include "gtkentry.h"
#include "gtkspinbutton.h"
#include "gtkradiobutton.h"
#include "gtkcheckbutton.h"
#include "gtkbutton.h"
#include "gtkscrolledwindow.h"
#include "gtkdrawingarea.h"
#include "gtkmenuitem.h"
#include "gtkcontainer.h"
#include "gtkmain.h"

#include "riscos/gdkriscos.h"
#include <stdlib.h>
#include <string.h>

#define MAX_DPI    600
#define DRAFT_DPI  150
#define PREVIEW_DPI 90

static gboolean
trace_on (void)
{
  return g_getenv ("RISCOS_GTK_DEBUG") != NULL || g_getenv ("RISCOS_GTK_PRINT_DEBUG") != NULL;
}

/* ---- the Print dialogue box (the Toolbox's PrintDbox) ------------------- */

enum { DBOX_CANCEL, DBOX_SAVE, DBOX_PRINT };

typedef struct {
  GtkWidget *window, *all, *from, *from_entry, *to_entry, *copies, *scale;
  GtkWidget *upright, *sideways, *draft, *print;
  int response;
  GMainLoop *loop;
} Dbox;

static GtkPrintSettings *saved;     /* the program's Save */

static void
dbox_done (Dbox *d, int response)
{
  d->response = response;
  if (d->loop && g_main_loop_is_running (d->loop))
    g_main_loop_quit (d->loop);
}

static void on_print (GtkButton *b, Dbox *d) { dbox_done (d, DBOX_PRINT); }
static void on_save (GtkButton *b, Dbox *d) { dbox_done (d, DBOX_SAVE); }
static void on_cancel (GtkButton *b, Dbox *d) { dbox_done (d, DBOX_CANCEL); }

static gboolean
on_delete (GtkWidget *w, GdkEvent *e, Dbox *d)
{
  dbox_done (d, DBOX_CANCEL);
  return TRUE;
}

static gboolean
on_key (GtkWidget *w, GdkEventKey *e, Dbox *d)
{
  if (e->keyval == GDK_KEY_Escape)
    {
      dbox_done (d, DBOX_CANCEL);
      return TRUE;
    }
  return FALSE;
}

static void
on_range (GtkToggleButton *b, Dbox *d)
{
  gboolean from = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (d->from));
  gtk_widget_set_sensitive (d->from_entry, from);
  gtk_widget_set_sensitive (d->to_entry, from);
}

/* A widget at (x, y), w x h, in the Toolbox picture's pixels */
static GtkWidget *
at (GtkWidget *fixed, GtkWidget *w, int x, int y, int width, int height)
{
  gtk_widget_set_size_request (w, width, height);
  gtk_fixed_put (GTK_FIXED (fixed), w, x, y);
  return w;
}

static GtkWidget *
label (const char *text, float xalign)
{
  GtkWidget *l = gtk_label_new (text);
  gtk_label_set_xalign (GTK_LABEL (l), xalign);
  return l;
}

static GtkWidget *
number_field (int value)
{
  GtkWidget *e = gtk_entry_new ();
  char text[16];
  g_snprintf (text, sizeof text, "%d", value);
  gtk_entry_set_text (GTK_ENTRY (e), text);
  gtk_entry_set_width_chars (GTK_ENTRY (e), 1);
  gtk_entry_set_alignment (GTK_ENTRY (e), 0.5);
  gtk_entry_set_activates_default (GTK_ENTRY (e), TRUE);
  return e;
}

static int
dbox_run (GtkWindow *parent, const char *printer_name, GtkPrintSettings *settings)
{
  Dbox d;
  GtkWidget *fixed, *b;
  GtkPrintPages pages = gtk_print_settings_get_print_pages (settings);
  GtkPageRange *ranges = NULL;
  int n_ranges = 0, from = 1, to = 1, copies, scale;
  GtkPageOrientation orient = gtk_print_settings_get_orientation (settings);

  memset (&d, 0, sizeof d);
  if (pages == GTK_PRINT_PAGES_RANGES)
    ranges = gtk_print_settings_get_page_ranges (settings, &n_ranges);
  if (n_ranges > 0)
    {
      from = ranges[0].start + 1;
      to = ranges[0].end + 1;
    }
  g_free (ranges);
  copies = MAX (1, gtk_print_settings_get_n_copies (settings));
  scale = (int) (gtk_print_settings_get_scale (settings) + 0.5);
  if (scale <= 0)
    scale = 100;

  d.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (d.window), printer_name);
  gtk_window_set_resizable (GTK_WINDOW (d.window), FALSE);
  gtk_window_set_deletable (GTK_WINDOW (d.window), FALSE);
  gtk_window_set_type_hint (GTK_WINDOW (d.window), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_modal (GTK_WINDOW (d.window), TRUE);
  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (d.window), parent);
  gtk_window_set_position (GTK_WINDOW (d.window), GTK_WIN_POS_MOUSE);
  g_signal_connect (d.window, "delete-event", G_CALLBACK (on_delete), &d);
  g_signal_connect (d.window, "key-press-event", G_CALLBACK (on_key), &d);

  /* The Toolbox manual's PrintDbox (print-2.png), measured at 1 pixel =
     2 OS units: work area 320 x 196; rows at 10, 50, 84, 122, 162. Draft
     and "%" sit a little further right than in the picture: the desktop
     font here is wider than the picture's system font */
  fixed = gtk_fixed_new ();
  gtk_widget_set_size_request (fixed, 320, 196);
  gtk_container_add (GTK_CONTAINER (d.window), fixed);

  d.all = at (fixed, gtk_radio_button_new_with_label (NULL, _("All")), 10, 10, 60, 26);
  d.from = at (fixed, gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (d.all), _("From")),
               79, 10, 70, 26);
  d.from_entry = at (fixed, number_field (from), 154, 10, 38, 26);
  at (fixed, label (_("to"), 0.5), 196, 10, 36, 26);
  d.to_entry = at (fixed, number_field (to), 236, 10, 38, 26);

  at (fixed, label (_("Copies"), 1.0), 40, 50, 104, 26);
  d.copies = at (fixed, gtk_spin_button_new_with_range (1, 999, 1), 154, 50, 74, 26);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (d.copies), copies);
  at (fixed, label (_("Scale"), 1.0), 40, 84, 104, 26);
  d.scale = at (fixed, gtk_spin_button_new_with_range (10, 999, 1), 154, 84, 74, 26);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (d.scale), scale);
  at (fixed, label ("%", 0.0), 240, 84, 20, 26);

  d.upright = at (fixed, gtk_radio_button_new_with_label (NULL, _("Upright")), 14, 122, 100, 24);
  d.sideways = at (fixed, gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (d.upright),
                                                                       _("Sideways")),
                   132, 122, 86, 24);
  d.draft = at (fixed, gtk_check_button_new_with_label (_("Draft")), 230, 122, 84, 24);
  if (orient == GTK_PAGE_ORIENTATION_LANDSCAPE || orient == GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (d.sideways), TRUE);
  if (gtk_print_settings_get_quality (settings) == GTK_PRINT_QUALITY_DRAFT)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (d.draft), TRUE);

  b = at (fixed, gtk_button_new_with_label (_("Save")), 96, 162, 63, 26);
  g_signal_connect (b, "clicked", G_CALLBACK (on_save), &d);
  b = at (fixed, gtk_button_new_with_label (_("Cancel")), 172, 162, 63, 26);
  g_signal_connect (b, "clicked", G_CALLBACK (on_cancel), &d);
  d.print = at (fixed, gtk_button_new_with_label (_("Print")), 250, 162, 57, 26);
  g_signal_connect (d.print, "clicked", G_CALLBACK (on_print), &d);
  gtk_widget_set_can_default (d.print, TRUE);
  gtk_window_set_default (GTK_WINDOW (d.window), d.print);

  g_signal_connect (d.all, "toggled", G_CALLBACK (on_range), &d);
  if (pages == GTK_PRINT_PAGES_RANGES && n_ranges > 0)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (d.from), TRUE);
  on_range (NULL, &d);

  g_object_set_data (G_OBJECT (d.window), "riscos-gtk-print-dbox", &d);
  gtk_widget_show_all (d.window);
  gtk_widget_grab_focus (d.print);
  if (trace_on ())
    g_printerr ("riscos-gtk: Print box shown (%s)\n", printer_name);

  d.response = -1;
  d.loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (d.loop);
  g_main_loop_unref (d.loop);
  d.loop = NULL;

  if (d.response != DBOX_CANCEL)
    {
      int c = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (d.copies));
      int s = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (d.scale));
      gtk_print_settings_set_n_copies (settings, c);
      gtk_print_settings_set_scale (settings, s);
      gtk_print_settings_set_orientation (settings,
                                          gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (d.sideways))
                                          ? GTK_PAGE_ORIENTATION_LANDSCAPE : GTK_PAGE_ORIENTATION_PORTRAIT);
      gtk_print_settings_set_quality (settings,
                                      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (d.draft))
                                      ? GTK_PRINT_QUALITY_DRAFT : GTK_PRINT_QUALITY_NORMAL);
      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (d.from)))
        {
          GtkPageRange r;
          r.start = MAX (1, atoi (gtk_entry_get_text (GTK_ENTRY (d.from_entry)))) - 1;
          r.end = MAX (r.start + 1, atoi (gtk_entry_get_text (GTK_ENTRY (d.to_entry)))) - 1;
          gtk_print_settings_set_print_pages (settings, GTK_PRINT_PAGES_RANGES);
          gtk_print_settings_set_page_ranges (settings, &r, 1);
        }
      else
        gtk_print_settings_set_print_pages (settings, GTK_PRINT_PAGES_ALL);
    }
  gtk_widget_destroy (d.window);
  if (trace_on ())
    g_printerr ("riscos-gtk: Print box: %s\n",
                d.response == DBOX_PRINT ? "Print" : d.response == DBOX_SAVE ? "Save" : "Cancel");
  return d.response;
}

/* ---- printing ------------------------------------------------------ */

typedef struct {
  cairo_surface_t *surface;    /* this page: a recording surface, points */
  double paper_w, paper_h;     /* points */
  int dpi;
  int copies;
  gboolean failed;
} RiscosJob;

static void
job_free (RiscosJob *job)
{
  if (job->surface)
    cairo_surface_destroy (job->surface);
  g_free (job);
}

/* a fresh page for GTK to draw (it takes the context after start_page) */
static gboolean
new_page (GtkPrintContext *ctx, RiscosJob *job)
{
  cairo_rectangle_t paper = { 0, 0, job->paper_w, job->paper_h };
  cairo_t *cr;

  if (job->surface)
    cairo_surface_destroy (job->surface);
  job->surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA, &paper);
  if (cairo_surface_status (job->surface) != CAIRO_STATUS_SUCCESS)
    return FALSE;
  cr = cairo_create (job->surface);
  gtk_print_context_set_cairo_context (ctx, cr, 72, 72);
  cairo_destroy (cr);
  return TRUE;
}

static void
riscos_start_page (GtkPrintOperation *op, GtkPrintContext *ctx, GtkPageSetup *page_setup)
{
  RiscosJob *job = op->priv->platform_data;

  if (!job->failed)
    new_page (ctx, job);
}

static void
riscos_end_page (GtkPrintOperation *op, GtkPrintContext *ctx)
{
  RiscosJob *job = op->priv->platform_data;
  GError *error = NULL;

  if (job->failed)
    return;
  cairo_surface_flush (job->surface);
  if (!gdk_riscos_print_page (job->surface, job->paper_w, job->paper_h, job->dpi, job->copies, &error))
    {
      job->failed = TRUE;
      if (op->priv->error == NULL)
        op->priv->error = error;
      else
        g_error_free (error);
      gtk_print_operation_cancel (op);
    }
}

static void
riscos_end_run (GtkPrintOperation *op, gboolean wait, gboolean cancelled)
{
  RiscosJob *job = op->priv->platform_data;

  gdk_riscos_print_end (cancelled || job->failed);
}

GtkPrintOperationResult
_gtk_print_operation_riscos_run_dialog (GtkPrintOperation *op,
                                        gboolean           show_dialog,
                                        GtkWindow         *parent,
                                        gboolean          *do_print)
{
  GtkPrintOperationPrivate *priv = op->priv;
  GtkPrintSettings *settings;
  GdkRiscosPrinter printer;
  gboolean have = gdk_riscos_printer_info (&printer);
  GtkPageSetup *page_setup, *base;
  GtkPaperSize *paper;
  RiscosJob *job;
  int dpi, w, h;

  *do_print = FALSE;
  if (priv->print_settings == NULL)
    priv->print_settings = gtk_print_settings_new ();
  settings = priv->print_settings;
  if (saved && gtk_print_settings_get (settings, GTK_PRINT_SETTINGS_N_COPIES) == NULL)
    {
      GtkPrintSettings *copy = gtk_print_settings_copy (saved);
      gtk_print_operation_set_print_settings (op, copy);
      g_object_unref (copy);
      settings = priv->print_settings;
    }

  if (show_dialog)
    {
      int response = dbox_run (parent, have ? printer.name : _("Unknown printer"), settings);
      if (response == DBOX_CANCEL)
        return GTK_PRINT_OPERATION_RESULT_CANCEL;
      if (response == DBOX_SAVE)
        {
          g_clear_object (&saved);
          saved = gtk_print_settings_copy (settings);
          return GTK_PRINT_OPERATION_RESULT_APPLY;
        }
    }

  /* the printer's paper, turned as the box says */
  base = gtk_print_operation_get_default_page_setup (op);
  page_setup = base ? gtk_page_setup_copy (base) : gtk_page_setup_new ();
  if (have)
    {
      paper = gtk_paper_size_new_custom ("riscos-printer", printer.name, printer.paper_width,
                                         printer.paper_height, GTK_UNIT_POINTS);
      gtk_page_setup_set_paper_size (page_setup, paper);
      gtk_paper_size_free (paper);
    }
  gtk_page_setup_set_orientation (page_setup, gtk_print_settings_get_orientation (settings));
  gtk_print_operation_set_default_page_setup (op, page_setup);

  priv->print_context = _gtk_print_context_new (op);
  _gtk_print_context_set_page_setup (priv->print_context, page_setup);
  if (have)
    _gtk_print_context_set_hard_margins (priv->print_context, printer.top, printer.bottom,
                                         printer.left, printer.right);

  if (!gdk_riscos_print_begin (priv->job_name, &priv->error))
    {
      g_object_unref (page_setup);
      return GTK_PRINT_OPERATION_RESULT_ERROR;
    }

  dpi = have ? MIN (printer.dpi_x, printer.dpi_y) : MAX_DPI;
  dpi = CLAMP (dpi, 72, gtk_print_settings_get_quality (settings) == GTK_PRINT_QUALITY_DRAFT
                        ? DRAFT_DPI : MAX_DPI);
  job = g_new0 (RiscosJob, 1);
  job->paper_w = have ? printer.paper_width : gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_POINTS);
  job->paper_h = have ? printer.paper_height : gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_POINTS);
  if (job->paper_w > job->paper_h && !have)
    {
      double t = job->paper_w;     /* (GTK turns landscape pages itself) */
      job->paper_w = job->paper_h;
      job->paper_h = t;
    }
  job->copies = MAX (1, gtk_print_settings_get_n_copies (settings));
  job->dpi = dpi;
  w = (int) (job->paper_w * dpi / 72.0 + 0.5);
  h = (int) (job->paper_h * dpi / 72.0 + 0.5);
  if (!new_page (priv->print_context, job))
    {
      if (job->surface)
        cairo_surface_destroy (job->surface);
      g_set_error_literal (&priv->error, GTK_PRINT_ERROR, GTK_PRINT_ERROR_NOMEM,
                           _("There isn't enough memory to print a page."));
      gdk_riscos_print_end (TRUE);
      g_free (job);
      g_object_unref (page_setup);
      return GTK_PRINT_OPERATION_RESULT_ERROR;
    }
  priv->platform_data = job;
  priv->free_platform_data = (GDestroyNotify) job_free;
  priv->print_pages = gtk_print_settings_get_print_pages (settings);
  priv->page_ranges = NULL;
  priv->num_page_ranges = 0;
  if (priv->print_pages == GTK_PRINT_PAGES_RANGES)
    priv->page_ranges = gtk_print_settings_get_page_ranges (settings, &priv->num_page_ranges);
  priv->manual_num_copies = 1;              /* (the printer driver makes the copies) */
  priv->manual_collation = FALSE;
  priv->manual_reverse = FALSE;
  priv->manual_orientation = TRUE;          /* GTK turns sideways pages */
  priv->manual_scale = gtk_print_settings_get_scale (settings) / 100.0;
  if (priv->manual_scale <= 0)
    priv->manual_scale = 1.0;
  priv->manual_page_set = GTK_PAGE_SET_ALL;
  priv->manual_number_up = 1;
  priv->manual_number_up_layout = GTK_NUMBER_UP_LAYOUT_LEFT_TO_RIGHT_TOP_TO_BOTTOM;
  priv->start_page = riscos_start_page;
  priv->end_page = riscos_end_page;
  priv->end_run = riscos_end_run;
  if (trace_on ())
    g_printerr ("riscos-gtk: printing \"%s\": %d dpi, %d x %d pixels, %d copies, scale %.0f%%\n",
                priv->job_name ? priv->job_name : "", dpi, w, h, job->copies,
                priv->manual_scale * 100);
  g_object_unref (page_setup);
  *do_print = TRUE;
  return GTK_PRINT_OPERATION_RESULT_APPLY;
}

void
_gtk_print_operation_riscos_run_dialog_async (GtkPrintOperation          *op,
                                              gboolean                    show_dialog,
                                              GtkWindow                  *parent,
                                              GtkPrintOperationPrintFunc  print_cb)
{
  gboolean do_print;
  GtkPrintOperationResult result = _gtk_print_operation_riscos_run_dialog (op, show_dialog, parent, &do_print);

  if (print_cb)
    print_cb (op, parent, do_print, result);
}

/* ---- Print Preview ---------------------------------------------------- */

#define PAGES_KEY "riscos-gtk-preview-pages"
#define PIXELS_KEY "riscos-gtk-preview-pixels"

cairo_surface_t *
_gtk_print_operation_riscos_create_preview_surface (GtkPrintOperation *op,
                                                    GtkPageSetup      *page_setup,
                                                    gdouble           *dpi_x,
                                                    gdouble           *dpi_y,
                                                    gchar            **target)
{
  int w = (int) (gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_INCH) * PREVIEW_DPI + 0.5);
  int h = (int) (gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_INCH) * PREVIEW_DPI + 0.5);

  *dpi_x = *dpi_y = 72;         /* points to GTK; the bitmap is scaled (see top) */
  *target = g_strdup ("riscos-preview");
  if (trace_on ())
    g_printerr ("riscos-gtk: print preview: pages %d x %d pixels\n", w, h);
  g_object_set_data_full (G_OBJECT (op), PAGES_KEY, g_ptr_array_new_with_free_func
                          ((GDestroyNotify) cairo_surface_destroy), (GDestroyNotify) g_ptr_array_unref);
  /* our own pixels: GTK finishes the surface before it's launched, and
     the last page is still to be kept then */
  {
    int stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, MAX (1, w));
    guchar *pixels = g_malloc0 ((gsize) stride * MAX (1, h));
    g_object_set_data_full (G_OBJECT (op), PIXELS_KEY, pixels, g_free);
    cairo_surface_t *s = cairo_image_surface_create_for_data (pixels, CAIRO_FORMAT_RGB24,
                                                              MAX (1, w), MAX (1, h), stride);
    cairo_surface_set_device_scale (s, PREVIEW_DPI / 72.0, PREVIEW_DPI / 72.0);
    return s;
  }
}

void
_gtk_print_operation_riscos_resize_preview_surface (GtkPrintOperation *op,
                                                    GtkPageSetup      *page_setup,
                                                    cairo_surface_t   *surface)
{
}

/* GTK's preview ends each page with cairo_show_page() only (nothing to
   an image surface), so a page is kept when the next starts, and the last
   when the preview is launched */
#define DRAWING_KEY "riscos-gtk-preview-drawing"

static void
keep_page (GtkPrintOperation *op, cairo_surface_t *surface)
{
  GPtrArray *pages = g_object_get_data (G_OBJECT (op), PAGES_KEY);
  int w = cairo_image_surface_get_width (surface), h = cairo_image_surface_get_height (surface);
  cairo_surface_t *copy;
  guchar *pixels = g_object_get_data (G_OBJECT (op), PIXELS_KEY);
  int stride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, w), y;

  if (pages == NULL || pixels == NULL || !g_object_get_data (G_OBJECT (op), DRAWING_KEY))
    return;
  g_object_set_data (G_OBJECT (op), DRAWING_KEY, NULL);
  if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
    cairo_surface_flush (surface);
  /* from the pixels themselves (the surface may be finished) */
  copy = cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);
  cairo_surface_flush (copy);
  for (y = 0; y < h; y++)
    memcpy (cairo_image_surface_get_data (copy) + (gsize) y * cairo_image_surface_get_stride (copy),
            pixels + (gsize) y * stride, (gsize) w * 4);
  cairo_surface_mark_dirty (copy);
  g_ptr_array_add (pages, copy);
}

void
_gtk_print_operation_riscos_preview_start_page (GtkPrintOperation *op,
                                                cairo_surface_t   *surface,
                                                cairo_t           *cr)
{
  cairo_t *c;

  keep_page (op, surface);
  c = cairo_create (surface);
  cairo_set_source_rgb (c, 1, 1, 1);
  cairo_paint (c);
  cairo_destroy (c);
  g_object_set_data (G_OBJECT (op), DRAWING_KEY, GINT_TO_POINTER (1));
}

void
_gtk_print_operation_riscos_preview_end_page (GtkPrintOperation *op,
                                              cairo_surface_t   *surface,
                                              cairo_t           *cr)
{
  keep_page (op, surface);
}

typedef struct {
  GPtrArray *pages;
  int page;
  GtkWidget *window, *area, *label, *back, *next, *print;
  GtkWindow *parent;
} Preview;

static void
preview_free (Preview *p)
{
  g_ptr_array_unref (p->pages);
  if (p->parent)
    g_object_remove_weak_pointer (G_OBJECT (p->parent), (gpointer *) &p->parent);
  g_free (p);
}

static void
preview_show_page (Preview *p)
{
  char *text = g_strdup_printf (_("Page %d of %d"), p->page + 1, p->pages->len);
  cairo_surface_t *s = g_ptr_array_index (p->pages, p->page);

  gtk_label_set_text (GTK_LABEL (p->label), text);
  g_free (text);
  gtk_widget_set_size_request (p->area, cairo_image_surface_get_width (s),
                               cairo_image_surface_get_height (s));
  gtk_widget_set_sensitive (p->back, p->page > 0);
  gtk_widget_set_sensitive (p->next, p->page + 1 < (int) p->pages->len);
  gtk_widget_queue_draw (p->area);
}

static gboolean
preview_draw (GtkWidget *w, cairo_t *cr, Preview *p)
{
  cairo_set_source_surface (cr, g_ptr_array_index (p->pages, p->page), 0, 0);
  cairo_paint (cr);
  return TRUE;
}

static void
preview_back (GtkButton *b, Preview *p)
{
  if (p->page > 0)
    {
      p->page--;
      preview_show_page (p);
    }
}

static void
preview_next (GtkButton *b, Preview *p)
{
  if (p->page + 1 < (int) p->pages->len)
    {
      p->page++;
      preview_show_page (p);
    }
}

static gboolean
preview_key (GtkWidget *w, GdkEventKey *e, Preview *p)
{
  switch (e->keyval)
    {
    case GDK_KEY_Page_Down:
      preview_next (NULL, p);
      return TRUE;
    case GDK_KEY_Page_Up:
      preview_back (NULL, p);
      return TRUE;
    case GDK_KEY_Escape:
      gtk_widget_destroy (p->window);
      return TRUE;
    default:
      return FALSE;
    }
}

/* The program's own Print menu item (its window's menu), to print from the
   preview */
static GtkWidget *
find_print_item (GtkWidget *w)
{
  GtkWidget *found = NULL;

  if (GTK_IS_MENU_ITEM (w))
    {
      const char *l = gtk_menu_item_get_label (GTK_MENU_ITEM (w));
      GtkWidget *sub = gtk_menu_item_get_submenu (GTK_MENU_ITEM (w));
      if (l && sub == NULL)
        {
          GString *s = g_string_new (NULL);
          const char *c;
          for (c = l; *c; c++)
            if (*c != '_')
              g_string_append_c (s, g_ascii_tolower (*c));
          while (s->len && (s->str[s->len - 1] == '.' || s->str[s->len - 1] == ' '))
            g_string_truncate (s, s->len - 1);
          if (g_str_has_suffix (s->str, "\xe2\x80\xa6"))
            g_string_truncate (s, s->len - 3);
          if (strcmp (s->str, "print") == 0)
            found = w;
          g_string_free (s, TRUE);
        }
      if (found == NULL && sub)
        found = find_print_item (sub);
      return found;
    }
  if (GTK_IS_CONTAINER (w))
    {
      GList *kids = gtk_container_get_children (GTK_CONTAINER (w)), *k;
      for (k = kids; k && !found; k = k->next)
        found = find_print_item (k->data);
      g_list_free (kids);
    }
  return found;
}

static void
preview_print (GtkButton *b, Preview *p)
{
  GtkWidget *item = p->parent ? find_print_item (GTK_WIDGET (p->parent)) : NULL;

  if (item)
    g_object_ref (item);
  gtk_widget_destroy (p->window);
  if (item)
    {
      gtk_menu_item_activate (GTK_MENU_ITEM (item));
      g_object_unref (item);
    }
}

void
_gtk_print_operation_riscos_launch_preview (GtkPrintOperation *op,
                                            cairo_surface_t   *surface,
                                            GtkWindow         *parent,
                                            const gchar       *filename)
{
  GPtrArray *pages = g_object_get_data (G_OBJECT (op), PAGES_KEY);
  Preview *p;
  GtkWidget *vbox, *row, *scroll;
  cairo_surface_t *first;
  char *title;
  int sw, sh, w, h;

  keep_page (op, surface);
  cairo_surface_destroy (surface);
  if (trace_on ())
    g_printerr ("riscos-gtk: print preview: %u pages drawn\n", pages ? pages->len : 0);
  if (pages == NULL || pages->len == 0)
    return;
  p = g_new0 (Preview, 1);
  p->pages = g_ptr_array_ref (pages);
  p->parent = parent;
  if (parent)
    g_object_add_weak_pointer (G_OBJECT (parent), (gpointer *) &p->parent);

  p->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  title = g_strdup_printf (_("Preview: %s"), op->priv->job_name ? op->priv->job_name : "");
  gtk_window_set_title (GTK_WINDOW (p->window), title);
  g_free (title);
  g_signal_connect_swapped (p->window, "destroy", G_CALLBACK (preview_free), p);
  g_signal_connect (p->window, "key-press-event", G_CALLBACK (preview_key), p);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (p->window), vbox);
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);
  p->area = gtk_drawing_area_new ();
  g_signal_connect (p->area, "draw", G_CALLBACK (preview_draw), p);
  gtk_container_add (GTK_CONTAINER (scroll), p->area);

  /* Page n of m, then Back, Next and Print across the bottom */
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_container_set_border_width (GTK_CONTAINER (row), 8);
  gtk_widget_set_margin_end (row, 24);       /* clear of the size icon in the corner */
  gtk_box_pack_start (GTK_BOX (vbox), row, FALSE, FALSE, 0);
  p->label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (row), p->label, FALSE, FALSE, 0);
  p->print = gtk_button_new_with_label (_("Print"));
  p->next = gtk_button_new_with_label (_("Next"));
  p->back = gtk_button_new_with_label (_("Back"));
  gtk_box_pack_end (GTK_BOX (row), p->print, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (row), p->next, FALSE, FALSE, 0);
  gtk_box_pack_end (GTK_BOX (row), p->back, FALSE, FALSE, 0);
  g_signal_connect (p->back, "clicked", G_CALLBACK (preview_back), p);
  g_signal_connect (p->next, "clicked", G_CALLBACK (preview_next), p);
  g_signal_connect (p->print, "clicked", G_CALLBACK (preview_print), p);
  gtk_widget_set_can_default (p->print, TRUE);

  /* the page at actual size, as much as fits on the screen */
  first = g_ptr_array_index (pages, 0);
  {
    GdkDisplay *display = gdk_screen_get_display (gtk_widget_get_screen (p->window));
    GdkMonitor *monitor = gdk_display_get_monitor (display, 0);
    GdkRectangle area = { 0, 0, 1280, 1024 };
    if (monitor)
      gdk_monitor_get_geometry (monitor, &area);
    sw = area.width;
    sh = area.height;
  }
  w = MIN (cairo_image_surface_get_width (first) + 24, sw - 64);
  h = MIN (cairo_image_surface_get_height (first) + 64, sh - 160);
  gtk_window_set_default_size (GTK_WINDOW (p->window), w, h);

  gtk_widget_show_all (p->window);
  if (!(p->parent && find_print_item (GTK_WIDGET (p->parent))))
    gtk_widget_hide (p->print);
  preview_show_page (p);
  if (trace_on ())
    g_printerr ("riscos-gtk: print preview: %u pages\n", pages->len);
  g_object_set_data (G_OBJECT (op), PAGES_KEY, NULL);
}
