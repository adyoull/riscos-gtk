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
