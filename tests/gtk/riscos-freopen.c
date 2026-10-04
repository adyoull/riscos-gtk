/*
 * riscos-freopen.c - linked into the QEMU tests with -Wl,--wrap=freopen.
 * MIT licence.
 */
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* RISC OS allows one write-open per file; Linux doesn't care. hellotest is
   linked with -Wl,--wrap=freopen so that a second write-mode freopen of a
   file that is still open for writing fails here as it does on RISC OS
   (GTKHello 0.1/0.2 opened its log twice and quit with an empty log). */
FILE *__real_freopen (const char *, const char *, FILE *);
static struct { char path[512]; FILE *f; } wopen[8];
FILE *
__wrap_freopen (const char *path, const char *mode, FILE *f)
{
  int i, w = path && (strchr (mode, 'w') || strchr (mode, 'a') || strchr (mode, '+'));
  FILE *r;
  for (i = 0; i < 8; i++)
    if (wopen[i].f == f)
      wopen[i].f = NULL;                /* freopen closes f first */
  if (w)
    for (i = 0; i < 8; i++)
      if (wopen[i].f && strcmp (wopen[i].path, path) == 0)
        {
          errno = EACCES;               /* RISC OS: "File open" */
          return NULL;
        }
  r = __real_freopen (path, mode, f);
  if (r && w)
    for (i = 0; i < 8; i++)
      if (!wopen[i].f)
        {
          g_strlcpy (wopen[i].path, path, sizeof wopen[i].path);
          wopen[i].f = r;
          break;
        }
  return r;
}

