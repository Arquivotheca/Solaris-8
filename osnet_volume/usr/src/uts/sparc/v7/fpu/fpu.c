/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fpu.c	1.43	99/11/20 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/fault.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/core.h>
#include <sys/pcb.h>
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/disp.h>
#include <sys/stack.h>
#include <sys/cmn_err.h>
#include <sys/privregs.h>

#include <sys/fpu/fpu_simulator.h>
#include <sys/fpu/globals.h>
#include <sys/fpu/fpusystm.h>

static void fp_free(struct fpu *, int);
static void fp_fork(kthread_id_t, kthread_id_t);

int fpdispr = 0;

static void
fp_null()
{}

void
fp_core(corep)
	struct core *corep;
{
	unsigned int i;
	register klwp_id_t lwp = ttolwp(curthread);
	struct fpu *fp;

	fp = lwptofpu(lwp);
	corep->c_fpu = *fp;
	corep->c_fpu.fpu_q = corep->c_fpu_q;
	for (i = 0; i < fp->fpu_qcnt; i++)
		corep->c_fpu_q[i] = fp->fpu_q[i];
}

/*
 * For use by procfs to save the floating point context of the thread.
 */
void
fp_prsave(fp)
	struct fpu *fp;
{
	register klwp_id_t lwp = ttolwp(curthread);

	if (fp->fpu_en) {
		kpreempt_disable();
		if (fpu_exists && CPU->cpu_fpowner == lwp) {
			fp_fksave(fp);
			CPU->cpu_fpowner = NULL;
		}
		kpreempt_enable();
	}
}

/*
 * Copy the floating point context of the forked thread.
 */
static void
fp_fork(t, ct)
	kthread_id_t t, ct;
{
	struct fpu *cfp, *pfp;
	unsigned char i;

	cfp = lwptofpu(ct->t_lwp);
	pfp = lwptofpu(t->t_lwp);

	/*
	 * copy the parents fpq
	 */
	cfp->fpu_qcnt = pfp->fpu_qcnt;
	for (i = 0; i < pfp->fpu_qcnt; i++)
		cfp->fpu_q[i] = pfp->fpu_q[i];

	/*
	 * save the context of the parent into the childs fpu structure
	 */
	fp_fksave(cfp);
	cfp->fpu_en = 1;
	lwptoregs(ct->t_lwp)->r_psr |= PSR_EF;
	installctx(ct, cfp, fp_save, fp_restore, fp_fork, fp_null, fp_free);
}

/*
 * Create a floating-point context for an lwp.
 */
void
fp_installctx(klwp_t *lwp, struct fpu *fp)
{
	fp->fpu_en = 1;
	lwptoregs(lwp)->r_psr |= PSR_EF;
	installctx(lwptot(lwp),
	    fp, fp_save, fp_restore, fp_fork, fp_null, fp_free);
}

/*
 * Free any state associated with floating point context.
 * Fp_free can be called in two cases:
 * 1) from reaper -> thread_free -> ctxfree -> fp_free
 *	fp context belongs to a thread on deathrow
 *	nothing to do,  thread will never be resumed
 *	thread calling ctxfree is reaper
 *
 * 2) from exec -> ctxfree -> fp_free
 *	fp context belongs to the current thread
 *	must disable fpu, thread calling ctxfree is curthread
 */
/*ARGSUSED1*/
static void
fp_free(struct fpu *fp, int isexec)
{
	int s;

	if (curthread->t_lwp != NULL && lwptofpu(curthread->t_lwp) == fp) {
		lwptoregs(curthread->t_lwp)->r_psr &= ~PSR_EF;
		s = splhigh();
		fp_disable();
		splx(s);
	}
}

void
fp_disabled(rp)
	struct regs *rp;
{
	register klwp_id_t lwp = ttolwp(curthread);
	register struct fpu *fp = lwptofpu(lwp);
	int ftt;

	if (fpu_exists) {
		kpreempt_disable();
		if (fp->fpu_en) {
			if (fpdispr)
				printf("fpu disabled, but already enabled\n");
			rp->r_psr |= PSR_EF;
			fp_restore(fp);
		} else {
			fp->fpu_en = 1;
			fp->fpu_fsr = 0;
			rp->r_psr |= PSR_EF;
			/*
			 * installctx may block in kmem_alloc, so we set up
			 * the fp registers (via fp_enable) after it returns
			 */
			installctx(curthread, fp, fp_save, fp_restore,
			    fp_fork, fp_null, fp_free);
			CPU->cpu_fpowner = lwp;
			fp_enable(fp);
		}
		kpreempt_enable();
	} else {
		fp_simd_type fpsd;
		register int i;

		(void) flush_user_windows_to_stack(NULL);
		if (!fp->fpu_en) {
			fp->fpu_en = 1;
			fp->fpu_fsr = 0;
			for (i = 0; i < 32; i++)
				fp->fpu_fr.fpu_regs[0] = (uint_t)-1; /* NaN */
		}
		if (ftt = fp_emulator(&fpsd, (fp_inst_type *)rp->r_pc,
				rp, (struct rwindow *)rp->r_sp,
				(struct fpu *)&fp->fpu_fr.fpu_regs[0]))
			fp_traps(&fpsd, ftt, rp);
	}
}

/*
 * Process the floating point queue in lwp->lwp_pcb.
 *
 * Each entry in the floating point queue is processed in turn.
 * If processing an entry results in an exception fp_traps() is called to
 * handle the exception - this usually results in the generation of a signal
 * to be delivered to the user. There are 2 possible outcomes to this (note
 * that hardware generated signals cannot be held!):
 *
 *   1. If the signal is being ignored we continue to process the rest
 *	of the entries in the queue.
 *
 *   2. If arrangements have been made for return to a user signal handler,
 *	sendsig() will have copied the floating point queue onto the user's
 *	signal stack and zero'ed the queue count in the u_pcb. Note that
 *	this has the side effect of terminating fp_runq's processing loop.
 *	We will re-run the floating point queue on return from the user
 *	signal handler if necessary as part of normal setcontext processing.
 */
void
fp_runq(rp)
	register struct regs *rp;	/* ptr to regs for trap */
{
	register fpregset_t *fp =	lwptofpu(curthread->t_lwp);
	register struct fq *fqp =	fp->fpu_q;
	fp_simd_type			fpsd;
	klwp_id_t lwp = ttolwp(curthread);

	if ((USERMODE(rp->r_psr)))
		lwp->lwp_state = LWP_SYS;

	/*
	 * don't preempt while manipulating the queue
	 */
	kpreempt_disable();
	while (fp->fpu_qcnt) {
		int fptrap;

		fptrap = fpu_simulator((fp_simd_type *)&fpsd,
					(fp_inst_type *)fqp->FQu.fpq.fpq_addr,
					(fsr_type *)&fp->fpu_fsr,
					fqp->FQu.fpq.fpq_instr);
		if (fptrap) {

			/*
			 * Instruction could not be simulated so we will
			 * attempt to deliver a signal.
			 * We may be called again upon signal exit (setcontext)
			 * and can continue to process the queue then.
			 */
			if (fqp != fp->fpu_q) {
				register int i;
				register struct fq *fqdp;

				/*
				 * We need to normalize the floating queue so
				 * the excepting instruction is at the head,
				 * so that the queue may be copied onto the
				 * user signal stack by sendsig().
				 */
				fqdp = fp->fpu_q;
				for (i = fp->fpu_qcnt; i; i--) {
					*fqdp++ = *fqp++;
				}
				fqp = fp->fpu_q;
			}
			fp->fpu_q_entrysize = sizeof (struct fpq);

			/*
			 * fpu_simulator uses the fp registers directly but it
			 * uses the software copy of the fsr. We need to write
			 * that back to fpu so that fpu's state is current for
			 * ucontext.
			 */
			if (fpu_exists)
				_fp_write_pfsr(&fp->fpu_fsr);

			/* post signal */
			fp_traps(&fpsd, fptrap, rp);

			/*
			 * Break from loop to allow signal to be sent.
			 * If there are other instructions in the fp queue
			 * they will be processed when/if the user retuns
			 * from the signal handler with a non-empty queue.
			 */
			break;
		}
		fp->fpu_qcnt--;
		fqp++;
	}

	/*
	 * fpu_simulator uses the fp registers directly, so we have
	 * to update the pcb copies to keep current, but it uses the
	 * software copy of the fsr, so we write that back to fpu
	 */
	if (fpu_exists) {
		register int i;

		for (i = 0; i < 32; i++)
			_fp_read_pfreg(&fp->fpu_fr.fpu_regs[i], i);
		_fp_write_pfsr(&fp->fpu_fsr);
	}

	kpreempt_enable();
	if ((USERMODE(rp->r_psr)))
		lwp->lwp_state = LWP_USER;
}


/*
 * Set FPU_TRAPPED bit in pcb_flags to indicate to syscall that there is a
 * pending signal for the process so it can return before doing a getcontext()
 * system call
 */

/*
 * Handle floaing point traps generated by simulation/emulation.
 */
void
fp_traps(pfpsd, ftt, rp)
	fp_simd_type	*pfpsd;		/* Pointer to simulator data */
	register enum ftt_type ftt;	/* trap type */
	register struct regs *rp;	/* ptr to regs fro trap */
{
	extern void trap();

	/*
	 * If we take a user's exception in kernel mode, we want to trap
	 * with the user's registers.
	 * XXX - if the following is not needed
	 * delete it from the structure used for fp_simd_type;
	 *	if (pfpsd->fp_traprp)
	 *		rp = pfpsd->fp_traprp;
	 */

	switch (ftt) {
	case ftt_ieee:
		trap(T_FP_EXCEPTION, rp, pfpsd->fp_trapaddr,
		    pfpsd->fp_trapcode, 0);
		break;
	case ftt_fault:
		trap(T_DATA_FAULT, rp, pfpsd->fp_trapaddr, 0, pfpsd->fp_traprw);
		break;
	case ftt_alignment:
		trap(T_ALIGNMENT, rp, pfpsd->fp_trapaddr, 0, 0);
		break;
	case ftt_unimplemented:
		trap(T_UNIMP_INSTR, rp, pfpsd->fp_trapaddr, 0, 0);
		break;
	default:
		/*
		 * We don't expect any of the other types here.
		 */
		panic("fp_traps: bad ftt");
	}
}
