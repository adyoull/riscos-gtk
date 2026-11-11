/* gdkvfp-riscos.h - riscos-gtk: keeping the task's VFP context across
 * Wimp_PollIdle and Wimp_StartTask. Part of riscos-gtk's GTK backend:
 * LGPL version 2.1 or later.
 *
 * UnixLib makes its VFP contexts as user-mode ones, not application-space
 * ones (VFPSupport_CreateContext R0 bit 0 only), so VFPSupport leaves them
 * alone when the Wimp switches tasks. When Wimp_PollIdle (or Wimp_StartTask,
 * which runs the new task until it polls) returns, the active context is
 * whatever the last task left: another program's (this task would then
 * compute on its registers), or none at all (a UnixLib program that ended:
 * SharedUnixLibrary's exit calls VFPSupport_ChangeContext 0, which turns
 * VFP off, so the next VFP instruction is an undefined instruction). That
 * lasts until UnixLib's next thread switch activates our context again,
 * and a program's first floating point after the poll can come first.
 *
 * So: read the active context (ours) before the call, and put it back
 * straight after, before any floating point (VFPSupport_ChangeContext
 * saves the context it replaces, so the other task loses nothing). No
 * floating point in here: only integer code and SWIs.
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

/* Make CONTEXT active again if the Wimp call left another (or none). */
static inline void
gdk_riscos_vfp_reclaim (int context)
{
  _kernel_swi_regs r;
  if (context == 0)
    return;
  if (_kernel_swi (GDK_VFPSupport_ActiveContext, &r, &r) != NULL ||
      r.r[0] == context)
    return;
  r.r[0] = context;
  r.r[1] = 0;
  _kernel_swi (GDK_VFPSupport_ChangeContext, &r, &r);
}

#endif
