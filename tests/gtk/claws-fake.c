/*
 * claws-fake.c - riscos-gtk: Claws Mail on the fake RISC OS desktop, with
 * its TLS through the fake AcornSSL (tests/fake/mesa-egl/fake_riscos.c) and
 * tests/claws/mailserver.py as the mail servers.
 *
 *   claws-fake APPDIR OUTDIR POP3PORT SMTPPORT IMAPPORT HTTPPORT
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

/* claws-fake-claws.c (with Claws Mail's headers) */
int fake_claws_show_message (const char *folder, int num);
const char *fake_claws_viewer (GtkWidget **widget);
void fake_claws_remote_content (int on);
const char *fake_claws_theme (void);
int fake_claws_promote_html (void);
GdkPixbuf *fake_claws_stock (int which);
void fake_claws_about (void);

static int n_tests, n_failed;
static int pop_inbox_files;
static char pop_inbox_text[4096];
/* HTML mail and the RISC OS icons (below) */
static const char *html_viewer = "(none)";
static int html_polls, html_red, html_green, html_yellow;
static char http_port[16];
static int folder_from_desktop = -1;
static int html_headers = -1, html_white = -1, link_opened = -1, about_acornssl = -1, about_logo = -1;
static char html_header_text[512];
static int theme_chosen = -1, theme_icons = -1, defaults_marker = -1, promote_html = -1;
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

/* the browser: URI_Dispatch, Wimp_StartTask, TaskManager_EnumerateTasks
   (riscos-claws.c's riscos_claws_open_uri); "Iris" runs once started */
static char uri_dispatched[256], task_started[128];
static int iris_running, uri_dispatches, start_tasks;

static int
swi_hook (int no, _kernel_swi_regs *r, _kernel_oserror **e)
{
  static char iris[] = "Iris\x0d";
  switch (no & ~0x20000)
    {
    case 0x4E381:       /* URI_Dispatch */
      g_strlcpy (uri_dispatched, (const char *) (long) r->r[1], sizeof uri_dispatched);
      uri_dispatches++;
      r->r[0] = 0;
      return 1;
    case 0x400DE:       /* Wimp_StartTask */
      g_strlcpy (task_started, (const char *) (long) r->r[0], sizeof task_started);
      start_tasks++;
      iris_running = 1;
      r->r[0] = 0;
      return 1;
    case 0x42681:       /* TaskManager_EnumerateTasks */
      if (iris_running && r->r[0] == 0 && r->r[2] >= 16)
        {
          int *b = (int *) (long) r->r[1];
          b[0] = 0x1234; b[1] = (int) (long) iris; b[2] = 640; b[3] = 0;
          r->r[1] += 16;
        }
      r->r[0] = -1;
      return 1;
    }
  return 0;
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

/* RISC OS behaviour: Claws Mail starts on the icon bar (no main window
   until Select on the icon); closing the main window hides it and leaves
   Claws Mail running on the icon bar */
static int start_hidden = -1, shown_by_select, close_hides = -1;

/* Menu in the main window: the window's menu everywhere; over the message
   list and the folder list, with that list's menu as its first entry
   ("Message", "Folder"; riscos-claws.c names them), as one RISC OS menu
   per window. menu_first[k], menu_second[k]: the opened menu's first two
   entries for k = message list, folder list, elsewhere (the status bar). */
static char menu_title[5][64], menu_first[5][64], menu_second[5][64];
static int menu_sub_items[5] = { -1, -1, -1, -1, -1 };
static char menu_sub_first[5][64];
/* k = 3, 4: Menu over the HTML (litehtml) view, away from a link and on
   one; html_area / html_link: those points in the view's drawing area */
static GtkWidget *html_area;
static int html_blank_x, html_blank_y, html_link_x = -1, html_link_y = -1;

/* a click (button 1 also moves the pointer there first) at x, y in w's
   own GDK window, as GDK would deliver it */
static void
click_widget (GtkWidget *w, int x, int y, int button)
{
  GdkWindow *win = gtk_widget_get_window (w);
  GdkDevice *dev = gdk_seat_get_pointer (gdk_display_get_default_seat (gdk_display_get_default ()));
  GdkEvent *ev;
  int ox, oy;

  gdk_window_get_origin (win, &ox, &oy);
  ev = gdk_event_new (GDK_MOTION_NOTIFY);
  ev->motion.window = g_object_ref (win);
  ev->motion.x = x;
  ev->motion.y = y;
  ev->motion.x_root = ox + x;
  ev->motion.y_root = oy + y;
  gdk_event_set_device (ev, dev);
  gtk_main_do_event (ev);
  gdk_event_free (ev);
  ev = gdk_event_new (GDK_BUTTON_PRESS);
  ev->button.window = g_object_ref (win);
  ev->button.time = GDK_CURRENT_TIME;
  ev->button.x = x;
  ev->button.y = y;
  ev->button.x_root = ox + x;
  ev->button.y_root = oy + y;
  ev->button.button = button;
  gdk_event_set_device (ev, dev);
  gtk_main_do_event (ev);
  ev->type = GDK_BUTTON_RELEASE;
  gtk_main_do_event (ev);
  gdk_event_free (ev);
}

static GtkWidget *
find_named (GtkWidget *w, const char *name)
{
  const char *n = g_object_get_data (G_OBJECT (w), "riscos-gtk-context-name");
  GtkWidget *r = NULL;
  if (n && strcmp (n, name) == 0)
    return w;
  if (GTK_IS_CONTAINER (w))
    {
      GList *l, *kids = gtk_container_get_children (GTK_CONTAINER (w));
      for (l = kids; l && !r; l = l->next)
        r = find_named (l->data, name);
      g_list_free (kids);
    }
  return r;
}

static GtkWidget *
find_statusbar (GtkWidget *w)
{
  GtkWidget *r = NULL;
  if (GTK_IS_STATUSBAR (w))
    return w;
  if (GTK_IS_CONTAINER (w))
    {
      GList *l, *kids = gtk_container_get_children (GTK_CONTAINER (w));
      for (l = kids; l && !r; l = l->next)
        r = find_statusbar (l->data);
      g_list_free (kids);
    }
  return r;
}

static const char *
menu_item_text (const int *m, int i)
{
  const int *it = m + 7 + 6 * i;
  return (it[2] & 0x100) ? (const char *) (long) it[3] : "";
}

/* A Menu click (GDK button 3) on widget w's middle, as GDK would deliver
   it: on the widget's own GDK window under that point. (Scripted Wimp
   clicks can't be used here: the test runs on null events throughout.) */
static void
menu_press (GtkWidget *mw, GtkWidget *w)
{
  GdkWindow *top = gtk_widget_get_window (mw), *win = gtk_widget_get_window (w), *best = win;
  GList *l, *kids;
  int ox, oy, x, y, rx, ry, wx, wy;
  GdkEvent *ev;

  /* the middle, or 60 pixels down if that's higher (a short folder
     list: on its rows, where its menu is) */
  gtk_widget_translate_coordinates (w, mw, gtk_widget_get_allocated_width (w) / 2,
                                    MIN (gtk_widget_get_allocated_height (w) / 2, 60), &x, &y);
  gdk_window_get_origin (top, &ox, &oy);
  rx = ox + x;
  ry = oy + y;
  /* a child window of the widget's (a list's rows) under the point */
  kids = gdk_window_peek_children (win);
  for (l = kids; l; l = l->next)
    {
      gpointer ud = NULL;
      int cx, cy;
      gdk_window_get_user_data (l->data, &ud);
      gdk_window_get_origin (l->data, &cx, &cy);
      if (ud == w && rx >= cx && ry >= cy && rx < cx + gdk_window_get_width (l->data) &&
          ry < cy + gdk_window_get_height (l->data))
        best = l->data;
    }
  gdk_window_get_origin (best, &wx, &wy);
  ev = gdk_event_new (GDK_BUTTON_PRESS);
  ev->button.window = g_object_ref (best);
  ev->button.send_event = FALSE;
  ev->button.time = GDK_CURRENT_TIME;
  ev->button.x = rx - wx;
  ev->button.y = ry - wy;
  ev->button.x_root = rx;
  ev->button.y_root = ry;
  ev->button.button = 3;
  gdk_event_set_device (ev, gdk_seat_get_pointer (gdk_display_get_default_seat (gdk_display_get_default ())));
  gtk_main_do_event (ev);
  ev->type = GDK_BUTTON_RELEASE;
  gtk_main_do_event (ev);
  gdk_event_free (ev);
}

static int menu_k = -1;

static gboolean
menu_go (gpointer data)
{
  int k = GPOINTER_TO_INT (data);
  GtkWidget *mw = window_titled ("Claws Mail"), *w;

  if (k >= 3)
    {
      if (html_area && (k == 3 || html_link_x >= 0))
        click_widget (html_area, k == 3 ? html_blank_x : html_link_x, k == 3 ? html_blank_y : html_link_y, 3);
      else
        g_strlcpy (menu_first[k], "(no widget)", sizeof menu_first[k]);
      return G_SOURCE_REMOVE;
    }
  w = !mw ? NULL : k == 0 ? find_named (mw, "Message") : k == 1 ? find_named (mw, "Folder") : find_statusbar (mw);
  if (w)
    menu_press (mw, w);
  else
    g_strlcpy (menu_first[k], "(no widget)", sizeof menu_first[k]);
  return G_SOURCE_REMOVE;
}

/* One Menu click (k: 0 message list, 1 folder list, 2 status bar);
   TRUE when its menu has been looked at */
static int
menu_step (int k)
{
  static int opened_before, waits;
  int i;

  if (menu_k != k)
    {
      menu_k = k;
      waits = 0;
      opened_before = fake_menus_opened;
      g_idle_add (menu_go, GINT_TO_POINTER (k));
      return 0;
    }
  if (fake_menus_opened != opened_before && fake_menu)
    {
      const int *m = fake_menu, *sub = (const int *) (long) m[7 + 1];
      g_strlcpy (menu_title[k], (m[7] & 0x100) ? (const char *) (long) m[0] : "", sizeof menu_title[k]);
      g_strlcpy (menu_first[k], menu_item_text (m, 0), sizeof menu_first[k]);
      g_strlcpy (menu_second[k], menu_item_text (m, 1), sizeof menu_second[k]);
      menu_sub_items[k] = 0;
      if (m[7 + 1] != -1 && sub)
        g_strlcpy (menu_sub_first[k], menu_item_text (sub, 0), sizeof menu_sub_first[k]);
      if (m[7 + 1] != -1 && sub)
        for (i = 0; i < 64; i++)
          {
            menu_sub_items[k]++;
            if (sub[7 + 6 * i] & 0x80)
              break;
          }
      /* close it, as a click elsewhere would */
      gdk_riscos_display_close_menu (gdk_display_get_default ());
      return 1;
    }
  if (++waits > 50 || menu_first[k][0])
    {
      if (!menu_first[k][0])
        g_strlcpy (menu_first[k], "(no menu)", sizeof menu_first[k]);
      return 1;
    }
  return 0;
}
static GtkWidget *closed_window;
static int strip_ok = -1, corner_clean = -1, main_w, main_exact, main_wider, corner_same;

/* The main window (while shown): its message list's scroll bar ends above
   the corner, so the window has the empty scroll bar outside it (its Wimp
   window a bar wider than GTK's), the grip at its foot; and the grip
   isn't left behind in the GTK window's own corner as well (the Pi's
   "two resize icons" at first start). */
static void
check_main_window (GtkWidget *mw)
{
  int i, x, y, w, h, dx, dy, gw = 14;
  GdkWindow *gw_ = gtk_widget_get_window (mw);

  main_w = w = gtk_widget_get_allocated_width (mw);
  h = gtk_widget_get_allocated_height (mw);
  for (i = 0; i < FAKE_MAX_WINDOWS; i++)
    if (fake_windows[i].handle)
      {
        int vw = (fake_windows[i].x1 - fake_windows[i].x0) >> 1;
        main_exact += vw == w;
        main_wider += vw > w && vw <= w + 40;
      }
  strip_ok = w > 0 && main_exact == 0 && main_wider >= 1;
  gdk_window_get_origin (gw_, &x, &y);
  /* the strip's grip: x + w .. x + w + gw, bottom gw rows; the GTK
     corner: x + w - gw .. x + w, the same rows */
  corner_same = 0;
  for (dy = 0; dy < gw; dy++)
    for (dx = 0; dx < gw; dx++)
      corner_same += fake_screen_pixel (x + w - gw + dx, y + h - gw + dy) ==
                     fake_screen_pixel (x + w + dx, y + h - gw + dy);
  corner_clean = corner_same < gw * gw / 2;
}

static gboolean
iconbar_select (gpointer data)
{
  gpointer d;
  const GdkRiscosIconbarHandlers *h = gdk_riscos_iconbar_get_handlers (&d);
  if (h && h->show)
    {
      h->show (d);
      shown_by_select = 1;
    }
  return G_SOURCE_REMOVE;
}

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

  check (start_hidden == 1, "Claws Mail starts on the icon bar: no main window until Select");
  check (main_window_seen && shown_by_select, "Select on the icon bar shows the main window");
  check (password_answers >= 1, "it asks for the POP3 password (answered)");
  check (strstr (server_log, "POP3 RETR 1") != NULL,
         "it fetches the message over POP3 (pop3s: TLS from the start)");
  check (pop_inbox_files == 1 && strstr (pop_inbox_text, "Subject: Hello from the test") != NULL,
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
  check (strip_ok == 1,
         "the main window's scroll bars don't reach its bottom right corner: the size grip has a scroll bar of its own");
  check (corner_clean == 1,
         "and the grip isn't also left in the window's own corner (one size icon, not two)");
  printf ("# main window %d px wide: %d Wimp windows that wide, %d a scroll bar wider; corner %d grip-like pixels\n",
          main_w, main_exact, main_wider, corner_same);
  check (strncmp (menu_first[0], "Message", 7) == 0 && strncmp (menu_second[0], "File", 4) == 0 &&
         menu_sub_items[0] > 0,
         "Menu over the message list: the window's menu, with the list's menu as \"Message >\" first");
  check (strncmp (menu_first[1], "Folder", 6) == 0 && strncmp (menu_second[1], "File", 4) == 0 &&
         menu_sub_items[1] > 0,
         "Menu over the folder list: the window's menu, with \"Folder >\" first");
  check (strncmp (menu_first[2], "File", 4) == 0,
         "Menu elsewhere in the window: the window's menu (File first)");
  printf ("# menus: message list \"%s\" [%s (%s...; %d), %s], folder list \"%s\" [%s (%s...; %d), %s], status bar \"%s\" [%s, %s]\n",
          menu_title[0], menu_first[0], menu_sub_first[0], menu_sub_items[0], menu_second[0],
          menu_title[1], menu_first[1], menu_sub_first[1], menu_sub_items[1], menu_second[1],
          menu_title[2], menu_first[2], menu_second[2]);
  if (closed_window && GTK_IS_WINDOW (closed_window))
    close_hides = !gtk_widget_get_visible (closed_window);
  check (theme_chosen == 1 && defaults_marker == 1,
         "the RISC OS icons are chosen at the first start (RISCOSDefaults written)");
  check (theme_icons == 1, "Get Mail's toolbar icon comes from the RISC OS icon theme");
  check (folder_from_desktop == 1,
         "the inbox's icon is the desktop's directory sprite with the theme's inbox emblem");
  check (promote_html == 1, "the HTML part of multipart/alternative is shown by default");
  check (strcmp (html_viewer, "text/html") == 0, "an HTML message is shown by the litehtml viewer");
  check (html_red >= 200 * 50 * 9 / 10, "the HTML is laid out: the 200x50 red block is drawn");
  check (strstr (server_log, "HTTP GET /moved/green.png") && strstr (server_log, "HTTP GET /green.png") &&
         html_green >= 600, "a remote image over http (through a redirect, sent chunked) is shown");
  check (strstr (server_log, "HTTP GET /yellow.png") && html_yellow >= 600,
         "a remote image over https (through AcornSSL) is shown");
  check (html_headers == 1, "the message's headers (From, Subject...) are shown above the HTML");
  check (html_white == 1, "the HTML's canvas is white where the page doesn't colour it");
  check (strncmp (menu_first[3], "File", 4) == 0, "Menu over the HTML away from a link: the window's menu (File first)");
  check (strncmp (menu_first[4], "Link", 4) == 0 && strncmp (menu_second[4], "File", 4) == 0 &&
         menu_sub_items[4] >= 2, "Menu on a link: the window's menu with \"Link >\" (Open, Copy) first");
  check (link_opened == 1, "a click on a link starts Iris (Iris$Dir) and gives it the link (URI_Dispatch)");
  check (about_acornssl == 1, "the About box names AcornSSL, not GnuTLS");
  check (about_logo == 1, "the About box shows the RISC OS port's logo");
  printf ("# HTML view: link at %d,%d; menus \"%s\" / \"%s\" (\"%s\", %d items); started \"%s\", dispatched \"%s\"\n",
          html_link_x, html_link_y, menu_first[3], menu_first[4], menu_sub_first[4], menu_sub_items[4],
          task_started, uri_dispatched);
  printf ("# headers: %s\n", html_header_text);
  printf ("# HTML: viewer %s, red %d, green %d, yellow %d pixels after %d looks\n",
          html_viewer, html_red, html_green, html_yellow, html_polls);
  check (close_hides == 1,
         "closing the main window hides it: Claws Mail stays on the icon bar");
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

/* HTML mail (the litehtml viewer): a multipart/alternative message is
   put in the inbox as message 2 and shown; its HTML part has a red block
   and two remote images, green over http (through a redirect) and yellow
   over https (AcornSSL), both from mailserver.py */

static gboolean
html_go (gpointer d)
{
  fake_claws_remote_content (1);
  fake_claws_show_message ("#mh/Mailbox/inbox", 2);
  return G_SOURCE_REMOVE;
}

/* pixels of colour c (0xBBGGRR) in the main window's viewer area */
static int
count_colour (GtkWidget *w, unsigned int c)
{
  int x, y, x0, y0, wx, wy, n = 0;
  GdkWindow *gw = gtk_widget_get_window (w);

  if (!gw)
    return 0;
  gtk_widget_translate_coordinates (w, gtk_widget_get_toplevel (w), 0, 0, &wx, &wy);
  gdk_window_get_origin (gtk_widget_get_window (gtk_widget_get_toplevel (w)), &x0, &y0);
  x0 += wx;
  y0 += wy;
  for (y = MAX (y0, 0); y < MIN (y0 + gtk_widget_get_allocated_height (w), 1024); y++)
    for (x = MAX (x0, 0); x < MIN (x0 + gtk_widget_get_allocated_width (w), 1280); x++)
      {
        unsigned int p = fake_screen_pixel (x, y);
        if (abs ((int) (p & 0xFF) - (int) (c & 0xFF)) < 12 &&
            abs ((int) ((p >> 8) & 0xFF) - (int) ((c >> 8) & 0xFF)) < 12 &&
            abs ((int) ((p >> 16) & 0xFF) - (int) ((c >> 16) & 0xFF)) < 12)
          n++;
      }
  return n;
}

static void
write_png (const char *dir, const char *name, unsigned int rgb)
{
  GdkPixbuf *pb = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 40, 40);
  char *p = g_build_filename (dir, name, NULL);
  gdk_pixbuf_fill (pb, rgb << 8 | 0xFF);
  gdk_pixbuf_save (pb, p, "png", NULL, NULL);
  g_object_unref (pb);
  g_free (p);
}

/* the RISC OS icons: the theme is chosen, and Get Mail's icon and the
   inbox's are the theme's PNGs */

static int
same_pixbuf (GdkPixbuf *a, const char *file)
{
  GdkPixbuf *b = gdk_pixbuf_new_from_file (file, NULL);
  int r = 0;

  if (a && b && gdk_pixbuf_get_width (a) == gdk_pixbuf_get_width (b) &&
      gdk_pixbuf_get_height (a) == gdk_pixbuf_get_height (b) &&
      gdk_pixbuf_get_n_channels (a) == gdk_pixbuf_get_n_channels (b))
    {
      int y, w = gdk_pixbuf_get_width (a) * gdk_pixbuf_get_n_channels (a);
      r = 1;
      for (y = 0; y < gdk_pixbuf_get_height (a); y++)
        if (memcmp (gdk_pixbuf_get_pixels (a) + y * gdk_pixbuf_get_rowstride (a),
                    gdk_pixbuf_get_pixels (b) + y * gdk_pixbuf_get_rowstride (b), w))
          r = 0;
    }
  if (b)
    g_object_unref (b);
  return r;
}

/* the inbox's icon: the desktop's directory sprite with the theme's
   inbox emblem bottom right (riscos-claws.c), not the theme's PNG */
static int
desktop_folder (GdkPixbuf *pb, const char *theme)
{
  char *f = g_build_filename (theme, "emblem_inbox.png", NULL), *png = g_build_filename (theme, "inbox_close.png", NULL);
  GdkPixbuf *e = gdk_pixbuf_new_from_file (f, NULL);
  int ok = pb && e && gdk_pixbuf_get_n_channels (pb) == 4 && !same_pixbuf (pb, png), x, y, opaque = 0;

  for (y = 0; ok && y < 9; y++)
    for (x = 0; ok && x < 9; x++)
      {
        const guchar *a = gdk_pixbuf_get_pixels (e) + y * gdk_pixbuf_get_rowstride (e) + x * 4;
        const guchar *b = gdk_pixbuf_get_pixels (pb) + (y + 7) * gdk_pixbuf_get_rowstride (pb) + (x + 7) * 4;
        if (a[3] == 255)
          {
            opaque++;
            if (a[0] != b[0] || a[1] != b[1] || a[2] != b[2])
              ok = 0;
          }
      }
  if (e)
    g_object_unref (e);
  g_free (f);
  g_free (png);
  return ok && opaque > 10;
}

static void
check_theme (void)
{
  const char *want = getenv ("CLAWS_RISCOS_THEME");
  char *f1, *marker;

  theme_chosen = want && fake_claws_theme () && strcmp (fake_claws_theme (), want) == 0;
  f1 = g_build_filename (want ? want : ".", "mail_receive.png", NULL);
  theme_icons = same_pixbuf (fake_claws_stock (0), f1);
  folder_from_desktop = want && desktop_folder (fake_claws_stock (1), want);
  marker = g_build_filename (home, ".claws-mail", "RISCOSDefaults", NULL);
  defaults_marker = g_file_test (marker, G_FILE_TEST_EXISTS);
  promote_html = fake_claws_promote_html ();
  g_free (f1);
  g_free (marker);
}

/* all of a widget tree's text views' text, joined */
static void
all_text (GtkWidget *w, GString *out)
{
  if (GTK_IS_TEXT_VIEW (w))
    {
      GtkTextIter a, b;
      GtkTextBuffer *buf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (w));
      char *t;
      gtk_text_buffer_get_bounds (buf, &a, &b);
      t = gtk_text_buffer_get_text (buf, &a, &b, FALSE);
      g_string_append (out, t);
      g_free (t);
    }
  if (GTK_IS_CONTAINER (w))
    {
      GList *l, *kids = gtk_container_get_children (GTK_CONTAINER (w));
      for (l = kids; l; l = l->next)
        all_text (l->data, out);
      g_list_free (kids);
    }
}

/* the HTML view once drawn: the headers above it, its white canvas, and
   where its link (the green picture) and an empty place are */
static void
html_look (GtkWidget *vw)
{
  GtkWidget *tv;
  int x, y, ox, oy, w, h, gx = 0, gy = 0, n = 0;

  if (!vw)
    return;
  tv = find_type (vw, GTK_TYPE_TEXT_VIEW);
  if (tv)
    {
      GString *t = g_string_new (NULL);
      all_text (tv, t);
      g_strlcpy (html_header_text, t->str, sizeof html_header_text);
      g_string_free (t, TRUE);
      html_headers = gtk_widget_is_visible (tv) && strstr (html_header_text, "From:") &&
                     strstr (html_header_text, "Dave") && strstr (html_header_text, "Subject:") &&
                     strstr (html_header_text, "HTML from the test");
    }
  else
    html_headers = 0;
  html_area = find_type (vw, GTK_TYPE_DRAWING_AREA);
  if (!html_area || !gtk_widget_get_window (html_area))
    return;
  gdk_window_get_origin (gtk_widget_get_window (html_area), &ox, &oy);
  w = gtk_widget_get_allocated_width (html_area);
  h = MIN (gtk_widget_get_allocated_height (html_area), 300);
  for (y = MAX (oy, 0); y < MIN (oy + h, 1024); y++)
    for (x = MAX (ox, 0); x < MIN (ox + w, 1280); x++)
      {
        unsigned int p = fake_screen_pixel (x, y);
        if ((p & 0xFF) < 12 && abs ((int) ((p >> 8) & 0xFF) - 0xC8) < 12 && ((p >> 16) & 0xFF) < 12)
          gx += x - ox, gy += y - oy, n++;
      }
  if (n)
    html_link_x = gx / n, html_link_y = gy / n;
  html_blank_x = w - 40;
  html_blank_y = 12;
  if (ox + html_blank_x < 1280 && oy + html_blank_y < 1024)
    html_white = (fake_screen_pixel (ox + html_blank_x, oy + html_blank_y) & 0xFFFFFF) == 0xFFFFFF;
}

static gboolean
about_go (gpointer d)
{
  fake_claws_about ();
  return G_SOURCE_REMOVE;
}

/* a GtkImage in the tree showing the PNG file */
static int
has_image (GtkWidget *w, const char *file)
{
  int r = 0;
  if (GTK_IS_IMAGE (w) && gtk_image_get_storage_type (GTK_IMAGE (w)) == GTK_IMAGE_PIXBUF)
    r = same_pixbuf (gtk_image_get_pixbuf (GTK_IMAGE (w)), file);
  if (!r && GTK_IS_CONTAINER (w))
    {
      GList *l, *kids = gtk_container_get_children (GTK_CONTAINER (w));
      for (l = kids; l && !r; l = l->next)
        r = has_image (l->data, file);
      g_list_free (kids);
    }
  return r;
}

static void
about_look (GtkWidget *ab)
{
  GString *t = g_string_new (NULL);
  const char *want = getenv ("CLAWS_RISCOS_THEME");
  char *logo = g_build_filename (want ? want : ".", "claws_mail_logo.png", NULL);

  all_text (ab, t);
  about_acornssl = strstr (t->str, " AcornSSL ") != NULL && strstr (t->str, "GnuTLS") == NULL;
  about_logo = has_image (ab, logo);
  g_string_free (t, TRUE);
  g_free (logo);
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
          pop_inbox_files = inbox_files;
          g_strlcpy (pop_inbox_text, inbox_text, sizeof pop_inbox_text);
          /* started, mail fetched: and no main window yet; Select on the icon */
          start_hidden = window_titled ("Claws Mail") == NULL;
          g_idle_add (iconbar_select, NULL);
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
            char *msg, d2[700];
            dump_shot ("sent");
            check_theme ();
            g_snprintf (d2, sizeof d2, "%s/Mail/inbox", home);
            msg = g_strdup_printf (
              "From: Dave <dave@example.com>\r\nTo: test@example.com\r\n"
              "Subject: HTML from the test\r\nDate: Wed, 07 Oct 2026 12:00:00 +0100\r\n"
              "Message-ID: <html1@example.com>\r\nMIME-Version: 1.0\r\n"
              "Content-Type: multipart/alternative; boundary=\"b1\"\r\n\r\n"
              "--b1\r\nContent-Type: text/plain; charset=us-ascii\r\n\r\nThe plain version.\r\n"
              "--b1\r\nContent-Type: text/html; charset=us-ascii\r\n\r\n"
              "<html><body><p>HTML mail on RISC OS</p>"
              "<div style=\"background:#d02020;width:200px;height:50px\"></div>"
              "<a href=\"https://example.com/news\"><img src=\"http://localhost:%s/moved/green.png\" width=\"40\" height=\"40\"></a>"
              "<img src=\"https://localhost:%s/yellow.png\" width=\"40\" height=\"40\">"
              "</body></html>\r\n--b1--\r\n", http_port, http_port);
            write_file (d2, "2", msg);
            g_free (msg);
            g_idle_add (html_go, NULL);
            stage = 20;
          }
      }
      break;
    case 20:    /* HTML mail shown by the litehtml viewer, remote images in */
      {
        GtkWidget *vw = NULL;
        const char *t = fake_claws_viewer (&vw);
        html_polls++;
        if (t)
          html_viewer = t;
        if (vw && gtk_widget_get_mapped (vw))
          {
            html_red = count_colour (vw, 0x2020D0);
            html_green = count_colour (vw, 0x00C800);
            html_yellow = count_colour (vw, 0x00E6E6);
          }
        if ((html_red > 0 && html_green > 0 && html_yellow > 0) || html_polls > 4000)
          {
            dump_shot ("html");
            html_look (vw);
            stage = 21;
          }
      }
      break;
    case 21:    /* Menu over the HTML away from a link: the window's menu */
      if (menu_step (3))
        stage = 22;
      break;
    case 22:    /* Menu on a link: the window's menu with "Link >" first */
      if (menu_step (4))
        {
          stage = 23;
          html_polls = 0;
          if (html_area && html_link_x >= 0)
            click_widget (html_area, html_link_x, html_link_y, 1);
        }
      break;
    case 23:    /* a click on the link: Iris is started, then gets it */
      if (uri_dispatches || ++html_polls > 3000)
        {
          link_opened = start_tasks == 1 && strcmp (task_started, "/<Iris$Dir>") == 0 &&
                        uri_dispatches == 1 && strcmp (uri_dispatched, "https://example.com/news") == 0;
          g_idle_add (about_go, NULL);
          html_polls = 0;
          stage = 24;
        }
      break;
    case 24:    /* the About box: AcornSSL, the RISC OS logo */
      {
        GtkWidget *ab = window_titled ("About");
        if (ab || ++html_polls > 2000)
          {
            if (ab)
              about_look (ab);
            dump_shot ("about");
            if (ab)
              gtk_widget_destroy (ab);
            stage = 10;
          }
      }
      break;
    case 10:    /* Menu over the message list, the folder list, elsewhere */
    case 11:
    case 12:
      if (menu_step (stage - 10))
        stage++;
      if (stage == 13)
        {
          stage = 4;
          /* look at the main window, then close it (its close icon) */
          if ((closed_window = window_titled ("Claws Mail")))
            {
              check_main_window (closed_window);
              gtk_window_close (GTK_WINDOW (closed_window));
            }
          g_idle_add (imap_go, NULL);
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
  if (polls > 150000)
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

  if (argc < 7)
    return 2;
  g_strlcpy (http_port, argv[6], sizeof http_port);
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
  g_snprintf (pop, sizeof pop, "%s/http", outdir);
  g_mkdir_with_parents (pop, 0755);
  write_png (pop, "green.png", 0x00C800);
  write_png (pop, "yellow.png", 0xE6E600);
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
  fake_swi_hook = swi_hook;
  g_setenv ("Iris$Dir", "/nonexistent/!Iris", TRUE);
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
