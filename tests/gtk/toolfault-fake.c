/*
 * toolfault-fake.c - riscos-gtk: a tool sprite block that runs into
 * memory that isn't there, on the fake Wimp.
 *
 * Wimp_ReadSysInfo 9's block claims more memory than is mapped: the last
 * page of its claimed size can't be read, and none of the ways that use
 * pointers finds "uicon", so the scan by name walks up to it. Seen on
 * RISC OS on Linux, under load, as a data abort in the scan.
 *   1. OS_Memory 24 answering truly: the scan stops at the page.
 *   2. OS_Memory 24 saying every range is readable (as it must have
 *      there): the read faults, the guard catches it, and the program
 *      carries on without window border sprites, with its own handler
 *      for the signal put back.
 *
 * Each case runs in a child process (the tool sprites are looked for
 * once per program). Prints TAP; exit status = failures. MIT licence.
 */
#include <gdk/gdk.h>
#include <gdk/gdkriscos.h>
#include <kernel.h>
#include "fake_riscos.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

static int n_tests, failures;

static void
check (int ok, const char *name)
{
  n_tests++;
  if (!ok)
    failures++;
  printf ("%sok %d - %s\n", ok ? "" : "not ", n_tests, name);
  fflush (stdout);
}

static void
own_handler (int sig)
{
  (void) sig;
}

/* 0: no sprite and the program's handler kept; 1: a sprite came back;
   2: the handler changed */
static int
child (int lie)
{
  long pg = sysconf (_SC_PAGESIZE);
  char *mem = mmap (NULL, 4 * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  int *block = (int *) mem;
  struct sigaction sa, now;
  GdkPixbuf *pb;

  memset (mem, 0, 4 * pg);
  mprotect (mem + 3 * pg, pg, PROT_NONE);
  block[0] = 4 * pg;               /* claims the unreadable page too */
  block[1] = 90;
  block[2] = 0x0042F3A4;           /* not offsets: not a plain area */
  block[3] = 0x00438124;
  block[4] = block[5] = 0;         /* no table of pointers */
  fake_tool_area_override = block;
  fake_memory_all_readable = lie;

  memset (&sa, 0, sizeof sa);
  sa.sa_handler = own_handler;
  sigaction (SIGSEGV, &sa, NULL);
  pb = gdk_riscos_wimp_tool_sprite ("uicon");
  sigaction (SIGSEGV, NULL, &now);
  if (pb)
    return 1;
  if (now.sa_handler != own_handler)
    return 2;
  /* asked again: still none, without reading the block again */
  return gdk_riscos_wimp_tool_sprite ("vbarmid") ? 1 : 0;
}

int
main (void)
{
  int lie;

  for (lie = 0; lie <= 1; lie++)
    {
      pid_t pid = fork ();
      int status = -1;

      if (pid == 0)
        _exit (child (lie));
      waitpid (pid, &status, 0);
      if (!lie)
        check (WIFEXITED (status) && WEXITSTATUS (status) == 0,
               "with OS_Memory 24 right, nothing past the memory that is there is read");
      else
        {
          check (WIFEXITED (status), "with OS_Memory 24 wrong, the fault is caught (the program carries on)");
          check (WIFEXITED (status) && WEXITSTATUS (status) == 0,
                 "no window border sprites, and the program's own SIGSEGV handler is back");
        }
    }
  printf ("1..%d\n", n_tests);
  return failures;
}
