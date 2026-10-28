/*
 * http.cpp - the litehtml viewer's image loader on RISC OS (riscos-gtk);
 * see http.h. A GET through apps/claws/net/riscos-http.c (sockets, and
 * AcornSSL for https), following up to 5 redirects.
 *
 * GPL version 3 or later, as Claws Mail. Copyright (C) 2026 Andrew Youll.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "http.h"

extern "C" {
#include "riscos-http.h"
}

#define MAX_REDIRECTS 5

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
      static const char *const accept[] = { "Accept: image/*", NULL };
      RiscosHttpRequest rq;
      GByteArray *body = g_byte_array_new ();
      int status;

      memset (&rq, 0, sizeof rq);
      rq.url = url;
      rq.headers = accept;
      rq.timeout = HTTP_GET_TIMEOUT;
      rq.max_redirects = MAX_REDIRECTS;
      status = riscos_http_request (&rq, NULL, body, NULL, &_error);
      if (status == 200)
        {
          len = body->len;
          stream = g_memory_input_stream_new_from_data (g_byte_array_free (body, FALSE), len, g_free);
        }
      else
        {
          if (!_error)
            _error = g_error_new (riscos_http_error_quark (), status, "HTTP %d from %s", status, url);
          g_byte_array_free (body, TRUE);
        }
    }

  if (error && _error)
    *error = _error;
  else if (_error)
    g_error_free (_error);
  return stream;
}
