/*
 * riscos-curl.c - riscos-gtk's libcurl subset for ClawsMail's RSS and
 * vCalendar plug-ins; see curl/curl.h. The requests are made by
 * apps/claws/net/riscos-http.c.
 *
 * GPL version 3 or later, as Claws Mail. Copyright (C) 2026 Andrew Youll.
 */
#include <stdarg.h>
#include <string.h>
#include <glib.h>
#include "curl/curl.h"
#include "riscos-http.h"

typedef struct
{
  char *url, *user_agent, *userpwd, *username, *password;
  struct curl_slist *headers;
  curl_write_callback write_fn;
  void *write_data;
  curl_read_callback read_fn;
  void *read_data;
  long timeout, maxredirs, follow, upload, failonerror;
  curl_off_t infilesize;
  /* the reply */
  long response_code;
  char *effective_url;
  GHashTable *reply_headers;
  struct curl_header header;
} Easy;

CURLcode
curl_global_init (long flags)
{
  return CURLE_OK;
}

void
curl_global_cleanup (void)
{
}

char *
curl_version (void)
{
  return (char *) "libcurl/" LIBCURL_VERSION " (riscos-gtk subset)";
}

CURL *
curl_easy_init (void)
{
  Easy *e = g_new0 (Easy, 1);

  e->maxredirs = -1;
  e->infilesize = -1;
  return e;
}

static void
reply_free (Easy *e)
{
  g_free (e->effective_url);
  e->effective_url = NULL;
  if (e->reply_headers)
    g_hash_table_destroy (e->reply_headers);
  e->reply_headers = NULL;
  e->response_code = 0;
}

void
curl_easy_reset (CURL *curl)
{
  Easy *e = curl;

  reply_free (e);
  g_free (e->url);
  g_free (e->user_agent);
  g_free (e->userpwd);
  g_free (e->username);
  g_free (e->password);
  memset (e, 0, sizeof *e);
  e->maxredirs = -1;
  e->infilesize = -1;
}

void
curl_easy_cleanup (CURL *curl)
{
  if (curl)
    {
      curl_easy_reset (curl);
      g_free (curl);
    }
}

static void
set_string (char **where, const char *s)
{
  g_free (*where);
  *where = s ? g_strdup (s) : NULL;
}

CURLcode
curl_easy_setopt (CURL *curl, CURLoption option, ...)
{
  Easy *e = curl;
  va_list ap;
  long l = 0;
  void *p = NULL;
  curl_off_t o = 0;
  CURLcode r = CURLE_OK;

  if (!e)
    return CURLE_BAD_FUNCTION_ARGUMENT;
  va_start (ap, option);
  if (option < CURLOPTTYPE_OBJECTPOINT)
    l = va_arg (ap, long);
  else if (option < CURLOPTTYPE_OFF_T)
    p = va_arg (ap, void *);
  else
    o = va_arg (ap, curl_off_t);
  va_end (ap);

  switch (option)
    {
    case CURLOPT_URL: set_string (&e->url, p); break;
    case CURLOPT_USERAGENT: set_string (&e->user_agent, p); break;
    case CURLOPT_USERPWD: set_string (&e->userpwd, p); break;
    case CURLOPT_USERNAME: set_string (&e->username, p); break;
    case CURLOPT_PASSWORD: set_string (&e->password, p); break;
    case CURLOPT_HTTPHEADER: e->headers = p; break;
    case CURLOPT_WRITEFUNCTION: e->write_fn = (curl_write_callback) p; break;
    case CURLOPT_WRITEDATA: e->write_data = p; break;
    case CURLOPT_READFUNCTION: e->read_fn = (curl_read_callback) p; break;
    case CURLOPT_READDATA: e->read_data = p; break;
    case CURLOPT_TIMEOUT: e->timeout = l; break;
    case CURLOPT_FOLLOWLOCATION: e->follow = l; break;
    case CURLOPT_MAXREDIRS: e->maxredirs = l; break;
    case CURLOPT_UPLOAD: e->upload = l; break;
    case CURLOPT_FAILONERROR: e->failonerror = l; break;
    case CURLOPT_INFILESIZE: e->infilesize = l; break;
    case CURLOPT_INFILESIZE_LARGE: e->infilesize = o; break;
    /* taken, nothing to do: no progress meter or signals here, no
       compression asked for, AcornSSL checks certificates itself, Basic
       is the only authentication */
    case CURLOPT_NOPROGRESS:
    case CURLOPT_NOSIGNAL:
    case CURLOPT_ENCODING:
    case CURLOPT_NETRC:
    case CURLOPT_COOKIEFILE:
    case CURLOPT_SSL_VERIFYPEER:
    case CURLOPT_SSL_VERIFYHOST:
    case CURLOPT_CAINFO:
    case CURLOPT_HTTPAUTH:
      break;
    default:
      r = CURLE_UNKNOWN_OPTION;
    }
  return r;
}

static CURLcode
code_for (const GError *err)
{
  if (!err)
    return CURLE_GOT_NOTHING;
  if (err->domain != riscos_http_error_quark ())
    return CURLE_READ_ERROR;      /* a file:// address */
  switch (err->code)
    {
    case RISCOS_HTTP_ERROR_TIMEOUT: return CURLE_OPERATION_TIMEDOUT;
    case RISCOS_HTTP_ERROR_CONNECT: return CURLE_COULDNT_CONNECT;
    case RISCOS_HTTP_ERROR_TLS: return CURLE_SSL_CONNECT_ERROR;
    case RISCOS_HTTP_ERROR_NO_REPLY: return CURLE_GOT_NOTHING;
    case RISCOS_HTTP_ERROR_URL: return CURLE_UNSUPPORTED_PROTOCOL;
    case RISCOS_HTTP_ERROR_SEND: return CURLE_SEND_ERROR;
    default: return CURLE_RECV_ERROR;
    }
}

CURLcode
curl_easy_perform (CURL *curl)
{
  Easy *e = curl;
  RiscosHttpRequest rq;
  GPtrArray *hdrs = g_ptr_array_new ();
  GByteArray *upload = NULL, *body = g_byte_array_new ();
  GError *err = NULL;
  char *url, *user = NULL, *pass = NULL;
  struct curl_slist *s;
  CURLcode r = CURLE_OK;
  int status;

  if (!e || !e->url)
    return CURLE_URL_MALFORMAT;
  reply_free (e);
  e->reply_headers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  /* webcal:// is http:// */
  if (!g_ascii_strncasecmp (e->url, "webcal://", 9))
    url = g_strconcat ("http://", e->url + 9, NULL);
  else if (!g_ascii_strncasecmp (e->url, "webcals://", 10))
    url = g_strconcat ("https://", e->url + 10, NULL);
  else
    url = g_strdup (e->url);

  memset (&rq, 0, sizeof rq);
  rq.url = url;
  rq.user_agent = e->user_agent;
  rq.timeout = e->timeout;
  rq.max_redirects = e->follow ? (e->maxredirs >= 0 ? e->maxredirs : 30) : -1;
  for (s = e->headers; s; s = s->next)
    if (s->data && strchr (s->data, ':'))
      g_ptr_array_add (hdrs, s->data);
  g_ptr_array_add (hdrs, NULL);
  rq.headers = (const char *const *) hdrs->pdata;
  if (e->username)
    {
      user = g_strdup (e->username);
      pass = g_strdup (e->password ? e->password : "");
    }
  else if (e->userpwd)
    {
      char *c = strchr (e->userpwd, ':');
      user = c ? g_strndup (e->userpwd, c - e->userpwd) : g_strdup (e->userpwd);
      pass = g_strdup (c ? c + 1 : "");
    }
  rq.user = user;
  rq.password = pass;

  if (e->upload)
    {
      /* PUT, the body from the read function (or fread from READDATA) */
      char buf[8192];
      size_t n;

      upload = g_byte_array_new ();
      for (;;)
        {
          if (e->infilesize >= 0 && (curl_off_t) upload->len >= e->infilesize)
            break;
          if (e->read_fn)
            n = e->read_fn (buf, 1, sizeof buf, e->read_data);
          else
            n = e->read_data ? fread (buf, 1, sizeof buf, (FILE *) e->read_data) : 0;
          if (n == 0 || n > sizeof buf)
            break;
          g_byte_array_append (upload, (guint8 *) buf, n);
        }
      if (e->infilesize >= 0 && (curl_off_t) upload->len > e->infilesize)
        g_byte_array_set_size (upload, e->infilesize);
      rq.method = "PUT";
      rq.body = upload->data;
      rq.body_len = upload->len;
    }

  status = riscos_http_request (&rq, e->reply_headers, body, &e->effective_url, &err);
  if (status < 0)
    r = code_for (err);
  else
    {
      e->response_code = !g_ascii_strncasecmp (url, "file:", 5) ? 0 : status;
      if (e->failonerror && status >= 400)
        r = CURLE_HTTP_RETURNED_ERROR;
      else if (body->len)
        {
          /* to the write function (or fwrite to WRITEDATA, or stdout) */
          size_t done;
          if (e->write_fn)
            done = e->write_fn ((char *) body->data, 1, body->len, e->write_data);
          else
            done = fwrite (body->data, 1, body->len, e->write_data ? (FILE *) e->write_data : stdout);
          if (done != body->len)
            r = CURLE_WRITE_ERROR;
        }
    }
  if (err)
    g_error_free (err);
  g_byte_array_free (body, TRUE);
  if (upload)
    g_byte_array_free (upload, TRUE);
  g_ptr_array_free (hdrs, TRUE);
  g_free (user);
  g_free (pass);
  g_free (url);
  return r;
}

CURLcode
curl_easy_getinfo (CURL *curl, CURLINFO info, ...)
{
  Easy *e = curl;
  va_list ap;
  CURLcode r = CURLE_OK;

  if (!e)
    return CURLE_BAD_FUNCTION_ARGUMENT;
  va_start (ap, info);
  switch (info)
    {
    case CURLINFO_RESPONSE_CODE:
      *va_arg (ap, long *) = e->response_code;
      break;
    case CURLINFO_EFFECTIVE_URL:
      *va_arg (ap, const char **) = e->effective_url ? e->effective_url : e->url;
      break;
    default:
      r = CURLE_UNKNOWN_OPTION;
    }
  va_end (ap);
  return r;
}

CURLHcode
curl_easy_header (CURL *curl, const char *name, size_t index, unsigned int origin,
                  int request, struct curl_header **hout)
{
  Easy *e = curl;
  char *lower;
  const char *v;

  if (!e || !name || !hout)
    return CURLHE_BAD_ARGUMENT;
  if (!e->reply_headers)
    return CURLHE_NOREQUEST;
  if (!(origin & CURLH_HEADER))
    return CURLHE_MISSING;
  lower = g_ascii_strdown (name, -1);
  v = g_hash_table_lookup (e->reply_headers, lower);
  g_free (lower);
  if (!v)
    return CURLHE_MISSING;
  /* one value a name (the last one sent) */
  if (index > 0)
    return CURLHE_BADINDEX;
  e->header.name = (char *) name;
  e->header.value = (char *) v;
  e->header.amount = 1;
  e->header.index = 0;
  e->header.origin = CURLH_HEADER;
  e->header.anchor = NULL;
  *hout = &e->header;
  return CURLHE_OK;
}

const char *
curl_easy_strerror (CURLcode code)
{
  switch (code)
    {
    case CURLE_OK: return "No error";
    case CURLE_UNSUPPORTED_PROTOCOL: return "Unsupported protocol";
    case CURLE_FAILED_INIT: return "Failed initialization";
    case CURLE_URL_MALFORMAT: return "URL using bad/illegal format or missing URL";
    case CURLE_COULDNT_RESOLVE_HOST: return "Couldn't resolve host name";
    case CURLE_COULDNT_CONNECT: return "Couldn't connect to server";
    case CURLE_HTTP_RETURNED_ERROR: return "HTTP response code said error";
    case CURLE_WRITE_ERROR: return "Failed writing received data to disk/application";
    case CURLE_READ_ERROR: return "Failed to open/read local data from file/application";
    case CURLE_OUT_OF_MEMORY: return "Out of memory";
    case CURLE_OPERATION_TIMEDOUT: return "Timeout was reached";
    case CURLE_SSL_CONNECT_ERROR: return "SSL connect error";
    case CURLE_BAD_FUNCTION_ARGUMENT: return "A libcurl function was given a bad argument";
    case CURLE_TOO_MANY_REDIRECTS: return "Number of redirects hit maximum amount";
    case CURLE_UNKNOWN_OPTION: return "An unknown option was passed in to libcurl";
    case CURLE_GOT_NOTHING: return "Server returned nothing (no headers, no data)";
    case CURLE_SEND_ERROR: return "Failed sending data to the peer";
    case CURLE_RECV_ERROR: return "Failure when receiving data from the peer";
    default: return "Unknown error";
    }
}

struct curl_slist *
curl_slist_append (struct curl_slist *list, const char *data)
{
  struct curl_slist *n = g_new0 (struct curl_slist, 1), *l;

  n->data = g_strdup (data);
  if (!list)
    return n;
  for (l = list; l->next; l = l->next)
    ;
  l->next = n;
  return list;
}

void
curl_slist_free_all (struct curl_slist *list)
{
  while (list)
    {
      struct curl_slist *n = list->next;
      g_free (list->data);
      g_free (list);
      list = n;
    }
}
