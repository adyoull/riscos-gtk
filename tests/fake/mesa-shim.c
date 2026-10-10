/*
 * mesa-shim.c - lets riscos-mesa's RISC OS libraries (the devkit's
 * libEGL.a and libOSMesa.a, built by GCCSDK for RISC OS) link into the
 * ARM Linux tests, so GtkGLArea runs on the real Mesa and EGL under
 * qemu-arm, against the fake RISC OS (tests/gtk/gltest-fake.c).
 *
 * The code is plain ARM EABI hard-float on both; only a few UnixLib
 * symbols differ:
 *   - __stdin/__stdout/__stderr and __ctype: UnixLib's names for the
 *     standard streams and its character class table;
 *   - errno: a plain global in UnixLib, thread-local in glibc;
 *   - pthread mutexes: UnixLib lays them out differently from glibc.
 *     EGL and Mesa are only used from the test's main thread, so
 *     build-linuxarm.sh renames their mutex calls (objcopy
 *     --redefine-sym) to the do-nothing stand-ins below. pthread_once
 *     and keys are ints in both, so glibc's are used;
 *   - the shader JIT's code memory (ro_jit_code_memory / ro_jit_sync).
 * _kernel_swi and friends are the fake RISC OS's (fake_riscos.c), which
 * answers VFPSupport_Features as a Cortex-A8.
 *
 * Adapted from riscos-mesa's tests/host-harness/mesa/arm/shim.c (MIT).
 * MIT licence.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/mman.h>

int ro_errno_shim;

int ro_pthread_mutex_init_shim (void *m, const void *a) { (void) m; (void) a; return 0; }
int ro_pthread_mutex_lock_shim (void *m) { (void) m; return 0; }
int ro_pthread_mutex_unlock_shim (void *m) { (void) m; return 0; }
int ro_pthread_mutex_destroy_shim (void *m) { (void) m; return 0; }
int ro_pthread_mutexattr_init_shim (void *a) { (void) a; return 0; }
int ro_pthread_mutexattr_settype_shim (void *a, int t) { (void) a; (void) t; return 0; }
int ro_pthread_mutexattr_destroy_shim (void *a) { (void) a; return 0; }

FILE *__stdin, *__stdout, *__stderr;
static unsigned char tbl[384];
const unsigned char *const __ctype = tbl + 128;

__attribute__ ((constructor)) static void
init (void)
{
  int c;

  __stdin = stdin;
  __stdout = stdout;
  __stderr = stderr;
  for (c = 0; c < 256; c++)
    {
      unsigned char f = 0;
      if (iscntrl (c)) f |= 1;
      if (isupper (c)) f |= 2;
      if (islower (c)) f |= 4;
      if (isalpha (c)) f |= 8;
      if (ispunct (c)) f |= 16;
      if (isspace (c)) f |= 32;
      if (isdigit (c)) f |= 64;
      if (isxdigit (c)) f |= 128;
      tbl[128 + c] = f;
    }
}

static void *jit_mem;
static size_t jit_bytes = 512 * 1024;

void *
ro_jit_code_memory (size_t *bytes)
{
  jit_mem = mmap (NULL, jit_bytes, PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (jit_mem == MAP_FAILED)
    return NULL;
  *bytes = jit_bytes;
  return jit_mem;
}

void
ro_jit_sync (void *start, void *end)
{
  __builtin___clear_cache (start, end);
}
