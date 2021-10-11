/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PCB_H
#define	_SYS_PCB_H

#pragma ident	"@(#)pcb.h	1.32	99/08/15 SMI"

#include <sys/regset.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Sun software process control block
 */

#ifndef _ASM
typedef struct pcb {
	int	pcb_flags;	/* various state flags */
	uint32_t pcb_trap0addr;	/* addr of user level trap 0 handler */
				/* deliberately restricted to 32 bits */
				/* because only used for SunOS programs */
	uint_t	pcb_instr;	/* /proc: instruction at stop */
	enum { XREGNONE = 0, XREGPRESENT, XREGMODIFIED }
		pcb_xregstat;	/* state of contents of pcb_xregs */
	struct	rwindow pcb_xregs; /* locals+ins fetched/set via /proc */
	int	pcb_step;	/* used while single-stepping */
	caddr_t	pcb_tracepc;	/* used while single-stepping */
} pcb_t;
#endif /* ! _ASM */

/* pcb_flags */
#define	INSTR_VALID	0x02	/* value in pcb_instr is valid (/proc) */
#define	NORMAL_STEP	0x04	/* normal debugger requested single-step */
#define	WATCH_STEP	0x08	/* single-stepping in watchpoint emulation */
#define	CPC_OVERFLOW	0x10	/* performance counters overflowed */

/* pcb_step */
#define	STEP_NONE	0	/* no single step */
#define	STEP_REQUESTED	1	/* arrange to single-step the lwp */
#define	STEP_ACTIVE	2	/* actively patching addr, set active flag */
#define	STEP_WASACTIVE	3	/* wrap up after taking single-step fault */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCB_H */
