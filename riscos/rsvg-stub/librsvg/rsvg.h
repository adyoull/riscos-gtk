/*
 * rsvg.h - riscos-gtk's stand-in for librsvg 2 (which is written in Rust
 * since 2.42, and can't be built for RISC OS): the calls GOffice makes.
 * Loading always fails (rsvg_handle_new_from_data returns NULL and sets
 * the error), so SVG pictures aren't shown; everything else is a no-op on
 * a NULL handle. MIT licence.
 */
#ifndef RISCOS_RSVG_STUB_H
#define RISCOS_RSVG_STUB_H
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef struct _RsvgHandle RsvgHandle;
typedef struct {
  int width, height;
  gdouble em, ex;
} RsvgDimensionData;

#define RSVG_HANDLE(o) ((RsvgHandle *) (o))
#define RSVG_ERROR (rsvg_error_quark ())

GQuark      rsvg_error_quark (void);
RsvgHandle *rsvg_handle_new_from_data (const guint8 *data, gsize data_len, GError **error);
void        rsvg_handle_set_dpi_x_y (RsvgHandle *handle, double dpi_x, double dpi_y);
void        rsvg_handle_get_dimensions (RsvgHandle *handle, RsvgDimensionData *dimension_data);
GdkPixbuf  *rsvg_handle_get_pixbuf (RsvgHandle *handle);

G_END_DECLS
#endif
