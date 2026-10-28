/*
 * curl/curl.h - riscos-gtk's libcurl subset (riscos-curl.c): what Claws
 * Mail's RSS (rssyl, libfeed) and vCalendar plug-ins call, with the
 * requests made by apps/claws/net/riscos-http.c (sockets, and AcornSSL for
 * https). Not libcurl: the same names and meanings for this subset only.
 *   - easy handles: init, setopt, perform, getinfo (the response code,
 *     the address fetched), header (a reply's header), cleanup, strerror,
 *     reset;
 *   - http, https, file and webcal (as http) addresses; GET, or PUT
 *     with CURLOPT_UPLOAD; Basic authentication; extra headers;
 *     redirects; a timeout;
 *   - accepted and not acted on: progress, signals, compression
 *     (none is asked for), netrc, cookies, the certificate options
 *     (AcornSSL checks certificates against its own store).
 *
 * GPL version 3 or later, as Claws Mail. Copyright (C) 2026 Andrew Youll.
 */
#ifndef RISCOS_CURL_H
#define RISCOS_CURL_H

#include <stddef.h>
#include <stdio.h>
#include "curlver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void CURL;
typedef long long curl_off_t;

typedef enum
{
  CURLE_OK = 0,
  CURLE_UNSUPPORTED_PROTOCOL = 1,
  CURLE_FAILED_INIT = 2,
  CURLE_URL_MALFORMAT = 3,
  CURLE_COULDNT_RESOLVE_HOST = 6,
  CURLE_COULDNT_CONNECT = 7,
  CURLE_HTTP_RETURNED_ERROR = 22,
  CURLE_WRITE_ERROR = 23,
  CURLE_READ_ERROR = 26,
  CURLE_OUT_OF_MEMORY = 27,
  CURLE_OPERATION_TIMEDOUT = 28,
  CURLE_BAD_FUNCTION_ARGUMENT = 43,
  CURLE_TOO_MANY_REDIRECTS = 47,
  CURLE_UNKNOWN_OPTION = 48,
  CURLE_SSL_CONNECT_ERROR = 35,
  CURLE_GOT_NOTHING = 52,
  CURLE_SEND_ERROR = 55,
  CURLE_RECV_ERROR = 56
} CURLcode;
#define CURLE_OPERATION_TIMEOUTED CURLE_OPERATION_TIMEDOUT

/* option numbers as libcurl's: the type is in the thousands */
#define CURLOPTTYPE_LONG          0
#define CURLOPTTYPE_OBJECTPOINT   10000
#define CURLOPTTYPE_FUNCTIONPOINT 20000
#define CURLOPTTYPE_OFF_T         30000

typedef enum
{
  CURLOPT_WRITEDATA = CURLOPTTYPE_OBJECTPOINT + 1,
  CURLOPT_URL = CURLOPTTYPE_OBJECTPOINT + 2,
  CURLOPT_USERPWD = CURLOPTTYPE_OBJECTPOINT + 5,
  CURLOPT_READDATA = CURLOPTTYPE_OBJECTPOINT + 9,
  CURLOPT_WRITEFUNCTION = CURLOPTTYPE_FUNCTIONPOINT + 11,
  CURLOPT_READFUNCTION = CURLOPTTYPE_FUNCTIONPOINT + 12,
  CURLOPT_TIMEOUT = CURLOPTTYPE_LONG + 13,
  CURLOPT_INFILESIZE = CURLOPTTYPE_LONG + 14,
  CURLOPT_USERAGENT = CURLOPTTYPE_OBJECTPOINT + 18,
  CURLOPT_HTTPHEADER = CURLOPTTYPE_OBJECTPOINT + 23,
  CURLOPT_COOKIEFILE = CURLOPTTYPE_OBJECTPOINT + 31,
  CURLOPT_NOPROGRESS = CURLOPTTYPE_LONG + 43,
  CURLOPT_FAILONERROR = CURLOPTTYPE_LONG + 45,
  CURLOPT_UPLOAD = CURLOPTTYPE_LONG + 46,
  CURLOPT_NETRC = CURLOPTTYPE_LONG + 51,
  CURLOPT_FOLLOWLOCATION = CURLOPTTYPE_LONG + 52,
  CURLOPT_SSL_VERIFYPEER = CURLOPTTYPE_LONG + 64,
  CURLOPT_CAINFO = CURLOPTTYPE_OBJECTPOINT + 65,
  CURLOPT_MAXREDIRS = CURLOPTTYPE_LONG + 68,
  CURLOPT_SSL_VERIFYHOST = CURLOPTTYPE_LONG + 81,
  CURLOPT_NOSIGNAL = CURLOPTTYPE_LONG + 99,
  CURLOPT_ENCODING = CURLOPTTYPE_OBJECTPOINT + 102,
  CURLOPT_HTTPAUTH = CURLOPTTYPE_LONG + 107,
  CURLOPT_USERNAME = CURLOPTTYPE_OBJECTPOINT + 173,
  CURLOPT_PASSWORD = CURLOPTTYPE_OBJECTPOINT + 174,
  CURLOPT_INFILESIZE_LARGE = CURLOPTTYPE_OFF_T + 115
} CURLoption;
#define CURLOPT_ACCEPT_ENCODING CURLOPT_ENCODING
#define CURLOPT_FILE CURLOPT_WRITEDATA
#define CURLOPT_INFILE CURLOPT_READDATA

#define CURLAUTH_BASIC 1UL
#define CURLAUTH_ANY (~0UL)

enum { CURL_NETRC_IGNORED, CURL_NETRC_OPTIONAL, CURL_NETRC_REQUIRED };

#define CURLINFO_STRING 0x100000
#define CURLINFO_LONG   0x200000
typedef enum
{
  CURLINFO_EFFECTIVE_URL = CURLINFO_STRING + 1,
  CURLINFO_RESPONSE_CODE = CURLINFO_LONG + 2
} CURLINFO;
#define CURLINFO_HTTP_CODE CURLINFO_RESPONSE_CODE

#define CURL_GLOBAL_SSL 1L
#define CURL_GLOBAL_WIN32 2L
#define CURL_GLOBAL_ALL (CURL_GLOBAL_SSL | CURL_GLOBAL_WIN32)
#define CURL_GLOBAL_DEFAULT CURL_GLOBAL_ALL

typedef size_t (*curl_write_callback) (char *buffer, size_t size, size_t nitems, void *outstream);
typedef size_t (*curl_read_callback) (char *buffer, size_t size, size_t nitems, void *instream);

struct curl_slist
{
  char *data;
  struct curl_slist *next;
};

/* curl_easy_header (7.83) */
struct curl_header
{
  char *name;
  char *value;
  size_t amount;
  size_t index;
  unsigned int origin;
  void *anchor;
};
#define CURLH_HEADER  (1 << 0)
#define CURLH_TRAILER (1 << 1)
#define CURLH_CONNECT (1 << 2)
#define CURLH_1XX     (1 << 3)
#define CURLH_PSEUDO  (1 << 4)
typedef enum
{
  CURLHE_OK,
  CURLHE_BADINDEX,
  CURLHE_MISSING,
  CURLHE_NOHEADERS,
  CURLHE_NOREQUEST,
  CURLHE_OUT_OF_MEMORY,
  CURLHE_BAD_ARGUMENT,
  CURLHE_NOT_BUILT_IN
} CURLHcode;

CURLcode curl_global_init (long flags);
void curl_global_cleanup (void);
CURL *curl_easy_init (void);
CURLcode curl_easy_setopt (CURL *curl, CURLoption option, ...);
CURLcode curl_easy_perform (CURL *curl);
CURLcode curl_easy_getinfo (CURL *curl, CURLINFO info, ...);
CURLHcode curl_easy_header (CURL *curl, const char *name, size_t index, unsigned int origin,
                            int request, struct curl_header **hout);
void curl_easy_reset (CURL *curl);
void curl_easy_cleanup (CURL *curl);
const char *curl_easy_strerror (CURLcode code);
struct curl_slist *curl_slist_append (struct curl_slist *list, const char *data);
void curl_slist_free_all (struct curl_slist *list);
char *curl_version (void);

#ifdef __cplusplus
}
#endif

#endif
