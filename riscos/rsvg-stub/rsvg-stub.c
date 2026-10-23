/* rsvg-stub.c - riscos-gtk's stand-in for librsvg (see librsvg/rsvg.h).
   MIT licence. */
#include <librsvg/rsvg.h>
#include <librsvg/rsvg-cairo.h>

GQuark
rsvg_error_quark (void)
{
  return g_quark_from_static_string ("rsvg-error-quark");
}

RsvgHandle *
rsvg_handle_new_from_data (const guint8 *data, gsize data_len, GError **error)
{
  g_set_error_literal (error, RSVG_ERROR, 0, "SVG pictures can't be shown on RISC OS yet");
  return NULL;
}

void
rsvg_handle_set_dpi_x_y (RsvgHandle *handle, double dpi_x, double dpi_y)
{
}

void
rsvg_handle_get_dimensions (RsvgHandle *handle, RsvgDimensionData *d)
{
  if (d)
    {
      d->width = d->height = 1;      /* never 0: callers divide by it */
      d->em = d->ex = 1.;
    }
}

GdkPixbuf *
rsvg_handle_get_pixbuf (RsvgHandle *handle)
{
  return NULL;
}

gboolean
rsvg_handle_render_cairo (RsvgHandle *handle, cairo_t *cr)
{
  return FALSE;
}

RsvgHandle *
rsvg_handle_new (void)
{
  return (RsvgHandle *) g_object_new (G_TYPE_OBJECT, NULL);
}

gboolean
rsvg_handle_write (RsvgHandle *handle, const guchar *buf, gsize count, GError **error)
{
  g_set_error_literal (error, RSVG_ERROR, 0, "SVG pictures can't be shown on RISC OS yet");
  return FALSE;
}

gboolean
rsvg_handle_close (RsvgHandle *handle, GError **error)
{
  g_set_error_literal (error, RSVG_ERROR, 0, "SVG pictures can't be shown on RISC OS yet");
  return FALSE;
}
