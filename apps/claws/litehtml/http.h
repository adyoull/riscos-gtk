/*
 * http.h - the litehtml viewer's image loader on RISC OS (riscos-gtk):
 * replaces the plug-in's http.h, which uses libcurl. Same class, fetched
 * with plain sockets and, for https, Claws Mail's TLS (ssl_acornssl.c,
 * through AcornSSL). Called from a GTask thread.
 *
 * GPL version 3 or later, as Claws Mail. Copyright (C) 2026 Andrew Youll.
 */
#include <glib.h>
#include <gio/gio.h>

#define HTTP_GET_TIMEOUT 10

class http
{
    GInputStream *stream;

public:
    http();
    ~http();

    GInputStream *load_url(const gchar *url, GError **error);

private:
    void destroy_giostream();
};
