/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getcontext.c	1.8	99/05/04 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/reg.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/psw.h>
#include <sys/ucontext.h>
#include <sys/asm_linkage.h>
#include <sys/errno.h>
#include <sys/archsystm.h>
#include <sys/debug.h>

/*
 * Save user context.
 */
void
savecontext(
	register ucontext_t *ucp,
	k_sigset_t mask)
{
	register proc_t *p = ttoproc(curthread);
	register klwp_t *lwp = ttolwp(curthread);
	struct regs *regs;

	regs = (struct regs *)lwp->lwp_regs;

	ucp->uc_flags = UC_ALL;
	ucp->uc_link = (struct ucontext *)lwp->lwp_oldcontext;

	/*
	 * Save current stack state.
	 */
	if (lwp->lwp_sigaltstack.ss_flags == SS_ONSTACK)
		ucp->uc_stack = lwp->lwp_sigaltstack;
	else {
		ucp->uc_stack.ss_sp = (caddr_t)USRSTACK - p->p_stksize;
		ucp->uc_stack.ss_size = p->p_stksize;
		ucp->uc_stack.ss_flags = 0;
	}

	/*
	 * Save machine context.
	 */
	getgregs(lwp, ucp->uc_mcontext.gregs);

	/*
	 * If we are using the floating point unit, save state
	 */
	if (lwp->lwp_pcb.pcb_fpu.fpu_flags & FPU_EN)
		getfpregs(lwp, &ucp->uc_mcontext.fpregs);
	else
		ucp->uc_flags &= ~UC_FPU;
	/*
	 * Save signal mask.
	 */
	sigktou(&mask, &ucp->uc_sigmask);
	regs->r_efl &= ~PS_T; 		/* disable single step XXX */
}

/*
 * Restore user context.
 */
void
restorecontext(ucontext_t *ucp)
{

	register klwp_id_t lwp = ttolwp(curthread);

	lwp->lwp_oldcontext = (uintptr_t)ucp->uc_link;

	if (ucp->uc_flags & UC_STACK) {
		if (ucp->uc_stack.ss_flags == SS_ONSTACK)
			lwp->lwp_sigaltstack = ucp->uc_stack;
		else
			lwp->lwp_sigaltstack.ss_flags &= ~SS_ONSTACK;
	}

	if (ucp->uc_flags & UC_CPU) {
		setgregs(lwp, ucp->uc_mcontext.gregs);
		lwp->lwp_eosys = JUSTRETURN;
		curthread->t_post_sys = 1;
	}

	if (ucp->uc_flags & UC_FPU)
		setfpregs(lwp, &ucp->uc_mcontext.fpregs);

	if (ucp->uc_flags & UC_SIGMASK) {
		sigutok(&ucp->uc_sigmask, &curthread->t_hold);
		sigdiffset(&curthread->t_hold, &cantmask);
		aston(curthread); /* so post_syscall() will see new t_hold */
	}
}


int
getsetcontext(int flag, ucontext_t *ucp)
{
	ucontext_t uc;

	/*
	 * In future releases, when the ucontext structure grows,
	 * getcontext should be modified to only return the fields
	 * specified in the uc_flags.  That way, the structure can grow
	 * and still be binary compatible will all .o's which will only
	 * have old fields defined in uc_flags
	 */

	switch (flag) {
	default:
		return (set_errno(EINVAL));

	case GETCONTEXT:
		savecontext(&uc, curthread->t_hold);
		if (copyout((caddr_t)&uc, (caddr_t)ucp,
		    sizeof (ucontext_t)))
			return (set_errno(EFAULT));
		return (0);

	case SETCONTEXT:
		if (ucp == NULL)
			exit(CLD_EXITED, 0);
		/*
		 * Don't copyin filler or floating state unless we need it.
		 * The ucontext_t struct and fields are specified in the ABI.
		 */
		if (copyin((caddr_t)ucp, (caddr_t)&uc,
			sizeof (ucontext_t) - sizeof (uc.uc_filler) -
			sizeof (uc.uc_mcontext.fpregs))) {
			return (set_errno(EFAULT));
		}
		if (uc.uc_flags & UC_FPU) {
			/*
			 * Need to copyin floating point state
			 */
			if (copyin((caddr_t)&ucp->uc_mcontext.fpregs,
			    (caddr_t)&uc.uc_mcontext.fpregs,
			    sizeof (uc.uc_mcontext.fpregs)))
				return (set_errno(EFAULT));
		}
		restorecontext(&uc);
		return (0);
	}
}
