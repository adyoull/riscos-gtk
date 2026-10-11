/*
 * spawn-fake.c - g_spawn_async*() as RISC OS tasks (gdk/riscos/
 * gdkspawn-riscos.c, GLib's GRiscosSpawnHooks), on the fake RISC OS with a
 * stand-in TaskWindow module:
 *   - output read through pipes (a build command, "sh -c CMD"): a
 *     TaskWindow with us as its parent, running an Obey file that sets the
 *     directory and prints the return code; Unix file names in the command
 *     become RISC OS names; the output (longer than one message, the end
 *     marker split across two) reaches the pipe without the marker; the
 *     pipe ends; the child watch has the exit code;
 *   - a command stopped by an error (no marker): status 1, its message
 *     read;
 *   - no output wanted (g_spawn_async, g_spawn_command_line_async):
 *     Wimp_StartTask with the command; the child watch fires at its
 *     Message_TaskCloseDown;
 *   - gdk_riscos_spawn_kill: TaskWindow_Morite to the child;
 *   - throwback (the DDE's tools with -throwback): registered with
 *     DDEUtils while the TaskWindow runs; its errors become compiler
 *     lines with Unix names (c.hello is hello.c) in the output; a recorded
 *     message is acknowledged.
 *   spawn-fake APPDIR
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

static int n_tests, n_failed, stage, next_child = 0x5100, starts;
static char last_cmd[1024], obey_text[2048];
static GString *got;
static gboolean got_eof, watched;
static int got_status, morite_to, tb_registered, tb_unregistered, tb_acked;
static GPid pid;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
  fflush (stdout);
}

static void
message (int action, int sender, int word5, const char *data, int len)
{
  int b[64];
  memset (b, 0, sizeof b);
  b[0] = 24 + ((len + 3) & ~3);
  b[1] = sender;
  b[4] = action;
  b[5] = data ? len : word5;
  if (data)
    memcpy (&b[6], data, len);
  fake_queue_message (17, b);
}

/* TaskWindow_Output in pieces of at most 232 bytes, cut where asked */
static void
output (int child, const char *text, int cut)
{
  int len = strlen (text), at = 0;
  while (at < len)
    {
      int n = MIN (232, len - at);
      if (cut > at && cut < at + n)
        n = cut - at;
      message (0x808C1, child, 0, text + at, n);
      at += n;
    }
}

static char long_text[700];

/* The stand-in TaskWindow module (and the programs it runs) */
static int
start_task (const char *command)
{
  const char *o, *t;
  int child = next_child++;

  starts++;
  snprintf (last_cmd, sizeof last_cmd, "%s", command);
  if (!strstr (command, "-task &"))
    return 0x5300;                              /* a task of its own */
  t = strstr (command, "-txt &");
  o = strstr (command, "\"Obey ");
  obey_text[0] = 0;
  if (o)
    {
      /* the fake's RISC OS name of a host file: back to the host's */
      char ro[512], host[520], *p;
      FILE *f;
      size_t n = strcspn (o + 6, "\"");
      snprintf (ro, sizeof ro, "%.*s", (int) n, o + 6);
      snprintf (host, sizeof host, "/%s", ro);
      for (p = host + 1; *p; p++)
        *p = *p == '.' ? '/' : *p == '/' ? '.' : *p;
      if ((f = fopen (host, "r")))
        {
          n = fread (obey_text, 1, sizeof obey_text - 1, f);
          obey_text[n] = 0;
          fclose (f);
        }
    }
  message (0x808C2, child, t ? (int) strtoul (t + 6, NULL, 16) : 0, NULL, 0);
  if (strstr (obey_text, "\ncc /") || strstr (obey_text, "\ncc home"))
    {
      /* output longer than a message; the marker split over two */
      char text[1024];
      snprintf (text, sizeof text, "%shello.c:3: error: oops\n##riscos-gtk-exit 1\n", long_text);
      output (child, text, strstr (text, "##riscos") - text + 4);
      message (0x808C3, child, 0, NULL, 0);
    }
  else if (strstr (obey_text, "\nnosuch"))
    {
      output (child, "File 'nosuch' not found\n", 0);
      message (0x808C3, child, 0, NULL, 0);          /* the Obey file stopped */
    }
  else if (strstr (obey_text, "\necho"))
    {
      output (child, "fine\n##riscos-gtk-exit 0\n", 0);
      message (0x808C3, child, 0, NULL, 0);
    }
  else if (strstr (obey_text, "\ncc -throwback"))
    {
      int b[64];
      output (child, "Norcroft RISC OS ARM C\n", 0);
      message (0x42580, 0x5900, 0, NULL, 0);                    /* ThrowbackStart */
      memset (b, 0, sizeof b);                                   /* ProcessingFile, ErrorsIn: +20 name */
      b[0] = 20 + 20; b[1] = 0x5900; b[4] = 0x42581;
      strcpy ((char *) &b[5], "home.me.src.c.hello");
      fake_queue_message (17, b);
      b[4] = 0x42582;
      fake_queue_message (17, b);
      memset (b, 0, sizeof b);                                   /* ErrorDetails, recorded */
      b[0] = 28 + 16; b[1] = 0x5900; b[2] = 777; b[4] = 0x42583; b[5] = 3; b[6] = 1;
      strcpy ((char *) &b[7], "Missing ';'");
      fake_queue_message (18, b);
      memset (b, 0, sizeof b);
      b[0] = 28 + 16; b[1] = 0x5900; b[4] = 0x42583; b[5] = 7; b[6] = 0;
      strcpy ((char *) &b[7], "x unused");
      fake_queue_message (17, b);
      message (0x42584, 0x5900, 0, NULL, 0);                    /* ThrowbackEnd */
      output (child, "##riscos-gtk-exit 1\n", 0);
      message (0x808C3, child, 0, NULL, 0);
    }
  /* "loop": runs until it is killed */
  return 0x5200;
}

static void
on_message (int reason, int *block, int dest, int icon)
{
  if (reason == 19 && block[4] == 0x42583 && block[3] == 777 && dest == 0x5900)
    tb_acked++;
  if (block[4] == 0x808C4)
    {
      morite_to = dest;
      message (0x808C3, dest, 0, NULL, 0);
    }
}

static int
swi_hook (int no, _kernel_swi_regs *r, _kernel_oserror **e)
{
  switch (no & ~0x20000)
    {
    case 0x42585: if (r->r[0] == 0x4A00) tb_registered++; return 1;    /* DDEUtils_ThrowbackRegister */
    case 0x42586: if (r->r[0] == 0x4A00) tb_unregistered++; return 1;  /* _UnRegister */
    }
  return 0;
}

static gboolean
on_out (GIOChannel *ch, GIOCondition cond, gpointer d)
{
  char buf[256];
  gsize n = 0;
  GIOStatus st = g_io_channel_read_chars (ch, buf, sizeof buf, &n, NULL);

  if (n > 0)
    g_string_append_len (got, buf, n);
  if (st == G_IO_STATUS_EOF || (n == 0 && (cond & G_IO_HUP)))
    {
      got_eof = TRUE;
      return FALSE;
    }
  return TRUE;
}

static void
on_child (GPid p, gint status, gpointer d)
{
  watched = TRUE;
  got_status = status;
  g_spawn_close_pid (p);
}

static void
start_captured (const char *cmd)
{
  const char *argv[] = { "/bin/sh", "-c", cmd, NULL };
  gint out = -1, err = -1;
  GIOChannel *ch;
  GError *e = NULL;

  g_string_truncate (got, 0);
  got_eof = watched = FALSE;
  if (!g_spawn_async_with_pipes ("/home/me/src", (char **) argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL, NULL, &pid, NULL, &out, &err, &e))
    {
      printf ("# spawn: %s\n", e->message);
      return;
    }
  ch = g_io_channel_unix_new (out);
  g_io_channel_set_flags (ch, G_IO_FLAG_NONBLOCK, NULL);
  g_io_channel_set_encoding (ch, NULL, NULL);
  g_io_channel_set_close_on_unref (ch, TRUE);
  g_io_add_watch (ch, G_IO_IN | G_IO_HUP | G_IO_ERR, on_out, NULL);
  g_io_channel_unref (ch);
  close (err);
  g_child_watch_add (pid, on_child, NULL);
}

static int waits;

static gboolean
step (gpointer d)
{
  if (++waits > 400)
    {
      check (0, "timed out");
      printf ("1..%d\n", n_tests);
      exit (n_failed + 1);
    }
  switch (stage)
    {
    case 0:
      memset (long_text, 'x', sizeof long_text - 2);
      long_text[sizeof long_text - 2] = '\n';
      got = g_string_new (NULL);
      start_captured ("cc /home/me/src/hello.c -o hello");
      check (strncmp (last_cmd, "TaskWindow \"Obey ", 17) == 0 && strstr (last_cmd, " -quit -task &00004A00 -txt &"),
             "output wanted: a TaskWindow with this task as its parent");
      check (strstr (obey_text, "Dir home.me.src\n") && strstr (obey_text, "\ncc home.me.src.hello/c -o hello\n") &&
             strstr (obey_text, "Echo ##riscos-gtk-exit <Sys$ReturnCode>"),
             "its Obey file: the directory, the command with RISC OS names, the return code");
      stage = 1;
      break;
    case 1:
      if (!watched || !got_eof)
        break;
      {
        GString *want = g_string_new (long_text);
        g_string_append (want, "hello.c:3: error: oops\n");
        check (g_str_equal (got->str, want->str), "the output reaches the pipe (three messages), without the marker");
        g_string_free (want, TRUE);
      }
      check (WIFEXITED (got_status) && WEXITSTATUS (got_status) == 1, "the pipe ends; the child watch has exit code 1");
      start_captured ("nosuch");
      stage = 2;
      waits = 0;
      break;
    case 2:
      if (!watched || !got_eof)
        break;
      check (strstr (got->str, "File 'nosuch' not found") && WEXITSTATUS (got_status) == 1,
             "stopped by an error: its message, status 1");
      start_captured ("echo fine");
      stage = 3;
      waits = 0;
      break;
    case 3:
      if (!watched || !got_eof)
        break;
      check (g_str_equal (got->str, "fine\n") && got_status == 0, "a command that succeeds: status 0");
      {
        const char *argv[] = { "Filer_Run", "/home/me/doc.txt", NULL };
        watched = FALSE;
        if (g_spawn_async (NULL, (char **) argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, &pid, NULL))
          g_child_watch_add (pid, on_child, NULL);
        check (g_str_equal (last_cmd, "Filer_Run home.me.doc/txt"),
               "no output wanted: Wimp_StartTask with the command");
      }
      stage = 4;
      waits = 0;
      break;
    case 4:
      if (waits == 10)
        {
          check (!watched, "its child watch waits for the task");
          message (0x400C3, 0x5300, 0, NULL, 0);    /* Message_TaskCloseDown */
        }
      if (!watched)
        break;
      check (got_status == 0, "and fires when it closes down");
      starts = 0;
      check (g_spawn_command_line_async ("Filer_OpenDir RAM::RamDisc0.$", NULL) && starts == 1 &&
             g_str_equal (last_cmd, "Filer_OpenDir RAM::RamDisc0.$"), "g_spawn_command_line_async");
      start_captured ("loop");
      stage = 5;
      waits = 0;
      break;
    case 5:
      if (waits < 10)
        break;
      check (!watched, "a task that is still running");
      check (gdk_riscos_spawn_kill (pid), "gdk_riscos_spawn_kill");
      stage = 6;
      break;
    case 6:
      if (!watched || !got_eof)
        break;
      check (morite_to >= 0x5100 && WEXITSTATUS (got_status) == 1,
             "TaskWindow_Morite to the child; it ends (status 1)");
      tb_registered = tb_unregistered = 0;
      start_captured ("cc -throwback -c c.hello");
      check (tb_registered == 1, "a TaskWindow: registered with DDEUtils for throwback");
      stage = 7;
      waits = 0;
      break;
    case 7:
      if (!watched || !got_eof)
        break;
      check (g_str_equal (got->str, "Norcroft RISC OS ARM C\n"
                                    "/home/me/src/hello.c:3: error: Missing ';'\n"
                                    "/home/me/src/hello.c:7: warning: x unused\n"),
             "throwback errors become compiler lines (c.hello as hello.c)");
      check (tb_acked == 1, "a recorded throwback message is acknowledged");
      check (tb_unregistered == 1, "unregistered when the TaskWindow ends");
      printf ("1..%d\n", n_tests);
      fflush (stdout);
      _exit (n_failed);
    }
  return TRUE;
}

static void
hook (int reason)
{
  if (fake_wimp_script_pos () >= fake_wimp_script_len - 2)
    {
      int *e = fake_wimp_script[fake_wimp_script_len++];
      e[0] = FAKE_SLEEP; e[1] = 5; e[2] = e[3] = 0;
    }
}

int
main (int argc, char **argv)
{
  char *conf = g_build_filename (argv[1], "fonts", "fonts.conf", NULL);
  char *share = g_build_filename (argv[1], "share", NULL);

  g_setenv ("FONTCONFIG_FILE", conf, TRUE);
  g_setenv ("XDG_DATA_DIRS", share, TRUE);
  fake_set_screen (1280, 1024, 0, 5);
  fake_wimp_nulls = 20;
  fake_wimp_hook = hook;
  fake_starttask_hook = start_task;
  fake_message_hook = on_message;
  fake_swi_hook = swi_hook;
  gtk_init (&argc, &argv);
  g_timeout_add (20, step, NULL);
  gtk_main ();
  return 1;
}
