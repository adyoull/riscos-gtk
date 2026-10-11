/*
 * gdkspawn-riscos.c - riscos-gtk: programs started with g_spawn_async*()
 * run as RISC OS tasks, alongside the desktop.
 *
 * On RISC OS, UnixLib's exec() runs the new program in place of this one
 * until it ends: the desktop waits, and its output can't be read as it
 * comes. GLib's spawn functions (patches/glib-2.80.0-riscos.diff,
 * GRiscosSpawnHooks) come here instead once GDK is running:
 *
 *   - output wanted (stdout or stderr pipes, as Geany's Build commands):
 *     the command runs in a TaskWindow with this program as its parent
 *     ("*TaskWindow ... -task &<us> -txt &<handle> -quit"). Its output
 *     comes in TaskWindow_Output messages and is written to the pipe the
 *     program reads (stdout; TaskWindow gives stdout and stderr as one).
 *     The command is run from an Obey file in the scrap directory that
 *     first sets the directory asked for (*Dir) and ends by printing
 *     Sys$ReturnCode, read back as the exit status: an error stops an
 *     Obey file, so no status line means it failed (status 1).
 *     TaskWindow_Morio: the task has ended; the pipes are closed (end of
 *     file) and the child watch fires.
 *   - no output wanted: Wimp_StartTask runs the command as a task of its
 *     own; Message_TaskCloseDown from it ends it.
 *
 * Commands: argv is joined into one RISC OS command line; "sh -c CMD"
 * gives CMD. Words that are absolute Unix file names (an argument, or a
 * word in one: "Obey /tmp/x") are given as RISC OS names, as UnixLib
 * would open them (/tmp is the scrap directory). Standard input isn't passed on: the
 * pipe's data is dropped. The environment (envp) isn't passed on.
 *
 * gdk_riscos_spawn_kill() ends a task: TaskWindow_Morite, or Message_Quit
 * for a task of its own.
 *
 * Throwback: while a TaskWindow of ours runs, this task is registered
 * with the DDEUtils module (DDEUtils_ThrowbackRegister), so the DDE's
 * tools given -throwback (cc, objasm, link, amu...) send their errors here
 * as Wimp messages. Each is written to the newest running task's output
 * as a compiler line, "FILE:LINE: error: TEXT" (warning, fatal error,
 * note), with FILE as a Unix name (c.hello is hello.c, as UnixLib names
 * it), so a program that reads compiler messages (Geany's Compiler tab)
 * finds the file and line. Registration ends when the last one ends, so
 * an editor such as StrongED gets throwback again.
 *   Message_ThrowbackStart &42580, _ProcessingFile &42581 (+20 file name),
 *   _ErrorsIn &42582 (+20 file name), _ErrorDetails &42583 (+20 line,
 *   +24 severity: 0 warning, 1 error, 2 serious error, +28 text),
 *   _ThrowbackEnd &42584, _InfoForFile &42585 (+20 file name),
 *   _InfoDetails &42586 (+20 line, +24 -1, +28 text);
 *   DDEUtils_ThrowbackRegister &42585, _UnRegister &42586 (R0 = task).
 *
 * Messages as in the RISC OS PRMs (TaskWindow): TaskWindow_Output &808C1
 * (+20 size, +24 data; sent as a plain user message, not acknowledged),
 * TaskWindow_Ego &808C2 (sender = child task, +20 = our txt handle),
 * TaskWindow_Morio &808C3, TaskWindow_Morite &808C4; Message_TaskCloseDown
 * &400C3; Wimp_StartTask (R0 = command; returns the new task's handle,
 * 0 if it has ended already).
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as GTK.
 */
#include "config.h"

#include "gdkriscos-server.h"
#include "gdkriscoswimp.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <kernel.h>
#ifdef __riscos__
#include <unixlib/local.h>
#endif

#define Wimp_StartTask   0x400DE
#define Wimp_SendMessage 0x400E7

#define MESSAGE_QUIT             0
#define MESSAGE_TASKCLOSEDOWN    0x400C3
#define MESSAGE_TASKWINDOW_OUTPUT 0x808C1
#define MESSAGE_TASKWINDOW_EGO   0x808C2
#define MESSAGE_TASKWINDOW_MORIO 0x808C3
#define MESSAGE_TASKWINDOW_MORITE 0x808C4

/* the Obey file's last line prints this and Sys$ReturnCode */
#define EXIT_MARK "##riscos-gtk-exit "

#define DDEUtils_ThrowbackRegister   0x42585
#define DDEUtils_ThrowbackUnRegister 0x42586
#define MESSAGE_THROWBACK_FIRST   0x42580
#define MESSAGE_ERRORS_IN         0x42582
#define MESSAGE_ERROR_DETAILS     0x42583
#define MESSAGE_INFO_FOR_FILE     0x42585
#define MESSAGE_INFO_DETAILS      0x42586
#define MESSAGE_THROWBACK_LAST    0x42586

typedef struct
{
  GPid pid;                  /* also the txt handle given to *TaskWindow */
  int task;                  /* the child's task handle (0: not known yet) */
  gboolean captured;         /* in a TaskWindow, its output to us */
  gboolean reap;             /* nobody will ask how it ended: forget it then */
  int out_fd, err_fd, in_fd; /* our ends of its pipes, or -1 */
  GString *pending;          /* output held back: maybe the start of EXIT_MARK */
  gboolean marked;           /* EXIT_MARK seen: the rest is the status */
  GString *status_text;
  char *obey;                /* the Obey file (Unix name) */
  gboolean done;
  int status;                /* wait status: exit code << 8 */
} Task;

static GHashTable *tasks;    /* pid -> Task */
static int our_task, next_pid = 1;
static gboolean throwback_registered;
static char *throwback_file;      /* Unix name of the file errors are in */

static void
throwback_register (gboolean on)
{
  _kernel_swi_regs r;

  if (on == throwback_registered || g_strcmp0 (g_getenv ("RISCOS_GTK_THROWBACK"), "0") == 0)
    return;
  r.r[0] = our_task;
  if (_kernel_swi (on ? DDEUtils_ThrowbackRegister : DDEUtils_ThrowbackUnRegister, &r, &r) == NULL || !on)
    throwback_registered = on;   /* no DDEUtils: nothing to do */
}

static void
task_free (gpointer p)
{
  Task *t = p;

  if (t->out_fd >= 0)
    close (t->out_fd);
  if (t->err_fd >= 0)
    close (t->err_fd);
  if (t->in_fd >= 0)
    close (t->in_fd);
  if (t->obey)
    {
      g_unlink (t->obey);
      g_free (t->obey);
    }
  if (t->pending)
    g_string_free (t->pending, TRUE);
  if (t->status_text)
    g_string_free (t->status_text, TRUE);
  g_free (t);
}

static char *
riscos_name (const char *unix_name)
{
#ifdef __riscos__
  char buf[1024];
  if (__riscosify_std (unix_name, 0, buf, sizeof buf, NULL) != NULL)
    return g_strdup (buf);
#endif
  return gdk_riscos_path_from_unix (unix_name);
}

/* An absolute Unix file name (not "/" or an option) */
static gboolean
is_unix_name (const char *s, size_t len)
{
  return len > 1 && s[0] == '/' && s[1] != ' ';
}

/* CMD with each absolute Unix file name in it (a word, or in quotes) given
   as a RISC OS name */
static char *
convert_words (const char *cmd)
{
  GString *out = g_string_new (NULL);
  const char *p = cmd;

  while (*p)
    {
      gboolean start = p == cmd || p[-1] == ' ' || p[-1] == '"' || p[-1] == '\'';
      size_t len = strcspn (p, " \"'");

      if (start && is_unix_name (p, len))
        {
          char *word = g_strndup (p, len), *ro = riscos_name (word);
          g_string_append (out, ro);
          g_free (word);
          g_free (ro);
          p += len;
        }
      else if (len > 0)
        {
          g_string_append_len (out, p, len);
          p += len;
        }
      else
        g_string_append_c (out, *p++);
    }
  return g_string_free (out, FALSE);
}

static char *
command_line (const gchar * const *argv)
{
  GString *line;
  int i;
  const char *base;

  base = strrchr (argv[0], '/') ? strrchr (argv[0], '/') + 1 : argv[0];
  if ((strcmp (base, "sh") == 0 || strcmp (base, "bash") == 0) &&
      argv[1] && strcmp (argv[1], "-c") == 0 && argv[2])
    return convert_words (argv[2]);
  line = g_string_new (NULL);
  for (i = 0; argv[i]; i++)
    {
      char *arg = convert_words (argv[i]);
      if (i)
        g_string_append_c (line, ' ');
      if (*arg == '\0' || strchr (arg, ' '))
        g_string_append_printf (line, "\"%s\"", arg);
      else
        g_string_append (line, arg);
      g_free (arg);
    }
  return g_string_free (line, FALSE);
}

static int
start_task (const char *command, GError **error)
{
  _kernel_swi_regs r;
  _kernel_oserror *e;

  r.r[0] = (int) (long) command;
  if ((e = _kernel_swi (Wimp_StartTask, &r, &r)) != NULL)
    {
      g_set_error (error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "%s", e->errmess);
      return -1;
    }
  return r.r[0];
}

static gboolean
make_pipe (int *ours, gint *theirs, gboolean we_write)
{
  int p[2];

  if (pipe (p) != 0)
    return FALSE;
  *ours = we_write ? p[1] : p[0];
  *theirs = we_write ? p[0] : p[1];
  return TRUE;
}

static gboolean
spawn (const gchar *working_directory, const gchar * const *argv,
       const gchar * const *envp, GSpawnFlags flags, GPid *child_pid,
       gint *stdin_pipe, gint *stdout_pipe, gint *stderr_pipe, GError **error)
{
  Task *t = g_new0 (Task, 1);
  char *cmd = command_line (argv);
  int handle;

  t->pid = 0x40000000 | next_pid++;
  t->out_fd = t->err_fd = t->in_fd = -1;
  t->reap = child_pid == NULL || !(flags & G_SPAWN_DO_NOT_REAP_CHILD);
  t->captured = stdout_pipe != NULL || stderr_pipe != NULL;

  if (t->captured)
    {
      const char *name = g_get_application_name ();
      char *obey_ro, *text, *line, *dir_ro = NULL;
      int slot = 16384;
      const char *s = g_getenv ("RISCOS_GTK_TASK_WIMPSLOT");

      if (s && atoi (s) > 0)
        slot = atoi (s);
      t->obey = g_strdup_printf ("%s/riscos-gtk-task-%d-%d", g_get_tmp_dir (), our_task & 0xFFFF,
                                 t->pid & 0xFFFF);
      if (working_directory && *working_directory)
        dir_ro = working_directory[0] == '/' ? riscos_name (working_directory)
                                              : g_strdup (working_directory);
      text = g_strdup_printf ("SetEval Sys$ReturnCode 0\n%s%s%s%s\nEcho " EXIT_MARK "<Sys$ReturnCode>\n",
                              dir_ro ? "Dir " : "", dir_ro ? dir_ro : "", dir_ro ? "\n" : "", cmd);
      g_free (dir_ro);
      if (!g_file_set_contents (t->obey, text, -1, error))
        {
          g_free (text);
          g_free (cmd);
          task_free (t);
          return FALSE;
        }
      g_free (text);
      if ((stdout_pipe && !make_pipe (&t->out_fd, stdout_pipe, TRUE)) ||
          (stderr_pipe && !make_pipe (&t->err_fd, stderr_pipe, TRUE)) ||
          (stdin_pipe && !make_pipe (&t->in_fd, stdin_pipe, FALSE)))
        {
          g_set_error_literal (error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "Can't make a pipe");
          g_free (cmd);
          task_free (t);
          return FALSE;
        }
      t->pending = g_string_new (NULL);
      t->status_text = g_string_new (NULL);
      obey_ro = riscos_name (t->obey);
      line = g_strdup_printf ("TaskWindow \"Obey %s\" -name \"%s\" -wimpslot %dK -quit -task &%08X -txt &%08X",
                              obey_ro, name ? name : "GTK", slot, our_task, (unsigned) t->pid);
      g_free (obey_ro);
      handle = start_task (line, error);
      g_free (line);
    }
  else
    {
      handle = start_task (cmd, error);
      if (handle == 0)
        t->done = TRUE;          /* it has ended already */
      else
        t->task = handle;
    }
  g_free (cmd);
  if (handle < 0)
    {
      if (stdout_pipe && *stdout_pipe >= 0 && t->out_fd >= 0)
        close (*stdout_pipe);
      if (stderr_pipe && *stderr_pipe >= 0 && t->err_fd >= 0)
        close (*stderr_pipe);
      if (stdin_pipe && *stdin_pipe >= 0 && t->in_fd >= 0)
        close (*stdin_pipe);
      task_free (t);
      return FALSE;
    }
  if (child_pid)
    *child_pid = t->pid;
  if (t->captured)
    throwback_register (TRUE);
  if (t->done && t->reap)
    task_free (t);
  else
    g_hash_table_insert (tasks, GINT_TO_POINTER (t->pid), t);
  return TRUE;
}

static gboolean
owns (GPid pid)
{
  return tasks && g_hash_table_contains (tasks, GINT_TO_POINTER (pid));
}

static gboolean
exited (GPid pid, gint *wait_status)
{
  Task *t = g_hash_table_lookup (tasks, GINT_TO_POINTER (pid));

  if (t == NULL || !t->done)
    return FALSE;
  *wait_status = t->status;
  g_hash_table_remove (tasks, GINT_TO_POINTER (pid));
  return TRUE;
}

static void
write_all (int fd, const char *data, gsize len)
{
  while (fd >= 0 && len > 0)
    {
      ssize_t n = write (fd, data, len);
      if (n <= 0)
        return;
      data += n;
      len -= n;
    }
}

/* Output from the task: to the stdout pipe, all but EXIT_MARK and what
   follows it */
static void
task_output (Task *t, const char *data, gsize len)
{
  const char *mark;
  gsize keep, ml = strlen (EXIT_MARK);

  if (t->marked)
    {
      g_string_append_len (t->status_text, data, len);
      return;
    }
  g_string_append_len (t->pending, data, len);
  mark = g_strstr_len (t->pending->str, t->pending->len, EXIT_MARK);
  if (mark)
    {
      gsize before = mark - t->pending->str;
      write_all (t->out_fd, t->pending->str, before);
      g_string_append (t->status_text, mark + ml);
      g_string_truncate (t->pending, 0);
      t->marked = TRUE;
      return;
    }
  /* hold back an end that could be the start of EXIT_MARK */
  for (keep = MIN (ml - 1, t->pending->len); keep > 0; keep--)
    if (memcmp (t->pending->str + t->pending->len - keep, EXIT_MARK, keep) == 0)
      break;
  write_all (t->out_fd, t->pending->str, t->pending->len - keep);
  g_string_erase (t->pending, 0, t->pending->len - keep);
}

static void
task_ended (Task *t)
{
  if (t->captured)
    {
      if (t->marked)
        t->status = (atoi (t->status_text->str) & 0xFF) << 8;
      else
        {
          write_all (t->out_fd, t->pending->str, t->pending->len);
          t->status = 1 << 8;    /* stopped by an error */
        }
    }
  if (t->out_fd >= 0)
    close (t->out_fd);
  if (t->err_fd >= 0)
    close (t->err_fd);
  if (t->in_fd >= 0)
    close (t->in_fd);
  t->out_fd = t->err_fd = t->in_fd = -1;
  if (t->obey)
    {
      g_unlink (t->obey);
      g_clear_pointer (&t->obey, g_free);
    }
  t->done = TRUE;
  if (t->reap)
    g_hash_table_remove (tasks, GINT_TO_POINTER (t->pid));
  if (throwback_registered)
    {
      GHashTableIter it;
      gpointer k, v;
      gboolean running = FALSE;

      g_hash_table_iter_init (&it, tasks);
      while (g_hash_table_iter_next (&it, &k, &v))
        if (((Task *) v)->captured && !((Task *) v)->done)
          running = TRUE;
      if (!running)
        throwback_register (FALSE);
    }
}

/* The newest TaskWindow still running (throwback goes to its output) */
static Task *
newest_running (void)
{
  GHashTableIter it;
  gpointer k, v;
  Task *best = NULL;

  g_hash_table_iter_init (&it, tasks);
  while (g_hash_table_iter_next (&it, &k, &v))
    {
      Task *t = v;
      if (t->captured && !t->done && (best == NULL || t->pid > best->pid))
        best = t;
    }
  return best;
}

/* A RISC OS file name as UnixLib gives it: "Disc.$.src.c.hello" is
   "/Disc/$/src/hello.c" when "c" is one of UnixLib's suffix directories
   (UnixLib$sfix, or its own list) */
static char *
throwback_unix_name (const char *riscos)
{
  char *unix_name = gdk_riscos_path_to_unix (riscos), *leaf, *dir;
  const char *sfix = g_getenv ("UnixLib$sfix");
  char **list;
  int i;
  gboolean swap = FALSE;

  leaf = strrchr (unix_name, '/');
  if (leaf == NULL || leaf == unix_name)
    return unix_name;
  *leaf = '\0';
  dir = strrchr (unix_name, '/');
  *leaf = '/';
  if (dir == NULL)
    return unix_name;
  list = g_strsplit (sfix && *sfix ? sfix : "a:c:cc:c++:cpp:f:h:hh:i:l:m:o:p:pl:s:y", ":", -1);
  for (i = 0; list[i] && !swap; i++)
    swap = (int) strlen (list[i]) == leaf - dir - 1 && g_ascii_strncasecmp (dir + 1, list[i], leaf - dir - 1) == 0;
  if (swap)
    {
      char *base = g_strndup (unix_name, dir - unix_name), *ext = g_strndup (dir + 1, leaf - dir - 1);
      char *swapped = g_strdup_printf ("%s/%s.%s", base, leaf + 1, ext);
      g_free (base);
      g_free (ext);
      g_free (unix_name);
      unix_name = swapped;
    }
  g_strfreev (list);
  return unix_name;
}

static gboolean
throwback_message (int reason, int *block)
{
  Task *t = newest_running ();
  int len = MIN (block[0], 256) - 28;

  if (t == NULL)
    return FALSE;
  if (reason == 18)
    {
      /* acknowledge, so it isn't returned to the sender */
      int ack[64];
      _kernel_swi_regs r;
      memcpy (ack, block, 20);
      ack[3] = block[2];
      r.r[0] = 19;
      r.r[1] = (int) (long) ack;
      r.r[2] = block[1];
      _kernel_swi (Wimp_SendMessage, &r, &r);
    }
  switch (block[4])
    {
    case MESSAGE_ERRORS_IN:
    case MESSAGE_INFO_FOR_FILE:
      g_free (throwback_file);
      throwback_file = throwback_unix_name ((const char *) &block[5]);
      break;
    case MESSAGE_ERROR_DETAILS:
    case MESSAGE_INFO_DETAILS:
      {
        const char *kind = block[4] == MESSAGE_INFO_DETAILS ? "note"
                           : block[6] == 0 ? "warning" : block[6] == 1 ? "error" : "fatal error";
        char *text = g_strndup ((const char *) &block[7], MAX (len, 0));
        char *line = g_strdup_printf ("%s:%d: %s: %s\n", throwback_file ? throwback_file : "?",
                                      block[5], kind, g_strchomp (text));
        task_output (t, line, strlen (line));
        g_free (line);
        g_free (text);
      }
      break;
    }
  return TRUE;
}

static Task *
task_by_handle (int handle, gboolean captured)
{
  GHashTableIter it;
  gpointer k, v;

  if (tasks == NULL || handle == 0)
    return NULL;
  g_hash_table_iter_init (&it, tasks);
  while (g_hash_table_iter_next (&it, &k, &v))
    {
      Task *t = v;
      if (t->task == handle && !t->done && t->captured == captured)
        return t;
    }
  return NULL;
}

gboolean
_gdk_riscos_spawn_message (int reason, int *block)
{
  Task *t;

  if (tasks == NULL || reason < 17 || reason > 18)
    return FALSE;
  if (block[4] >= MESSAGE_THROWBACK_FIRST && block[4] <= MESSAGE_THROWBACK_LAST)
    return throwback_registered && throwback_message (reason, block);
  switch (block[4])
    {
    case MESSAGE_TASKWINDOW_EGO:
      t = g_hash_table_lookup (tasks, GINT_TO_POINTER (block[5]));
      if (t == NULL || !t->captured)
        return FALSE;
      t->task = block[1];
      return TRUE;
    case MESSAGE_TASKWINDOW_OUTPUT:
      if ((t = task_by_handle (block[1], TRUE)) == NULL)
        return FALSE;
      task_output (t, (const char *) &block[6], CLAMP (block[5], 0, 232));
      return TRUE;
    case MESSAGE_TASKWINDOW_MORIO:
      if ((t = task_by_handle (block[1], TRUE)) == NULL)
        return FALSE;
      task_ended (t);
      return TRUE;
    case MESSAGE_TASKCLOSEDOWN:
      if ((t = task_by_handle (block[1], FALSE)) != NULL)
        {
          t->status = 0;
          task_ended (t);
        }
      return FALSE;              /* others may want to know too */
    }
  return FALSE;
}

/**
 * gdk_riscos_spawn_kill:
 * @pid: a child started with g_spawn_async*() while GDK is running
 *
 * Ends it: a TaskWindow is sent TaskWindow_Morite, a task of its own
 * Message_Quit. Its child watch fires when it has gone.
 *
 * Returns: %FALSE if @pid isn't a task riscos-gtk started, or it has
 * ended already
 */
gboolean
gdk_riscos_spawn_kill (GPid pid)
{
  Task *t = tasks ? g_hash_table_lookup (tasks, GINT_TO_POINTER (pid)) : NULL;
  int block[6];
  _kernel_swi_regs r;

  if (t == NULL || t->done || t->task == 0)
    return FALSE;
  memset (block, 0, sizeof block);
  block[0] = 20;
  block[4] = t->captured ? MESSAGE_TASKWINDOW_MORITE : MESSAGE_QUIT;
  r.r[0] = 17;
  r.r[1] = (int) (long) block;
  r.r[2] = t->task;
  return _kernel_swi (Wimp_SendMessage, &r, &r) == NULL;
}

typedef struct
{
  gboolean (*spawn)  (const gchar *, const gchar * const *, const gchar * const *, GSpawnFlags,
                      GPid *, gint *, gint *, gint *, GError **);
  gboolean (*owns)   (GPid);
  gboolean (*exited) (GPid, gint *);
} GRiscosSpawnHooks;

/* glib/gspawn.c (patches/glib-2.80.0-riscos.diff) */
extern const GRiscosSpawnHooks *g_riscos_spawn_hooks;

void
_gdk_riscos_spawn_startup (int task)
{
  static const GRiscosSpawnHooks hooks = { spawn, owns, exited };

  our_task = task;
  if (tasks == NULL)
    tasks = g_hash_table_new_full (NULL, NULL, NULL, task_free);
  if (g_strcmp0 (g_getenv ("RISCOS_GTK_SPAWN"), "exec") != 0)
    g_riscos_spawn_hooks = &hooks;
}
