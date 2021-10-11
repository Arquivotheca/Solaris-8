/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.1	99/10/14 SMI"

/*
 * Blatently stolen from libc
 */

#include "liblwp.h"
#include <ucontext.h>

extern void **getpriptr(int);

void
setup_context(ucontext_t *ucp, void (*func)(void *), void *arg,
	ulwp_t *ulwp, caddr_t stk, size_t stksize)
{
	static int initialized;
	static greg_t fs, es, ds, cs, ss;

	uint32_t *stack;

	if (!initialized) {
		ucontext_t uc;

		/* do this once to load the segment registers */
		(void) _getcontext(&uc);
		fs = uc.uc_mcontext.gregs[FS];
		es = uc.uc_mcontext.gregs[ES];
		ds = uc.uc_mcontext.gregs[DS];
		cs = uc.uc_mcontext.gregs[CS];
		ss = uc.uc_mcontext.gregs[SS];
		initialized = 1;
	}
	/* clear the context and set the segment registers */
	(void) _memset(ucp, 0, sizeof (*ucp));
	ucp->uc_mcontext.gregs[FS] = fs;
	ucp->uc_mcontext.gregs[ES] = es;
	ucp->uc_mcontext.gregs[DS] = ds;
	ucp->uc_mcontext.gregs[CS] = cs;
	ucp->uc_mcontext.gregs[SS] = ss;
	if (ulwp->ul_gs == 0)
		ulwp->ul_gs = __setupgs(ulwp);
	if (ulwp->ul_gs == 0)
		panic("cannot assign segment register for new thread");
	*getpriptr(ulwp->ul_gs) = ulwp;
	ucp->uc_mcontext.gregs[GS] = (greg_t)ulwp->ul_gs;

	/* top-of-stack must be rounded down to STACK_ALIGN */
	stack = (uint32_t *)(((uintptr_t)stk + stksize) & ~(STACK_ALIGN-1));

	/* set up top stack frame */
	*--stack = 0;
	*--stack = 0;
	*--stack = (uint32_t)arg;
	*--stack = (uint32_t)_lwp_start;

	/* fill in registers of interest */
	ucp->uc_flags |= UC_CPU;
	ucp->uc_mcontext.gregs[EIP] = (greg_t)func;
	ucp->uc_mcontext.gregs[UESP] = (greg_t)stack;
	ucp->uc_mcontext.gregs[EBP] = (greg_t)(stack+2);

	/* set the non-volatile regs for thr_getstate() */
	setgregs(ulwp, ucp->uc_mcontext.gregs);
	ulwp->ul_validregs = 1;
}

void
getgregs(ulwp_t *ulwp, gregset_t rs)
{
	rs[EIP] = ulwp->ul_savedregs.rs_pc;
	rs[EDI] = ulwp->ul_savedregs.rs_edi;
	rs[ESI] = ulwp->ul_savedregs.rs_esi;
	rs[EBP] = ulwp->ul_savedregs.rs_bp;
	rs[EBX] = ulwp->ul_savedregs.rs_ebx;
	rs[UESP] = ulwp->ul_savedregs.rs_sp;
}

void
setgregs(ulwp_t *ulwp, gregset_t rs)
{
	ulwp->ul_savedregs.rs_pc  = rs[EIP];
	ulwp->ul_savedregs.rs_edi = rs[EDI];
	ulwp->ul_savedregs.rs_esi = rs[ESI];
	ulwp->ul_savedregs.rs_bp  = rs[EBP];
	ulwp->ul_savedregs.rs_ebx = rs[EBX];
	ulwp->ul_savedregs.rs_sp  = rs[UESP];
}
