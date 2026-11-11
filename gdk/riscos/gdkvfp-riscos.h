/* gdkvfp-riscos.h - riscos-gtk: the task's VFP context after
 * Wimp_PollIdle and Wimp_StartTask. Part of riscos-gtk's GTK backend:
 * LGPL version 2.1 or later.
 *
 * The Wimp switches UnixLib's VFP contexts with the task, but lazily: the
 * task's context is only activated at its first VFP instruction after
 * the poll, through VFPSupport's undefined-instruction (lazy) handler.
 * That handler restarts the instruction at lr - 4, right for ARM code but
 * 2 bytes early for Thumb code (lr is the instruction + 2 there), which
 * re-executes the instruction before it or lands mid-instruction: an
 * Illegal Instruction. riscos-unixlib's VFPPollT test shows it on a Pi 4
 * with RISC OS 5.31 (295 of 300 resumptions); the ARM build is clean.
 *
 * GDK's own code is ARM, but the poll function returns into the program,
 * which may be Thumb-2 (a browser was). So straight after the Wimp call,
 * GDK makes the context active itself, in ARM code, before any of the
 * program's floating point:
 *   - if the active (or lazily pending) context isn't the one that was
 *     active before the call, VFPSupport_ChangeContext puts it back
 *     (activate now; it saves the context it replaces);
 *   - then one VFP instruction in ARM state (VMRS from FPSCR), so a lazy
 *     activation happens here, where the restart is right.
 * Nothing else in here uses floating point.
 *
 * VFPSupport_ActiveContext SWI &58EC6: R0 = active context (0 = none).
 * VFPSupport_ChangeContext SWI &58EC3: R0 = context, R1 = flags (0 =
 * activate now); returns R0 = the previous one. */
#ifndef __GDK_VFP_RISCOS_H__
#define __GDK_VFP_RISCOS_H__

#include <kernel.h>

#define GDK_VFPSupport_ChangeContext 0x58EC3
#define GDK_VFPSupport_ActiveContext 0x58EC6

/* Our context now (call just before the Wimp call), or 0. */
static inline int
gdk_riscos_vfp_mine (void)
{
  _kernel_swi_regs r;
  if (_kernel_swi (GDK_VFPSupport_ActiveContext, &r, &r) != NULL)
    return 0;
  return r.r[0];
}

/* One VFP instruction in ARM state: activates a lazily pending context
   here, where VFPSupport's restart is right. */
static __attribute__ ((target ("arm"), noinline)) void
gdk_riscos_vfp_touch (void)
{
  unsigned int fpscr;
  __asm__ volatile ("vmrs %0, fpscr" : "=r" (fpscr));
  (void) fpscr;
}

/* After the Wimp call: CONTEXT (from gdk_riscos_vfp_mine) active again,
   and activated now rather than lazily. */
static inline void
gdk_riscos_vfp_reclaim (int context)
{
  _kernel_swi_regs r;
  if (context == 0)
    return;
  if (_kernel_swi (GDK_VFPSupport_ActiveContext, &r, &r) != NULL)
    return;
  if (r.r[0] != context)
    {
      r.r[0] = context;
      r.r[1] = 0;
      _kernel_swi (GDK_VFPSupport_ChangeContext, &r, &r);
    }
  gdk_riscos_vfp_touch ();
}

#endif
