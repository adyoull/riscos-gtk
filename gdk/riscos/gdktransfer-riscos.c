/*
 * gdktransfer-riscos.c - the clipboard, and data dragged between tasks, on
 * the RISC OS Wimp's data transfer protocol (riscos-gtk's GDK backend).
 *
 * GTK works with X-style selections (gdkselection-riscos.c). This file
 * joins them to the Wimp:
 *
 *   - Copy/Cut: GTK takes the CLIPBOARD selection; we broadcast
 *     Message_ClaimEntity (flags 4) so other tasks drop theirs. When
 *     another task claims it, GTK's owner gets a selection clear.
 *   - Paste with the clipboard in another task: we broadcast
 *     Message_DataRequest (flags 4, text &FFF) as User_Message_Recorded;
 *     the owner replies with Message_DataSave; we fetch the data with
 *     Message_RAMFetch / RAMTransmit, or, if the first RAMFetch comes back
 *     unanswered, Message_DataSaveAck with <Wimp$Scrap> and DataLoad /
 *     DataLoadAck. Text is Latin-1 on RISC OS and UTF-8 in GTK.
 *   - Another task pastes our clipboard: its Message_DataRequest gets the
 *     text from GTK's owner (a selection request) and replies with
 *     Message_DataSave, then answers RAMFetch (Wimp_TransferBlock,
 *     RAMTransmit) or DataSaveAck (OS_File 10, DataLoad).
 *   - A drag from a GTK widget dropped on another task's window (or the
 *     icon bar, or a Filer window) saves the text there the same way,
 *     with Message_DataSave to the window under the pointer
 *     (gdkdnd-riscos.c). The Wimp draws a dashed box while dragging
 *     (Wimp_DragBox type 5).
 *   - Text saved into one of our windows by another task (DataSave), or a
 *     text file dropped from the Filer (DataLoad, type &FFF), is fetched
 *     and given to GTK as a drop at that point (gdkdnd-riscos.c).
 *
 * Messages, SWIs and blocks as in the RISC OS PRMs: Message_DataSave (1),
 * DataSaveAck (2), DataLoad (3), DataLoadAck (4), RAMFetch (6),
 * RAMTransmit (7), ClaimEntity (&F), DataRequest (&10); Wimp_SendMessage
 * (17 User_Message, 18 Recorded; 19 = returned unanswered),
 * Wimp_TransferBlock, Wimp_GetPointerInfo, Wimp_DragBox, OS_File 5/6/10/18/255; DragASprite_Start/Stop; OS_Byte 161; Wimp_ReportError.
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as the GTK code it
 * plugs into.
 */
#include "config.h"

#include "gdkprivate-riscos.h"
#include "gdkdisplay-riscos.h"
#include "gdkriscos-server.h"
#include "gdkriscoswimp.h"

#include <string.h>
#include <kernel.h>
#include <swis.h>

#define MESSAGE_DATASAVE     1
#define MESSAGE_DATASAVEACK  2
#define MESSAGE_DATALOAD     3
#define MESSAGE_DATALOADACK  4
#define MESSAGE_RAMFETCH     6
#define MESSAGE_RAMTRANSMIT  7
#define MESSAGE_CLAIMENTITY  0xF
#define MESSAGE_DATAREQUEST  0x10

#define USER_MESSAGE             17
#define USER_MESSAGE_RECORDED    18
#define USER_MESSAGE_ACKNOWLEDGE 19

#define FILETYPE_TEXT 0xFFF
#define SCRAP "<Wimp$Scrap>"

#ifndef Wimp_SendMessage
#define Wimp_SendMessage 0x400E7
#endif
#ifndef Wimp_TransferBlock
#define Wimp_TransferBlock 0x400F1
#endif
#ifndef Wimp_DragBox
#define Wimp_DragBox 0x400D0
#endif
#ifndef Wimp_GetPointerInfo
#define Wimp_GetPointerInfo 0x400CF
#endif
#ifndef Wimp_ReportError
#define Wimp_ReportError 0x400DF
#endif
#ifndef OS_Byte
#define OS_Byte 0x06
#endif
#ifndef OS_File
#define OS_File 0x08
#endif
#ifndef DragASprite_Start
#define DragASprite_Start 0x42400
#define DragASprite_Stop  0x42401
#endif

static gboolean debug;

#define TRACE(...) G_STMT_START { if (debug) g_printerr ("riscos-gtk transfer: " __VA_ARGS__); } G_STMT_END

/* Is the clipboard (maybe) in another task? We can't know at start-up,
   so Paste is offered until we learn otherwise. */
static gboolean foreign_clipboard = TRUE;

/* Sending data to another task (our clipboard, or a drop on its window) */
static struct
{
  gboolean waiting;           /* for GTK's owner to give the data */
  GdkWindow *owner;
  GdkAtom selection, target;
  int dest, icon;             /* Wimp_SendMessage R2, R3 */
  int fields[4];              /* +20..+32: window, icon, x, y */
  int your_ref;
  char *data;                 /* Latin-1 */
  int len, sent;
  int datasave_ref, transmit_ref, dataload_ref;
  gboolean scrap;             /* saved to a temporary file */
} out;

/* Receiving data from another task */
enum { IN_NONE, IN_PASTE, IN_DROP };
static struct
{
  int purpose;
  GdkWindow *requestor;       /* paste: who asked, for what */
  GdkAtom target, property;
  GdkWindow *window;          /* drop: our window, root pixel x, y */
  int x, y;
  int request_ref;            /* our DataRequest */
  int peer, peer_ref;         /* their DataSave */
  int fields[4];              /* its +20..+32 (window, icon, x, y) */
  int fetch_ref, fetches, ack_ref;
  char *buf;
  int size, got;
} in;

/* Text dropped on us, waiting for GTK to ask for it (UTF-8), and/or a
   file dropped from the Filer (a file:// URI list) */
static char *drop_text, *drop_uri;

/* A Save box's drag (gdk_riscos_save_drop): the DataSave sent, then the
   receiver's DataSaveAck, kept for the DataLoad after the program saved */
static struct
{
  GdkRiscosSaveFunc func;
  gpointer data;
  int datasave_ref;
  int ack[64];                /* their DataSaveAck */
  gboolean acked;
  int filetype;                    /* to set after saving */
} save;

static GdkRiscosServer *
server (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  return display ? GDK_RISCOS_DISPLAY (display)->server : NULL;
}

static GdkWindow *
gdk_window_for_handle (int handle)
{
  GdkDisplay *display = gdk_display_get_default ();
  guint32 id;

  if (display == NULL || server () == NULL)
    return NULL;
  id = _gdk_riscos_server_id_for_handle (server (), handle);
  if (id == 0)
    return NULL;
  return g_hash_table_lookup (GDK_RISCOS_DISPLAY (display)->id_ht, GINT_TO_POINTER (id));
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
  return block[2];           /* my_ref, filled in by the Wimp */
}

/* A message block of action with n words after the header (+20) */
static void
header (int *block, int action, int your_ref, int bytes)
{
  block[0] = (bytes + 3) & ~3;
  block[1] = 0;
  block[2] = 0;
  block[3] = your_ref;
  block[4] = action;
}

/* ---- text conversion ---------------------------------------------------- */

static char *
utf8_to_latin1 (const char *utf8, int len, int *out_len)
{
  char *s = g_malloc (len + 1);
  const char *p = utf8, *end = utf8 + len;
  int n = 0;

  while (p < end)
    {
      gunichar c = g_utf8_get_char_validated (p, end - p);
      if (c == (gunichar) -1 || c == (gunichar) -2)
        {
          c = '?';
          p++;
        }
      else
        p = g_utf8_next_char (p);
      s[n++] = c < 0x100 ? (char) c : '?';
    }
  s[n] = 0;
  *out_len = n;
  return s;
}

static char *
latin1_to_utf8 (const char *latin1, int len)
{
  GString *s = g_string_sized_new (len + 1);
  int i;

  for (i = 0; i < len; i++)
    g_string_append_unichar (s, (unsigned char) latin1[i]);
  return g_string_free (s, FALSE);
}

static gboolean
is_text_target (GdkAtom target)
{
  return target == gdk_atom_intern_static_string ("UTF8_STRING") ||
         target == gdk_atom_intern_static_string ("STRING") ||
         target == gdk_atom_intern_static_string ("TEXT") ||
         target == gdk_atom_intern_static_string ("COMPOUND_TEXT") ||
         target == gdk_atom_intern_static_string ("text/plain;charset=utf-8") ||
         target == gdk_atom_intern_static_string ("text/plain");
}

static void
set_targets (GdkWindow *requestor, GdkAtom property)
{
  GdkAtom atoms[6];

  atoms[0] = gdk_atom_intern_static_string ("TARGETS");
  atoms[1] = gdk_atom_intern_static_string ("UTF8_STRING");
  atoms[2] = gdk_atom_intern_static_string ("STRING");
  atoms[3] = gdk_atom_intern_static_string ("TEXT");
  atoms[4] = gdk_atom_intern_static_string ("text/plain;charset=utf-8");
  atoms[5] = gdk_atom_intern_static_string ("text/plain");
  _gdk_riscos_property_set (requestor, property, gdk_atom_intern_static_string ("ATOM"),
                            32, (const guchar *) atoms, sizeof atoms, FALSE);
}

/* ---- files (OS_File, RISC OS names) -------------------------------------- */

static gboolean
save_file (const char *name, const char *data, int len)
{
  _kernel_swi_regs r;

  r.r[0] = 10;                     /* save, with a file type */
  r.r[1] = (int) name;
  r.r[2] = FILETYPE_TEXT;
  r.r[4] = (int) data;
  r.r[5] = (int) data + len;
  return _kernel_swi (OS_File, &r, &r) == NULL;
}

static char *
load_file (const char *name, int *len)
{
  _kernel_swi_regs r;
  char *data;

  r.r[0] = 5;                      /* read catalogue information */
  r.r[1] = (int) name;
  if (_kernel_swi (OS_File, &r, &r) != NULL || r.r[0] != 1)
    return NULL;
  *len = r.r[4];
  data = g_malloc (*len + 1);
  r.r[0] = 255;                    /* load to R2 */
  r.r[1] = (int) name;
  r.r[2] = (int) data;
  r.r[3] = 0;
  if (_kernel_swi (OS_File, &r, &r) != NULL)
    {
      g_free (data);
      return NULL;
    }
  data[*len] = 0;
  return data;
}

static void
delete_file (const char *name)
{
  _kernel_swi_regs r;

  r.r[0] = 6;
  r.r[1] = (int) name;
  _kernel_swi (OS_File, &r, &r);
}

/* ---- sending ---------------------------------------------------------- */

static void
out_reset (void)
{
  g_clear_object (&out.owner);
  g_free (out.data);
  memset (&out, 0, sizeof out);
}

/* Ask GTK's owner of selection for its text; the answer comes to
   _gdk_riscos_transfer_notify, then goes out with Message_DataSave. */
static gboolean
out_start (GdkWindow *owner, GdkAtom selection, GList *targets,
           int dest, int icon, const int fields[4], int your_ref)
{
  static const char *prefer[] = { "UTF8_STRING", "text/plain;charset=utf-8", "STRING", "text/plain" };
  GdkAtom target = GDK_NONE;
  guint i;

  if (owner == NULL)
    return FALSE;
  for (i = 0; i < G_N_ELEMENTS (prefer) && target == GDK_NONE; i++)
    {
      GdkAtom a = gdk_atom_intern_static_string (prefer[i]);
      if (targets == NULL || g_list_find (targets, a))
        target = a;
    }
  if (target == GDK_NONE)
    return FALSE;
  out_reset ();
  out.waiting = TRUE;
  out.owner = g_object_ref (owner);
  out.selection = selection;
  out.target = target;
  out.dest = dest;
  out.icon = icon;
  memcpy (out.fields, fields, sizeof out.fields);
  out.your_ref = your_ref;
  TRACE ("asking for %s's %s (for %d)\n", gdk_atom_name (selection), gdk_atom_name (target), dest);
  _gdk_riscos_selection_event (GDK_SELECTION_REQUEST, owner, owner, selection, target,
                               gdk_atom_intern_static_string ("RISCOS_EXPORT"), GDK_CURRENT_TIME);
  return TRUE;
}

static void
out_send_datasave (void)
{
  int block[64];

  header (block, MESSAGE_DATASAVE, out.your_ref, 44 + 9);
  memcpy (&block[5], out.fields, sizeof out.fields);
  block[9] = out.len;
  block[10] = FILETYPE_TEXT;
  strcpy ((char *) &block[11], "TextFile");
  out.datasave_ref = send_message (USER_MESSAGE, block, out.dest, out.icon);
  TRACE ("DataSave %d bytes to %d (ref %d)\n", out.len, out.dest, out.datasave_ref);
}

gboolean
_gdk_riscos_transfer_notify (GdkWindow *requestor, GdkAtom selection, GdkAtom target,
                             GdkAtom property)
{
  guchar *data = NULL;
  gint len = 0;
  GdkAtom type;

  if (!out.waiting || requestor != out.owner || selection != out.selection)
    return FALSE;
  out.waiting = FALSE;
  if (property == GDK_NONE ||
      !_gdk_riscos_property_take (requestor, property, TRUE, &type, NULL, &data, &len))
    {
      TRACE ("the owner had no %s\n", gdk_atom_name (target));
      out_reset ();
      return TRUE;
    }
  if (type == gdk_atom_intern_static_string ("STRING"))
    {
      out.data = (char *) data;     /* Latin-1 already */
      out.len = len;
    }
  else
    {
      out.data = utf8_to_latin1 ((char *) data, len, &out.len);
      g_free (data);
    }
  out_send_datasave ();
  return TRUE;
}

static void
out_ramfetch (int *block)
{
  int buf = block[5], size = block[6], n = MIN (size, out.len - out.sent);
  int peer = block[1];
  _kernel_swi_regs r;

  if (n > 0)
    {
      r.r[0] = _gdk_riscos_server_get_task (server ());
      r.r[1] = (int) (out.data + out.sent);
      r.r[2] = peer;
      r.r[3] = buf;
      r.r[4] = n;
      if (_kernel_swi (Wimp_TransferBlock, &r, &r) != NULL)
        {
          out_reset ();
          return;
        }
      out.sent += n;
    }
  block[3] = block[2];
  block[4] = MESSAGE_RAMTRANSMIT;
  block[5] = buf;
  block[6] = n;
  block[0] = 28;
  TRACE ("RAMTransmit %d of %d bytes\n", n, out.len);
  if (n == size)
    out.transmit_ref = send_message (USER_MESSAGE_RECORDED, block, peer, 0);
  else
    {
      send_message (USER_MESSAGE, block, peer, 0);
      out_reset ();             /* done */
    }
}

static void
out_datasaveack (int *block)
{
  const char *path = (const char *) &block[11];
  int peer = block[1];

  if (!save_file (path, out.data, out.len))
    {
      out_reset ();
      return;
    }
  out.scrap = block[9] == -1;      /* "unsafe": <Wimp$Scrap>, not a Filer save */
  block[3] = block[2];
  block[4] = MESSAGE_DATALOAD;
  block[9] = out.len;
  block[10] = FILETYPE_TEXT;
  block[0] = (44 + strlen (path) + 1 + 3) & ~3;
  out.datasave_ref = 0;
  out.dataload_ref = send_message (USER_MESSAGE_RECORDED, block, peer, 0);
  g_free (out.data);
  out.data = NULL;
  TRACE ("saved to %s, DataLoad (ref %d)\n", path, out.dataload_ref);
}

/* ---- receiving ------------------------------------------------------- */

static void
in_reset (void)
{
  g_clear_object (&in.requestor);
  g_clear_object (&in.window);
  g_free (in.buf);
  memset (&in, 0, sizeof in);
}

/* Done (data = Latin-1, or NULL for failure): give it to GTK */
static void
in_finish (const char *data, int len)
{
  char *text = data ? latin1_to_utf8 (data, len) : NULL;

  TRACE ("received %d bytes (%s)\n", data ? len : -1, in.purpose == IN_PASTE ? "paste" : "drop");
  if (in.purpose == IN_PASTE && in.requestor)
    {
      if (text)
        _gdk_riscos_property_set (in.requestor, in.property,
                                  gdk_atom_intern_static_string ("UTF8_STRING"), 8,
                                  (const guchar *) text, strlen (text), FALSE);
      _gdk_riscos_selection_event (GDK_SELECTION_NOTIFY, in.requestor, NULL,
                                   GDK_SELECTION_CLIPBOARD, in.target,
                                   text ? in.property : GDK_NONE, GDK_CURRENT_TIME);
      g_free (text);
    }
  else if (in.purpose == IN_DROP && in.window && text)
    {
      g_free (drop_text);
      drop_text = text;
      _gdk_riscos_dnd_incoming (gdk_window_get_display (in.window), in.window, in.x, in.y,
                                TRUE, drop_uri != NULL);
    }
  else
    g_free (text);
  in_reset ();
}

static void
in_fetch (void)
{
  int block[8];

  if (in.got + 256 > in.size)
    {
      in.size = MAX (in.size * 2, in.got + 1024);
      in.buf = g_realloc (in.buf, in.size);
    }
  header (block, MESSAGE_RAMFETCH, in.peer_ref, 28);
  block[5] = (int) (in.buf + in.got);
  block[6] = in.size - in.got;
  in.fetches++;
  in.fetch_ref = send_message (USER_MESSAGE_RECORDED, block, in.peer, 0);
  TRACE ("RAMFetch %d bytes (ref %d)\n", block[6], in.fetch_ref);
}

/* Their Message_DataSave: fetch the data */
static void
in_datasave (int *block)
{
  int est = block[9];

  in.peer = block[1];
  in.peer_ref = block[2];
  memcpy (in.fields, &block[5], sizeof in.fields);
  in.size = MAX (256, (est > 0 && est < (16 << 20) ? est : 0) + 1);
  in.buf = g_malloc (in.size);
  in.got = 0;
  in.fetches = 0;
  in_fetch ();
}

static void
in_ramtransmit (int *block)
{
  int n = block[6];
  int asked = in.size - in.got;

  in.got += n;
  in.peer_ref = block[2];
  if (n < asked)
    in_finish (in.buf, in.got);
  else
    in_fetch ();
}

/* The first RAMFetch came back: use a file */
static void
in_use_file (int *block)
{
  block[3] = in.peer_ref;
  block[4] = MESSAGE_DATASAVEACK;
  memcpy (&block[5], in.fields, sizeof in.fields);   /* as in their DataSave */
  block[10] = FILETYPE_TEXT;
  block[9] = -1;                   /* not safe */
  strcpy ((char *) &block[11], SCRAP);
  block[0] = (44 + sizeof SCRAP + 3) & ~3;
  in.ack_ref = send_message (USER_MESSAGE, block, in.peer, 0);
  in.fetch_ref = 0;
}

/* Their Message_DataLoad (after our DataSaveAck, or from the Filer) */
static gboolean
in_dataload (int *block, gboolean scrap)
{
  const char *path = (const char *) &block[11];
  int len = 0, peer = block[1];
  char *data = load_file (path, &len);

  if (data == NULL)
    return FALSE;
  block[3] = block[2];
  block[4] = MESSAGE_DATALOADACK;
  send_message (USER_MESSAGE, block, peer, 0);
  if (scrap)
    delete_file (path);
  in_finish (data, len);
  g_free (data);
  return TRUE;
}

/* ---- for gdkselection-riscos.c and gdkdnd-riscos.c ------------------------ */

void
_gdk_riscos_clipboard_claimed (void)
{
  int block[6];

  foreign_clipboard = FALSE;
  if (server () == NULL)
    return;
  header (block, MESSAGE_CLAIMENTITY, 0, 24);
  block[5] = 4;                    /* the clipboard */
  send_message (USER_MESSAGE, block, 0, 0);
  TRACE ("ClaimEntity (clipboard)\n");
}

/* A selection with no owner in this program: the clipboard in another
   task, or text dropped on us. TRUE if the answer will come. */
gboolean
_gdk_riscos_transfer_convert (GdkWindow *requestor, GdkAtom selection, GdkAtom target,
                              GdkAtom property)
{
  if (selection == gdk_atom_intern_static_string ("RISCOS_DROP"))
    {
      if (drop_uri && target == gdk_atom_intern_static_string ("text/uri-list"))
        _gdk_riscos_property_set (requestor, property,
                                  gdk_atom_intern_static_string ("text/uri-list"), 8,
                                  (const guchar *) drop_uri, strlen (drop_uri), FALSE);
      else if (!drop_text && drop_uri && target == gdk_atom_intern_static_string ("TARGETS"))
        {
          GdkAtom atoms[2] = { gdk_atom_intern_static_string ("TARGETS"),
                               gdk_atom_intern_static_string ("text/uri-list") };
          _gdk_riscos_property_set (requestor, property, gdk_atom_intern_static_string ("ATOM"),
                                    32, (const guchar *) atoms, sizeof atoms, FALSE);
        }
      else if (drop_text && target == gdk_atom_intern_static_string ("TARGETS"))
        set_targets (requestor, property);
      else if (drop_text && is_text_target (target))
        _gdk_riscos_property_set (requestor, property,
                                  gdk_atom_intern_static_string ("UTF8_STRING"), 8,
                                  (const guchar *) drop_text, strlen (drop_text), FALSE);
      else
        return FALSE;
      _gdk_riscos_selection_event (GDK_SELECTION_NOTIFY, requestor, NULL, selection, target,
                                   property, GDK_CURRENT_TIME);
      return TRUE;
    }

  if (selection != GDK_SELECTION_CLIPBOARD || server () == NULL)
    return FALSE;
  if (target == gdk_atom_intern_static_string ("TARGETS"))
    {
      /* Paste is offered while another task may have the clipboard */
      if (!foreign_clipboard)
        return FALSE;
      set_targets (requestor, property);
      _gdk_riscos_selection_event (GDK_SELECTION_NOTIFY, requestor, NULL, selection, target,
                                   property, GDK_CURRENT_TIME);
      return TRUE;
    }
  if (!is_text_target (target))
    return FALSE;

  /* ask the clipboard's owner, whoever it is */
  {
    int block[16], fields[4] = { 0, 0, 0, 0 };
    int handle = _gdk_riscos_server_focus_handle (server ());

    if (in.purpose != IN_NONE)
      in_finish (NULL, 0);
    in.purpose = IN_PASTE;
    in.requestor = g_object_ref (requestor);
    in.target = target;
    in.property = property;
    header (block, MESSAGE_DATAREQUEST, 0, 48);
    fields[0] = handle;
    memcpy (&block[5], fields, sizeof fields);
    block[9] = 4;                  /* send the clipboard */
    block[10] = FILETYPE_TEXT;
    block[11] = -1;
    in.request_ref = send_message (USER_MESSAGE_RECORDED, block, 0, 0);
    TRACE ("DataRequest (ref %d)\n", in.request_ref);
    if (in.request_ref == 0)
      {
        in_reset ();
        return FALSE;
      }
  }
  return TRUE;
}

/* The window under the pointer: ours (returned), or target = another
   task's window, icon and the pointer (OS units); target[0] = -1 for the
   desktop background. */
GdkWindow *
_gdk_riscos_transfer_window_at (GdkDisplay *display, int target[4])
{
  int block[5];
  _kernel_swi_regs r;
  GdkWindow *window;

  target[0] = -1;
  target[1] = -1;
  target[2] = target[3] = 0;
  r.r[1] = (int) block;
  if (_kernel_swi (Wimp_GetPointerInfo, &r, &r) != NULL)
    return NULL;
  window = gdk_window_for_handle (block[3]);
  if (window)
    return window;
  target[0] = block[3];
  target[1] = block[4];
  target[2] = block[0];
  target[3] = block[1];
  return NULL;
}

/* The dashed box the Wimp draws at the pointer while dragging */
gboolean
_gdk_riscos_transfer_drag_box (gboolean start)
{
  _kernel_swi_regs r;
  int block[11], ptr[5];
  GdkRiscosServer *s = server ();
  int sw = 0, sh = 0;

  if (s == NULL)
    return FALSE;
  if (!start)
    {
      r.r[1] = -1;
      _kernel_swi (Wimp_DragBox, &r, &r);
      return FALSE;
    }
  r.r[1] = (int) ptr;
  if (_kernel_swi (Wimp_GetPointerInfo, &r, &r) != NULL)
    return FALSE;
  _gdk_riscos_server_screen_os_size (s, &sw, &sh);
  memset (block, 0, sizeof block);
  block[1] = 5;                    /* drag a fixed size box */
  block[2] = ptr[0] - 32;
  block[3] = ptr[1] - 20;
  block[4] = ptr[0] + 32;
  block[5] = ptr[1] + 20;
  block[6] = -0x10000;             /* parent box: anywhere */
  block[7] = -0x10000;
  block[8] = sw + 0x10000;
  block[9] = sh + 0x10000;
  r.r[1] = (int) block;
  return _kernel_swi (Wimp_DragBox, &r, &r) == NULL;
}

/* A drag from this program dropped on another task's window */
gboolean
_gdk_riscos_transfer_drop_foreign (GdkWindow *source, GdkAtom selection, GList *targets,
                                   const int target[4])
{
  GdkDisplay *display = gdk_window_get_display (source);
  GdkWindow *owner = _gdk_riscos_display_get_selection_owner (display, selection);

  if (owner == NULL)
    owner = source;
  /* R2 = the window (or -2, the icon bar, with R3 its icon) */
  return out_start (owner, selection, targets, target[0], target[1], target, 0);
}

void
_gdk_riscos_transfer_drop_done (gboolean success)
{
  g_free (drop_text);
  drop_text = NULL;
  g_free (drop_uri);
  drop_uri = NULL;
}

/* ---- file names, Save boxes and loading files --------------------------- */

/**
 * gdk_riscos_path_to_unix:
 * @riscos_path: a RISC OS file name, e.g. "ADFS::HD4.$.Docs.Report/txt"
 *
 * Returns: (transfer full): the same file as UnixLib names it
 * ("/ADFS::HD4/$/Docs/Report.txt"): '.' and '/' swapped, a '/' in front.
 * UnixLib maps it back when the program opens it.
 */
char *
gdk_riscos_path_to_unix (const char *riscos_path)
{
  GString *s = g_string_new ("/");
  const char *p;

  for (p = riscos_path; *p; p++)
    g_string_append_c (s, *p == '.' ? '/' : *p == '/' ? '.' : *p);
  return g_string_free (s, FALSE);
}

/**
 * gdk_riscos_path_from_unix:
 * @unix_path: a file name as gdk_riscos_path_to_unix gives
 *
 * Returns: (transfer full): the RISC OS file name.
 */
char *
gdk_riscos_path_from_unix (const char *unix_path)
{
  GString *s = g_string_new (NULL);
  const char *p = unix_path[0] == '/' ? unix_path + 1 : unix_path;

  for (; *p; p++)
    g_string_append_c (s, *p == '/' ? '.' : *p == '.' ? '/' : *p);
  return g_string_free (s, FALSE);
}

/* A file:// URI list (one file) for a RISC OS file name */
static char *
uri_list (const char *riscos_path)
{
  char *unix_path = gdk_riscos_path_to_unix (riscos_path);
  char *uri = g_filename_to_uri (unix_path, NULL, NULL);
  char *list = uri ? g_strconcat (uri, "\r\n", NULL) : NULL;

  g_free (unix_path);
  g_free (uri);
  return list;
}

/**
 * gdk_riscos_save_drag_begin:
 * @display: the display
 * @sprite: a sprite in the Wimp's pool, e.g. "file_fff"
 * @root_x, @root_y, @width, @height: where the icon is (root pixels)
 *
 * A Save box's file icon is being dragged: the Wimp shows the sprite
 * following the pointer (DragASprite), or a dashed box (Wimp_DragBox) if
 * DragASprite isn't there.
 */
void
gdk_riscos_save_drag_begin (GdkDisplay *display, const char *sprite,
                            gint root_x, gint root_y, gint width, gint height)
{
  _kernel_swi_regs r;
  int box[4], sw = 0, sh = 0, xe = 1, ye = 1;
  GdkRiscosServer *s = server ();

  if (s == NULL)
    return;
  _gdk_riscos_server_screen_os_size (s, &sw, &sh);
  {
    int px_w = gdk_screen_get_width (gdk_display_get_default_screen (display));
    int px_h = gdk_screen_get_height (gdk_display_get_default_screen (display));
    xe = px_w > 0 ? sw / px_w : 2;
    ye = px_h > 0 ? sh / px_h : 2;
  }
  /* screen OS units, from the bottom left */
  box[0] = root_x * xe;
  box[1] = sh - (root_y + height) * ye;
  box[2] = (root_x + width) * xe;
  box[3] = sh - root_y * ye;
  /* the user's choice (*Configure DragASprite: CMOS byte 28 bit 1) */
  memset (&r, 0, sizeof r);
  r.r[0] = 161;
  r.r[1] = 28;
  if (_kernel_swi (OS_Byte, &r, &r) != NULL || (r.r[2] & 2))
    {
      memset (&r, 0, sizeof r);
      r.r[0] = 0x85;               /* centred in the box, a drop shadow */
  r.r[1] = 1;                      /* the Wimp's sprite pool */
  r.r[2] = (int) sprite;
  r.r[3] = (int) box;
      if (_kernel_swi (DragASprite_Start, &r, &r) == NULL)
        {
          TRACE ("DragASprite_Start %s\n", sprite);
          return;
        }
    }
  {
    int block[11];
    memset (block, 0, sizeof block);
    block[1] = 5;                  /* a fixed size dashed box */
    memcpy (&block[2], box, sizeof box);
    block[6] = -0x10000;
    block[7] = -0x10000;
    block[8] = sw + 0x10000;
    block[9] = sh + 0x10000;
    r.r[1] = (int) block;
    _kernel_swi (Wimp_DragBox, &r, &r);
  }
}

/**
 * gdk_riscos_save_drop:
 * @display: the display
 * @leafname: the file's RISC OS leaf name
 * @filetype: its file type
 * @size: its estimated size (bytes)
 * @func: called with the full RISC OS name to save to (from the
 *   receiver's Message_DataSaveAck), and whether it's "safe" (a real file;
 *   not, for <Wimp$Scrap> on its way to another program); after saving,
 *   call gdk_riscos_save_done())
 * @data: for @func
 *
 * The drag ended: ends the Wimp's drag and sends Message_DataSave to the
 * window under the pointer (a Filer window, another program's window or
 * the icon bar). Returns %FALSE (and @func won't be called) if it ended
 * over this program or the desktop background.
 */
gboolean
gdk_riscos_save_drop (GdkDisplay *display, const char *leafname, int filetype, int size,
                      GdkRiscosSaveFunc func, gpointer data)
{
  _kernel_swi_regs r;
  int ptr[5], block[64];

  memset (&r, 0, sizeof r);
  _kernel_swi (DragASprite_Stop, &r, &r);
  r.r[1] = -1;
  _kernel_swi (Wimp_DragBox, &r, &r);
  r.r[1] = (int) ptr;
  if (_kernel_swi (Wimp_GetPointerInfo, &r, &r) != NULL)
    return FALSE;
  if (ptr[3] == -1 || gdk_window_for_handle (ptr[3]))
    return FALSE;
  header (block, MESSAGE_DATASAVE, 0, 44 + (int) strlen (leafname) + 1);
  block[5] = ptr[3];
  block[6] = ptr[4];
  block[7] = ptr[0];
  block[8] = ptr[1];
  block[9] = size;
  block[10] = filetype;
  g_strlcpy ((char *) &block[11], leafname, 212);
  memset (&save, 0, sizeof save);
  save.func = func;
  save.data = data;
  save.filetype = filetype;
  save.datasave_ref = send_message (USER_MESSAGE, block, ptr[3], ptr[4]);
  TRACE ("Save box: DataSave \"%s\" (&%03X) to window &%X (ref %d)\n", leafname, filetype,
         (unsigned int) ptr[3], save.datasave_ref);
  return save.datasave_ref != 0;
}

/**
 * gdk_riscos_save_done:
 * @riscos_path: the file the program saved (what the save function got)
 *
 * After saving: tells the receiver (Message_DataLoad) that the file is
 * there, as the data transfer protocol asks. Another program then loads
 * it from <Wimp$Scrap>; the Filer just acknowledges.
 */
void
gdk_riscos_save_done (const char *riscos_path)
{
  _kernel_swi_regs r;
  int *block = save.ack;

  if (!save.acked)
    return;
  r.r[0] = 5;
  r.r[1] = (int) riscos_path;
  if (_kernel_swi (OS_File, &r, &r) != NULL || r.r[0] != 1)
    return;
  /* a typed file (load address &FFFtttxx) of another type: give it the
     type it was offered as (programs writing through UnixLib may not) */
  if ((r.r[2] & 0xFFF00000) == (int) 0xFFF00000
      && ((r.r[2] >> 8) & 0xFFF) != save.filetype && save.filetype != 0xFFD)
    {
      _kernel_swi_regs t;
      int size = r.r[4];
      t.r[0] = 18;
      t.r[1] = (int) riscos_path;
      t.r[2] = save.filetype;
      _kernel_swi (OS_File, &t, &t);
      r.r[4] = size;
    }
  block[3] = block[2];
  block[4] = MESSAGE_DATALOAD;
  block[9] = r.r[4];
  g_strlcpy ((char *) &block[11], riscos_path, 212);
  block[0] = (44 + strlen (riscos_path) + 1 + 3) & ~3;
  out_reset ();
  /* if nobody loads it, a scrap file is ours to delete; never a file
     saved into a Filer window */
  out.scrap = save.ack[9] == -1;
  out.dataload_ref = send_message (USER_MESSAGE_RECORDED, block, block[1], 0);
  save.acked = FALSE;
  TRACE ("Save box: saved, DataLoad %s\n", riscos_path);
}

/**
 * gdk_riscos_window_set_load_func:
 * @window: a toplevel
 * @func: (nullable): called when a file is dropped on @window from the
 *   Filer (Message_DataLoad) with its RISC OS name and type; returns
 *   %TRUE if it took the file (the Filer gets Message_DataLoadAck)
 * @data: for @func
 *
 * Without one, a file dropped on a window goes to the GTK widget under
 * the pointer as a drop of a file:// URI (text/uri-list; text files also
 * as their text).
 */
void
gdk_riscos_window_set_load_func (GdkWindow *window, GdkRiscosLoadFunc func, gpointer data)
{
  g_object_set_data (G_OBJECT (window), "riscos-gtk-load-func", func);
  g_object_set_data (G_OBJECT (window), "riscos-gtk-load-data", data);
}

/**
 * gdk_riscos_report_error:
 * @message: what to say (UTF-8)
 *
 * A Wimp error box with an OK button (Wimp_ReportError), titled "Message
 * from" the program, as the Save As module reports "To save, drag the
 * icon to a directory display".
 */
void
gdk_riscos_report_error (const char *message)
{
  _kernel_swi_regs r;
  struct { int errnum; char text[252]; } err;
  int n;
  char *latin1 = utf8_to_latin1 (message, strlen (message), &n);   /* no iconv */
  const char *app = g_get_application_name ();
  char *app1 = app ? utf8_to_latin1 (app, strlen (app), &n) : NULL;

  err.errnum = 0;
  g_strlcpy (err.text, latin1 ? latin1 : message, sizeof err.text);
  r.r[0] = (int) &err;
  r.r[1] = 1;                      /* an OK button */
  r.r[2] = (int) (app1 ? app1 : "GTK");
  if (_kernel_swi (Wimp_ReportError, &r, &r) != NULL)
    g_printerr ("riscos-gtk: %s\n", message);
  else
    TRACE ("error box: %s\n", message);
  g_free (latin1);
  g_free (app1);
}

/* Wimp messages (17, 18) and returned ones (19), from the server */
void
_gdk_riscos_transfer_message (int reason, int *block)
{
  int task = server () ? _gdk_riscos_server_get_task (server ()) : 0;
  GdkDisplay *display = gdk_display_get_default ();

  debug = g_getenv ("RISCOS_GTK_DEBUG") != NULL;
  if (display == NULL)
    return;

  if (reason == USER_MESSAGE_ACKNOWLEDGE)
    {
      int ref = block[2];

      if (ref == 0)
        return;
      if (in.purpose != IN_NONE && ref == in.request_ref && in.fetches == 0)
        {
          TRACE ("nobody has the clipboard\n");
          in_finish (NULL, 0);
        }
      else if (in.purpose != IN_NONE && ref == in.fetch_ref)
        {
          if (in.fetches == 1)
            in_use_file (block);
          else
            in_finish (NULL, 0);
        }
      else if (ref == out.transmit_ref)
        out_reset ();
      else if (ref == out.dataload_ref)
        {
          /* nobody loaded it: a temporary file is ours to delete (never a
             file saved into a Filer window) */
          if (out.scrap)
            delete_file ((const char *) &block[11]);
          out_reset ();
        }
      return;
    }

  switch (block[4])
    {
    case MESSAGE_CLAIMENTITY:
      if (block[1] != task && (block[5] & 4))
        {
          TRACE ("another task has the clipboard\n");
          foreign_clipboard = TRUE;
          _gdk_riscos_selection_lost (display, GDK_SELECTION_CLIPBOARD);
        }
      break;

    case MESSAGE_DATAREQUEST:
      if (block[1] != task && (block[9] & 4))
        {
          GdkWindow *owner = _gdk_riscos_display_get_selection_owner (display, GDK_SELECTION_CLIPBOARD);
          if (owner)
            out_start (owner, GDK_SELECTION_CLIPBOARD, NULL, block[1], 0, &block[5], block[2]);
        }
      break;

    case MESSAGE_DATASAVE:
      if (block[3] != 0 && in.purpose == IN_PASTE && block[3] == in.request_ref)
        {
          if (block[10] == FILETYPE_TEXT)
            in_datasave (block);
          else
            in_finish (NULL, 0);
        }
      else if (block[3] == 0 && block[10] == FILETYPE_TEXT && block[1] != task)
        {
          GdkWindow *window = gdk_window_for_handle (block[5]);
          if (window)
            {
              int x = block[7], y = block[8];
              if (in.purpose != IN_NONE)
                in_finish (NULL, 0);
              _gdk_riscos_server_os_to_px (server (), &x, &y);
              g_free (drop_uri);
              drop_uri = NULL;
              in.purpose = IN_DROP;
              in.window = g_object_ref (window);
              in.x = x;
              in.y = y;
              in_datasave (block);
            }
        }
      break;

    case MESSAGE_RAMTRANSMIT:
      if (in.purpose != IN_NONE && block[3] == in.fetch_ref)
        in_ramtransmit (block);
      break;

    case MESSAGE_DATALOAD:
      if (block[3] != 0)
        {
          if (in.purpose != IN_NONE && block[3] == in.ack_ref)
            in_dataload (block, TRUE);
        }
      else
        {
          /* a file dropped from the Filer */
          GdkWindow *window = gdk_window_for_handle (block[5]);
          GdkRiscosLoadFunc func = window ? g_object_get_data (G_OBJECT (window), "riscos-gtk-load-func") : NULL;
          int x = block[7], y = block[8];

          if (window == NULL)
            break;
          _gdk_riscos_server_os_to_px (server (), &x, &y);
          if (func)
            {
              if (func ((const char *) &block[11], block[10],
                        g_object_get_data (G_OBJECT (window), "riscos-gtk-load-data")))
                {
                  int peer = block[1];
                  block[3] = block[2];
                  block[4] = MESSAGE_DATALOADACK;
                  send_message (USER_MESSAGE, block, peer, 0);
                }
              break;
            }
          if (in.purpose != IN_NONE)
            in_finish (NULL, 0);
          g_free (drop_uri);
          drop_uri = uri_list ((const char *) &block[11]);
          if (block[10] == FILETYPE_TEXT)
            {
              /* its text, and the file */
              in.purpose = IN_DROP;
              in.window = g_object_ref (window);
              in.x = x;
              in.y = y;
              if (!in_dataload (block, FALSE))
                in_reset ();
            }
          else if (drop_uri)
            {
              /* the file, as a file:// URI */
              int peer = block[1];
              g_free (drop_text);
              drop_text = NULL;
              block[3] = block[2];
              block[4] = MESSAGE_DATALOADACK;
              send_message (USER_MESSAGE, block, peer, 0);
              TRACE ("file dropped: %s", drop_uri);
              _gdk_riscos_dnd_incoming (gdk_window_get_display (window), window, x, y, FALSE, TRUE);
            }
        }
      break;

    case MESSAGE_RAMFETCH:
      if (block[3] != 0 && out.data && (block[3] == out.datasave_ref || block[3] == out.transmit_ref))
        out_ramfetch (block);
      break;

    case MESSAGE_DATASAVEACK:
      if (block[3] != 0 && out.data && block[3] == out.datasave_ref)
        out_datasaveack (block);
      else if (block[3] != 0 && save.func && block[3] == save.datasave_ref)
        {
          GdkRiscosSaveFunc func = save.func;
          memcpy (save.ack, block, sizeof save.ack);
          save.acked = TRUE;
          save.func = NULL;
          TRACE ("Save box: DataSaveAck %s (%s)\n", (const char *) &block[11],
                 block[9] == -1 ? "unsafe" : "safe");
          func ((const char *) &block[11], block[9] != -1, save.data);
        }
      break;

    case MESSAGE_DATALOADACK:
      if (block[3] != 0 && block[3] == out.dataload_ref)
        out_reset ();
      break;

    default:
      break;
    }
}
