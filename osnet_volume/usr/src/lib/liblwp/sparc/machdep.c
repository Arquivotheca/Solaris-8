/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.1	99/10/14 SMI"

/*
 * Blatently stolen from libc
 */

#include "liblwp.h"
#include <sys/types.h>
#include <signal.h>
#include <ucontext.h>

extern int __getcontext(ucontext_t *);
extern greg_t _getsp(void);

void
setup_context(ucontext_t *ucp, void (*func)(void *), void *arg,
	ulwp_t *ulwp, caddr_t stk, size_t stksize)
{
	/*
	 * Top-of-stack must be rounded down to STACK_ALIGN and
	 * there must be a minimum frame for the register window.
	 */
	uintptr_t stack = (((uintptr_t)stk + stksize) & ~(STACK_ALIGN - 1)) -
	    SA(MINFRAME);

	/* clear the context and the top stack frame */
	(void) _memset(ucp, 0, sizeof (*ucp));
	(void) _memset((void *)stack, 0, SA(MINFRAME));

	/* fill in registers of interest */
	ucp->uc_flags |= UC_CPU;
	ucp->uc_mcontext.gregs[REG_PC] = (greg_t)func;
	ucp->uc_mcontext.gregs[REG_nPC] = (greg_t)func + 4;
	ucp->uc_mcontext.gregs[REG_O0] = (greg_t)arg;
	ucp->uc_mcontext.gregs[REG_SP] = (greg_t)(stack - STACK_BIAS);
	ucp->uc_mcontext.gregs[REG_O7] = (greg_t)_lwp_start;
	ucp->uc_mcontext.gregs[REG_G7] = (greg_t)ulwp;

	/* set the non-volatile regs for thr_getstate() */
	setgregs(ulwp, ucp->uc_mcontext.gregs);
	ulwp->ul_validregs = 1;
}

void
getgregs(ulwp_t *ulwp, gregset_t rs)
{
	rs[REG_PC] = ulwp->ul_savedregs.rs_pc;
	rs[REG_O6] = ulwp->ul_savedregs.rs_sp;
	rs[REG_O7] = ulwp->ul_savedregs.rs_o7;
	rs[REG_G1] = ulwp->ul_savedregs.rs_g1;
	rs[REG_G2] = ulwp->ul_savedregs.rs_g2;
	rs[REG_G3] = ulwp->ul_savedregs.rs_g3;
	rs[REG_G4] = ulwp->ul_savedregs.rs_g4;
}

void
setgregs(ulwp_t *ulwp, gregset_t rs)
{
	ulwp->ul_savedregs.rs_pc = rs[REG_PC];
	ulwp->ul_savedregs.rs_sp = rs[REG_O6];
	ulwp->ul_savedregs.rs_o7 = rs[REG_O7];
	ulwp->ul_savedregs.rs_g1 = rs[REG_G1];
	ulwp->ul_savedregs.rs_g2 = rs[REG_G2];
	ulwp->ul_savedregs.rs_g3 = rs[REG_G3];
	ulwp->ul_savedregs.rs_g4 = rs[REG_G4];
}
