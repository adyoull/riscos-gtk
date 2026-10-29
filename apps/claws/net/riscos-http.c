/*
 * riscos-http.c - ClawsMail on RISC OS (riscos-gtk): HTTP and HTTPS
 * requests; see riscos-http.h. HTTP/1.1 with "Connection: close" and no
 * compression; the body read to the end of the connection, chunked or
 * plain, at most 16 MB; https through Claws Mail's ssl_init_socket
 * (AcornSSL checks the certificate against the server's name).
 *
 * GPL version 3 or later, as Claws Mail. Copyright (C) 2026 Andrew Youll.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include "riscos-http.h"

#include "socket.h"
#include "ssl.h"

#define MAX_BODY (16 * 1024 * 1024)

GQuark
riscos_http_error_quark (void)
{
  return g_quark_from_static_string ("riscos-http");
}

static void
set_error (GError **error, int code, const char *fmt, const char *arg)
{
  if (error && !*error)
    *error = g_error_new (riscos_http_error_quark (), code, fmt, arg);
}

/* a TCP connection to host:port, giving up after timeout seconds; reads
   time out after it too (*timed_out says if connecting did) */
static int
tcp_connect (const char *host, const char *port, int timeout, gboolean *timed_out)
{
  struct addrinfo hints, *res, *ai;
  int fd = -1;

  *timed_out = FALSE;
  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo (host, port, &hints, &res) != 0)
    return -1;
  for (ai = res; ai; ai = ai->ai_next)
    {
      int flags, err = 0, r;
      socklen_t len = sizeof err;
      struct sockaddr_storage peer;
      socklen_t plen = sizeof peer;
      fd_set w;
      struct timeval tv = { timeout, 0 };

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
          r = select (fd + 1, NULL, &w, NULL, &tv);
          if (r == 0)
            *timed_out = TRUE;
          /* (RISC OS's stack can report a refused connection as writable
             with no SO_ERROR: connected only if it has a peer) */
          if (r > 0 && getsockopt (fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0 &&
              getpeername (fd, (struct sockaddr *) &peer, &plen) == 0)
            {
              fcntl (fd, F_SETFL, flags);
              break;
            }
        }
      close (fd);
      fd = -1;
    }
  freeaddrinfo (res);
  if (fd >= 0)
    {
      /* reads give up after the timeout too (where the stack has it) */
      struct timeval tv = { timeout, 0 };
      setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
      setsockopt (fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    }
  return fd;
}

/* splits http[s]://[user[:password]@]host[:port]/path */
static gboolean
split_url (const char *url, gboolean *tls, char **userinfo, char **host, char **port, char **path)
{
  const char *p, *slash, *colon, *at;

  if (!g_ascii_strncasecmp (url, "http://", 7))
    *tls = FALSE, p = url + 7;
  else if (!g_ascii_strncasecmp (url, "https://", 8))
    *tls = TRUE, p = url + 8;
  else
    return FALSE;
  slash = p + strcspn (p, "/?#");
  at = memchr (p, '@', slash - p);
  *userinfo = NULL;
  if (at)
    {
      *userinfo = g_uri_unescape_segment (p, at, NULL);
      p = at + 1;
    }
  colon = memchr (p, ':', slash - p);
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
  if (*slash == '/')
    *path = g_strdup (slash);
  else
    *path = g_strconcat ("/", slash, NULL);
  {
    char *h = strchr (*path, '#');
    if (h)
      *h = 0;
  }
  return **host != 0;
}

/* the body of a chunked reply */
static void
unchunk (const guint8 *b, gsize blen, GByteArray *body)
{
  gsize i = 0;

  while (i < blen)
    {
      char *e;
      unsigned long size;
      const guint8 *crlf;
      char num[20];
      gsize n = MIN (blen - i, sizeof num - 1);

      memcpy (num, b + i, n);
      num[n] = 0;
      size = strtoul (num, &e, 16);
      crlf = (const guint8 *) g_strstr_len ((const char *) b + i, blen - i, "\r\n");
      if (!crlf || e == num || size == 0)
        break;
      i = crlf + 2 - b;
      if (size > blen - i)
        size = blen - i;
      if (body)
        g_byte_array_append (body, b + i, size);
      i += size + 2;
    }
}

/* one request: the status, the headers (lower-cased names), the body */
static int
request_once (const RiscosHttpRequest *rq, const char *url, GHashTable *headers,
              GByteArray *body, GError **error)
{
  gboolean tls, timed_out;
  char *userinfo = NULL, *host = NULL, *port = NULL, *path = NULL;
  GByteArray *in = g_byte_array_new ();
  GString *req;
  SockInfo *sock = NULL;
  int fd, status = -1, timeout = rq->timeout > 0 ? rq->timeout : 30;
  const char *method = rq->method ? rq->method : "GET";
  char buf[8192];
  gint n;

  if (!split_url (url, &tls, &userinfo, &host, &port, &path))
    {
      set_error (error, RISCOS_HTTP_ERROR_URL, "not an http or https address: %s", url);
      goto out;
    }
  fd = tcp_connect (host, port, timeout, &timed_out);
  if (fd < 0)
    {
      set_error (error, timed_out ? RISCOS_HTTP_ERROR_TIMEOUT : RISCOS_HTTP_ERROR_CONNECT, timed_out ? "timed out connecting to %s" : "can't connect to %s", host);
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
      set_error (error, RISCOS_HTTP_ERROR_TLS, "TLS connection to %s failed", host);
      goto out;
    }

  req = g_string_new (NULL);
  g_string_append_printf (req, "%s %s HTTP/1.1\r\nHost: %s", method, path, host);
  if (strcmp (port, tls ? "443" : "80") != 0)
    g_string_append_printf (req, ":%s", port);
  g_string_append_printf (req, "\r\nUser-Agent: %s\r\nConnection: close\r\n",
                          rq->user_agent ? rq->user_agent : "Claws Mail (RISC OS)");
  if (rq->user || userinfo)
    {
      char *up = rq->user ? g_strconcat (rq->user, ":", rq->password ? rq->password : "", NULL)
                          : g_strdup (userinfo);
      char *b64 = g_base64_encode ((const guchar *) up, strlen (up));
      g_string_append_printf (req, "Authorization: Basic %s\r\n", b64);
      g_free (b64);
      g_free (up);
    }
  {
    gboolean accept = FALSE;
    const char *const *h;
    for (h = rq->headers; h && *h; h++)
      {
        g_string_append_printf (req, "%s\r\n", *h);
        if (!g_ascii_strncasecmp (*h, "accept:", 7))
          accept = TRUE;
      }
    if (!accept)
      g_string_append (req, "Accept: */*\r\n");
  }
  if (rq->body || strcmp (method, "GET") != 0)
    g_string_append_printf (req, "Content-Length: %" G_GSIZE_FORMAT "\r\n", rq->body ? rq->body_len : 0);
  g_string_append (req, "\r\n");
  n = sock_write_all (sock, req->str, req->len);
  g_string_free (req, TRUE);
  if (n >= 0 && rq->body && rq->body_len)
    n = sock_write_all (sock, (const char *) rq->body, rq->body_len);
  if (n < 0)
    {
      set_error (error, RISCOS_HTTP_ERROR_SEND, "can't send to %s", host);
      goto out;
    }
  errno = 0;
  while ((n = sock_read (sock, buf, sizeof buf)) > 0)
    {
      g_byte_array_append (in, (guint8 *) buf, n);
      if (in->len > MAX_BODY + 65536)
        break;
    }
  if (n < 0 && in->len == 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT))
    {
      set_error (error, RISCOS_HTTP_ERROR_TIMEOUT, "timed out waiting for %s", host);
      goto out;
    }

  {
    /* the status line and headers */
    char *data, *end, *line, *eol;
    guint hlen;
    gboolean chunked = FALSE;

    g_byte_array_append (in, (const guint8 *) "", 1);
    data = (char *) in->data;
    end = strstr (data, "\r\n\r\n");
    if (!end || strncmp (data, "HTTP/1.", 7) != 0)
      {
        set_error (error, RISCOS_HTTP_ERROR_NO_REPLY, "no HTTP reply from %s", host);
        goto out;
      }
    hlen = end + 4 - data;
    status = atoi (data + 9);
    for (line = strstr (data, "\r\n") + 2; line < end; line = eol + 2)
      {
        char *c;
        eol = strstr (line, "\r\n");
        c = memchr (line, ':', eol - line);
        if (c && headers)
          {
            char *v = c + 1;
            while (*v == ' ' || *v == '\t')
              v++;
            g_hash_table_replace (headers, g_ascii_strdown (line, c - line), g_strndup (v, eol - v));
          }
        if (c && !g_ascii_strncasecmp (line, "transfer-encoding:", 18) && g_strstr_len (line, eol - line, "chunked"))
          chunked = TRUE;
      }

    /* the body, unchunked */
    if (body && strcmp (method, "HEAD") != 0)
      {
        guint8 *b = in->data + hlen;
        guint blen = in->len - 1 - hlen;

        if (chunked)
          unchunk (b, blen, body);
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
  g_free (userinfo);
  g_free (host);
  g_free (port);
  g_free (path);
  return status;
}

char *
riscos_http_resolve (const char *base, const char *loc)
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
    const char *q = strchr (base, '?');
    char *b = q ? g_strndup (base, q - base) : g_strdup (base);
    const char *last = strrchr (b, '/');
    char *dir, *r;
    if (!p || last < b + (p - base))
      dir = g_strconcat (b, "/", NULL);
    else
      dir = g_strndup (b, last - b + 1);
    r = g_strconcat (dir, loc, NULL);
    g_free (dir);
    g_free (b);
    return r;
  }
}

int
riscos_http_request (const RiscosHttpRequest *rq, GHashTable *headers,
                     GByteArray *body, char **final_url, GError **error)
{
  char *u;
  int i, status = -1;

  if (final_url)
    *final_url = NULL;
  if (!g_ascii_strncasecmp (rq->url, "file:", 5))
    {
      char *name = g_filename_from_uri (rq->url, NULL, error);
      gchar *data;
      gsize len;

      if (name && g_file_get_contents (name, &data, &len, error))
        {
          if (body)
            g_byte_array_append (body, (guint8 *) data, len);
          g_free (data);
          status = 200;
        }
      g_free (name);
      if (final_url && status == 200)
        *final_url = g_strdup (rq->url);
      return status;
    }

  u = g_strdup (rq->url);
  for (i = 0;; i++)
    {
      GHashTable *h = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
      GByteArray *b = g_byte_array_new ();
      const char *loc;

      status = request_once (rq, u, h, b, error);
      loc = g_hash_table_lookup (h, "location");
      if (status >= 300 && status < 400 && status != 304 && loc && rq->max_redirects >= 0 &&
          i < rq->max_redirects)
        {
          char *n = riscos_http_resolve (u, loc);
          g_free (u);
          u = n;
          g_hash_table_destroy (h);
          g_byte_array_free (b, TRUE);
          continue;
        }
      if (headers)
        {
          GHashTableIter it;
          gpointer k, v;
          g_hash_table_iter_init (&it, h);
          while (g_hash_table_iter_next (&it, &k, &v))
            g_hash_table_replace (headers, g_strdup (k), g_strdup (v));
        }
      if (body)
        g_byte_array_append (body, b->data, b->len);
      g_hash_table_destroy (h);
      g_byte_array_free (b, TRUE);
      break;
    }
  if (final_url)
    *final_url = u;
  else
    g_free (u);
  return status;
}
