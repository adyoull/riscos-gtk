/*
 * riscos-http.h - ClawsMail on RISC OS (riscos-gtk): HTTP and HTTPS
 * requests with plain sockets and, for https, Claws Mail's TLS
 * (ssl_acornssl.c, through RISC OS's AcornSSL module). No signals and none
 * of Claws Mail's main-thread socket helpers (sock_connect uses SIGALRM),
 * so it can be called from any thread.
 *
 * Used by the litehtml viewer's picture loader (litehtml/http.cpp) and by
 * the libcurl subset the RSS and vCalendar plug-ins are built against
 * (curl/riscos-curl.c).
 *
 * GPL version 3 or later, as Claws Mail. Copyright (C) 2026 Andrew Youll.
 */
#ifndef RISCOS_HTTP_H
#define RISCOS_HTTP_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct
{
  const char *method;           /* "GET" (NULL), "PUT", "POST" ... */
  const char *url;              /* http://, https:// or file:// */
  const char *const *headers;   /* NULL-terminated "Name: value" lines */
  const char *user_agent;       /* NULL: "Claws Mail (RISC OS)" */
  const char *user, *password;  /* Basic authentication, if user is set */
  const guint8 *body;           /* the request body (PUT, POST) */
  gsize body_len;
  int timeout;                  /* seconds for connecting and each read; 0: 30 */
  int max_redirects;            /* followed for 3xx with Location; -1: none */
} RiscosHttpRequest;

/* the GError codes (domain riscos_http_error_quark) */
enum
{
  RISCOS_HTTP_ERROR_FAILED = 1,  /* anything else (a file:// address: GFileError) */
  RISCOS_HTTP_ERROR_TIMEOUT,     /* connecting or waiting timed out */
  RISCOS_HTTP_ERROR_CONNECT,     /* no connection to the server */
  RISCOS_HTTP_ERROR_TLS,         /* the TLS handshake failed */
  RISCOS_HTTP_ERROR_NO_REPLY,    /* no HTTP reply */
  RISCOS_HTTP_ERROR_URL,         /* not an http, https or file address */
  RISCOS_HTTP_ERROR_SEND         /* the request couldn't be sent */
};
GQuark riscos_http_error_quark (void);

/* The request; the status (200, 404 ...), or -1 with *error set. The
   response's headers go into headers (names lower-cased; may be NULL),
   its body (unchunked, at most 16 MB) into body (may be NULL), the address
   finally fetched into *final_url (may be NULL). A file:// address gives
   200 and the file. */
int riscos_http_request (const RiscosHttpRequest *rq, GHashTable *headers,
                         GByteArray *body, char **final_url, GError **error);

/* A Location against the address it came from */
char *riscos_http_resolve (const char *base, const char *location);

G_END_DECLS

#endif
