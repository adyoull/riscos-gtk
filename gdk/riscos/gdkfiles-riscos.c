/*
 * gdkfiles-riscos.c - riscos-gtk: directories as the Filer sees them, for
 * the Filer-style file chooser (gtk/riscos/gtkfilebrowser-riscos.c).
 *
 * SWIs as in the RISC OS PRMs:
 *   OS_GBPB 12       read a directory's entries with their file types
 *                    (load, exec, length, attributes, object type, file
 *                    type, name; word-aligned records; repeat until R4 =
 *                    -1, R3 may be 0 before the end)
 *   OS_FSControl 33  filing system number to name ("" if none)
 *   OS_FSControl 37  canonicalise a path
 *   OS_FSControl 18  file type to its name (8 characters, space padded)
 *   OS_File 5 / 18   read a file's type (load address &FFFtttxx) / set it
 *   Wimp_StartTask   "Filer_Run name": open a file as a double-click in
 *                    the Filer does (gdk_riscos_open_file)
 * and MimeMap's MimeMap_Translate (&50B00: R0 the input format, 2 a MIME
 * type, 3 an extension; R1 -> it; R2 = 0 for a file type, returned in R3).
 *
 * The discs available (gdk_riscos_discs): the icon bar's device icons,
 * the ones the Filers (SDFS Filer, SCSIFiler, RAMFS Filer, LanMan98...)
 * put there. Wimp_WhichIcon on the icon bar (-2) lists the icons;
 * Wimp_GetIconState (-2) gives each one's flags and data; Wimp_SendMessage
 * with R2 = -2, R3 = icon (an acknowledgement: nothing is delivered) gives
 * the task that created it; TaskManager_TaskNameFromHandle its name, which
 * starts with its filing system's ("SDFS Filer"); Wimp_TransferBlock reads
 * the indirected text and validation string from that task. A Filer's
 * icon is only there while its disc or mount is.
 *
 * Copyright (C) 2026 Andrew Youll. LGPL-2.1-or-later, as GTK.
 */
#include "config.h"

#include "gdkriscoswimp.h"

#include <gio/gio.h>
#include <string.h>
#include <kernel.h>

#define OS_GBPB       0x0C
#define OS_FSControl  0x29
#define Wimp_GetIconState  0x400CE
#define Wimp_WhichIcon     0x400D6
#define Wimp_SendMessage   0x400E7
#define Wimp_TransferBlock 0x400F1
#define Wimp_ReadSysInfo   0x400F2
#define TaskManager_TaskNameFromHandle 0x42680
#define TaskManager_EnumerateTasks     0x42681
#define OS_Memory     0x68
#define OS_File       0x08
#define Wimp_StartTask     0x400DE
#define MimeMap_Translate  0x50B00

static void
entry_free (gpointer p)
{
  GdkRiscosDirEntry *e = p;

  g_free (e->name);
  g_free (e);
}

GPtrArray *
gdk_riscos_read_dir (const char *riscos_dir, GError **error)
{
  GPtrArray *list = g_ptr_array_new_with_free_func (entry_free);
  int *buf = g_malloc (4096);
  _kernel_swi_regs r;
  _kernel_oserror *e;
  int offset = 0, guard = 0;

  do
    {
      int n, i;
      const char *p = (const char *) buf;

      r.r[0] = 12;
      r.r[1] = (int) (long) riscos_dir;
      r.r[2] = (int) (long) buf;
      r.r[3] = 64;
      r.r[4] = offset;
      r.r[5] = 4096;
      r.r[6] = 0;
      if ((e = _kernel_swi (OS_GBPB, &r, &r)) != NULL)
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, e->errmess);
          g_ptr_array_unref (list);
          g_free (buf);
          return NULL;
        }
      n = r.r[3];
      offset = r.r[4];
      for (i = 0; i < n; i++)
        {
          const int *w = (const int *) (const void *) p;
          GdkRiscosDirEntry *d = g_new0 (GdkRiscosDirEntry, 1);
          guint32 load = (guint32) w[0], exec = (guint32) w[1];
          size_t len = strlen (p + 24);

          d->name = g_strndup (p + 24, len);
          d->length = (guint32) w[2];
          d->attributes = (guint32) w[3];
          d->object_type = w[4];
          d->filetype = w[5];
          d->date = (load & 0xFFF00000u) == 0xFFF00000u
                    ? ((gint64) (load & 0xFF) << 32) | exec : -1;
          g_ptr_array_add (list, d);
          p += (24 + len + 1 + 3) & ~3;
        }
    }
  while (offset != -1 && ++guard < 100000);
  g_free (buf);
  return list;
}

char **
gdk_riscos_filing_systems (void)
{
  GPtrArray *names = g_ptr_array_new ();
  char name[64];
  int fs;

  for (fs = 1; fs < 256; fs++)
    {
      _kernel_swi_regs r;

      name[0] = '\0';
      r.r[0] = 33;
      r.r[1] = fs;
      r.r[2] = (int) (long) name;
      r.r[3] = sizeof name;
      if (_kernel_swi (OS_FSControl, &r, &r) == NULL && name[0])
        {
          name[sizeof name - 1] = '\0';
          g_ptr_array_add (names, g_strdup (name));
        }
    }
  g_ptr_array_add (names, NULL);
  return (char **) g_ptr_array_free (names, FALSE);
}

char *
gdk_riscos_canonicalise (const char *name)
{
  _kernel_swi_regs r;
  char *buf;
  int size;

  r.r[0] = 37;
  r.r[1] = (int) (long) name;
  r.r[2] = 0;
  r.r[3] = 0;
  r.r[4] = 0;
  r.r[5] = 0;
  if (_kernel_swi (OS_FSControl, &r, &r) != NULL || r.r[5] > 0)
    size = 1024;              /* not told: a generous buffer */
  else
    size = 1 - r.r[5];
  buf = g_malloc0 (size + 1);
  r.r[0] = 37;
  r.r[1] = (int) (long) name;
  r.r[2] = (int) (long) buf;
  r.r[3] = 0;
  r.r[4] = 0;
  r.r[5] = size;
  if (_kernel_swi (OS_FSControl, &r, &r) != NULL || buf[0] == '\0')
    {
      g_free (buf);
      return g_strdup (name);
    }
  return buf;
}

char *
gdk_riscos_filetype_name (int filetype)
{
  _kernel_swi_regs r;
  char text[9];
  int i;

  r.r[0] = 18;
  r.r[2] = filetype & 0xFFF;
  if (_kernel_swi (OS_FSControl, &r, &r) != NULL)
    return g_strdup_printf ("&%03X", filetype & 0xFFF);
  memcpy (text, &r.r[2], 4);
  memcpy (text + 4, &r.r[3], 4);
  text[8] = '\0';
  for (i = 7; i >= 0 && (text[i] == ' ' || text[i] == '\0'); i--)
    text[i] = '\0';
  return text[0] ? g_strdup (text) : g_strdup_printf ("&%03X", filetype & 0xFFF);
}

/* name as a full path, or NULL if the filing system can't find it */
static char *
canonical_or_null (const char *name)
{
  _kernel_swi_regs r;
  char buf[1024];

  r.r[0] = 37;
  r.r[1] = (int) (long) name;
  r.r[2] = (int) (long) buf;
  r.r[3] = 0;
  r.r[4] = 0;
  r.r[5] = sizeof buf;
  if (_kernel_swi (OS_FSControl, &r, &r) != NULL || r.r[5] < 0 || buf[0] == '\0')
    return NULL;
  return g_strdup (buf);
}

static void
disc_free (gpointer p)
{
  GdkRiscosDisc *d = p;

  g_free (d->fs);
  g_free (d->name);
  g_free (d->root);
  g_free (d->sprite);
  g_free (d);
}

/* The filing system a Filer task's name belongs to: the longest FS name
   it starts with, followed by nothing, a space, "-", "FS" or "Filer"
   ("SDFS Filer", "SCSIFiler", "RAMFS Filer", "LanMan98"; not "NetSurf") */
static const char *
fs_of_task (const char *task, char **fss)
{
  const char *best = NULL;
  int i;

  for (i = 0; fss[i]; i++)
    {
      size_t n = strlen (fss[i]);
      const char *rest = task + n;
      if (g_ascii_strncasecmp (task, fss[i], n) != 0)
        continue;
      if (*rest && *rest != ' ' && *rest != '-' && g_ascii_strncasecmp (rest, "FS", 2) != 0 &&
          g_ascii_strncasecmp (rest, "Filer", 5) != 0)
        continue;
      if (best == NULL || n > strlen (best))
        best = fss[i];
    }
  return best;
}

/* Is task a module task (TaskManager_EnumerateTasks flags bit 0)? The
   Filers are: their icons' indirected text is in the RMA, not in an
   application slot */
static gboolean
module_task (int task)
{
  _kernel_swi_regs r;
  int buf[4 * 16], *b, next = 0;

  do
    {
      r.r[0] = next;
      r.r[1] = (int) (long) buf;
      r.r[2] = sizeof buf;
      if (_kernel_swi (TaskManager_EnumerateTasks, &r, &r) != NULL)
        return FALSE;
      next = r.r[0];
      for (b = buf; (char *) b < (char *) (long) r.r[1]; b += 4)
        if (b[0] == task)
          return (b[3] & 1) != 0;
    }
  while (next >= 0);
  return FALSE;
}

/* Is lo..lo+len readable in user mode (OS_Memory 24, RISC OS 5.21)? */
static gboolean
user_readable (guint32 lo, int len)
{
  _kernel_swi_regs r;

  r.r[0] = 24;
  r.r[1] = (int) lo;
  r.r[2] = (int) (lo + len);
  return _kernel_swi (OS_Memory, &r, &r) == NULL && (r.r[1] & 1);
}

/* Read a string from another task (control-terminated, at most size - 1):
   a module task's directly (it is in the RMA, which every task sees, and
   Wimp_TransferBlock is for application slots), an application's with
   Wimp_TransferBlock */
static gboolean
task_string (int task, int me, guint32 addr, char *buf, int size)
{
  _kernel_swi_regs r;
  int i, len = size - 1;
  gboolean module = module_task (task);

  /* the string may end near the end of the memory: try shorter */
  for (; len >= 4; len /= 2)
    {
      gboolean ok;
      if (module)
        {
          ok = user_readable (addr, len);
          if (ok)
            memcpy (buf, (const void *) (long) addr, len);
        }
      else
        {
          r.r[0] = task;
          r.r[1] = (int) addr;
          r.r[2] = me;
          r.r[3] = (int) (long) buf;
          r.r[4] = len;
          ok = _kernel_swi (Wimp_TransferBlock, &r, &r) == NULL;
        }
      if (ok)
        {
          buf[len] = '\0';
          for (i = 0; i < len; i++)
            if ((unsigned char) buf[i] < 32)
              buf[i] = '\0';
          return TRUE;
        }
    }
  return FALSE;
}

static void
add_disc (GPtrArray *list, const char *fs, const char *name, const char *root, const char *sprite)
{
  GdkRiscosDisc *d;
  guint i;

  for (i = 0; i < list->len; i++)
    if (g_ascii_strcasecmp (((GdkRiscosDisc *) list->pdata[i])->root, root) == 0)
      return;
  d = g_new0 (GdkRiscosDisc, 1);
  d->fs = g_strdup (fs);
  d->name = g_strdup (name);
  d->root = g_strdup (root);
  d->sprite = sprite && *sprite ? g_strdup (sprite) : NULL;
  g_ptr_array_add (list, d);
}

static void
icon_bar_discs (GPtrArray *list, char **fss, GString *log)
{
  int handles[256], me, i, j, order[256], n = 0;
  int x0[256];
  _kernel_swi_regs r;

  r.r[0] = 5;
  if (_kernel_swi (Wimp_ReadSysInfo, &r, &r) != NULL)
    return;
  me = r.r[0];
  handles[0] = -1;
  r.r[0] = -2;
  r.r[1] = (int) (long) handles;
  r.r[2] = 1 << 23;           /* not deleted */
  r.r[3] = 0;
  if (_kernel_swi (Wimp_WhichIcon, &r, &r) != NULL || handles[0] == -1)
    {
      /* each icon handle in turn, to the first that isn't there */
      g_string_append (log, " (WhichIcon failed: by handle)");
      for (i = 0, n = 0; i < 255; i++)
        {
          int st[10];
          st[0] = -2;
          st[1] = i;
          r.r[1] = (int) (long) st;
          if (_kernel_swi (Wimp_GetIconState, &r, &r) != NULL)
            break;
          if (!(st[6] & (1 << 23)))
            handles[n++] = i;
        }
      handles[n] = -1;
      n = 0;
    }
  /* left to right */
  for (i = 0; i < 255 && handles[i] != -1; i++)
    {
      int st[10];
      st[0] = -2;
      st[1] = handles[i];
      r.r[1] = (int) (long) st;
      x0[i] = _kernel_swi (Wimp_GetIconState, &r, &r) == NULL ? st[2] : 0;
      order[n++] = i;
    }
  for (i = 1; i < n; i++)
    for (j = i; j > 0 && x0[order[j]] < x0[order[j - 1]]; j--)
      {
        int t = order[j];
        order[j] = order[j - 1];
        order[j - 1] = t;
      }
  for (j = 0; j < n; j++)
    {
      int st[10], msg[6], owner, flags;
      char text[64], valid[96], sprite[16], *root, *canon, *p;
      const char *fs, *task;

      st[0] = -2;
      st[1] = handles[order[j]];
      r.r[1] = (int) (long) st;
      if (_kernel_swi (Wimp_GetIconState, &r, &r) != NULL)
        continue;
      flags = st[6];
      if (!(flags & 1))
        continue;               /* no text: an application's icon */
      memset (msg, 0, sizeof msg);
      msg[0] = 20;
      r.r[0] = 19;
      r.r[1] = (int) (long) msg;
      r.r[2] = -2;
      r.r[3] = handles[order[j]];
      if (_kernel_swi (Wimp_SendMessage, &r, &r) != NULL)
        continue;
      owner = r.r[2];
      r.r[0] = owner;
      if (_kernel_swi (TaskManager_TaskNameFromHandle, &r, &r) != NULL || r.r[0] == 0)
        continue;
      task = (const char *) (long) r.r[0];
      text[0] = valid[0] = sprite[0] = '\0';
      if (flags & 0x100)
        {
          int size = st[9] > 0 ? MIN (st[9] + 1, (int) sizeof text) : (int) sizeof text;
          if (!task_string (owner, me, (guint32) st[7], text, size))
            {
              g_string_append_printf (log, "; %s: text at &%X unreadable", task, (unsigned) st[7]);
              continue;
            }
          if (st[8] != 0 && st[8] != -1)
            task_string (owner, me, (guint32) st[8], valid, sizeof valid);
        }
      else
        {
          memcpy (text, &st[7], 12);
          text[12] = '\0';
          for (i = 0; i < 12; i++)
            if ((unsigned char) text[i] < 32)
              text[i] = '\0';
        }
      /* the sprite: the validation string's S command */
      for (p = valid; *p; p++)
        if ((p == valid || p[-1] == ';') && (*p == 'S' || *p == 's'))
          {
            for (i = 0; p[1 + i] && p[1 + i] != ';' && p[1 + i] != ',' && i < 12; i++)
              sprite[i] = p[1 + i];
            sprite[i] = '\0';
            break;
          }
      g_strstrip (text);
      if (text[0] == '\0')
        continue;
      fs = fs_of_task (task, fss);
      g_string_append_printf (log, "; %s \"%s\"%s", task, text, fs ? "" : " (not a filing system's)");
      if (fs == NULL)
        {
          /* ResourceFS's Apps */
          if (g_ascii_strcasecmp (text, "Apps") == 0 && g_strstr_len (task, -1, "Resource"))
            add_disc (list, "Resources", text, "Resources:$.Apps", sprite);
          continue;
        }
      if (text[0] == ':')
        {
          /* a drive number (a floppy drive): not read until it's opened */
          char *name = g_strconcat (fs, ":", text, NULL);
          root = g_strconcat (fs, ":", text, ".$", NULL);
          add_disc (list, fs, name, root, sprite);
          g_free (name);
          g_free (root);
          continue;
        }
      root = g_strconcat (fs, "::", text, ".$", NULL);
      canon = canonical_or_null (root);
      if (canon)
        add_disc (list, fs, text, canon, sprite);
      else
        g_string_append_printf (log, " (%s not found)", root);
      g_free (canon);
      g_free (root);
    }
}

/* Filing systems that are devices, not directories */
static gboolean
fs_has_dirs (const char *fs)
{
  static const char *const no[] = {
    "null", "vdu", "rawvdu", "kbd", "rawkbd", "printer", "serial", "netprint",
    "pipe", "devices", "parallel", "fax", "socket", "netfs_stream", "pdumper",
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (no); i++)
    if (g_ascii_strcasecmp (fs, no[i]) == 0)
      return FALSE;
  return TRUE;
}

GPtrArray *
gdk_riscos_discs (void)
{
  GPtrArray *list = g_ptr_array_new_with_free_func (disc_free);
  char **fss = gdk_riscos_filing_systems ();
  GString *log = g_string_new (NULL);
  static char *last_log;
  int i;

  if (g_strcmp0 (g_getenv ("RISCOS_GTK_DISCS"), "all") != 0)
    icon_bar_discs (list, fss, log);
  if (list->len == 0)
    {
      g_string_append (log, "; none: every filing system's current disc");
      /* no icon bar to read: each filing system's current disc */
      for (i = 0; fss[i]; i++)
        {
          char *root, *canon;
          if (!fs_has_dirs (fss[i]))
            continue;
          root = g_strconcat (fss[i], ":$", NULL);
          canon = canonical_or_null (root);
          if (canon)
            {
              const char *disc = strstr (canon, "::");
              char *name = disc ? g_strndup (disc + 2, strcspn (disc + 2, ".")) : g_strdup (fss[i]);
              add_disc (list, fss[i], name, canon, NULL);
              g_free (name);
            }
          g_free (canon);
          g_free (root);
        }
    }
  /* what was found, in the Log (once, unless it changes) */
  for (i = 0; i < (int) list->len; i++)
    g_string_append_printf (log, "%s%s", i ? " " : "; discs: ", ((GdkRiscosDisc *) list->pdata[i])->root);
  if (g_strcmp0 (last_log, log->str) != 0)
    {
      g_printerr ("riscos-gtk: icon bar%s\n", log->str);
      g_free (last_log);
      last_log = g_strdup (log->str);
    }
  g_string_free (log, TRUE);
  g_strfreev (fss);
  return list;
}

/* ---- opening a file with the program the desktop runs for it ---- */

static int
mimemap (int from, const char *what)
{
  _kernel_swi_regs r;

  memset (&r, 0, sizeof r);
  r.r[0] = from;
  r.r[1] = (int) (long) what;
  r.r[2] = 0;
  if (what == NULL || *what == '\0' || _kernel_swi (MimeMap_Translate, &r, &r) != NULL ||
      r.r[3] < 0 || r.r[3] > 0xFFF)
    return -1;
  return r.r[3];
}

/**
 * gdk_riscos_open_file:
 * @path: the file, as the program names it (UnixLib, or RISC OS form)
 * @mime_type: (nullable): what it is, if known (an attachment's
 *   Content-Type, say)
 * @error: return location for an error
 *
 * Opens the file as a double-click in a Filer window would (Filer_Run),
 * so the program the desktop has for its type loads it. A file without a
 * type of its own (UnixLib writes text, &FFF, or data, &FFD) is first
 * given the type MimeMap has for @mime_type, or else for its extension.
 *
 * Returns: %TRUE if Filer_Run was started
 */
gboolean
gdk_riscos_open_file (const char *path, const char *mime_type, GError **error)
{
  char *ro, *cmd;
  const char *leaf, *ext;
  _kernel_swi_regs r;
  _kernel_oserror *e;
  int type = -1, now = -1;

  g_return_val_if_fail (path != NULL, FALSE);
  ro = path[0] == '/' ? gdk_riscos_path_from_unix (path) : g_strdup (path);
  memset (&r, 0, sizeof r);
  r.r[0] = 5;
  r.r[1] = (int) (long) ro;
  if ((e = _kernel_swi (OS_File, &r, &r)) != NULL || r.r[0] != 1)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "%s: %s", ro,
                   e ? e->errmess : "not a file");
      g_free (ro);
      return FALSE;
    }
  if (((unsigned) r.r[2] >> 20) == 0xFFF)
    now = ((unsigned) r.r[2] >> 8) & 0xFFF;
  if (now < 0 || now == 0xFFF || now == 0xFFD)
    {
      if (mime_type && g_ascii_strcasecmp (mime_type, "application/octet-stream") != 0)
        type = mimemap (2, mime_type);
      leaf = strrchr (ro, '.');
      leaf = leaf ? leaf + 1 : ro;
      ext = strrchr (leaf, '/');
      if (type < 0 && ext)
        type = mimemap (3, ext + 1);
      if (type >= 0 && type != now)
        {
          memset (&r, 0, sizeof r);
          r.r[0] = 18;
          r.r[1] = (int) (long) ro;
          r.r[2] = type;
          _kernel_swi (OS_File, &r, &r);
        }
    }
  cmd = g_strdup_printf ("Filer_Run %s", ro);
  memset (&r, 0, sizeof r);
  r.r[0] = (int) (long) cmd;
  e = _kernel_swi (Wimp_StartTask, &r, &r);
  if (e)
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s", e->errmess);
  g_free (cmd);
  g_free (ro);
  return e == NULL;
}

/**
 * gdk_riscos_show_uri:
 * @uri: a file:// URI, or a web (or other) address
 * @error: return location for an error
 *
 * What gtk_show_uri_on_window() does on RISC OS: a file is opened as the
 * Filer would (gdk_riscos_open_file), anything else goes to RISC OS's URI
 * handler (URI_Dispatch &4E381: R0 = 0 to dispatch now, R1 -> the URI;
 * R0 bit 0 set on return if nothing took it).
 */
gboolean
gdk_riscos_show_uri (const char *uri, GError **error)
{
  _kernel_swi_regs r;
  _kernel_oserror *e;

  g_return_val_if_fail (uri != NULL, FALSE);
  if (g_ascii_strncasecmp (uri, "file:", 5) == 0)
    {
      char *path = g_filename_from_uri (uri, NULL, error);
      gboolean ok = path && gdk_riscos_open_file (path, NULL, error);
      g_free (path);
      return ok;
    }
  memset (&r, 0, sizeof r);
  r.r[1] = (int) (long) uri;
  e = _kernel_swi (0x4E381, &r, &r);
  if (e == NULL && !(r.r[0] & 1))
    return TRUE;
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Nothing took the address %s%s%s",
               uri, e ? ": " : " (start a web browser, then try again)", e ? e->errmess : "");
  return FALSE;
}
