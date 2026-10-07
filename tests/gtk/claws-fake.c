/*
 * claws-fake.c - riscos-gtk: Claws Mail on the fake RISC OS desktop, with
 * its TLS through the fake AcornSSL (tests/fake/mesa-egl/fake_riscos.c) and
 * tests/claws/mailserver.py as the mail servers.
 *
 *   claws-fake APPDIR OUTDIR POP3PORT SMTPPORT IMAPPORT
 *
 * APPDIR has the fonts and GTK's data (as !GTKHello's); OUTDIR gets the home
 * directory (an account: POP3 over TLS from the start, SMTP with STARTTLS;
 * an MH mailbox) and is where mailserver.py runs (OUTDIR/server.log,
 * OUTDIR/pop, OUTDIR/smtp). Claws Mail (main renamed claws_main) starts
 * with --receive-all and --compose mailto:...; the test answers the
 * password question, waits for the mail, presses Send in the compose
 * window, waits for the server to have it and for Claws Mail to file it
 * in Sent. CLAWS_SHOTS=1 saves the screen (OUTDIR/shot-*.ppm); on a time-
 * out the windows and their labels are listed. CLAWS_FAKE_WIZARD=1 (no
 * settings: the first-start set-up) shows what comes up and stops.
 *
 * Prints TAP; exit status = failures. MIT licence.
 */
#include <gtk/gtk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int claws_main (int argc, char **argv);

/* Claws Mail's own calls (folder.h), for the IMAP stage */
typedef struct _FolderItem FolderItem;
FolderItem *folder_find_item_from_identifier (const char *identifier);
int folder_item_scan (FolderItem *item);
char *folder_item_fetch_msg (FolderItem *item, int num);

static int n_tests, n_failed;
static const char *outdir;
static char home[600];
static int stage, polls, password_answers, send_pressed, compose_seen;
static int inbox_files, smtp_files, main_window_seen;
static char inbox_text[4096], smtp_text[4096], server_log[8192];

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    n_failed++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
}

static void
add (int reason, int a, int b, int c)
{
  int *e = fake_wimp_script[fake_wimp_script_len++];
  e[0] = reason; e[1] = a; e[2] = b; e[3] = c;
}

static void
write_file (const char *dir, const char *name, const char *text)
{
  char *p = g_build_filename (dir, name, NULL);
  g_file_set_contents (p, text, -1, NULL);
  g_free (p);
}

static int
count_files (const char *dir, char *first, size_t len)
{
  GDir *d = g_dir_open (dir, 0, NULL);
  const char *n;
  int c = 0;

  if (!d)
    return 0;
  while ((n = g_dir_read_name (d)))
    if (n[0] >= '1' && n[0] <= '9')
      {
        if (c++ == 0 && first)
          {
            char *p = g_build_filename (dir, n, NULL), *t = NULL;
            if (g_file_get_contents (p, &t, NULL, NULL))
              g_strlcpy (first, t, len);
            g_free (t);
            g_free (p);
          }
      }
  g_dir_close (d);
  return c;
}

static GtkWidget *
find_type (GtkWidget *w, GType type)
{
  GList *kids, *l;
  GtkWidget *r = NULL;

  if (G_TYPE_CHECK_INSTANCE_TYPE (w, type))
    return w;
  if (!GTK_IS_CONTAINER (w))
    return NULL;
  kids = gtk_container_get_children (GTK_CONTAINER (w));
  for (l = kids; l && !r; l = l->next)
    r = find_type (l->data, type);
  g_list_free (kids);
  return r;
}

/* a button (or tool button) whose label is text */
static GtkWidget *
find_button (GtkWidget *w, const char *text)
{
  GList *kids, *l;
  GtkWidget *r = NULL;

  if (GTK_IS_TOOL_BUTTON (w) && gtk_tool_button_get_label (GTK_TOOL_BUTTON (w)) &&
      strcmp (gtk_tool_button_get_label (GTK_TOOL_BUTTON (w)), text) == 0)
    return w;
  if (GTK_IS_BUTTON (w) && gtk_button_get_label (GTK_BUTTON (w)) &&
      g_str_has_suffix (gtk_button_get_label (GTK_BUTTON (w)), text))
    return w;
  if (!GTK_IS_CONTAINER (w))
    return NULL;
  kids = gtk_container_get_children (GTK_CONTAINER (w));
  for (l = kids; l && !r; l = l->next)
    r = find_button (l->data, text);
  g_list_free (kids);
  return r;
}

static GtkWidget *
window_titled (const char *part)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  GtkWidget *r = NULL;

  for (l = tops; l && !r; l = l->next)
    if (gtk_widget_get_visible (l->data) && gtk_window_get_title (l->data) &&
        strstr (gtk_window_get_title (l->data), part))
      r = l->data;
  g_list_free (tops);
  return r;
}

/* a visible dialogue with a password entry (not shown text) */
static GtkWidget *
password_entry (void)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  GtkWidget *r = NULL;

  for (l = tops; l && !r; l = l->next)
    if (gtk_widget_get_visible (l->data))
      {
        GtkWidget *e = find_type (l->data, GTK_TYPE_ENTRY);
        if (e && !gtk_entry_get_visibility (GTK_ENTRY (e)))
          r = e;
      }
  g_list_free (tops);
  return r;
}

static void
dump_shot (const char *name)
{
  char path[700];
  FILE *f;
  int x, y;

  if (!getenv ("CLAWS_SHOTS"))
    return;
  g_snprintf (path, sizeof path, "%s/shot-%s.ppm", outdir, name);
  if (!(f = fopen (path, "wb")))
    return;
  fprintf (f, "P6\n1280 1024\n255\n");
  for (y = 0; y < 1024; y++)
    for (x = 0; x < 1280; x++)
      {
        unsigned int p = fake_screen_pixel (x, y);
        fputc (p & 0xFF, f);
        fputc ((p >> 8) & 0xFF, f);
        fputc ((p >> 16) & 0xFF, f);
      }
  fclose (f);
}

static int sent_files, queue_files = -1;

/* IMAP (libetpan, its TLS through the same AcornSSL code): the INBOX of
   the second account, scanned and its message fetched, from the main
   loop as a click on the folder would */
static int imap_found = -1, imap_scanned = -1, imap_done;
static char imap_text[4096];

static gboolean
imap_go (gpointer data)
{
  FolderItem *item = folder_find_item_from_identifier ("#imap/IMAPTest/INBOX");
  char *path;

  imap_found = item != NULL;
  if (item)
    {
      imap_scanned = folder_item_scan (item);
      if ((path = folder_item_fetch_msg (item, 1)))
        {
          char *t = NULL;
          if (g_file_get_contents (path, &t, NULL, NULL))
            g_strlcpy (imap_text, t, sizeof imap_text);
          g_free (t);
          g_free (path);
        }
    }
  imap_done = 1;
  return G_SOURCE_REMOVE;
}

static void
dump_labels (GtkWidget *w)
{
  if (GTK_IS_LABEL (w) && gtk_widget_get_mapped (w) && *gtk_label_get_text (GTK_LABEL (w)))
    printf ("#     label \"%s\"\n", gtk_label_get_text (GTK_LABEL (w)));
  if (GTK_IS_CONTAINER (w))
    {
      GList *l, *kids = gtk_container_get_children (GTK_CONTAINER (w));
      for (l = kids; l; l = l->next)
        dump_labels (l->data);
      g_list_free (kids);
    }
}

/* on a failure: what's on the screen */
static void
dump_windows (void)
{
  GList *l, *tops = gtk_window_list_toplevels ();
  for (l = tops; l; l = l->next)
    if (gtk_widget_get_visible (l->data))
      {
        printf ("#   window \"%s\"\n", gtk_window_get_title (l->data) ? gtk_window_get_title (l->data) : "");
        if (!strstr (gtk_window_get_title (l->data) ? gtk_window_get_title (l->data) : "", "Claws Mail"))
          dump_labels (l->data);
      }
  g_list_free (tops);
}

static void
finish (void)
{
  char *p, *t = NULL;

  p = g_build_filename (outdir, "server.log", NULL);
  if (g_file_get_contents (p, &t, NULL, NULL))
    g_strlcpy (server_log, t, sizeof server_log);
  g_free (t);
  g_free (p);

  check (main_window_seen, "Claws Mail starts, with its main window");
  check (password_answers >= 1, "it asks for the POP3 password (answered)");
  check (strstr (server_log, "POP3 RETR 1") != NULL,
         "it fetches the message over POP3 (pop3s: TLS from the start)");
  check (inbox_files == 1 && strstr (inbox_text, "Subject: Hello from the test") != NULL,
         "the message is in the inbox (an MH folder)");
  check (fake_acornssl_sessions >= 2 && fake_acornssl_handshake_steps >= 6,
         "TLS goes through AcornSSL: a session each for POP3 and SMTP, handshakes done");
  check (strcmp (fake_acornssl_host, "localhost") == 0,
         "the server's name is given to AcornSSL (SNI, and its certificate check)");
  check (compose_seen && send_pressed, "the compose window opens (--compose); Send is pressed");
  check (strstr (server_log, "SMTP STARTTLS") != NULL && strstr (server_log, "SMTP MAIL FROM") != NULL,
         "SMTP: STARTTLS, then the message (the server refuses MAIL before STARTTLS)");
  check (smtp_files == 1 && strstr (smtp_text, "Subject: Reply from Claws") != NULL &&
         strstr (smtp_text, "To: bob@example.com") != NULL,
         "the server has the message sent (To and Subject from the mailto: URL)");
  check (sent_files == 1 && queue_files == 0,
         "the message moves from Queue to Sent once it's sent");
  {
    /* the main window: no scroll bar strip outside it (its message list's
       scroll bar, shown always at the right edge, has the grip at its
       foot); its Wimp window is as wide as GTK's */
    GtkWidget *mw = window_titled ("Claws Mail");
    int i, w = mw ? gtk_widget_get_allocated_width (mw) : 0, exact = 0, wider = 0;
    for (i = 0; i < FAKE_MAX_WINDOWS; i++)
      if (fake_windows[i].handle)
        {
          int vw = (fake_windows[i].x1 - fake_windows[i].x0) >> 1;
          exact += vw == w;
          wider += vw > w && vw <= w + 40;
        }
    check (w > 0 && exact >= 1 && wider == 0,
           "the main window's own scroll bar at its right edge takes the size grip: no extra scroll bar");
    printf ("# main window %d px wide: %d Wimp windows that wide, %d a scroll bar wider\n", w, exact, wider);
  }
  check (imap_found == 1 && imap_scanned == 0,
         "IMAP: the second account's INBOX is found and scanned (imaps through AcornSSL)");
  check (strstr (server_log, "IMAP") && strstr (server_log, "LOGIN ****") &&
         strstr (server_log, "SELECT") != NULL,
         "IMAP: Claws Mail logs in and selects the INBOX");
  check (strstr (imap_text, "Subject: Hello over IMAP") != NULL &&
         strstr (imap_text, "An IMAP message for RISC OS.") != NULL,
         "IMAP: the message is fetched whole and cached");
  check (fake_acornssl_sessions >= 3 && fake_acornssl_closed_open == fake_acornssl_closes,
         "IMAP has its own AcornSSL session; sessions are closed before their sockets");
  printf ("# AcornSSL: %d sessions, %d handshake steps, %d closes (%d with the socket open), host \"%s\"\n",
          fake_acornssl_sessions, fake_acornssl_handshake_steps, fake_acornssl_closes,
          fake_acornssl_closed_open, fake_acornssl_host);
  printf ("# server log:\n");
  for (p = strtok (server_log, "\n"); p; p = strtok (NULL, "\n"))
    printf ("#   %s\n", p);
  printf ("1..%d\n", n_tests);
  fflush (stdout);
  _exit (n_failed);
}

static void
hook (int reason)
{
  char inbox[700], smtp[700];
  GtkWidget *w;

  /* the desktop gives null events throughout (fake_wimp_nulls); if the
     program polls without them, the script has some too */
  if (fake_wimp_script_pos () >= fake_wimp_script_len - 2 && fake_wimp_script_len < FAKE_SCRIPT_MAX - 10)
    {
      int i;
      for (i = 0; i < 8; i++)
        add (FAKE_NULL, 0, 0, 0);
    }
  if (++polls % 20)
    return;
  /* CLAWS_FAKE_WIZARD: no settings, so the first-start set-up: what's shown */
  if (getenv ("CLAWS_FAKE_WIZARD") && polls >= 400)
    {
      dump_windows ();
      dump_shot ("wizard");
      fflush (stdout);
      _exit (0);
    }

  g_snprintf (inbox, sizeof inbox, "%s/Mail/inbox", home);
  g_snprintf (smtp, sizeof smtp, "%s/smtp", outdir);
  if (window_titled ("Claws Mail"))
    main_window_seen = 1;
  if ((w = password_entry ()))
    {
      GtkWidget *top = gtk_widget_get_toplevel (w), *ok;
      gtk_entry_set_text (GTK_ENTRY (w), "secret");
      ok = find_button (top, "OK");
      if (ok)
        gtk_button_clicked (GTK_BUTTON (ok));
      else
        gtk_widget_activate (w);
      password_answers++;
    }
  inbox_files = count_files (inbox, inbox_text, sizeof inbox_text);
  smtp_files = count_files (smtp, NULL, 0);
  if (smtp_files)
    {
      char *p = g_build_filename (smtp, "1.eml", NULL), *t = NULL;
      if (g_file_get_contents (p, &t, NULL, NULL))
        g_strlcpy (smtp_text, t, sizeof smtp_text);
      g_free (t);
      g_free (p);
    }
  switch (stage)
    {
    case 0:     /* the mail arrives */
      if (inbox_files >= 1)
        {
          dump_shot ("received");
          stage = 1;
        }
      break;
    case 1:     /* the compose window: Send */
      if ((w = window_titled ("Write message")))
        {
          GtkWidget *send = find_button (w, "Send");
          compose_seen = 1;
          dump_shot ("compose");
          if (send)
            {
              /* first: sending runs the main loop inside "clicked",
                 and this hook with it */
              stage = 2;
              if (GTK_IS_TOOL_BUTTON (send))
                g_signal_emit_by_name (send, "clicked");
              else
                gtk_button_clicked (GTK_BUTTON (send));
              send_pressed = 1;
            }
        }
      break;
    case 2:     /* the server has it */
      if (smtp_files >= 1)
        stage = 3;
      break;
    case 3:     /* and Claws has filed it in Sent */
      {
        char d[700];
        g_snprintf (d, sizeof d, "%s/Mail/sent", home);
        sent_files = count_files (d, NULL, 0);
        g_snprintf (d, sizeof d, "%s/Mail/queue", home);
        queue_files = count_files (d, NULL, 0);
        if (sent_files >= 1 && queue_files == 0)
          {
            dump_shot ("sent");
            stage = 4;
            g_idle_add (imap_go, NULL);
          }
      }
      break;
    case 4:     /* IMAP */
      if (imap_done)
        {
          dump_shot ("imap");
          finish ();
        }
      break;
    }
  if (polls > 60000)
    {
      printf ("# timed out at stage %d\n", stage);
      dump_windows ();
      dump_shot ("timeout");
      finish ();
    }
}

int
main (int argc, char **argv)
{
  char cfg[700], mail[700], pop[700], logpath[700], url[256];
  char *targv[6];
  static const char *boxes[] = { "inbox", "sent", "queue", "trash", "draft" };
  int i;

  if (argc < 6)
    return 2;
  outdir = argv[2];
  {
    /* absolute: Claws changes directory to its settings */
    char *app = g_canonicalize_filename (argv[1], NULL);
    char *conf = g_build_filename (app, "fonts", "fonts.conf", NULL);
    char *share = g_build_filename (app, "share", NULL);
    g_setenv ("FONTCONFIG_FILE", conf, TRUE);
    g_setenv ("XDG_DATA_DIRS", share, TRUE);
  }
  g_snprintf (home, sizeof home, "%s/home", outdir);
  g_setenv ("HOME", home, TRUE);
  g_snprintf (cfg, sizeof cfg, "%s/.claws-mail", home);
  g_mkdir_with_parents (cfg, 0755);
  for (i = 0; i < 5; i++)
    {
      g_snprintf (mail, sizeof mail, "%s/Mail/%s", home, boxes[i]);
      g_mkdir_with_parents (mail, 0755);
    }
  g_snprintf (pop, sizeof pop, "%s/imap", outdir);
  g_mkdir_with_parents (pop, 0755);
  write_file (pop, "1", "From: Carol <carol@example.com>\r\nTo: imap@example.com\r\n"
              "Subject: Hello over IMAP\r\nDate: Wed, 07 Oct 2026 11:00:00 +0100\r\n"
              "Message-ID: <imap1@example.com>\r\n\r\nAn IMAP message for RISC OS.\r\n");
  g_snprintf (pop, sizeof pop, "%s/pop", outdir);
  g_mkdir_with_parents (pop, 0755);
  write_file (pop, "1", "From: Alice <alice@example.com>\r\nTo: test@example.com\r\n"
              "Subject: Hello from the test\r\nDate: Wed, 07 Oct 2026 10:00:00 +0100\r\n"
              "Message-ID: <test1@example.com>\r\n\r\nA message for Claws Mail on RISC OS.\r\n");
  if (!getenv ("CLAWS_FAKE_WIZARD"))
  {
    char *acc = g_strdup_printf (
      "[Account: 1]\naccount_name=Test\nis_default=1\nname=Test User\naddress=test@example.com\n"
      "protocol=0\nreceive_server=localhost\nsmtp_server=localhost\nuser_id=test\n"
      "set_popport=1\npop_port=%s\nset_smtpport=1\nsmtp_port=%s\nssl_pop=1\nssl_smtp=2\n"
      "use_tls_sni=1\nuse_smtp_auth=0\nremove_mail=1\ninbox=#mh/Mailbox/inbox\n"
      "receive_at_get_all=1\nconfig_version=4\n"
      "[Account: 2]\naccount_name=IMAPTest\nis_default=0\nname=Test User\naddress=imap@example.com\n"
      "protocol=1\nreceive_server=localhost\nsmtp_server=localhost\nuser_id=test\n"
      "set_imapport=1\nimap_port=%s\nssl_imap=1\nimap_auth_method=1\nuse_tls_sni=1\n"
      "receive_at_get_all=0\nconfig_version=4\n", argv[3], argv[4], argv[5]);
    write_file (cfg, "accountrc", acc);
    g_free (acc);
  }
  if (!getenv ("CLAWS_FAKE_WIZARD"))
  write_file (cfg, "folderlist.xml",
    "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
    "<folderlist config_version=\"5\">\n"
    "<folder type=\"mh\" name=\"Mailbox\" path=\"Mail\" collapsed=\"0\" sort=\"0\">\n"
    "<folderitem type=\"inbox\" name=\"inbox\" path=\"inbox\" />\n"
    "<folderitem type=\"outbox\" name=\"sent\" path=\"sent\" />\n"
    "<folderitem type=\"queue\" name=\"queue\" path=\"queue\" />\n"
    "<folderitem type=\"trash\" name=\"trash\" path=\"trash\" />\n"
    "<folderitem type=\"draft\" name=\"draft\" path=\"draft\" />\n"
    "</folder>\n"
    "<folder type=\"imap\" name=\"IMAPTest\" account_id=\"2\" collapsed=\"0\" sort=\"0\">\n"
    "<folderitem type=\"inbox\" name=\"INBOX\" path=\"INBOX\" />\n"
    "</folder>\n</folderlist>\n");

  fake_set_screen (1280, 1024, 0, 5);
  fake_wimp_nulls = 1 << 30;           /* null events throughout */
  fake_wimp_hook = hook;
  g_snprintf (logpath, sizeof logpath, "%s/ClawsMail.Log", outdir);
  fflush (stdout);
  if (freopen (logpath, "w", stderr) == NULL)
    return 3;
  g_snprintf (url, sizeof url, "mailto:bob@example.com?subject=Reply%%20from%%20Claws&body=Sent%%20through%%20AcornSSL");
  targv[0] = "claws-mail";
  targv[1] = "--receive-all";
  targv[2] = "--compose";
  targv[3] = url;
  targv[4] = NULL;
  claws_main (4, targv);
  finish ();
  return 0;
}
