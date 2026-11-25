/*
 * zlibbench.c - riscos-gtk's zlib with and without NEON, timed on the
 * machine it runs on. Run it twice: as it is, then with
 * *Set RISCOS_GTK_NO_NEON 1 (the !Run file of !ZlibBench does both and
 * writes <Wimp$ScrapDir>.ZlibBench). Inflates 4 MB of text-like data and
 * of picture-like data 10 times each, and takes the Adler-32 of 4 MB
 * 20 times. MIT licence.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

extern int z_riscos_neon (void);

#define N (4 << 20)

static double
secs (clock_t t)
{
  return (double) (clock () - t) / CLOCKS_PER_SEC;
}

static void
inflate_test (const char *name, unsigned char *in, unsigned char *z, unsigned char *out)
{
  uLongf zl = compressBound (N), ol;
  clock_t t;
  int i;

  compress2 (z, &zl, in, N, 6);
  t = clock ();
  for (i = 0; i < 10; i++)
    {
      ol = N;
      if (uncompress (out, &ol, z, zl) != Z_OK || ol != N || memcmp (out, in, N))
        {
          printf ("%s: inflate FAILED\n", name);
          return;
        }
    }
  printf ("%-8s inflate 10 x 4 MB (%lu KB compressed): %.2f s\n", name,
          (unsigned long) zl / 1024, secs (t));
}

int
main (void)
{
  unsigned char *in = malloc (N), *z = malloc (compressBound (N)), *out = malloc (N);
  unsigned seed = 1, a = 1;
  clock_t t;
  int i;

  if (!in || !z || !out)
    {
      printf ("not enough memory\n");
      return 1;
    }
  printf ("zlib %s, NEON %s\n", zlibVersion (), z_riscos_neon () ? "on" : "off");
  for (i = 0; i < N; i++)
    in[i] = "the quick brown fox jumps over the lazy dog 0123456789\n"[(i * 7 + i / 999) % 54];
  inflate_test ("text", in, z, out);
  for (i = 0; i < N; i++)
    {
      seed = seed * 1103515245u + 12345u;
      /* rows of a picture: runs of the pixel above, some noise */
      in[i] = i >= 4096 && (seed >> 16) % 8 ? in[i - 4096] : (unsigned char) (seed >> 20);
    }
  inflate_test ("picture", in, z, out);
  t = clock ();
  for (i = 0; i < 20; i++)
    a = adler32 (a, in, N);
  printf ("adler32 20 x 4 MB: %.2f s (%08x)\n", secs (t), a);
  return 0;
}
