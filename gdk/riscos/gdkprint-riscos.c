/*
 * gdkprint-riscos.c - riscos-gtk: printing through RISC OS's printer
 * drivers (the PDriver module and !Printers).
 *
 * GTK's side (the Print dialogue box, page layout, the preview) is
 * gtk/gtkprint-riscos.c; this is the Wimp and PDriver part:
 *
 *   - the printer: PDriver_Info (name, resolution, features) and
 *     PDriver_PageSize (paper size and printable area, millipoints);
 *   - the printing protocol with !Printers (the RISC OS 3 PRMs' "printer
 *     protocol", as DeskLib's Print library does it): broadcast
 *     Message_PrintSave (recorded);
 *       . it comes back unanswered: no printer manager, print directly if
 *         a printer driver is loaded;
 *       . Message_PrintError: the printer can't print now (its message);
 *       . Message_PrintFile: the printer is free; then
 *         Message_PrintTypeOdd: answer Message_PrintTypeKnown and print
 *         (if it doesn't come, print anyway);
 *       . Message_DataSaveAck: the printer is busy and !Printers wants a
 *         file to queue: refused ("the printer is busy");
 *   - the job: "printer:" opened for output, PDriver_SelectJob; the job is
 *     selected only while a page is printed and suspended (SelectJob 0)
 *     between pages, so that no Wimp_Poll (and no redraw, ours or another
 *     program's) happens with it selected;
 *   - each page: GTK draws it into a 32bpp sprite (90 dpi in its mode word,
 *     plotted scaled to the paper: OS_SpriteOp 52); PDriver_GiveRectangle
 *     with the whole paper (OS units, 1 = 400 millipoints, at the paper's
 *     bottom left), PDriver_DrawPage with the number of copies, plotting
 *     the sprite for each rectangle it asks for, until PDriver_GetRectangle
 *     says the page is done;
 *   - PDriver_EndJob (PDriver_AbortJob on an error), the file closed, the
 *     previous job selected again; the hourglass on while printing.
 *
 * The PDriver SWIs and the printer messages aren't in the PRM Reader
 * Edition 2026; numbers and registers as the RISC OS 3 PRMs and DeskLib's
 * PDriver veneers (PDriver SWIs &80140-&80154; messages &80140-&80147).
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

#include <stdlib.h>
#include <string.h>
#include <kernel.h>
#include <swis.h>

#define PDriver_Info          0x80140
#define PDriver_PageSize      0x80143
#define PDriver_SelectJob     0x80145
#define PDriver_EndJob        0x80148
#define PDriver_AbortJob      0x80149
#define PDriver_GiveRectangle 0x8014B
#define PDriver_DrawPage      0x8014C
#define PDriver_GetRectangle  0x8014D
#ifndef OS_Find
#define OS_Find               0x0D
#endif
#ifndef OS_SpriteOp
#define OS_SpriteOp           0x2E
#endif
#ifndef Wimp_SendMessage
#define Wimp_SendMessage      0x400E7
#endif
#define Hourglass_On          0x406C0
#define Hourglass_Off         0x406C1

#define USER_MESSAGE             17
#define USER_MESSAGE_RECORDED    18
#define USER_MESSAGE_ACKNOWLEDGE 19
#define MESSAGE_DATASAVEACK      2
#define MESSAGE_PRINTFILE        0x80140
#define MESSAGE_PRINTSAVE        0x80142
#define MESSAGE_PRINTERROR       0x80144
#define MESSAGE_PRINTTYPEODD     0x80145
#define MESSAGE_PRINTTYPEKNOWN   0x80146

#define MILLIPOINTS_PER_OS 400   /* 72000 a inch / 180 */

typedef enum {
  P_IDLE,
  P_ASKED,          /* PrintSave sent */
  P_FREE,           /* PrintFile: waiting for PrintTypeOdd */
  P_GO,             /* print now */
  P_NO_MANAGER,     /* PrintSave bounced */
  P_ERROR,          /* PrintError */
  P_BUSY            /* DataSaveAck: !Printers wants to queue */
} Protocol;

static struct {
  Protocol state;
  int ref;                      /* our PrintSave's my_ref */
  char error[240];
  int file;                     /* "printer:" */
  int old_job;
  char title[64];
  int page;
  int *area;                    /* the page sprite's area */
  int w, h;                     /* its size, pixels */
} pr;

static gboolean
debug (void)
{
  return g_getenv ("RISCOS_GTK_DEBUG") != NULL || g_getenv ("RISCOS_GTK_PRINT_DEBUG") != NULL;
}

/* RISC OS text (Latin-1) to UTF-8, without iconv */
static char *
from_latin1 (const char *s)
{
  GString *out = g_string_new (NULL);

  for (; *s; s++)
    g_string_append_unichar (out, (unsigned char) *s);
  return g_string_free (out, FALSE);
}

/* The job is selected only while we print a page, and suspended
   (PDriver_SelectJob 0) the rest of the time: while it's selected every
   graphics call, a redraw included, goes to the printer, so a Wimp_Poll
   then would send the desktop's redraws (ours and other programs') to the
   printer and leave the screen unredrawn */
static _kernel_oserror *
job_select (void)
{
  _kernel_swi_regs r;

  r.r[0] = pr.file;
  r.r[1] = (int) pr.title;
  return _kernel_swi (PDriver_SelectJob, &r, &r);
}

static void
job_suspend (void)
{
  _kernel_swi_regs r;

  r.r[0] = 0;
  r.r[1] = 0;
  _kernel_swi (PDriver_SelectJob, &r, &r);
}

static void
set_error (GError **error, _kernel_oserror *e, const char *fallback)
{
  char *text = e ? from_latin1 (e->errmess) : g_strdup (fallback);
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED, text);
  g_free (text);
}

/* ---- the printer --------------------------------------------------------- */

/**
 * gdk_riscos_printer_info:
 *
 * The printer set up in !Printers. Sizes in points (1/72 inch). Returns
 * FALSE (and leaves the others) if no printer driver is loaded.
 */
gboolean
gdk_riscos_printer_info (GdkRiscosPrinter *printer)
{
  _kernel_swi_regs r;
  char *name;

  _kernel_oserror *e;

  memset (printer, 0, sizeof *printer);
  if ((e = _kernel_swi (PDriver_Info, &r, &r)) != NULL)
    {
      if (debug ())
        g_printerr ("riscos-gtk: print: no printer driver (%s)\n", e->errmess);
      return FALSE;
    }
  printer->dpi_x = r.r[1];
  printer->dpi_y = r.r[2];
  printer->colour = (r.r[3] & 1) != 0;
  name = r.r[4] ? from_latin1 ((const char *) r.r[4]) : NULL;
  g_strlcpy (printer->name, name ? name : "Unknown printer", sizeof printer->name);
  g_free (name);
  if (_kernel_swi (PDriver_PageSize, &r, &r) == NULL)
    {
      printer->paper_width = r.r[1] / 1000.0;
      printer->paper_height = r.r[2] / 1000.0;
      printer->left = r.r[3] / 1000.0;
      printer->bottom = r.r[4] / 1000.0;
      printer->right = (r.r[1] - r.r[5]) / 1000.0;
      printer->top = (r.r[2] - r.r[6]) / 1000.0;
    }
  else
    {
      printer->paper_width = 595.276;    /* A4 */
      printer->paper_height = 841.89;
    }
  return TRUE;
}

/* ---- the protocol with !Printers ---------------------------------------- */

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

/* From the Wimp poll (gdkriscos-server.c): TRUE if it was ours */
gboolean
_gdk_riscos_print_message (int reason, int *block)
{
  if (pr.state != P_ASKED && pr.state != P_FREE)
    return FALSE;
  if (reason == USER_MESSAGE_ACKNOWLEDGE)
    {
      if (pr.state == P_ASKED && block[2] == pr.ref)
        {
          pr.state = P_NO_MANAGER;
          return TRUE;
        }
      return FALSE;
    }
  switch (block[4])
    {
    case MESSAGE_PRINTERROR:
      if (block[3] != pr.ref)
        return FALSE;
      if (block[0] > 24)
        g_strlcpy (pr.error, (const char *) &block[6], sizeof pr.error);
      else
        g_strlcpy (pr.error, "The printer is busy", sizeof pr.error);
      pr.state = P_ERROR;
      return TRUE;
    case MESSAGE_PRINTFILE:
      if (block[3] != pr.ref)
        return FALSE;
      pr.state = P_FREE;
      return TRUE;
    case MESSAGE_PRINTTYPEODD:
      {
        /* !Printers asks who prints this: we do */
        int peer = block[1];
        block[3] = block[2];
        block[4] = MESSAGE_PRINTTYPEKNOWN;
        send_message (USER_MESSAGE, block, peer);
        pr.state = P_GO;
        return TRUE;
      }
    case MESSAGE_DATASAVEACK:
      if (block[3] != pr.ref)
        return FALSE;
      pr.state = P_BUSY;
      return TRUE;
    default:
      return FALSE;
    }
}

/* Ask !Printers (Message_PrintSave), and wait for the answer */
static gboolean
ask_printers (const char *leafname, GError **error)
{
  int block[64];
  gint64 start, free_at = 0;

  memset (block, 0, sizeof block);
  block[0] = 256;
  block[4] = MESSAGE_PRINTSAVE;
  block[9] = 0;                     /* estimated size */
  block[10] = 0xFF9;                /* sprite */
  g_strlcpy ((char *) &block[11], leafname, 200);
  pr.error[0] = 0;
  pr.state = P_ASKED;
  pr.ref = send_message (USER_MESSAGE_RECORDED, block, 0);
  if (pr.ref == 0)
    {
      pr.state = P_NO_MANAGER;
      return TRUE;
    }
  start = g_get_monotonic_time ();
  while (pr.state == P_ASKED || pr.state == P_FREE)
    {
      gint64 now = g_get_monotonic_time ();
      if (pr.state == P_FREE && free_at == 0)
        free_at = now;
      /* the printer's free but no PrintTypeOdd: print anyway */
      if (pr.state == P_FREE && now - free_at > 2 * G_USEC_PER_SEC)
        pr.state = P_GO;
      else if (pr.state == P_ASKED && now - start > 10 * G_USEC_PER_SEC)
        pr.state = P_NO_MANAGER;
      else
        g_main_context_iteration (NULL, TRUE);
    }
  if (debug ())
    g_printerr ("riscos-gtk: print: !Printers says %d%s%s\n", pr.state,
                pr.error[0] ? ": " : "", pr.error);
  switch (pr.state)
    {
    case P_ERROR:
      {
        char *t = from_latin1 (pr.error);
        set_error (error, NULL, t);
        g_free (t);
      }
      pr.state = P_IDLE;
      return FALSE;
    case P_BUSY:
      set_error (error, NULL, "The printer is busy. Try again when it has finished.");
      pr.state = P_IDLE;
      return FALSE;
    default:
      return TRUE;                  /* P_GO, or no !Printers: try the driver */
    }
}

/**
 * gdk_riscos_print_begin:
 * @title: the job's name (UTF-8)
 *
 * Asks !Printers, opens the printer and starts a job.
 */
gboolean
gdk_riscos_print_begin (const char *title, GError **error)
{
  _kernel_swi_regs r;
  _kernel_oserror *e;
  char latin1[64];

  if (_kernel_swi (PDriver_Info, &r, &r) != NULL)
    {
      set_error (error, NULL, "There's no printer: load !Printers and choose one.");
      return FALSE;
    }
  gdk_riscos_utf8_to_latin1 (title ? title : "GTK", latin1, sizeof latin1);
  if (!ask_printers (latin1, error))
    return FALSE;
  pr.state = P_IDLE;
  r.r[0] = 0x80;                    /* open for output */
  r.r[1] = (int) "printer:";
  if ((e = _kernel_swi (OS_Find, &r, &r)) != NULL || r.r[0] == 0)
    {
      set_error (error, e, "Can't open the printer");
      return FALSE;
    }
  pr.file = r.r[0];
  g_strlcpy (pr.title, latin1, sizeof pr.title);
  r.r[0] = pr.file;
  r.r[1] = (int) pr.title;
  if ((e = _kernel_swi (PDriver_SelectJob, &r, &r)) != NULL)
    {
      set_error (error, e, "Can't start printing");
      r.r[0] = 0;
      r.r[1] = pr.file;
      _kernel_swi (OS_Find, &r, &r);
      pr.file = 0;
      return FALSE;
    }
  pr.old_job = r.r[0];
  pr.page = 0;
  job_suspend ();                   /* until a page is printed */
  _kernel_swi (Hourglass_On, &r, &r);
  if (debug ())
    g_printerr ("riscos-gtk: print: job \"%s\" started (file %d)\n", latin1, pr.file);
  return TRUE;
}

/**
 * gdk_riscos_print_page_surface:
 *
 * A white page width x height pixels for GTK to draw, kept for the job.
 * The pixels are the page sprite's own (cairo's RGB24, turned into the
 * sprite's order when the page is printed).
 */
cairo_surface_t *
gdk_riscos_print_page_surface (int width, int height)
{
  int bytes = 16 + 44 + width * height * 4;
  int *spr;
  cairo_surface_t *s;

  if (pr.area == NULL || pr.w != width || pr.h != height)
    {
      g_free (pr.area);
      pr.area = g_try_malloc (bytes);
      if (pr.area == NULL)
        return NULL;
      pr.w = width;
      pr.h = height;
      pr.area[0] = bytes;
      pr.area[1] = 1;
      pr.area[2] = 16;
      pr.area[3] = bytes;
      spr = pr.area + 4;
      spr[0] = 44 + width * height * 4;
      memset (&spr[1], 0, 12);
      memcpy (&spr[1], "page", 4);
      spr[4] = width - 1;           /* words - 1 (32 bpp: one word a pixel) */
      spr[5] = height - 1;
      spr[6] = 0;
      spr[7] = 31;
      spr[8] = 44;
      spr[9] = 44;                  /* no mask */
      spr[10] = (int) ((6u << 27) | (90u << 14) | (90u << 1) | 1u);   /* 32 bpp, 90 dpi */
    }
  memset (pr.area + 4 + 11, 0xFF, (size_t) width * height * 4);
  s = cairo_image_surface_create_for_data ((unsigned char *) (pr.area + 4 + 11), CAIRO_FORMAT_RGB24,
                                           width, height, width * 4);
  return s;
}

/**
 * gdk_riscos_print_page:
 * @paper_width, @paper_height: the paper, points
 * @copies: how many
 *
 * Prints the page drawn into the page surface.
 */
gboolean
gdk_riscos_print_page (double paper_width, double paper_height, int copies, GError **error)
{
  _kernel_swi_regs r;
  _kernel_oserror *e;
  int rect[4], out[4], matrix[4] = { 65536, 0, 0, 65536 }, pos[2] = { 0, 0 };
  int scale[4], more, id, n;
  guint32 *p;
  char page[12];

  if (pr.area == NULL || pr.file == 0)
    {
      set_error (error, NULL, "Not printing");
      return FALSE;
    }
  /* cairo's 0xXXRRGGBB words to the sprite's &00BBGGRR */
  p = (guint32 *) (pr.area + 4 + 11);
  for (n = pr.w * pr.h; n--; p++)
    *p = ((*p >> 16) & 0xFF) | (*p & 0xFF00) | ((*p & 0xFF) << 16);
  pr.page++;
  if ((e = job_select ()) != NULL)
    goto fail;
  rect[0] = 0;
  rect[1] = 0;
  rect[2] = (int) (paper_width * 1000.0 / MILLIPOINTS_PER_OS + 0.5);
  rect[3] = (int) (paper_height * 1000.0 / MILLIPOINTS_PER_OS + 0.5);
  r.r[0] = 1;
  r.r[1] = (int) rect;
  r.r[2] = (int) matrix;
  r.r[3] = (int) pos;
  r.r[4] = (int) 0xFFFFFF00u;       /* white */
  if ((e = _kernel_swi (PDriver_GiveRectangle, &r, &r)) != NULL)
    goto fail;
  g_snprintf (page, sizeof page, "%d", pr.page);
  r.r[0] = MAX (1, copies);
  r.r[1] = (int) out;
  r.r[2] = pr.page;
  r.r[3] = (int) page;
  if ((e = _kernel_swi (PDriver_DrawPage, &r, &r)) != NULL)
    goto fail;
  more = r.r[0];
  id = r.r[2];
  /* the sprite (2 OS units a pixel at 90 dpi) scaled to the paper */
  scale[0] = rect[2];
  scale[1] = rect[3];
  scale[2] = pr.w * 2;
  scale[3] = pr.h * 2;
  while (more)
    {
      if (id == 1)
        {
          r.r[0] = 512 + 52;        /* put sprite scaled, by pointer */
          r.r[1] = (int) pr.area;
          r.r[2] = (int) (pr.area + 4);
          r.r[3] = 0;
          r.r[4] = 0;
          r.r[5] = 0;               /* overwrite */
          r.r[6] = (int) scale;
          r.r[7] = 0;
          if ((e = _kernel_swi (OS_SpriteOp, &r, &r)) != NULL)
            goto fail;
        }
      r.r[1] = (int) out;
      if ((e = _kernel_swi (PDriver_GetRectangle, &r, &r)) != NULL)
        goto fail;
      more = r.r[0];
      id = r.r[2];
    }
  job_suspend ();
  if (debug ())
    g_printerr ("riscos-gtk: print: page %d (%d x %d pixels, %d x %d OS units), %d copies\n",
                pr.page, pr.w, pr.h, rect[2], rect[3], copies);
  return TRUE;

fail:
  set_error (error, e, "Printing failed");
  job_suspend ();
  return FALSE;
}

/**
 * gdk_riscos_print_end:
 * @abort: stop the job (an error, or cancelled)
 */
void
gdk_riscos_print_end (gboolean abort)
{
  _kernel_swi_regs r;

  if (pr.file)
    {
      job_select ();
      r.r[0] = pr.file;
      _kernel_swi (abort ? PDriver_AbortJob : PDriver_EndJob, &r, &r);
      r.r[0] = 0;
      r.r[1] = pr.file;
      _kernel_swi (OS_Find, &r, &r);
      if (pr.old_job)
        {
          r.r[0] = pr.old_job;
          r.r[1] = 0;
          _kernel_swi (PDriver_SelectJob, &r, &r);
        }
      _kernel_swi (Hourglass_Off, &r, &r);
      if (debug ())
        g_printerr ("riscos-gtk: print: job %s, %d pages\n", abort ? "abandoned" : "done", pr.page);
    }
  pr.file = 0;
  pr.old_job = 0;
  g_free (pr.area);
  pr.area = NULL;
  pr.state = P_IDLE;
}
