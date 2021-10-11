/*
 * Copyright (c) 1990,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PCB_H
#define	_SYS_PCB_H

#pragma ident	"@(#)pcb.h	1.15	99/08/15 SMI"

#include <sys/reg.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
typedef struct fpu_ctx {
	struct fpu	fpu_regs;	/* save area for FPU */
	struct cpu	*fpu_cpu;	/* remember the last CPU/FPU it ran */
	int		fpu_flags;	/* FPU state flags */
} fpu_ctx_t;

typedef struct pcb {
	fpu_ctx_t	pcb_fpu;	/* fpu state */
	int		pcb_flags;	/* various state flags */
	dbregset_t 	pcb_dregs;	/* debug registers (0-7) */
	unsigned char	pcb_instr;	/* /proc: instruction at stop */
} pcb_t;

#endif /* ! _ASM */

/* pcb_flags */
#define	DEBUG_ON	0x01	/* debug registers are in use */
#define	DEBUG_PENDING	0x02	/* single-step of lcall for a sys call */
#define	DEBUG_MODIFIED	0x04	/* debug registers are modified (/proc) */
#define	INSTR_VALID	0x08	/* value in pcb_instr is valid (/proc) */
#define	NORMAL_STEP	0x10	/* normal debugger-requested single-step */
#define	WATCH_STEP	0x20	/* single-stepping in watchpoint emulation */
#define	CPC_OVERFLOW	0x40	/* performance counters overflowed */

/* fpu_flags */
#define	FPU_EN		0x1	/* flag signifying fpu in use */
#define	FPU_VALID	0x2	/* fpu_regs has valid fpu state */
#define	FPU_MODIFIED	0x4	/* fpu_regs is modified (/proc) */

#define	FPU_INVALID	0x0	/* fpu context is not in use */

/* fpu_flags */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PCB_H */
