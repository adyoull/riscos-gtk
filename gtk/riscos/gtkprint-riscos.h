/* gtkprint-riscos.h - riscos-gtk: GtkPrintOperation's RISC OS backend,
 * called from gtkprintoperation-unix.c (GTK patch).
 * LGPL-2.1-or-later. Copyright (C) 2026 Andrew Youll. */
#ifndef __GTK_PRINT_RISCOS_H__
#define __GTK_PRINT_RISCOS_H__

#include "gtkprintoperation-private.h"

G_BEGIN_DECLS

GtkPrintOperationResult _gtk_print_operation_riscos_run_dialog             (GtkPrintOperation          *op,
                                                                             gboolean                    show_dialog,
                                                                             GtkWindow                  *parent,
                                                                             gboolean                   *do_print);
void                    _gtk_print_operation_riscos_run_dialog_async       (GtkPrintOperation          *op,
                                                                             gboolean                    show_dialog,
                                                                             GtkWindow                  *parent,
                                                                             GtkPrintOperationPrintFunc  print_cb);
void                    _gtk_print_operation_riscos_launch_preview         (GtkPrintOperation          *op,
                                                                             cairo_surface_t            *surface,
                                                                             GtkWindow                  *parent,
                                                                             const gchar                *filename);
cairo_surface_t *       _gtk_print_operation_riscos_create_preview_surface (GtkPrintOperation          *op,
                                                                             GtkPageSetup               *page_setup,
                                                                             gdouble                    *dpi_x,
                                                                             gdouble                    *dpi_y,
                                                                             gchar                     **target);
void                    _gtk_print_operation_riscos_resize_preview_surface (GtkPrintOperation          *op,
                                                                             GtkPageSetup               *page_setup,
                                                                             cairo_surface_t            *surface);
void                    _gtk_print_operation_riscos_preview_start_page     (GtkPrintOperation          *op,
                                                                             cairo_surface_t            *surface,
                                                                             cairo_t                    *cr);
void                    _gtk_print_operation_riscos_preview_end_page       (GtkPrintOperation          *op,
                                                                             cairo_surface_t            *surface,
                                                                             cairo_t                    *cr);

G_END_DECLS

#endif
