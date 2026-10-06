/*
 * gdkiconbar-riscos.c - riscos-gtk: a GTK program's icon on the icon bar.
 *
 * The Wimp side; GTK's side (the menu, and what Select, Open..., Close,
 * Choices... and Quit do) is gtk/gtkiconbar-riscos.c.
 *
 *   - the icon: Wimp_CreateIcon on the icon bar (window -1, the right
 *     side, where applications go), the program's sprite from the Wimp
 *     pool, 68 x 68 OS units (PRMs, The Icon Bar);
 *   - clicks on it: Mouse_Click with window -2 and our icon handle;
 *   - files: Message_DataOpen (a file double-clicked in the Filer) for the
 *     types the program claims, acknowledged with Message_DataLoadAck;
 *     Message_DataLoad from the Filer to the icon (acknowledged the same
 *     way); Message_DataSave from another program to the icon, answered
 *     with Message_DataSaveAck to <Wimp$Scrap> (then its DataLoad);
 *   - Message_PreQuit: if the program has unsaved work it objects (the
 *     message is acknowledged) and asks the user; after a desktop shutdown
 *     (flag bit 0 clear) it carries the shutdown on when it exits, with a
 *     Ctrl-Shift-F12 (Wimp_ProcessKey &1FC), as the PRMs say;
 *   - the Info box: a Wimp window (Name, Purpose, Author, Version) given
 *     as the Info item's submenu, so the Wimp opens it.
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as the GTK code it
 * plugs into.
 */
#include "config.h"

#include "gdkprivate-riscos.h"
#include "gdkdisplay-riscos.h"
#include "gdkriscos-server.h"
#include "gdkriscoswimp.h"
#include "gdkriscosmenu.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <kernel.h>
#include <swis.h>

#ifndef Wimp_CreateIcon
#define Wimp_CreateIcon   0x400C2
#endif
#ifndef Wimp_CreateWindow
#define Wimp_CreateWindow 0x400C1
#endif
#ifndef Wimp_SendMessage
#define Wimp_SendMessage  0x400E7
#endif
#ifndef Wimp_SpriteOp
#define Wimp_SpriteOp     0x400E9
#endif
#ifndef Wimp_ProcessKey
#define Wimp_ProcessKey   0x400DC
#endif

#define USER_MESSAGE             17
#define USER_MESSAGE_ACKNOWLEDGE 19
#define MESSAGE_DATASAVE     1
#define MESSAGE_DATASAVEACK  2
#define MESSAGE_DATALOAD     3
#define MESSAGE_DATALOADACK  4
#define MESSAGE_DATAOPEN     5
#define MESSAGE_PREQUIT      8

#define ICONBAR_WINDOW (-2)       /* Mouse_Click and messages for the icon bar */
#define SCRAP "<Wimp$Scrap>"

static struct {
  gboolean disabled;
  int icon;                       /* our icon's handle (-1: none) */
  char sprite[16];
  char *sprite_wanted;
  GdkRiscosIconbarFunc func;
  gpointer func_data;
  GdkRiscosIconbarHandlers handlers;
  gboolean have_handlers;
  gpointer handlers_data;
  GArray *types;                  /* claimed for Message_DataOpen */
  char *info[4];                  /* Name, Purpose, Author, Version (UTF-8) */
  int info_window;
  int scrap_ref;                  /* our DataSaveAck's my_ref */
  int scrap_type;
  gboolean resume_shutdown;
} bar = { FALSE, -1 };

static gboolean
debug (void)
{
  return g_getenv ("RISCOS_GTK_DEBUG") != NULL;
}

static GdkRiscosServer *
server (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  return display ? GDK_RISCOS_DISPLAY (display)->server : NULL;
}

static int
send_message (int reason, int *block, int dest)
{
  _kernel_swi_regs r;

  r.r[0] = reason;
  r.r[1] = (int) block;
  r.r[2] = dest;
  r.r[3] = 0;
  if (_kernel_swi (Wimp_SendMessage, &r, &r) != NULL)
    return 0;
  return block[2];
}

/* ---- for programs ---------------------------------------------------- */

void
gdk_riscos_iconbar_set_handlers (const GdkRiscosIconbarHandlers *handlers, gpointer data)
{
  if (handlers)
    bar.handlers = *handlers;
  else
    memset (&bar.handlers, 0, sizeof bar.handlers);
  bar.have_handlers = handlers != NULL;
  bar.handlers_data = data;
}

const GdkRiscosIconbarHandlers *
gdk_riscos_iconbar_get_handlers (gpointer *data)
{
  if (data)
    *data = bar.handlers_data;
  return bar.have_handlers ? &bar.handlers : NULL;
}

void
gdk_riscos_iconbar_set_info (const char *name, const char *purpose,
                             const char *author, const char *version)
{
  const char *v[4] = { name, purpose, author, version };
  int i;

  for (i = 0; i < 4; i++)
    if (v[i])
      {
        g_free (bar.info[i]);
        bar.info[i] = g_strdup (v[i]);
      }
}

void
gdk_riscos_iconbar_set_sprite (const char *sprite)
{
  g_free (bar.sprite_wanted);
  bar.sprite_wanted = g_strdup (sprite);
}

void
gdk_riscos_iconbar_add_filetype (int filetype)
{
  if (bar.types == NULL)
    bar.types = g_array_new (FALSE, FALSE, sizeof (int));
  g_array_append_val (bar.types, filetype);
}

void
gdk_riscos_iconbar_disable (void)
{
  bar.disabled = TRUE;
}

gboolean
gdk_riscos_iconbar_enabled (void)
{
  return bar.icon >= 0;
}

/* ---- the icon ---------------------------------------------------------- */

static gboolean
sprite_exists (const char *name)
{
  _kernel_swi_regs r;

  r.r[0] = 24;                    /* select sprite: an error if there's none */
  r.r[2] = (int) name;
  return _kernel_swi (Wimp_SpriteOp, &r, &r) == NULL;
}

static void
choose_sprite (void)
{
  const char *prg = g_get_prgname ();
  char want[16];
  int i;

  if (bar.sprite_wanted)
    g_strlcpy (want, bar.sprite_wanted, sizeof want);
  else
    {
      want[0] = '!';
      for (i = 0; prg && prg[i] && i < 11; i++)
        want[i + 1] = tolower ((unsigned char) prg[i]);
      want[i + 1] = 0;
    }
  want[12] = 0;
  g_strlcpy (bar.sprite, sprite_exists (want) ? want : "application", sizeof bar.sprite);
}

static void
resume_at_exit (void)
{
  _kernel_swi_regs r;

  if (!bar.resume_shutdown)
    return;
  bar.resume_shutdown = FALSE;
  r.r[0] = 0x1FC;                 /* Ctrl-Shift-F12: shut down again */
  _kernel_swi (Wimp_ProcessKey, &r, &r);
}

gboolean
gdk_riscos_iconbar_create (GdkDisplay *display, GdkRiscosIconbarFunc func, gpointer data)
{
  int block[9];
  _kernel_swi_regs r;
  const char *env = g_getenv ("RISCOS_GTK_ICONBAR");

  if (bar.icon >= 0)
    return TRUE;
  if (bar.disabled || (env && strcmp (env, "0") == 0) || server () == NULL)
    return FALSE;
  choose_sprite ();
  block[0] = -1;                  /* the icon bar's right side */
  block[1] = 0;
  block[2] = 0;
  block[3] = 68;
  block[4] = 68;
  block[5] = 0x310A;              /* sprite, centred, indirected; click button type */
  block[6] = (int) bar.sprite;
  block[7] = 1;                   /* the Wimp's sprite pool */
  block[8] = strlen (bar.sprite) + 1;
  r.r[0] = 0;
  r.r[1] = (int) block;
  if (_kernel_swi (Wimp_CreateIcon, &r, &r) != NULL)
    {
      g_printerr ("riscos-gtk: couldn't put an icon on the icon bar\n");
      return FALSE;
    }
  bar.icon = r.r[0];
  bar.func = func;
  bar.func_data = data;
  atexit (resume_at_exit);
  if (debug ())
    g_printerr ("riscos-gtk: icon bar icon %d, sprite %s\n", bar.icon, bar.sprite);
  return TRUE;
}

void
gdk_riscos_iconbar_menu_position (gint click_x, gint menu_height, gint *root_x, gint *root_y)
{
  int x = click_x - 64, y = 96 + menu_height;
  GdkRiscosServer *s = server ();

  if (s)
    _gdk_riscos_server_os_to_px (s, &x, &y);
  *root_x = x;
  *root_y = y;
}

void
gdk_riscos_iconbar_resume_shutdown_at_exit (gboolean resume)
{
  bar.resume_shutdown = resume;
}

/* ---- the Info box ------------------------------------------------------ */

#define INFO_MIN_W 680
#define INFO_ROW  52
#define INFO_TOP  16
#define INFO_LABEL_W 168

gint
gdk_riscos_iconbar_info_window (void)
{
  static const char *labels[4] = { "Name", "Purpose", "Author", "Version" };
  static char text[8][128];
  static char title[] = "About this program";
  static char valid[] = "R2";
  int *block, i, h = INFO_TOP * 2 + 4 * INFO_ROW - 8, info_w = INFO_MIN_W;
  _kernel_swi_regs r;

  if (bar.info_window)
    return bar.info_window;
  for (i = 0; i < 4; i++)
    {
      const char *v = bar.info[i];
      if (i == 0 && v == NULL)
        v = g_get_application_name ();
      g_strlcpy (text[2 * i], labels[i], sizeof text[0]);
      gdk_riscos_utf8_to_latin1 (v ? v : "", text[2 * i + 1], sizeof text[0]);
      /* wide enough for the longest */
      info_w = MAX (info_w, INFO_LABEL_W + 16 + gdk_riscos_text_width (text[2 * i + 1]) + 40);
    }
  block = g_malloc0 (88 + 8 * 32);
  block[0] = 0;
  block[1] = 0;
  block[2] = info_w;
  block[3] = h;
  block[6] = -1;
  block[7] = (int) 0x84000012u;   /* new format, title bar, moveable, auto-redraw */
  ((unsigned char *) block)[32] = 7;    /* title foreground */
  ((unsigned char *) block)[33] = 2;    /* title background */
  ((unsigned char *) block)[34] = 7;    /* work area foreground */
  ((unsigned char *) block)[35] = 1;    /* work area background: grey */
  ((unsigned char *) block)[36] = 3;
  ((unsigned char *) block)[37] = 1;
  ((unsigned char *) block)[38] = 12;
  block[10] = 0;
  block[11] = -h;
  block[12] = info_w;
  block[13] = 0;
  block[14] = 0x13D;              /* title: text, border, centred, filled, indirected */
  block[15] = 0;                  /* clicks ignored */
  block[16] = 1;
  block[17] = 0;
  block[18] = (int) title;
  block[19] = -1;
  block[20] = sizeof title;
  block[21] = 8;
  for (i = 0; i < 4; i++)
    {
      int *lab = block + 22 + 8 * (2 * i), *val = lab + 8;
      int y1 = -INFO_TOP - INFO_ROW * i, y0 = y1 - 44;
      lab[0] = 8;
      lab[1] = y0;
      lab[2] = INFO_LABEL_W;
      lab[3] = y1;
      lab[4] = 0x17000311;        /* text, vertically centred, right aligned, indirected */
      lab[5] = (int) text[2 * i];
      lab[6] = -1;
      lab[7] = strlen (text[2 * i]) + 1;
      val[0] = INFO_LABEL_W + 16;
      val[1] = y0;
      val[2] = info_w - 12;
      val[3] = y1;
      val[4] = 0x07000135;        /* text, border, centred vertically, filled white, indirected */
      val[5] = (int) text[2 * i + 1];
      val[6] = (int) valid;       /* R2: a slab in */
      val[7] = strlen (text[2 * i + 1]) + 1;
    }
  r.r[1] = (int) block;
  if (_kernel_swi (Wimp_CreateWindow, &r, &r) == NULL)
    bar.info_window = r.r[0];
  g_free (block);
  return bar.info_window;
}

/* ---- from the Wimp poll (gdkriscos-server.c) ----------------------------- */

static void
deliver (GdkRiscosIconbarEvent *ev)
{
  if (bar.func)
    bar.func (ev, bar.func_data);
}

gboolean
_gdk_riscos_iconbar_click (int *block)
{
  GdkRiscosIconbarEvent ev;

  if (bar.icon < 0 || block[3] != ICONBAR_WINDOW || block[4] != bar.icon)
    return FALSE;
  memset (&ev, 0, sizeof ev);
  ev.x = block[0];
  ev.y = block[1];
  if (block[2] & 2)
    ev.type = GDK_RISCOS_ICONBAR_MENU;
  else if (block[2] & 4)
    ev.type = GDK_RISCOS_ICONBAR_SELECT;
  else if (block[2] & 1)
    ev.type = GDK_RISCOS_ICONBAR_ADJUST;
  else
    return TRUE;
  deliver (&ev);
  return TRUE;
}

static gboolean
claimed (int type)
{
  guint i;

  for (i = 0; bar.types && i < bar.types->len; i++)
    if (g_array_index (bar.types, int, i) == type)
      return TRUE;
  return FALSE;
}

static void
open_file (const char *riscos_path, int type, gboolean scrap)
{
  GdkRiscosIconbarEvent ev;
  char *unix_path = gdk_riscos_path_to_unix (riscos_path);

  memset (&ev, 0, sizeof ev);
  ev.type = GDK_RISCOS_ICONBAR_OPEN_FILE;
  ev.path = unix_path;
  ev.filetype = type;
  ev.scrap = scrap;
  if (debug ())
    g_printerr ("riscos-gtk: icon bar: open %s (&%03X)%s\n", riscos_path, type & 0xFFF,
                scrap ? ", scrap" : "");
  deliver (&ev);
  g_free (unix_path);
}

/* Acknowledge their DataLoad or DataOpen (Message_DataLoadAck) */
static void
load_ack (int *block)
{
  int peer = block[1];

  block[3] = block[2];
  block[4] = MESSAGE_DATALOADACK;
  send_message (USER_MESSAGE, block, peer);
}

gboolean
_gdk_riscos_iconbar_message (int reason, int *block)
{
  int task;

  if (bar.icon < 0 || reason == USER_MESSAGE_ACKNOWLEDGE)
    return FALSE;
  task = server () ? _gdk_riscos_server_get_task (server ()) : 0;
  switch (block[4])
    {
    case MESSAGE_DATAOPEN:
      if (block[1] != task && claimed (block[10]))
        {
          char path[256];
          int type = block[10];
          g_strlcpy (path, (const char *) &block[11], sizeof path);
          load_ack (block);
          open_file (path, type, FALSE);
          return TRUE;
        }
      return FALSE;

    case MESSAGE_DATALOAD:
      if (block[3] != 0 && block[3] == bar.scrap_ref)
        {
          char path[256];
          int type = block[10];
          g_strlcpy (path, (const char *) &block[11], sizeof path);
          bar.scrap_ref = 0;
          load_ack (block);
          open_file (path, type, TRUE);
          return TRUE;
        }
      if (block[3] == 0 && block[5] == ICONBAR_WINDOW && block[6] == bar.icon)
        {
          char path[256];
          int type = block[10];
          g_strlcpy (path, (const char *) &block[11], sizeof path);
          load_ack (block);
          open_file (path, type, FALSE);
          return TRUE;
        }
      return FALSE;

    case MESSAGE_DATASAVE:
      if (block[3] == 0 && block[5] == ICONBAR_WINDOW && block[6] == bar.icon && block[1] != task)
        {
          int peer = block[1];
          block[3] = block[2];
          block[4] = MESSAGE_DATASAVEACK;
          block[9] = -1;              /* not safe: it's going to a program */
          strcpy ((char *) &block[11], SCRAP);
          block[0] = (44 + sizeof SCRAP + 3) & ~3;
          bar.scrap_type = block[10];
          bar.scrap_ref = send_message (USER_MESSAGE, block, peer);
          return TRUE;
        }
      return FALSE;

    case MESSAGE_PREQUIT:
      {
        GdkRiscosIconbarEvent ev;
        int flags = block[0] >= 24 ? block[5] : 0;

        memset (&ev, 0, sizeof ev);
        ev.type = GDK_RISCOS_ICONBAR_PREQUIT;
        ev.shutdown = (flags & 1) == 0;
        deliver (&ev);
        if (ev.object)
          {
            /* don't quit yet: acknowledge it */
            int peer = block[1];
            block[3] = block[2];
            send_message (USER_MESSAGE_ACKNOWLEDGE, block, peer);
            if (debug ())
              g_printerr ("riscos-gtk: PreQuit: unsaved work, objected%s\n",
                          ev.shutdown ? " (desktop shutdown)" : "");
          }
        return TRUE;
      }

    default:
      return FALSE;
    }
}
