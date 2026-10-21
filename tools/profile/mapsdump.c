/* mapsdump.c - LD_PRELOAD into a profiled program (linux-user guest):
   at exit, copies /proc/self/maps to $INSPROF_MAPS, so symsum.py can name
   the code in glibc and ld.so. MIT licence. */
#include <stdio.h>
#include <stdlib.h>
__attribute__((destructor)) static void
dump (void)
{
  const char *to = getenv ("INSPROF_MAPS");
  FILE *in, *out;
  int c;
  if (!to || !(in = fopen ("/proc/self/maps", "r")))
    return;
  if ((out = fopen (to, "w")))
    {
      while ((c = fgetc (in)) != EOF)
        fputc (c, out);
      fclose (out);
    }
  fclose (in);
}
