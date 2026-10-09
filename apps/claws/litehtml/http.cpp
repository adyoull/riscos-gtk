/*
 * http.cpp - the litehtml viewer's image loader on RISC OS (riscos-gtk);
 * see http.h. HTTP/1.1 GET, "Connection: close", no compression; follows
 * up to 5 redirects; chunked or plain bodies up to 16 MB; https through
 * Claws Mail's ssl_init_socket (AcornSSL checks the certificate against
 * the server's name). Nothing here uses signals or Claws Mail's
 * main-thread socket helpers (sock_connect uses SIGALRM).
 *
 * GPL version 3 or later, as Claws Mail. Copyright (C) 2026 Andrew Youll.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include "http.h"

extern "C" {
#include "socket.h"
#include "ssl.h"
#include "utils.h"
}

#define MAX_BODY (16 * 1024 * 1024)
#define MAX_REDIRECTS 5

static GQuark
http_error_quark (void)
{
  return g_quark_from_static_string ("riscos-litehtml-http");
}

static void
set_error (GError **error, const char *fmt, const char *arg)
{
  if (error && !*error)
    *error = g_error_new (http_error_quark (), 1, fmt, arg);
}

/* a TCP connection to host:port, giving up after HTTP_GET_TIMEOUT */
static int
tcp_connect (const char *host, const char *port)
{
  struct addrinfo hints, *res, *ai;
  int fd = -1;

  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo (host, port, &hints, &res) != 0)
    return -1;
  for (ai = res; ai; ai = ai->ai_next)
    {
      int flags, err = 0;
      socklen_t len = sizeof err;
      fd_set w;
      struct timeval tv = { HTTP_GET_TIMEOUT, 0 };

      fd = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (fd < 0)
        continue;
      flags = fcntl (fd, F_GETFL, 0);
      fcntl (fd, F_SETFL, flags | O_NONBLOCK);
      if (connect (fd, ai->ai_addr, ai->ai_addrlen) == 0)
        {
          fcntl (fd, F_SETFL, flags);
          break;
        }
      if (errno == EINPROGRESS)
        {
          FD_ZERO (&w);
          FD_SET (fd, &w);
          if (select (fd + 1, NULL, &w, NULL, &tv) > 0 &&
              getsockopt (fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0)
            {
              fcntl (fd, F_SETFL, flags);
              break;
            }
        }
      close (fd);
      fd = -1;
    }
  freeaddrinfo (res);
  return fd;
}

/* splits http[s]://host[:port]/path */
static gboolean
split_url (const char *url, gboolean *tls, char **host, char **port, char **path)
{
  const char *p, *slash, *colon;

  if (!g_ascii_strncasecmp (url, "http://", 7))
    *tls = FALSE, p = url + 7;
  else if (!g_ascii_strncasecmp (url, "https://", 8))
    *tls = TRUE, p = url + 8;
  else
    return FALSE;
  slash = strchr (p, '/');
  if (!slash)
    slash = p + strlen (p);
  colon = (const char *) memchr (p, ':', slash - p);
  if (colon)
    {
      *host = g_strndup (p, colon - p);
      *port = g_strndup (colon + 1, slash - colon - 1);
    }
  else
    {
      *host = g_strndup (p, slash - p);
      *port = g_strdup (*tls ? "443" : "80");
    }
  *path = g_strdup (*slash ? slash : "/");
  { char *h = strchr (*path, '#'); if (h) *h = 0; }
  return **host != 0;
}

/* one GET: the status, the headers (lower-cased names) and the body */
static int
get_once (const char *url, GHashTable *headers, GByteArray *body, GError **error)
{
  gboolean tls;
  char *host = NULL, *port = NULL, *path = NULL, *req;
  GByteArray *in = g_byte_array_new ();
  SockInfo *sock = NULL;
  int fd, status = -1;
  char buf[8192];
  gint n;

  if (!split_url (url, &tls, &host, &port, &path))
    {
      set_error (error, "not an http or https address: %s", url);
      goto out;
    }
  fd = tcp_connect (host, port);
  if (fd < 0)
    {
      set_error (error, "can't connect to %s", host);
      goto out;
    }
  sock = g_new0 (SockInfo, 1);
  sock->sock = fd;
  sock->hostname = g_strdup (host);
  sock->port = atoi (port);
  sock->state = CONN_ESTABLISHED;
  sock->use_tls_sni = TRUE;
  if (tls && !ssl_init_socket (sock))
    {
      set_error (error, "TLS connection to %s failed", host);
      goto out;
    }
  req = g_strdup_printf ("GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Claws Mail (RISC OS)\r\n"
                         "Accept: image/*\r\nConnection: close\r\n\r\n", path, host);
  n = sock_write_all (sock, req, strlen (req));
  g_free (req);
  if (n < 0)
    {
      set_error (error, "can't send to %s", host);
      goto out;
    }
  while ((n = sock_read (sock, buf, sizeof buf)) > 0)
    {
      g_byte_array_append (in, (guint8 *) buf, n);
      if (in->len > MAX_BODY + 65536)
        break;
    }

  {
    /* status line and headers */
    char *data = (char *) in->data, *end, *line, *eol;
    guint hlen;

    g_byte_array_append (in, (const guint8 *) "", 1);
    data = (char *) in->data;
    end = strstr (data, "\r\n\r\n");
    if (!end || strncmp (data, "HTTP/1.", 7) != 0)
      {
        set_error (error, "no HTTP reply from %s", host);
        goto out;
      }
    hlen = end + 4 - data;
    status = atoi (data + 9);
    for (line = strstr (data, "\r\n") + 2; line < end; line = eol + 2)
      {
        char *c;
        eol = strstr (line, "\r\n");
        c = (char *) memchr (line, ':', eol - line);
        if (c)
          {
            char *v = c + 1;
            while (*v == ' ' || *v == '\t')
              v++;
            g_hash_table_replace (headers, g_ascii_strdown (line, c - line), g_strndup (v, eol - v));
          }
      }

    /* the body, unchunked */
    {
      const char *te = (const char *) g_hash_table_lookup (headers, "transfer-encoding");
      guint8 *b = in->data + hlen;
      guint blen = in->len - 1 - hlen;

      if (te && g_ascii_strcasecmp (te, "chunked") == 0)
        {
          guint i = 0;
          while (i < blen)
            {
              char *e;
              unsigned long size = strtoul ((char *) b + i, &e, 16);
              char *crlf = strstr ((char *) b + i, "\r\n");
              if (!crlf || size == 0)
                break;
              i = (crlf + 2) - (char *) b;
              if (i + size > blen)
                size = blen - i;
              g_byte_array_append (body, b + i, size);
              i += size + 2;
            }
        }
      else
        g_byte_array_append (body, b, blen);
      if (body->len > MAX_BODY)
        g_byte_array_set_size (body, MAX_BODY);
    }
  }

out:
  if (sock)
    {
      if (sock->ssl)
        ssl_done_socket (sock);
      close (sock->sock);
      g_free (sock->hostname);
      g_free (sock);
    }
  g_byte_array_free (in, TRUE);
  g_free (host);
  g_free (port);
  g_free (path);
  return status;
}

/* a Location against the address it came from */
static char *
resolve (const char *base, const char *loc)
{
  const char *p;

  if (strstr (loc, "://"))
    return g_strdup (loc);
  if (loc[0] == '/' && loc[1] == '/')
    return g_strconcat (!g_ascii_strncasecmp (base, "https:", 6) ? "https:" : "http:", loc, NULL);
  p = strstr (base, "://");
  p = p ? strchr (p + 3, '/') : NULL;
  if (loc[0] == '/')
    {
      char *origin = p ? g_strndup (base, p - base) : g_strdup (base);
      char *r = g_strconcat (origin, loc, NULL);
      g_free (origin);
      return r;
    }
  {
    const char *last = strrchr (base, '/');
    char *dir = g_strndup (base, (p && last >= p ? last : base + strlen (base) - 1) - base + 1);
    char *r = g_strconcat (dir, loc, NULL);
    g_free (dir);
    return r;
  }
}

http::http()
{
  stream = NULL;
}

http::~http()
{
  destroy_giostream();
}

void http::destroy_giostream()
{
  if (stream)
    {
      g_input_stream_close (stream, NULL, NULL);
      g_object_unref (stream);
      stream = NULL;
    }
}

GInputStream *http::load_url(const gchar *url, GError **error)
{
  GError *_error = NULL;
  gsize len;
  gchar *content;

  if (!strncmp (url, "file:///", 8) || g_file_test (url, G_FILE_TEST_EXISTS))
    {
      gchar *newurl = g_filename_from_uri (url, NULL, NULL);
      if (g_file_get_contents (newurl ? newurl : url, &content, &len, &_error))
        stream = g_memory_input_stream_new_from_data (content, len, g_free);
      g_free (newurl);
    }
  else
    {
      char *u = g_strdup (url);
      int i;

      for (i = 0; i <= MAX_REDIRECTS; i++)
        {
          GHashTable *h = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
          GByteArray *body = g_byte_array_new ();
          int status = get_once (u, h, body, &_error);
          const char *loc = (const char *) g_hash_table_lookup (h, "location");

          if (status >= 300 && status < 400 && loc && i < MAX_REDIRECTS)
            {
              char *n = resolve (u, loc);
              g_free (u);
              u = n;
              g_hash_table_destroy (h);
              g_byte_array_free (body, TRUE);
              continue;
            }
          if (status == 200)
            {
              len = body->len;
              stream = g_memory_input_stream_new_from_data (g_byte_array_free (body, FALSE), len, g_free);
            }
          else
            {
              if (!_error)
                _error = g_error_new (http_error_quark (), status, "HTTP %d from %s", status, u);
              g_byte_array_free (body, TRUE);
            }
          g_hash_table_destroy (h);
          break;
        }
      g_free (u);
    }

  if (error && _error)
    *error = _error;
  else if (_error)
    g_error_free (_error);
  return stream;
}
