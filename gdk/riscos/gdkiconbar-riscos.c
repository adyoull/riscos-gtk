/*
 * gdkiconbar-riscos.c - riscos-gtk: a GTK program's icon on the icon bar.
 *
 * Done as riscos-gtk's other RISC OS programs do it (Reel, !FFmpeg's front
 * end, YTDL, NetStats): plain Wimp SWIs, no Toolbox.
 *
 *   - the icon: Wimp_CreateIcon on the icon bar's right side (window -1),
 *     0,0,68,68, flags &301A (sprite, centred, click), the program's
 *     sprite named in the icon ("!" + the program's name; "application"
 *     if the Wimp pool hasn't got it);
 *   - Mouse_Click on it (window -2): Select and Adjust show the program's
 *     window, Menu opens the icon bar menu (GTK's side,
 *     gtk/gtkiconbar-riscos.c): Info >, Choices... (if the program has
 *     any), Quit, 64 OS units left of the click, its top 96 + 44 a line;
 *   - Info: the standard "About this program" window, laid out as the
 *     ProgInfo template (and Reel's proginfo.h): labels "Name:" ...
 *     right-aligned on grey, each value in a grey display field with a
 *     slabbed-in border (R2); the Info item's submenu, so the Wimp opens
 *     and closes it with the menu;
 *   - files: Message_DataOpen (a double-click in the Filer) for the types
 *     the program claims, and Message_DataLoad to the icon (also icon -1,
 *     from a second copy of the program), acknowledged with DataLoadAck;
 *     Message_DataSave to the icon (another program's Save box) answered
 *     with DataSaveAck to <Wimp$Scrap>;
 *   - Message_PreQuit (the Task Manager's Quit, or a desktop shutdown):
 *     with unsaved work, a question box (Wimp_ReportError, Cancel and
 *     Discard); Cancel objects (the message is acknowledged), Discard lets
 *     the program go (Message_Quit follows), as !FFmpeg's Convert does;
 *   - one copy: a second copy started (a file double-clicked, or the
 *     application run again) hands its file to the first with
 *     Message_DataLoad (window -2, icon -1) and closes down, as Reel does.
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
#ifndef Wimp_ReportError
#define Wimp_ReportError  0x400DF
#endif
#ifndef Wimp_CloseDown
#define Wimp_CloseDown    0x400DD
#endif
#define TaskManager_EnumerateTasks 0x42681

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
  char sprite[13];
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
  char *start_file;               /* a second copy hands this to the first */
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
send_message (int reason, int *block, int dest, int icon)
{
  _kernel_swi_regs r;

  r.r[0] = reason;
  r.r[1] = (int) block;
  r.r[2] = dest;
  r.r[3] = icon;
  if (_kernel_swi (Wimp_SendMessage, &r, &r) != NULL)
    return 0;
  return block[2];
}

static const char *
app_name (void)
{
  const char *name = bar.info[0] ? bar.info[0] : g_get_application_name ();
  return name ? name : "GTK";
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
gdk_riscos_iconbar_set_start_file (const char *riscos_path)
{
  g_free (bar.start_file);
  bar.start_file = g_strdup (riscos_path);
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

static gboolean
wanted (void)
{
  const char *env = g_getenv ("RISCOS_GTK_ICONBAR");
  return !bar.disabled && !(env && strcmp (env, "0") == 0);
}

/* A question box, as riscos-gtk's other programs ask (Wimp_ReportError:
   Cancel, our button, new style, a question, the program's sprite). TRUE
   if the button was chosen. */
gboolean
gdk_riscos_ask (const char *message, const char *button)
{
  _kernel_swi_regs r;
  struct { int errnum; char text[252]; } err;
  char title[64], butt[32];

  err.errnum = 0;
  gdk_riscos_utf8_to_latin1 (message, err.text, sizeof err.text);
  gdk_riscos_utf8_to_latin1 (app_name (), title, sizeof title);
  gdk_riscos_utf8_to_latin1 (button, butt, sizeof butt);
  r.r[0] = (int) &err;
  r.r[1] = 2 | (1 << 8) | (4 << 9);      /* Cancel; new style; a question */
  r.r[2] = (int) title;
  r.r[3] = (int) (bar.sprite[0] ? bar.sprite : "application");
  r.r[4] = 1;                            /* the Wimp's sprite pool */
  r.r[5] = (int) butt;
  if (_kernel_swi (Wimp_ReportError, &r, &r) != NULL)
    return FALSE;
  return r.r[1] == 3;
}

/* ---- one copy ------------------------------------------------------------ */

/* Another task with our name: its handle, or 0 (TaskManager_EnumerateTasks;
   task names end with a control character) */
static int
already_running (int task, const char *name)
{
  int buf[16 * 4];
  _kernel_swi_regs r;
  size_t n = strlen (name);

  r.r[0] = 0;
  do
    {
      int *p;
      r.r[1] = (int) buf;
      r.r[2] = sizeof buf;
      if (_kernel_swi (TaskManager_EnumerateTasks, &r, &r) != NULL)
        return 0;
      for (p = buf; (char *) p < (char *) r.r[1]; p += 4)
        if (p[0] != task)
          {
            const char *other = (const char *) p[1];
            if (strncmp (other, name, n) == 0 && (unsigned char) other[n] < 32)
              return p[0];
          }
    }
  while (r.r[0] >= 0);
  return 0;
}

/* Just after Wimp_Initialise (gdkriscos-server.c): a second copy gives its
   file to the first and goes */
void
_gdk_riscos_iconbar_startup (int task, const char *task_name)
{
  int other, block[64];
  _kernel_swi_regs r;

  if (!wanted () || (other = already_running (task, task_name)) == 0)
    return;
  if (bar.start_file)
    {
      memset (block, 0, sizeof block);
      block[0] = (44 + strlen (bar.start_file) + 1 + 3) & ~3;
      block[4] = MESSAGE_DATALOAD;
      block[5] = ICONBAR_WINDOW;
      block[6] = -1;
      block[9] = 0;
      block[10] = bar.types && bar.types->len ? g_array_index (bar.types, int, 0) : 0xFFF;
      g_strlcpy ((char *) &block[11], bar.start_file, 200);
      send_message (USER_MESSAGE, block, other, 0);
    }
  if (debug ())
    g_printerr ("riscos-gtk: %s is already running: %s\n", task_name,
                bar.start_file ? "file handed to it" : "nothing to do");
  r.r[0] = task;
  r.r[1] = 0x4B534154;
  _kernel_swi (Wimp_CloseDown, &r, &r);
  exit (0);
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
  char want[13];
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
  g_strlcpy (bar.sprite, sprite_exists (want) ? want : "application", sizeof bar.sprite);
}

gboolean
gdk_riscos_iconbar_create (GdkDisplay *display, GdkRiscosIconbarFunc func, gpointer data)
{
  struct { int w, x0, y0, x1, y1, flags; char name[12]; } b;
  _kernel_swi_regs r;

  if (bar.icon >= 0)
    return TRUE;
  if (!wanted () || server () == NULL)
    return FALSE;
  choose_sprite ();
  memset (&b, 0, sizeof b);
  b.w = -1;                       /* the icon bar's right side */
  b.x1 = 68;
  b.y1 = 68;
  b.flags = 0x301A;               /* sprite, centred, click */
  memcpy (b.name, bar.sprite, strlen (bar.sprite) < 12 ? strlen (bar.sprite) + 1 : 12);
  r.r[0] = 0;
  r.r[1] = (int) &b;
  if (_kernel_swi (Wimp_CreateIcon, &r, &r) != NULL)
    {
      g_printerr ("riscos-gtk: couldn't put an icon on the icon bar\n");
      return FALSE;
    }
  bar.icon = r.r[0];
  bar.func = func;
  bar.func_data = data;
  if (debug ())
    g_printerr ("riscos-gtk: icon bar icon %d, sprite %s\n", bar.icon, bar.sprite);
  return TRUE;
}

void
gdk_riscos_iconbar_menu_position (gint click_x, gint n_items, gint *root_x, gint *root_y)
{
  int x = click_x - 64, y = 96 + 44 * n_items, w, h, ow, oh;
  GdkRiscosServer *s = server ();

  *root_x = x;
  *root_y = y;
  if (s == NULL)
    return;
  /* (the inverse of gdk_riscos_display_open_menu's, so the Wimp gets
     exactly these OS units) */
  _gdk_riscos_server_get_screen_size (s, &w, &h);
  _gdk_riscos_server_screen_os_size (s, &ow, &oh);
  *root_x = x / (ow / w);
  *root_y = h - y / (oh / h);
}

/* ---- Info: About this program ---------------------------------------- */

#define PROGINFO_ROWS 4

gint
gdk_riscos_iconbar_info_window (void)
{
  static const char *labels[PROGINFO_ROWS] = { "Name:", "Purpose:", "Author:", "Version:" };
  static char text[PROGINFO_ROWS][64];
  static char title[] = "About this program";
  static char valid[] = "R2";              /* a display field's slabbed-in border */
  struct {
    int vis[4];
    int sx, sy, behind, flags;
    unsigned char tfg, tbg, wfg, wbg, sofg, sibg, tfocus, xflags;
    int ext[4];
    int tflags, wbutton, sprites;
    short minw, minh;
    int title[3];
    int nicons;
    struct { int box[4]; unsigned flags; int data[3]; } icon[PROGINFO_ROWS * 2];
  } w;
  _kernel_swi_regs r;
  int value_w = 450, width, height, i;

  if (bar.info_window)
    return bar.info_window;
  for (i = 0; i < PROGINFO_ROWS; i++)
    {
      const char *v = i == 0 ? app_name () : bar.info[i];
      int tw;
      gdk_riscos_utf8_to_latin1 (v ? v : "", text[i], sizeof text[i]);
      tw = (int) strlen (text[i]) * 16 + 32;   /* the desktop font: about 16 a letter */
      if (tw > value_w)
        value_w = tw;
    }
  width = 152 + value_w + 12;
  height = 8 + PROGINFO_ROWS * 52;

  memset (&w, 0, sizeof w);
  w.vis[0] = 400;
  w.vis[1] = 400;
  w.vis[2] = 400 + width;
  w.vis[3] = 400 + height;
  w.behind = -1;
  w.flags = (int) 0x84000012u;      /* new format, title bar, moveable, auto-redraw */
  w.tfg = 7; w.tbg = 2; w.wfg = 7; w.wbg = 1; w.sofg = 3; w.sibg = 1; w.tfocus = 12;
  w.ext[1] = -height;
  w.ext[2] = width;
  w.tflags = 0x13D;                 /* text, border, centred, filled, indirected */
  w.wbutton = 0;                    /* clicks ignored */
  w.sprites = 1;
  w.title[0] = (int) title;
  w.title[1] = -1;
  w.title[2] = sizeof title;
  w.nicons = PROGINFO_ROWS * 2;
  for (i = 0; i < PROGINFO_ROWS; i++)
    {
      int top = -8 - i * 52;
      /* the label: right-aligned text on the grey */
      w.icon[2 * i].box[0] = 8;
      w.icon[2 * i].box[1] = top - 48;
      w.icon[2 * i].box[2] = 148;
      w.icon[2 * i].box[3] = top;
      w.icon[2 * i].flags = 0x17000211u;
      strncpy ((char *) w.icon[2 * i].data, labels[i], 12);
      /* the value: a display field (grey, slabbed in) */
      w.icon[2 * i + 1].box[0] = 152;
      w.icon[2 * i + 1].box[1] = top - 48;
      w.icon[2 * i + 1].box[2] = 152 + value_w;
      w.icon[2 * i + 1].box[3] = top;
      w.icon[2 * i + 1].flags = 0x1700013Du;
      w.icon[2 * i + 1].data[0] = (int) text[i];
      w.icon[2 * i + 1].data[1] = (int) valid;
      w.icon[2 * i + 1].data[2] = sizeof text[i];
    }
  r.r[1] = (int) &w;
  if (_kernel_swi (Wimp_CreateWindow, &r, &r) == NULL)
    bar.info_window = r.r[0];
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

/* Acknowledge their DataLoad or DataOpen (Message_DataLoadAck) first, so
   the sender doesn't delete a scrap file */
static void
load_ack (int *block)
{
  int peer = block[1];

  block[3] = block[2];
  block[4] = MESSAGE_DATALOADACK;
  send_message (USER_MESSAGE, block, peer, 0);
}

static gboolean
to_icon (int *block)
{
  return block[5] == ICONBAR_WINDOW && (block[6] == bar.icon || block[6] == -1);
}

static void
report (const char *text)
{
  struct { int errnum; char text[252]; } err;
  _kernel_swi_regs r;
  char title[64];

  err.errnum = 0;
  gdk_riscos_utf8_to_latin1 (text, err.text, sizeof err.text);
  gdk_riscos_utf8_to_latin1 (app_name (), title, sizeof title);
  r.r[0] = (int) &err;
  r.r[1] = 1 | 16;                       /* OK, no "Error from" */
  r.r[2] = (int) title;
  _kernel_swi (Wimp_ReportError, &r, &r);
}

gboolean
_gdk_riscos_iconbar_message (int reason, int *block)
{
  int task;
  char path[256];
  int type;

  if (bar.icon < 0 || reason == USER_MESSAGE_ACKNOWLEDGE)
    return FALSE;
  task = server () ? _gdk_riscos_server_get_task (server ()) : 0;
  switch (block[4])
    {
    case MESSAGE_DATAOPEN:
      if (block[1] != task && claimed (block[10]))
        {
          type = block[10];
          g_strlcpy (path, (const char *) &block[11], sizeof path);
          load_ack (block);
          open_file (path, type, FALSE);
          return TRUE;
        }
      return FALSE;

    case MESSAGE_DATALOAD:
      if (block[3] != 0 && block[3] == bar.scrap_ref)
        {
          type = block[10];
          g_strlcpy (path, (const char *) &block[11], sizeof path);
          bar.scrap_ref = 0;
          load_ack (block);
          open_file (path, type, TRUE);
          return TRUE;
        }
      if (block[3] == 0 && to_icon (block))
        {
          type = block[10];
          g_strlcpy (path, (const char *) &block[11], sizeof path);
          load_ack (block);
          if (type == 0x1000 || type == 0x2000)
            {
              char *msg = g_strdup_printf ("That's a directory: drop a file on the %s icon.", app_name ());
              report (msg);
              g_free (msg);
            }
          else
            open_file (path, type, FALSE);
          return TRUE;
        }
      return FALSE;

    case MESSAGE_DATASAVE:
      if (block[3] == 0 && to_icon (block) && block[1] != task)
        {
          int peer = block[1];
          block[3] = block[2];
          block[4] = MESSAGE_DATASAVEACK;
          block[9] = -1;              /* not safe: it's a scrap file */
          strcpy ((char *) &block[11], SCRAP);
          block[0] = (44 + sizeof SCRAP + 3) & ~3;
          bar.scrap_ref = send_message (USER_MESSAGE, block, peer, 0);
          return TRUE;
        }
      return FALSE;

    case MESSAGE_PREQUIT:
      {
        gpointer data = bar.handlers_data;
        char *msg;
        gboolean go;

        if (!(bar.have_handlers && bar.handlers.unsaved && bar.handlers.unsaved (data)))
          return TRUE;                /* nothing to lose: Message_Quit follows */
        msg = g_strdup_printf ("%s has unsaved changes. Discard them and quit?", app_name ());
        go = gdk_riscos_ask (msg, "Discard");
        g_free (msg);
        if (!go)
          {
            /* Cancel: object (acknowledge the message) */
            int peer = block[1];
            block[3] = block[2];
            send_message (USER_MESSAGE_ACKNOWLEDGE, block, peer, 0);
          }
        if (debug ())
          g_printerr ("riscos-gtk: PreQuit with unsaved changes: %s\n", go ? "discard" : "cancelled");
        return TRUE;
      }

    default:
      return FALSE;
    }
}
