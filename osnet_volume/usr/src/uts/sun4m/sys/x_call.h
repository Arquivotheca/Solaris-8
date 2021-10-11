/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_X_CALL_H
#define	_SYS_X_CALL_H

#pragma ident	"@(#)x_call.h	1.18	99/04/13 SMI"

/*
 * For sun4m, we only have two cross call levels:
 * a low priority one which uses a level-1 soft interrupt
 * (printf uses this to print using CPU0), and a high
 * priority one which uses a level-15 soft interrupt
 * (the TLB/VAC consistency code uses this).
 */
#include <sys/xc_levels.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * States of a cross-call session. (stored in xc_state field of the
 * per-CPU area).
 */
#define	XC_DONE		0	/* x-call session done */
#define	XC_HOLD		1	/* spin doing nothing */
#define	XC_SYNC_OP	2	/* perform a synchronous operation */
#define	XC_CALL_OP	3	/* perform a call operation */
#define	XC_WAIT		4	/* capture/release. callee has seen wait */

#ifndef _ASM

#include <sys/cpuvar.h>

struct	xc_mbox {
	int	(*func)();
	int	arg1;
	int	arg2;
	int	arg3;
	cpuset_t set;
	int	saved_pri;
};

/*
 * Cross-call routines.
 */

#if defined(_KERNEL)

extern void	xc_init(void);
extern void	xc_serv(int);
extern void	xc_call(int, int, int, int, cpuset_t, int (*)());
extern void	xc_sync(int, int, int, int, cpuset_t, int (*)());
extern void	xc_prolog(cpuset_t);
extern void	xc_sync_cache(int, int, int, int (*)());
extern void	xc_epilog(void);
extern void	xc_capture_cpus(cpuset_t);
extern void	xc_release_cpus(void);
extern int	xc_level_ignore;
#endif	/* _KERNEL */

#endif	/* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_X_CALL_H */
