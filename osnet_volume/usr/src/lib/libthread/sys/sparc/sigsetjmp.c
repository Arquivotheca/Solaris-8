/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)sigsetjmp.c	1.5	98/08/07 SMI"

#ifdef __STDC__
#pragma weak siglongjmp	= _siglongjmp
#pragma weak _ti_siglongjmp	= _siglongjmp
#endif /* __STDC__ */

#include "libthread.h"
#include <ucontext.h>
#include <setjmp.h>

#define	JB_SAVEMASK	0x1
/*
 * _sigsetjmp() sets up the jmp buffer so that on a subsequent _siglongjmp()
 * to this buffer, control first goes to __thr_jmpret() below. The stack
 * has been carefully crafted so that on return from __thr_jmpret(), the
 * thread returns to the caller of _sigsetjmp(), at the place right after
 * the call. __thr_jmpret() is needed for correct switching of signal masks
 * in an MT process.
 */
static int
__thr_jmpret(int val, sigjmp_buf env)
{
	register sigjmp_struct_t *bp = (sigjmp_struct_t *)env;

	if (bp->sjs_flags & JB_SAVEMASK)
		thr_sigsetmask(SIG_SETMASK, &(bp->sjs_sigmask), NULL);
	return (val);
}

/*
 * Note that the following routine is quite fast for unbound threads since it
 * does not have to call the kernel (for __getcontext()) to support saving
 * of the alternate signal stack. Alternate signal stacks are supposed to
 * work only for bound threads, so why pay the extra expense for unbound
 * threads?
 * With the kernel call eliminated, obtaining the caller's sp and pc can no
 * longer reliably be done by the method of reading the saved values from the
 * stack since the windows may not have been flushed yet. So, the entry point
 * for _sigsetjmp() is in _sigsetjmp.s, written in assembler to get the
 * caller's sp and pc. These are stored in the sigjmp buffer "env" before
 * calling __csigsetjmp() below.
 */
int
__csigsetjmp(sigjmp_buf env, int savemask)
{
	register sigjmp_struct_t *bp = (sigjmp_struct_t *)env;

	if (ISBOUND(curthread)) {
		ucontext_t uc;

		getcontext(&uc);
		bp->sjs_stack	= uc.uc_stack;
	}
	bp->sjs_flags	= 0;
	if (savemask) {
		bp->sjs_flags |= JB_SAVEMASK;
		/*
		 * Save current thread's signal mask, not the LWP's mask
		 * for this thread.
		 */
		bp->sjs_sigmask = curthread->t_hold;
	}
	return (0);
}

void
_siglongjmp(sigjmp_buf env, int val)
{
	ucontext_t uc;
	register greg_t *reg = uc.uc_mcontext.gregs;
	register sigjmp_struct_t *bp = (sigjmp_struct_t *)env;

	/*
	 * Get the current context to use as a starting point to construct
	 * the sigsetjmp context. It might perhaps be more precise to
	 * constuct an entire context from scratch, but this method is
	 * closer to old (undocumented) semantics.
	 * XXX: we should really eliminate the call to __getcontext()
	 * below, since the values so obtained in uc are not really
	 * used except to call setcontext() below. Instead, we should
	 * just start with a zeroed ucontext.
	 */
	uc.uc_flags = UC_ALL;
	__getcontext(&uc);
	if (bp->sjs_flags & JB_SAVEMASK) {
		/*
		 * If the jmp buf has a saved signal mask, the current mask
		 * cannot be changed until the longjmp to the new context
		 * occurs. Until then, to make the signal mask change atomic
		 * with respect to the switch, mask all signals on the current
		 * thread, which flushes all signals for this thread. Now,
		 * this thread should not receive any signals until the mask
		 * switch occurs in __thr_jmpret().
		 */
		thr_sigsetmask(SIG_SETMASK, &_totalmasked, NULL);
		reg[REG_PC] = (greg_t)&__thr_jmpret;
		/*
		 * make %o7 on entry to __thr_jmpret(), point to the return
		 * pc as if the caller of _sigsetjmp() called __thr_jmpret().
		 * So that on return from __thr_jmpret(), control goes to the
		 * caller of _sigsetjmp().
		 */
		reg[REG_O7] = bp->sjs_pc - 0x8;
		reg[REG_O1] = (greg_t)(env);	/* 2nd arg to __thr_jmpret() */
	} else {
		reg[REG_PC] = bp->sjs_pc;
	}
	/*
	 * Use the information in the sigjmp_buf to modify the current
	 * context to execute as though we called __thr_jmpret() from
	 * the caller of _sigsetjmp().
	 */
	uc.uc_stack = bp->sjs_stack;
	reg[REG_nPC] = reg[REG_PC] + 0x4;
	reg[REG_SP] = bp->sjs_sp;

	if (val)
		reg[REG_O0] = (greg_t)val;
	else
		reg[REG_O0] = (greg_t)1;	/* 1st arg to __thr_jmpret() */

	reg[REG_G2] = bp->sjs_g2;
	reg[REG_G3] = bp->sjs_g3;
#ifndef __sparcv9
	reg[REG_G4] = bp->sjs_g4;
#endif /* __sparcv9 */
	setcontext(&uc);
}
