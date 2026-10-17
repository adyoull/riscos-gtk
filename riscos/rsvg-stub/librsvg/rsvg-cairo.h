/* rsvg-cairo.h - riscos-gtk's stand-in for librsvg (see rsvg.h). MIT licence. */
#ifndef RISCOS_RSVG_STUB_CAIRO_H
#define RISCOS_RSVG_STUB_CAIRO_H
#include <cairo.h>
#include <librsvg/rsvg.h>
G_BEGIN_DECLS
gboolean rsvg_handle_render_cairo (RsvgHandle *handle, cairo_t *cr);
G_END_DECLS
#endif
