/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_X_CALL_H
#define	_SYS_X_CALL_H

#pragma ident	"@(#)x_call.h	1.18	99/04/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	XCALL_PIL 13	/* prom uses 14, and error handling uses 15 */

#ifndef _ASM

#include <sys/cpuvar.h>

#if defined(_KERNEL)

#define	CPU_XCALL_READY(cpuid)			\
	(CPU_IN_SET(cpu_ready_set, (cpuid)))

extern cpuset_t cpu_ready_set;	/* cpus ready for x-calls */

/*
 * Cross-call function prototype.
 */
typedef void xcfunc_t(uint64_t, uint64_t);

/*
 * Cross-call routines.
 */
extern void xt_one(int, xcfunc_t *, uint64_t, uint64_t);
extern void xt_some(cpuset_t, xcfunc_t *, uint64_t, uint64_t);
extern void xt_all(xcfunc_t *, uint64_t, uint64_t);
extern void xt_sync(cpuset_t);
extern void xc_attention(cpuset_t);
extern void xc_dismissed(cpuset_t);
extern void xc_one(int, xcfunc_t *, uint64_t, uint64_t);
extern void xc_some(cpuset_t, xcfunc_t *, uint64_t, uint64_t);
extern void xc_all(xcfunc_t *, uint64_t, uint64_t);
extern void xc_init(void);
extern void xt_sync_tl1(void);
extern void idle_stop_xcall(void);

#endif	/* _KERNEL */

#endif	/* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_X_CALL_H */
