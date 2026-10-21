/*
 * insprof.c - a QEMU TCG plugin for linux-user: how many guest
 * instructions each translated block ran. At exit it writes
 * "start_address instructions" lines, one per block, to the file named
 * by the plugin argument out=FILE (default insprof.out). tools/profile/
 * symsum.py adds them up per function. Instruction counts, not times.
 *   qemu-arm -plugin ./insprof.so,out=prof.txt PROGRAM ...
 * With callers=ADDR:ADDR... (hex) it also counts, for each call into
 * those blocks, the block that ran just before (the caller's), and
 * writes "caller ADDR FROM count" lines.
 * Build: gcc -O2 -shared -fPIC -I<dir of qemu-plugin.h> insprof.c -o insprof.so
 * MIT licence.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

typedef struct Block { uint64_t pc; uint64_t insns; struct Block *next; } Block;
static Block *blocks;
static const char *out = "insprof.out";
#define MAXT 16
static uint64_t targets[MAXT];
static int n_targets;
static uint64_t last_pc;
typedef struct Edge { uint64_t to, from, n; struct Edge *next; } Edge;
static Edge *edges;

static void
on_exec (unsigned int vcpu, void *udata)
{
  uint64_t pc = (uint64_t) (uintptr_t) udata;
  int i;
  for (i = 0; i < n_targets; i++)
    if (pc == targets[i])
      {
        Edge *e;
        for (e = edges; e; e = e->next)
          if (e->to == pc && e->from == last_pc)
            break;
        if (!e)
          {
            e = calloc (1, sizeof *e);
            e->to = pc; e->from = last_pc;
            e->next = edges; edges = e;
          }
        e->n++;
      }
  last_pc = pc;
}

static void
tb_trans (qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
  Block *b = calloc (1, sizeof *b);
  b->pc = qemu_plugin_tb_vaddr (tb);
  b->next = blocks;
  blocks = b;               /* translation is serialised in linux-user */
  qemu_plugin_register_vcpu_tb_exec_inline (tb, QEMU_PLUGIN_INLINE_ADD_U64, &b->insns,
                                            qemu_plugin_tb_n_insns (tb));
  if (n_targets)
    qemu_plugin_register_vcpu_tb_exec_cb (tb, on_exec, QEMU_PLUGIN_CB_NO_REGS,
                                          (void *) (uintptr_t) b->pc);
}

static void
at_exit (qemu_plugin_id_t id, void *p)
{
  FILE *f = fopen (out, "w");
  Block *b;
  if (!f)
    return;
  for (b = blocks; b; b = b->next)
    if (b->insns)
      fprintf (f, "%llx %llu\n", (unsigned long long) b->pc, (unsigned long long) b->insns);
  {
    Edge *e;
    for (e = edges; e; e = e->next)
      fprintf (f, "caller %llx %llx %llu\n", (unsigned long long) e->to,
               (unsigned long long) e->from, (unsigned long long) e->n);
  }
  fclose (f);
}

QEMU_PLUGIN_EXPORT int
qemu_plugin_install (qemu_plugin_id_t id, const qemu_info_t *info, int argc, char **argv)
{
  int i;
  for (i = 0; i < argc; i++)
    if (strncmp (argv[i], "out=", 4) == 0)
      out = strdup (argv[i] + 4);
    else if (strncmp (argv[i], "callers=", 8) == 0)
      {
        char *p = argv[i] + 8;
        while (*p && n_targets < MAXT)
          {
            targets[n_targets++] = strtoull (p, &p, 16);
            if (*p == ':')
              p++;
          }
      }
  qemu_plugin_register_vcpu_tb_trans_cb (id, tb_trans);
  qemu_plugin_register_atexit_cb (id, at_exit, NULL);
  return 0;
}
