/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)getcontext.c	1.23	99/03/08 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/ucontext.h>
#include <sys/asm_linkage.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/archsystm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/debug.h>
#include <sys/model.h>
#include <sys/cmn_err.h>

#include <sys/privregs.h>


/*
 * Save user context.
 */
void
savecontext(
	ucontext_t *ucp,
	k_sigset_t mask)
{
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	fpregset_t *fp;

	bzero(ucp, sizeof (ucontext_t));
	(void) flush_user_windows_to_stack(NULL);

	ucp->uc_flags = UC_ALL;
	ucp->uc_link = (ucontext_t *)lwp->lwp_oldcontext;

	/*
	 * Save current stack state.
	 */
	if (lwp->lwp_sigaltstack.ss_flags == SS_ONSTACK)
		ucp->uc_stack = lwp->lwp_sigaltstack;
	else {
		ucp->uc_stack.ss_sp = (caddr_t)p->p_usrstack - p->p_stksize;
		ucp->uc_stack.ss_size = p->p_stksize;
		ucp->uc_stack.ss_flags = 0;
	}

	/*
	 * Save machine context.
	 */
	getgregs(lwp, ucp->uc_mcontext.gregs);
#ifdef __sparcv9
	if (p->p_model == DATAMODEL_LP64)
		getasrs(lwp, ucp->uc_mcontext.asrs);
	else
#endif
		xregs_getgregs(lwp, xregs_getptr(lwp, ucp));

	/*
	 * If we are using the floating point unit, save state
	 */
	fp = &ucp->uc_mcontext.fpregs;
	getfpregs(lwp, fp);
#ifdef __sparcv9
	if (p->p_model == DATAMODEL_LP64)
		getfpasrs(lwp, ucp->uc_mcontext.asrs);
	else
#endif
		xregs_getfpregs(lwp, xregs_getptr(lwp, ucp));
	if (!fp->fpu_en)
		ucp->uc_flags &= ~UC_FPU;
	ucp->uc_mcontext.gwins = (gwindows_t *)NULL;

	/*
	 * Save signal mask.
	 */
	sigktou(&mask, &ucp->uc_sigmask);
}


void
restorecontext(ucontext_t *ucp)
{
	klwp_id_t lwp = ttolwp(curthread);
	mcontext_t *mcp = &ucp->uc_mcontext;
#ifdef __sparcv9
	model_t model = lwp_getdatamodel(lwp);
#endif

	(void) flush_user_windows_to_stack(NULL);
	if (lwp->lwp_pcb.pcb_xregstat != XREGNONE)
		xregrestore(lwp, 0);

	lwp->lwp_oldcontext = (uintptr_t)ucp->uc_link;

	if (ucp->uc_flags & UC_STACK) {
		if (ucp->uc_stack.ss_flags == SS_ONSTACK)
			lwp->lwp_sigaltstack = ucp->uc_stack;
		else
			lwp->lwp_sigaltstack.ss_flags &= ~SS_ONSTACK;
	}

	if (ucp->uc_flags & UC_CPU) {
		if (mcp->gwins != 0)
			setgwins(lwp, mcp->gwins);
		setgregs(lwp, mcp->gregs);
#ifdef __sparcv9
		if (model == DATAMODEL_LP64)
			setasrs(lwp, mcp->asrs);
		else
#endif
			xregs_setgregs(lwp, xregs_getptr(lwp, ucp));
	}

	if (ucp->uc_flags & UC_FPU) {
		fpregset_t *fp = &ucp->uc_mcontext.fpregs;

		setfpregs(lwp, fp);
#ifdef __sparcv9
		if (model == DATAMODEL_LP64)
			setfpasrs(lwp, mcp->asrs);
		else
#endif
			xregs_setfpregs(lwp, xregs_getptr(lwp, ucp));
		run_fpq(lwp, fp);
	}

	if (ucp->uc_flags & UC_SIGMASK) {
		sigutok(&ucp->uc_sigmask, &curthread->t_hold);
		sigdiffset(&curthread->t_hold, &cantmask);
		aston(curthread);	/* so thread will see new t_hold */
	}
}


int
getsetcontext(int flag, ucontext_t *ucp)
{
	ucontext_t uc;
	struct fq fpu_q[MAXFPQ]; /* to hold floating queue */
	fpregset_t *fpp;
	gwindows_t *gwin = NULL;	/* to hold windows */
	caddr_t xregs = NULL;
	int xregs_size = 0;
	extern int nwindows;

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
		xregs_clrptr(curthread->t_lwp, &uc);
		savecontext(&uc, curthread->t_hold);
		/*
		 * When using floating point it should not be possible to
		 * get here with a fpu_qcnt other than zero since we go
		 * to great pains to handle all outstanding FP exceptions
		 * before any system call code gets executed. However we
		 * clear fpu_q and fpu_qcnt here before copyout anyway -
		 * this will prevent us from interpreting the garbage we
		 * get back (when FP is not enabled) as valid queue data on
		 * a later setcontext(2).
		 */
		uc.uc_mcontext.fpregs.fpu_qcnt = 0;
		uc.uc_mcontext.fpregs.fpu_q = (struct fq *)NULL;
		if (copyout(&uc, ucp, sizeof (ucontext_t)))
			return (set_errno(EFAULT));
		return (0);

	case SETCONTEXT:
		if (ucp == NULL)
			exit(CLD_EXITED, 0);
		/*
		 * Don't copyin filler or floating state unless we need it.
		 * The ucontext_t struct and fields are specified in the ABI.
		 */
		if (copyin(ucp, &uc, sizeof (ucontext_t) -
		    sizeof (uc.uc_filler) -
		    sizeof (uc.uc_mcontext.fpregs) -
		    sizeof (uc.uc_mcontext.xrs) -
#ifdef __sparcv9
		    sizeof (uc.uc_mcontext.asrs) -
#endif
		    sizeof (uc.uc_mcontext.filler))) {
			return (set_errno(EFAULT));
		}
		if (copyin(&ucp->uc_mcontext.xrs, &uc.uc_mcontext.xrs,
		    sizeof (uc.uc_mcontext.xrs))) {
			return (set_errno(EFAULT));
		}
		fpp = &uc.uc_mcontext.fpregs;
		if (uc.uc_flags & UC_FPU) {
			/*
			 * Need to copyin floating point state
			 */
			if (copyin(&ucp->uc_mcontext.fpregs,
			    &uc.uc_mcontext.fpregs,
			    sizeof (uc.uc_mcontext.fpregs)))
				return (set_errno(EFAULT));
			/* if floating queue not empty */
			if ((fpp->fpu_q) && (fpp->fpu_qcnt)) {
				if (fpp->fpu_qcnt > MAXFPQ ||
				    fpp->fpu_q_entrysize <= 0 ||
				    fpp->fpu_q_entrysize > sizeof (struct fq))
					return (set_errno(EINVAL));
				if (copyin(fpp->fpu_q, fpu_q,
				    fpp->fpu_qcnt * fpp->fpu_q_entrysize))
					return (set_errno(EFAULT));
				fpp->fpu_q = fpu_q;
			} else {
				fpp->fpu_qcnt = 0; /* avoid confusion later */
			}
		} else {
			fpp->fpu_qcnt = 0;
		}
		if (uc.uc_mcontext.gwins) {	/* if windows in context */
			size_t gwin_size;

			/*
			 * We do the same computation here to determine
			 * how many bytes of gwindows_t to copy in that
			 * is also done in sendsig() to decide how many
			 * bytes to copy out.  We just *know* that wbcnt
			 * is the first element of the structure.
			 */
			gwin = kmem_zalloc(sizeof (gwindows_t), KM_SLEEP);
			if (copyin(uc.uc_mcontext.gwins,
			    &gwin->wbcnt, sizeof (gwin->wbcnt))) {
				kmem_free(gwin, sizeof (gwindows_t));
				return (set_errno(EFAULT));
			}
			if (gwin->wbcnt < 0 || gwin->wbcnt > nwindows) {
				kmem_free(gwin, sizeof (gwindows_t));
				return (set_errno(EINVAL));
			}
			gwin_size = gwin->wbcnt * sizeof (struct rwindow) +
			    SPARC_MAXREGWINDOW * sizeof (int *) + sizeof (long);
			if (gwin_size > sizeof (gwindows_t) ||
			    copyin(uc.uc_mcontext.gwins, gwin, gwin_size)) {
				kmem_free(gwin, sizeof (gwindows_t));
				return (set_errno(EFAULT));
			}
			uc.uc_mcontext.gwins = gwin;
		}

		/*
		 * get extra register state or asrs if any exists
		 * there is no extra register state for _LP64 user programs
		 */
#ifdef __sparcv9
		xregs_clrptr(curthread->t_lwp, &uc);
		if (copyin(&ucp->uc_mcontext.asrs, &uc.uc_mcontext.asrs,
		    sizeof (asrset_t))) {
			/* Free up gwin structure if used */
			if (gwin)
				kmem_free(gwin, sizeof (gwindows_t));
			return (set_errno(EFAULT));
		}
#else
		if (xregs_hasptr(curthread->t_lwp, &uc) &&
		    ((xregs_size = xregs_getsize(curproc)) > 0)) {
			xregs = kmem_zalloc(xregs_size, KM_SLEEP);
			if (copyin(xregs_getptr(curthread->t_lwp, &uc),
			    xregs, xregs_size)) {
				if (gwin)
					kmem_free(gwin, sizeof (gwindows_t));
				kmem_free(xregs, xregs_size);
				return (set_errno(EFAULT));
			}
			xregs_setptr(curthread->t_lwp, &uc, xregs);
		} else {
			xregs_clrptr(curthread->t_lwp, &uc);
		}
#endif  /* __sparcv9 */

		restorecontext(&uc);

		/*
		 * free extra register state area
		 */
		if (xregs_size)
			kmem_free(xregs, xregs_size);

		if (gwin)
			kmem_free(gwin, sizeof (gwindows_t));

		return (0);
	}
}


#ifdef _SYSCALL32_IMPL

int
getsetcontext32(int flag, ucontext32_t *ucp)
{
	ucontext32_t uc;
	ucontext_t   ucnat;
	struct regs *rp = lwptoregs(curthread->t_lwp);
	struct fq fpu_qnat[MAXFPQ]; /* to hold "native" floating queue */
	struct fq32 fpu_q[MAXFPQ]; /* to hold 32 bit floating queue */
	fpregset32_t *fpp;
	gwindows32_t *gwin = NULL;	/* to hold windows */
	caddr_t xregs;
	int xregs_size = 0;
	extern int nwindows;

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
		xregs_clrptr(curthread->t_lwp, &ucnat);
		savecontext(&ucnat, curthread->t_hold);
		/*
		 * When using floating point it should not be possible to
		 * get here with a fpu_qcnt other than zero since we go
		 * to great pains to handle all outstanding FP exceptions
		 * before any system call code gets executed. However we
		 * clear fpu_q and fpu_qcnt here before copyout anyway -
		 * this will prevent us from interpreting the garbage we
		 * get back (when FP is not enabled) as valid queue data on
		 * a later setcontext(2).
		 */
		ucnat.uc_mcontext.fpregs.fpu_qcnt = 0;
		ucnat.uc_mcontext.fpregs.fpu_q = (struct fq *)NULL;
		ucontext_nto32(&ucnat, &uc, rp->r_tstate, NULL);
		if (copyout(&uc, ucp, sizeof (ucontext32_t)))
			return (set_errno(EFAULT));
		return (0);

	case SETCONTEXT:
		if (ucp == NULL)
			exit(CLD_EXITED, 0);
		/*
		 * Don't copyin filler or floating state unless we need it.
		 * The ucontext_t struct and fields are specified in the ABI.
		 */
		if (copyin(ucp, &uc, sizeof (uc) - sizeof (uc.uc_filler) -
		    sizeof (uc.uc_mcontext.fpregs) -
		    sizeof (uc.uc_mcontext.xrs) -
		    sizeof (uc.uc_mcontext.filler))) {
			return (set_errno(EFAULT));
		}
		if (copyin(&ucp->uc_mcontext.xrs, &uc.uc_mcontext.xrs,
		    sizeof (uc.uc_mcontext.xrs))) {
			return (set_errno(EFAULT));
		}
		fpp = &uc.uc_mcontext.fpregs;
		if (uc.uc_flags & UC_FPU) {
			/*
			 * Need to copyin floating point state
			 */
			if (copyin(&ucp->uc_mcontext.fpregs,
			    &uc.uc_mcontext.fpregs,
			    sizeof (uc.uc_mcontext.fpregs)))
				return (set_errno(EFAULT));
			/* if floating queue not empty */
			if ((fpp->fpu_q) && (fpp->fpu_qcnt)) {
				if (fpp->fpu_qcnt > MAXFPQ ||
				    fpp->fpu_q_entrysize <= 0 ||
				    fpp->fpu_q_entrysize > sizeof (struct fq32))
					return (set_errno(EINVAL));
				if (copyin((void *)fpp->fpu_q, fpu_q,
				    fpp->fpu_qcnt * fpp->fpu_q_entrysize))
					return (set_errno(EFAULT));
			} else {
				fpp->fpu_qcnt = 0; /* avoid confusion later */
			}
		} else {
			fpp->fpu_qcnt = 0;
		}

		if (uc.uc_mcontext.gwins) {	/* if windows in context */
			size_t gwin_size;

			/*
			 * We do the same computation here to determine
			 * how many bytes of gwindows_t to copy in that
			 * is also done in sendsig() to decide how many
			 * bytes to copy out.  We just *know* that wbcnt
			 * is the first element of the structure.
			 */
			gwin = kmem_zalloc(sizeof (gwindows32_t),
							KM_SLEEP);
			if (copyin((void *)uc.uc_mcontext.gwins,
			    &gwin->wbcnt, sizeof (gwin->wbcnt))) {
				kmem_free(gwin, sizeof (gwindows32_t));
				return (set_errno(EFAULT));
			}
			if (gwin->wbcnt < 0 || gwin->wbcnt > nwindows) {
				kmem_free(gwin, sizeof (gwindows32_t));
				return (set_errno(EINVAL));
			}
			gwin_size = gwin->wbcnt * sizeof (struct rwindow32) +
			    SPARC_MAXREGWINDOW * sizeof (caddr32_t) +
			    sizeof (int32_t);
			if (gwin_size > sizeof (gwindows32_t) ||
			    copyin((void *)uc.uc_mcontext.gwins,
			    gwin, gwin_size)) {
				kmem_free(gwin, sizeof (gwindows32_t));
				return (set_errno(EFAULT));
			}
			/* restorecontext() should ignore this */
			uc.uc_mcontext.gwins = (caddr32_t)0;
		}

		ucontext_32ton(&uc, &ucnat, fpu_q, fpu_qnat);

		/*
		 * get extra register state if any exists
		 */
		if (xregs_hasptr32(curthread->t_lwp, &uc) &&
		    ((xregs_size = xregs_getsize(curproc)) > 0)) {
			xregs = kmem_zalloc(xregs_size, KM_SLEEP);
			if (copyin((void *)
			    xregs_getptr32(curthread->t_lwp, &uc),
			    xregs, xregs_size)) {
				if (gwin)
					kmem_free(gwin, sizeof (gwindows32_t));
				return (set_errno(EFAULT));
			}
			xregs_setptr(curthread->t_lwp, &ucnat, xregs);
		} else {
			xregs_clrptr(curthread->t_lwp, &ucnat);
		}

		restorecontext(&ucnat);
		if (gwin)
			setgwins32(curthread->t_lwp, gwin);

		/*
		 * free extra register state area
		 */
		if (xregs_size)
			kmem_free(xregs, xregs_size);

		if (gwin)
			kmem_free(gwin, sizeof (gwindows32_t));

		return (0);
	}
}

#endif	/* _SYSCALL32_IMPL */
