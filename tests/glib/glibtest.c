/*
 * glibtest.c - M1 test for riscos-gtk's GLib 2.80 build.
 *
 * The same source is built twice:
 *   - for RISC OS (arm-riscos-gnueabihf, UnixLib), to run on the Pi from
 *     the command line or a TaskWindow;
 *   - for arm-linux (-mno-unaligned-access), to run under riscos-crossdev's
 *     qemu-arm-aligntrap, which faults on unaligned accesses as RISC OS does.
 *
 * Prints TAP ("ok N - name" / "not ok N - name"), then a summary line.
 * Exit status: the number of failed checks (0 = pass).
 *
 * What it covers (the M1 pass list in gtk-START-HERE.md, plus the parts of
 * GLib that GTK leans on and that differ per platform):
 *   main loop: timeouts, idle, nested loop, g_main_context_invoke from a
 *     thread, GWakeup (eventfd on UnixLib)
 *   GThreadPool: tasks on worker threads, results back on the main loop
 *   GIO: g_file_set_contents/load_contents, an async read (GTask runs it
 *     on GIO's worker pool), directory enumeration, query_info
 *   GObject: a type with a property and a signal using the generic
 *     marshaller (libffi's ARM ffi_call)
 *   GVariant round trip (alignment-sensitive), GRegex (PCRE2),
 *   g_convert (UnixLib iconv), UTF-8, GKeyFile, GDateTime, g_strdup_printf
 *   with doubles (VFP hard-float varargs)
 *
 * MIT licence (see LICENSE).
 */
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

static int n_tests, n_failed;

static void
check (gboolean ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
  fflush (stdout);
}

/* ---- main loop: timeouts and idle ---------------------------------- */

typedef struct {
  GMainLoop *loop;
  int timeout_ticks;
  int idle_runs;
  gint64 started;
  gint64 last_tick;
  gboolean order_ok;
} LoopState;

static gboolean
on_timeout (gpointer data)
{
  LoopState *s = data;
  s->last_tick = g_get_monotonic_time ();
  /* The idle source (default priority idle) must have run by the first
     timeout tick, because it is dispatched as soon as the loop is idle. */
  if (s->timeout_ticks == 0 && s->idle_runs == 0)
    s->order_ok = FALSE;
  if (++s->timeout_ticks == 3)
    {
      g_main_loop_quit (s->loop);
      return G_SOURCE_REMOVE;
    }
  return G_SOURCE_CONTINUE;
}

static gboolean
on_idle (gpointer data)
{
  LoopState *s = data;
  s->idle_runs++;
  return s->idle_runs < 5 ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

static gboolean
quit_soon (gpointer loop)
{
  g_main_loop_quit (loop);
  return G_SOURCE_REMOVE;
}

static void
test_main_loop (void)
{
  LoopState s = { 0 };
  s.loop = g_main_loop_new (NULL, FALSE);
  s.order_ok = TRUE;
  s.started = g_get_monotonic_time ();
  g_timeout_add (50, on_timeout, &s);
  g_idle_add (on_idle, &s);
  g_timeout_add_seconds (10, quit_soon, s.loop);   /* safety net */
  g_main_loop_run (s.loop);
  gint64 elapsed = (s.last_tick - s.started) / 1000;
  check (s.timeout_ticks == 3, "main loop: 3 timeout ticks");
  check (s.idle_runs == 5, "main loop: idle ran 5 times");
  check (s.order_ok, "main loop: idle dispatched before the first timeout");
  /* 3 x 50 ms; RISC OS's monotonic clock has centisecond resolution
     without UnixLib's high resolution clock, so allow some slack. */
  check (elapsed >= 140 && elapsed < 1000, "main loop: timeouts take ~150 ms");
  printf ("# 3 x 50 ms timeouts took %d ms\n", (int) elapsed);
  g_main_loop_unref (s.loop);
}

/* nested loop inside a dispatch, as gtk_dialog_run does */
static gboolean
nested_inner_quit (gpointer loop)
{
  g_main_loop_quit (loop);
  return G_SOURCE_REMOVE;
}

static gboolean
nested_outer (gpointer data)
{
  GMainLoop *outer = data;
  GMainLoop *inner = g_main_loop_new (NULL, FALSE);
  g_timeout_add (20, nested_inner_quit, inner);
  g_main_loop_run (inner);
  g_main_loop_unref (inner);
  g_main_loop_quit (outer);
  return G_SOURCE_REMOVE;
}

static void
test_nested_loop (void)
{
  GMainLoop *outer = g_main_loop_new (NULL, FALSE);
  guint safety = g_timeout_add_seconds (10, quit_soon, outer);
  g_idle_add (nested_outer, outer);
  gint64 t0 = g_get_monotonic_time ();
  g_main_loop_run (outer);
  g_source_remove (safety);
  check ((g_get_monotonic_time () - t0) < 5 * G_USEC_PER_SEC, "main loop: nested loop returns");
  g_main_loop_unref (outer);
}

/* ---- threads: GThreadPool and g_main_context_invoke ------------------ */

#define N_JOBS 8

typedef struct {
  GMainLoop *loop;
  gint done;            /* touched only on the main thread */
  guint64 results[N_JOBS];
  gboolean on_main[N_JOBS];
  GThread *main_thread;
} PoolState;

typedef struct {
  PoolState *s;
  int index;
  guint64 sum;
} Job;

static gboolean
job_done_on_main (gpointer data)
{
  Job *j = data;
  j->s->results[j->index] = j->sum;
  j->s->on_main[j->index] = (g_thread_self () == j->s->main_thread);
  if (++j->s->done == N_JOBS)
    g_main_loop_quit (j->s->loop);
  g_free (j);
  return G_SOURCE_REMOVE;
}

static void
pool_worker (gpointer data, gpointer user_data)
{
  Job *j = data;
  (void) user_data;
  guint64 sum = 0;
  /* Enough work that the ticker switches threads part way through. */
  for (guint64 i = 1; i <= 200000 + (guint64) j->index; i++)
    sum += i;
  j->sum = sum;
  g_main_context_invoke (NULL, job_done_on_main, j);
}

static void
test_thread_pool (void)
{
  PoolState s = { 0 };
  GError *err = NULL;
  s.loop = g_main_loop_new (NULL, FALSE);
  s.main_thread = g_thread_self ();
  /* Own the main context while the jobs run, so that
     g_main_context_invoke() from a worker queues the call for this thread
     instead of running it on the worker (what it does for an unowned
     context, e.g. before g_main_loop_run() has started). */
  g_main_context_acquire (NULL);
  GThreadPool *pool = g_thread_pool_new (pool_worker, NULL, 3, FALSE, &err);
  check (pool != NULL, "thread pool: created with 3 threads");
  if (!pool)
    {
      printf ("# %s\n", err ? err->message : "?");
      return;
    }
  for (int i = 0; i < N_JOBS; i++)
    {
      Job *j = g_new0 (Job, 1);
      j->s = &s;
      j->index = i;
      g_thread_pool_push (pool, j, NULL);
    }
  guint safety = g_timeout_add_seconds (30, quit_soon, s.loop);
  g_main_loop_run (s.loop);
  g_source_remove (safety);
  g_thread_pool_free (pool, FALSE, TRUE);
  g_main_context_release (NULL);

  gboolean sums_ok = TRUE, main_ok = TRUE;
  for (int i = 0; i < N_JOBS; i++)
    {
      guint64 n = 200000 + (guint64) i;
      if (s.results[i] != n * (n + 1) / 2)
        sums_ok = FALSE;
      if (!s.on_main[i])
        main_ok = FALSE;
    }
  check (s.done == N_JOBS, "thread pool: all 8 jobs reported back");
  check (sums_ok, "thread pool: results correct");
  check (main_ok, "thread pool: results delivered on the main thread");
  g_main_loop_unref (s.loop);
}

/* a plain GThread with a GMutex/GCond hand-off */
typedef struct {
  GMutex lock;
  GCond cond;
  int stage;
} Handoff;

static gpointer
handoff_thread (gpointer data)
{
  Handoff *h = data;
  g_mutex_lock (&h->lock);
  while (h->stage != 1)
    g_cond_wait (&h->cond, &h->lock);
  h->stage = 2;
  g_cond_signal (&h->cond);
  g_mutex_unlock (&h->lock);
  return GINT_TO_POINTER (42);
}

static void
test_thread_cond (void)
{
  Handoff h;
  g_mutex_init (&h.lock);
  g_cond_init (&h.cond);
  h.stage = 0;
  GThread *t = g_thread_new ("handoff", handoff_thread, &h);
  g_mutex_lock (&h.lock);
  h.stage = 1;
  g_cond_signal (&h.cond);
  gint64 deadline = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;
  gboolean timed_out = FALSE;
  while (h.stage != 2 && !timed_out)
    timed_out = !g_cond_wait_until (&h.cond, &h.lock, deadline);
  g_mutex_unlock (&h.lock);
  check (h.stage == 2, "threads: GCond hand-off");
  check (g_thread_join (t) == GINT_TO_POINTER (42), "threads: join returns the result");
  g_mutex_clear (&h.lock);
  g_cond_clear (&h.cond);
}

/* ---- GIO ------------------------------------------------------------ */

typedef struct {
  GMainLoop *loop;
  char *contents;
  gsize length;
  GError *error;
} AsyncRead;

static void
on_load_done (GObject *source, GAsyncResult *res, gpointer data)
{
  AsyncRead *a = data;
  g_file_load_contents_finish (G_FILE (source), res, &a->contents, &a->length,
                               NULL, &a->error);
  g_main_loop_quit (a->loop);
}

static void
test_gio (const char *dir)
{
  GError *err = NULL;
  char *path = g_build_filename (dir, "glibtest.txt", NULL);
  const char *text = "GTK on RISC OS\nline two\n";

  check (g_file_set_contents (path, text, -1, &err), "gio: g_file_set_contents");
  if (err)
    {
      printf ("# %s\n", err->message);
      g_clear_error (&err);
    }

  GFile *f = g_file_new_for_path (path);
  char *got = NULL;
  gsize len = 0;
  gboolean ok = g_file_load_contents (f, NULL, &got, &len, NULL, &err);
  check (ok && len == strlen (text) && memcmp (got, text, len) == 0,
         "gio: g_file_load_contents (sync)");
  g_free (got);
  g_clear_error (&err);

  /* async: GTask runs the read on GIO's worker thread pool */
  AsyncRead a = { 0 };
  a.loop = g_main_loop_new (NULL, FALSE);
  g_file_load_contents_async (f, NULL, on_load_done, &a);
  guint safety = g_timeout_add_seconds (30, quit_soon, a.loop);
  g_main_loop_run (a.loop);
  g_source_remove (safety);
  check (a.contents && a.length == strlen (text) && memcmp (a.contents, text, a.length) == 0,
         "gio: g_file_load_contents_async (worker thread)");
  if (a.error)
    printf ("# %s\n", a.error->message);
  g_free (a.contents);
  g_clear_error (&a.error);
  g_main_loop_unref (a.loop);

  GFileInfo *info = g_file_query_info (f, G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                       G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                       G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                       G_FILE_QUERY_INFO_NONE, NULL, &err);
  check (info && g_file_info_get_size (info) == (goffset) strlen (text)
         && g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR,
         "gio: query_info size and type");
  g_clear_object (&info);
  g_clear_error (&err);

  /* enumerate the directory and find the file */
  GFile *d = g_file_new_for_path (dir);
  GFileEnumerator *e = g_file_enumerate_children (d, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                  G_FILE_QUERY_INFO_NONE, NULL, &err);
  gboolean found = FALSE;
  if (e)
    {
      GFileInfo *fi;
      while ((fi = g_file_enumerator_next_file (e, NULL, NULL)) != NULL)
        {
          if (g_ascii_strcasecmp (g_file_info_get_name (fi), "glibtest.txt") == 0)
            found = TRUE;
          g_object_unref (fi);
        }
      g_object_unref (e);
    }
  check (found, "gio: enumerate_children finds the file");
  if (err)
    printf ("# %s\n", err->message);
  g_clear_error (&err);

  check (g_file_delete (f, NULL, &err), "gio: delete");
  g_clear_error (&err);
  g_object_unref (d);
  g_object_unref (f);
  g_free (path);
}

/* ---- GObject: a property and a signal via the generic marshaller ---- */

#define TEST_TYPE_THING (test_thing_get_type ())
G_DECLARE_FINAL_TYPE (TestThing, test_thing, TEST, THING, GObject)

struct _TestThing {
  GObject parent;
  int size;
};

G_DEFINE_TYPE (TestThing, test_thing, G_TYPE_OBJECT)

enum { PROP_0, PROP_SIZE };
enum { SIG_RESIZED, N_SIGNALS };
static guint thing_signals[N_SIGNALS];

static void
test_thing_set_property (GObject *obj, guint id, const GValue *v, GParamSpec *p)
{
  TestThing *t = TEST_THING (obj);
  if (id == PROP_SIZE)
    {
      int old = t->size;
      t->size = g_value_get_int (v);
      g_signal_emit (t, thing_signals[SIG_RESIZED], 0, old, (double) t->size / 2.0, "resized");
    }
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, id, p);
}

static void
test_thing_get_property (GObject *obj, guint id, GValue *v, GParamSpec *p)
{
  if (id == PROP_SIZE)
    g_value_set_int (v, TEST_THING (obj)->size);
  else
    G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, id, p);
}

static void
test_thing_class_init (TestThingClass *klass)
{
  GObjectClass *oc = G_OBJECT_CLASS (klass);
  oc->set_property = test_thing_set_property;
  oc->get_property = test_thing_get_property;
  g_object_class_install_property (oc, PROP_SIZE,
      g_param_spec_int ("size", NULL, NULL, 0, 1000, 10, G_PARAM_READWRITE));
  /* NULL marshaller: GLib uses g_cclosure_marshal_generic, i.e. libffi. */
  thing_signals[SIG_RESIZED] =
    g_signal_new ("resized", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 3,
                  G_TYPE_INT, G_TYPE_DOUBLE, G_TYPE_STRING);
}

static void
test_thing_init (TestThing *t)
{
  t->size = 10;
}

typedef struct { int old; double half; char *word; int calls; } SigSeen;

static void
on_resized (TestThing *t, int old, double half, const char *word, gpointer data)
{
  SigSeen *s = data;
  (void) t;
  s->old = old;
  s->half = half;
  g_free (s->word);
  s->word = g_strdup (word);
  s->calls++;
}

static void
test_gobject (void)
{
  SigSeen seen = { 0 };
  TestThing *t = g_object_new (TEST_TYPE_THING, NULL);
  g_signal_connect (t, "resized", G_CALLBACK (on_resized), &seen);
  g_object_set (t, "size", 33, NULL);
  int size = 0;
  g_object_get (t, "size", &size, NULL);
  check (size == 33, "gobject: property set and get");
  check (seen.calls == 1 && seen.old == 10 && seen.half == 16.5
         && g_strcmp0 (seen.word, "resized") == 0,
         "gobject: signal through the generic (libffi) marshaller");
  g_free (seen.word);
  g_object_unref (t);
}

/* ---- the rest -------------------------------------------------------- */

static void
test_misc (void)
{
  /* GVariant: a nested value with mixed alignment, serialised and back */
  GVariant *v = g_variant_new ("(ybxsa{sv}d)", 7, TRUE, (gint64) -1234567890123,
                               "riscos", NULL, 2.5);
  v = g_variant_ref_sink (v);
  GBytes *bytes = g_variant_get_data_as_bytes (v);
  /* copy to an odd address on purpose, then let GLib re-align it */
  gsize n = g_bytes_get_size (bytes);
  guchar *odd = g_malloc (n + 1);
  memcpy (odd + 1, g_bytes_get_data (bytes, NULL), n);
  GBytes *b2 = g_bytes_new (odd + 1, n);
  GVariant *w = g_variant_new_from_bytes (G_VARIANT_TYPE ("(ybxsa{sv}d)"), b2, FALSE);
  guchar y; gboolean bo; gint64 x; const char *s; double d;
  g_variant_get (w, "(ybx&sa{sv}d)", &y, &bo, &x, &s, NULL, &d);
  check (y == 7 && bo && x == -1234567890123 && strcmp (s, "riscos") == 0 && d == 2.5,
         "gvariant: round trip from an unaligned buffer");
  g_variant_unref (w);
  g_bytes_unref (b2);
  g_free (odd);
  g_bytes_unref (bytes);
  g_variant_unref (v);

  /* GRegex (PCRE2) */
  GRegex *re = g_regex_new ("^(\\w+)\\s+(\\d+)$", 0, 0, NULL);
  GMatchInfo *mi = NULL;
  gboolean m = re && g_regex_match (re, "Acorn 1987", 0, &mi);
  char *w1 = m ? g_match_info_fetch (mi, 1) : NULL;
  check (m && g_strcmp0 (w1, "Acorn") == 0, "gregex: match and capture");
  g_free (w1);
  g_match_info_free (mi);
  if (re)
    g_regex_unref (re);

  /* g_convert (iconv): Latin-1 is RISC OS's default alphabet. On RISC OS,
     UnixLib's iconv uses the Iconv module. The qemu run sets
     GLIBTEST_SKIP_ICONV: the arm-linux cross sysroot has no gconv modules. */
  GError *err = NULL;
  gsize wr = 0;
  if (g_getenv ("GLIBTEST_SKIP_ICONV"))
    {
      n_tests++;
      printf ("ok %d - g_convert: ISO-8859-1 to UTF-8 (iconv) # SKIP no gconv modules\n", n_tests);
    }
  else
    {
      char *u = g_convert ("caf\xe9", 4, "UTF-8", "ISO-8859-1", NULL, &wr, &err);
      check (u && strcmp (u, "caf\xc3\xa9") == 0, "g_convert: ISO-8859-1 to UTF-8 (iconv)");
      if (err)
        printf ("# %s\n", err->message);
      g_clear_error (&err);
      g_free (u);
    }

  /* UTF-8 */
  const char *utf = "\xc3\x84pfel \xe2\x82\xac";   /* "Äpfel €" */
  char *up = g_utf8_strup (utf, -1);
  check (g_utf8_validate (utf, -1, NULL) && g_utf8_strlen (utf, -1) == 7
         && strcmp (up, "\xc3\x84PFEL \xe2\x82\xac") == 0, "utf8: validate, strlen, strup");
  g_free (up);

  /* GKeyFile (settings.ini style) */
  GKeyFile *kf = g_key_file_new ();
  const char *ini = "[Settings]\ngtk-theme-name=RISCOS\ngtk-font-name=DejaVu Sans 10\n";
  gboolean kok = g_key_file_load_from_data (kf, ini, -1, 0, NULL);
  char *theme = kok ? g_key_file_get_string (kf, "Settings", "gtk-theme-name", NULL) : NULL;
  check (g_strcmp0 (theme, "RISCOS") == 0, "gkeyfile: parse settings.ini");
  g_free (theme);
  g_key_file_free (kf);

  /* GDateTime in UTC (no time zone database needed) */
  GDateTime *dt = g_date_time_new_utc (2026, 10, 3, 12, 30, 0);
  char *iso = dt ? g_date_time_format_iso8601 (dt) : NULL;
  check (g_strcmp0 (iso, "2026-10-03T12:30:00Z") == 0, "gdatetime: UTC ISO 8601");
  g_free (iso);
  if (dt)
    g_date_time_unref (dt);
  GDateTime *now = g_date_time_new_now_local ();
  check (now != NULL && g_date_time_get_year (now) >= 2024, "gdatetime: now (local)");
  if (now)
    g_date_time_unref (now);

  /* doubles through varargs (hard-float ABI) and GLib's printf */
  char *pf = g_strdup_printf ("%.3f|%g|%s|%d", 3.14159, 1e-5, "x", -42);
  check (strcmp (pf, "3.142|1e-05|x|-42") == 0, "g_strdup_printf: doubles and ints");
  g_free (pf);
  char buf[G_ASCII_DTOSTR_BUF_SIZE];
  check (g_ascii_strtod ("2.75", NULL) == 2.75
         && strcmp (g_ascii_dtostr (buf, sizeof buf, 0.1), "0.10000000000000001") == 0,
         "g_ascii_strtod / dtostr");

  /* environment and paths GTK will ask for */
  check (g_get_home_dir () != NULL && g_get_tmp_dir () != NULL
         && g_get_user_config_dir () != NULL, "paths: home, tmp, config dirs");
  printf ("# home=%s tmp=%s config=%s\n", g_get_home_dir (), g_get_tmp_dir (),
          g_get_user_config_dir ());

  /* Diagnostics for the file chooser work (M4): RISC OS filenames are
     usually Latin-1, GLib assumes UTF-8 unless told otherwise. */
  const char *charset = NULL;
  const gchar **fncs = NULL;
  g_get_charset (&charset);
  g_get_filename_charsets (&fncs);
  printf ("# locale charset=%s filename charset=%s\n", charset ? charset : "?",
          fncs && fncs[0] ? fncs[0] : "?");
}

int
main (int argc, char **argv)
{
  const char *dir = argc > 1 ? argv[1] : g_get_tmp_dir ();
  printf ("# glibtest: GLib %d.%d.%d, scratch dir %s\n",
          glib_major_version, glib_minor_version, glib_micro_version, dir);
  test_main_loop ();
  test_nested_loop ();
  test_thread_cond ();
  test_thread_pool ();
  test_gio (dir);
  test_gobject ();
  test_misc ();
  printf ("1..%d\n# glibtest: %d checks, %d failed\n", n_tests, n_tests, n_failed);
  return n_failed;
}
