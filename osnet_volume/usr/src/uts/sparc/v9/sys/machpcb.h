/*
 * Copyright (c) 1990-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MACHPCB_H
#define	_SYS_MACHPCB_H

#pragma ident	"@(#)machpcb.h	1.16	98/12/18 SMI"

#include <sys/stack.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#if defined(lint)
#include <sys/thread.h>
#endif /* lint */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This file is machine dependent.
 */

/*
 * Machine dependent per-thread data.
 */
#define	MAXWIN	8	/* max # of windows currently supported */

/*
 * The system actually supports one more than the above number.
 * There is always one window reserved for trap handlers that
 * never has to be saved into the pcb struct.
 */

/*
 * Distance from beginning of thread stack (t_stk) to saved regs struct.
 */
#define	REGOFF	SA(MINFRAME)

#ifndef _ASM

/*
 * The struct machpcb is always allocated stack aligned.
 */
typedef struct machpcb {
	char	mpcb_frame[REGOFF];
	struct	regs mpcb_regs;	/* user's saved registers */
	caddr_t	mpcb_wbuf;	/* pointer to wbuf */
	caddr_t	mpcb_spbuf[MAXWIN]; /* sp's for each wbuf */
	struct	rwindow mpcb_rwin[2]; /* windows used while doing watchpoints */
	caddr_t	mpcb_rsp[2];	/* sp's for pcb_rwin[]  */
	int	mpcb_wbcnt;	/* number of saved windows in pcb_wbuf */
	uint_t	mpcb_wstate;	/* per-lwp %wstate */
	struct	v9_fpu *mpcb_fpu; /* fpu state */
	struct	fq mpcb_fpu_q[MAXFPQ]; /* fpu exception queue */
	caddr_t	mpcb_illexcaddr; /* address of last illegal instruction */
	uint_t	mpcb_illexcinsn; /* last illegal instruction */
	int	mpcb_flags;	/* various state flags */
	int	mpcb_wocnt;	/* window overflow count */
	int	mpcb_wucnt;	/* window underflow count */
	kthread_t *mpcb_thread;	/* associated thread */
} machpcb_t;
#endif /* ! _ASM */

/* mpcb_flags */
#define	FP_TRAPPED	0x04	/* fp_traps call caused by fp queue */

/*
 * We can use lwp_regs to find the mpcb base.
 */
#ifndef _ASM
#define	lwptompcb(lwp)	((struct machpcb *) \
	    ((caddr_t)(lwp)->lwp_regs - REGOFF))
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHPCB_H */
