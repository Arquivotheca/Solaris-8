/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)v9dep.c	1.63	99/10/21 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/stack.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/ucontext.h>
#include <sys/cpuvar.h>
#include <sys/asm_linkage.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/bootconf.h>
#include <sys/archsystm.h>
#include <sys/fpu/fpusystm.h>
#include <sys/debug.h>
#include <sys/privregs.h>
#include <sys/machpcb.h>
#include <sys/psr_compat.h>
#include <sys/cmn_err.h>
#include <sys/asi.h>
#include <sys/copyops.h>
#include <sys/model.h>
#include <sys/panic.h>

/*
 * modify the lower 32bits of a uint64_t
 */
#define	SET_LOWER_32(all, lower)	\
	(((uint64_t)(all) & 0xffffffff00000000) | (uint32_t)(lower))

#define	MEMCPY_FPU_EN		2	/* fprs on and fpu_en == 0 */

static uint_t mkpsr(uint64_t tstate, uint32_t fprs);

#ifdef _SYSCALL32_IMPL
static void fpuregset_nto32(const fpregset_t *src, fpregset32_t *dest,
    struct fq32 *dfq);
static void fpuregset_32ton(const fpregset32_t *src, fpregset_t *dest,
    const struct fq32 *sfq, struct fq *dfq);
#endif /* _SYSCALL32_IMPL */

/*
 * Set floating-point registers.
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to set the registers of another lwp.
 */
void
setfpregs(klwp_t *lwp, fpregset_t *fp)
{
	struct machpcb *mpcb;
	kfpu_t *pfp;
	uint32_t fprs = (FPRS_FEF|FPRS_DU|FPRS_DL);
	model_t model = lwp_getdatamodel(lwp);

	mpcb = lwptompcb(lwp);
	pfp = lwptofpu(lwp);

	/*
	 * This is always true for both "real" fp programs and memcpy fp
	 * programs, because we force fpu_en to MEMCPY_FPU_EN in getfpregs,
	 * for the memcpy and threads cases where (fpu_en == 0) &&
	 * (fpu_fprs & FPRS_FEF), if setfpregs is called after getfpregs.
	 */
	if (fp->fpu_en) {
		if (!(pfp->fpu_en) && (!(pfp->fpu_fprs & FPRS_FEF)) &&
		    fpu_exists) {
			/*
			 * He's not currently using the FPU but wants to in his
			 * new context - arrange for this on return to userland.
			 */
			pfp->fpu_fprs = (V9_FPU_FPRS_TYPE)fprs;
		}
		/*
		 * Get setfpregs to restore fpu_en to zero
		 * for the memcpy/threads case (where pfp->fpu_en == 0 &&
		 * (pfp->fp_fprs & FPRS_FEF) == FPRS_FEF).
		 */
		if (fp->fpu_en == MEMCPY_FPU_EN)
			fp->fpu_en = 0;

		/*
		 * Load up a user's floating point context.
		 */
		if (fp->fpu_qcnt > MAXFPQ) 	/* plug security holes */
			fp->fpu_qcnt = MAXFPQ;
		fp->fpu_q_entrysize = sizeof (struct fq);

		/*
		 * For v9 kernel, copy all of the fp regs.
		 * For v8 kernel, copy v8 fp regs (lower half of v9 fp regs).
		 * Restore entire fsr for v9, only lower half for v8.
		 */
		(void) kcopy(fp, pfp, sizeof (fp->fpu_fr));
		if (model == DATAMODEL_LP64)
			pfp->fpu_fsr = fp->fpu_fsr;
		else
			pfp->fpu_fsr = SET_LOWER_32(pfp->fpu_fsr, fp->fpu_fsr);
		pfp->fpu_qcnt = fp->fpu_qcnt;
		pfp->fpu_q_entrysize = fp->fpu_q_entrysize;
		pfp->fpu_en = fp->fpu_en;
		pfp->fpu_q = mpcb->mpcb_fpu_q;
		if (fp->fpu_qcnt)
			(void) kcopy(fp->fpu_q, pfp->fpu_q,
			    fp->fpu_qcnt * fp->fpu_q_entrysize);
		/* FSR ignores these bits on load, so they can not be set */
		pfp->fpu_fsr &= ~(FSR_QNE|FSR_FTT);

		kpreempt_disable();

		/*
		 * If not the current process then resume() will handle it.
		 */
		if (lwp != ttolwp(curthread)) {
			/* force resume to reload fp regs */
			pfp->fpu_fprs |= FPRS_FEF;
			kpreempt_enable();
			return;
		}

		/*
		 * Load up FPU with new floating point context.
		 */
		if (fpu_exists) {
			pfp->fpu_fprs = _fp_read_fprs();
			if ((pfp->fpu_fprs & FPRS_FEF) != FPRS_FEF) {
				_fp_write_fprs(fprs);
				pfp->fpu_fprs = (V9_FPU_FPRS_TYPE)fprs;
#ifdef DEBUG
				if (fpdispr)
					cmn_err(CE_NOTE,
					    "setfpregs with fp disabled!\n");
#endif
			}
			/*
			 * Load all fp regs for v9 user programs, but only
			 * load the lower half for v8[plus] programs.
			 */
			if (model == DATAMODEL_LP64)
				fp_restore(pfp);
			else
				fp_v8_load(pfp);
		}

		kpreempt_enable();
	} else {
		if ((pfp->fpu_en) ||	/* normal fp case */
		    (pfp->fpu_fprs & FPRS_FEF)) { /* memcpy/threads case */
			/*
			 * Currently the lwp has floating point enabled.
			 * Turn off FPRS_FEF in user's fprs, saved and
			 * real copies thereof.
			 */
			pfp->fpu_en = 0;
			if (fpu_exists) {
				fprs = 0;
				if (lwp == ttolwp(curthread))
					_fp_write_fprs(fprs);
				pfp->fpu_fprs = (V9_FPU_FPRS_TYPE)fprs;
			}
		}
	}
}

#ifdef	_SYSCALL32_IMPL
void
setfpregs32(klwp_t *lwp, fpregset32_t *fp)
{
	fpregset_t fpregs;

	fpuregset_32ton(fp, &fpregs, NULL, NULL);
	setfpregs(lwp, &fpregs);
}
#endif	/* _SYSCALL32_IMPL */

/*
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to set the registers of another lwp.
 */
void
run_fpq(klwp_t *lwp, fpregset_t *fp)
{
	/*
	 * If the context being loaded up includes a floating queue,
	 * we need to simulate those instructions (since we can't reload
	 * the fpu) and pass the process any appropriate signals
	 */

	if (lwp == ttolwp(curthread)) {
		if (fpu_exists) {
			if (fp->fpu_qcnt)
				fp_runq(lwp->lwp_regs);
		}
	}
}

/*
 * Get floating-point registers.
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to set the registers of another lwp.
 */
void
getfpregs(klwp_t *lwp, fpregset_t *fp)
{
	kfpu_t *pfp;
	model_t model = lwp_getdatamodel(lwp);

	pfp = lwptofpu(lwp);
	kpreempt_disable();
	if (ttolwp(curthread) == lwp)
		pfp->fpu_fprs = _fp_read_fprs();
	/*
	 * First check the fpu_en case, for normal fp programs.
	 * Next check the fprs case, for fp use by memcpy/threads.
	 */
	if (((fp->fpu_en = pfp->fpu_en) != 0) ||
	    (pfp->fpu_fprs & FPRS_FEF)) {
		/*
		 * Force setfpregs to restore the fp context in
		 * setfpregs for the memcpy and threads cases (where
		 * pfp->fpu_en == 0 && (pfp->fp_fprs & FPRS_FEF) == FPRS_FEF).
		 */
		if (pfp->fpu_en == 0)
			fp->fpu_en = MEMCPY_FPU_EN;
		/*
		 * If we have an fpu and the current thread owns the fp
		 * context, flush fp * registers into the pcb. Save all
		 * the fp regs for v9, xregs_getfpregs saves the upper half
		 * for v8plus. Save entire fsr for v9, only lower half for v8.
		 */
		if (fpu_exists && ttolwp(curthread) == lwp) {
			if ((pfp->fpu_fprs & FPRS_FEF) != FPRS_FEF) {
				uint32_t fprs = (FPRS_FEF|FPRS_DU|FPRS_DL);

				_fp_write_fprs(fprs);
				pfp->fpu_fprs = fprs;
#ifdef DEBUG
				if (fpdispr)
					cmn_err(CE_NOTE,
					    "getfpregs with fp disabled!\n");
#endif
			}
			if (model == DATAMODEL_LP64)
				fp_fksave(pfp);
			else
				fp_v8_fksave(pfp);
		}
		(void) kcopy(pfp, fp, sizeof (fp->fpu_fr));
		fp->fpu_q = pfp->fpu_q;
		if (model == DATAMODEL_LP64)
			fp->fpu_fsr = pfp->fpu_fsr;
		else
			fp->fpu_fsr = (V7_FPU_FSR_TYPE)pfp->fpu_fsr;
		fp->fpu_qcnt = pfp->fpu_qcnt;
		fp->fpu_q_entrysize = pfp->fpu_q_entrysize;
	} else {
		int i;
		for (i = 0; i < 32; i++)		/* NaN */
			fp->fpu_fr.fpu_regs[i] = (uint32_t)-1;
		if (model == DATAMODEL_LP64) {
			for (i = 16; i < 32; i++)	/* NaN */
				fp->fpu_fr.fpu_dregs[i] = (uint64_t)-1;
		}
		fp->fpu_fsr = 0;
		fp->fpu_qcnt = 0;
	}
	kpreempt_enable();
}

#ifdef	_SYSCALL32_IMPL
void
getfpregs32(klwp_t *lwp, fpregset32_t *fp)
{
	fpregset_t fpregs;

	getfpregs(lwp, &fpregs);
	fpuregset_nto32(&fpregs, fp, NULL);
}
#endif	/* _SYSCALL32_IMPL */

/*
 * Set general registers.
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to set the registers of another lwp.
 */

#ifdef	__sparcv9

/* 64-bit gregset_t */
void
setgregs(klwp_t *lwp, gregset_t grp)
{
	struct regs *rp = lwptoregs(lwp);
	kfpu_t *fp = lwptofpu(lwp);
	uint64_t tbits;

	int current = (lwp == curthread->t_lwp);

	if (current)
		(void) save_syscall_args();	/* copy the args first */

	tbits = (((grp[REG_CCR] & TSTATE_CCR_MASK) << TSTATE_CCR_SHIFT) |
		((grp[REG_ASI] & TSTATE_ASI_MASK) << TSTATE_ASI_SHIFT));
	rp->r_tstate &= ~(((uint64_t)TSTATE_CCR_MASK << TSTATE_CCR_SHIFT) |
		((uint64_t)TSTATE_ASI_MASK << TSTATE_ASI_SHIFT));
	rp->r_tstate |= tbits;
	fp->fpu_fprs = (uint32_t)grp[REG_FPRS];
	if ((current) && (fp->fpu_fprs & FPRS_FEF))
		_fp_write_fprs(fp->fpu_fprs);

	/*
	 * pc and npc must be 4-byte aligned on sparc.
	 * We silently make it so to avoid a watchdog reset.
	 */
	rp->r_pc = grp[REG_PC] & ~03L;
	rp->r_npc = grp[REG_nPC] & ~03L;
	rp->r_y = grp[REG_Y];

	rp->r_g1 = grp[REG_G1];
	rp->r_g2 = grp[REG_G2];
	rp->r_g3 = grp[REG_G3];
	rp->r_g4 = grp[REG_G4];
	rp->r_g5 = grp[REG_G5];
	rp->r_g6 = grp[REG_G6];
	rp->r_g7 = grp[REG_G7];

	rp->r_o0 = grp[REG_O0];
	rp->r_o1 = grp[REG_O1];
	rp->r_o2 = grp[REG_O2];
	rp->r_o3 = grp[REG_O3];
	rp->r_o4 = grp[REG_O4];
	rp->r_o5 = grp[REG_O5];
	rp->r_o6 = grp[REG_O6];
	rp->r_o7 = grp[REG_O7];

	if (current) {
		/*
		 * This was called from a system call, but we
		 * do not want to return via the shared window;
		 * restoring the CPU context changes everything.
		 */
		lwp->lwp_eosys = JUSTRETURN;
		curthread->t_post_sys = 1;
	}
}

#else	/* __sparcv9 */

/* 32-bit gregset_t */
void
setgregs(klwp_t *lwp, gregset_t grp)
{
	struct regs *rp = lwptoregs(lwp);
	uint64_t icc;

	int current = (lwp == curthread->t_lwp);

	if (current)
		(void) save_syscall_args();	/* copy the args first */

	icc = grp[REG_PSR] & PSR_ICC;
	icc <<= PSR_TSTATE_CC_SHIFT;
	rp->r_tstate &= ~TSTATE_ICC;
	rp->r_tstate |= icc;
	/*
	 * pc and npc must be 4-byte aligned on sparc.
	 * We silently make it so to avoid a watchdog reset.
	 */
	rp->r_pc  = grp[REG_PC] & ~03;
	rp->r_npc = grp[REG_nPC] & ~03;
	rp->r_y   = grp[REG_Y];

	rp->r_g1 = SET_LOWER_32(rp->r_g1, grp[REG_G1]);
	rp->r_g2 = SET_LOWER_32(rp->r_g2, grp[REG_G2]);
	rp->r_g3 = SET_LOWER_32(rp->r_g3, grp[REG_G3]);
	rp->r_g4 = SET_LOWER_32(rp->r_g4, grp[REG_G4]);
	rp->r_g5 = SET_LOWER_32(rp->r_g5, grp[REG_G5]);
	rp->r_g6 = SET_LOWER_32(rp->r_g6, grp[REG_G6]);
	rp->r_g7 = SET_LOWER_32(rp->r_g7, grp[REG_G7]);

	rp->r_o0 = SET_LOWER_32(rp->r_o0, grp[REG_O0]);
	rp->r_o1 = SET_LOWER_32(rp->r_o1, grp[REG_O1]);
	rp->r_o2 = SET_LOWER_32(rp->r_o2, grp[REG_O2]);
	rp->r_o3 = SET_LOWER_32(rp->r_o3, grp[REG_O3]);
	rp->r_o4 = SET_LOWER_32(rp->r_o4, grp[REG_O4]);
	rp->r_o5 = SET_LOWER_32(rp->r_o5, grp[REG_O5]);
	rp->r_o6 = SET_LOWER_32(rp->r_o6, grp[REG_O6]);
	rp->r_o7 = SET_LOWER_32(rp->r_o7, grp[REG_O7]);

	if (current) {
		/*
		 * This was called from a system call, but we
		 * do not want to return via the shared window;
		 * restoring the CPU context changes everything.
		 */
		lwp->lwp_eosys = JUSTRETURN;
		curthread->t_post_sys = 1;
	}
}

#endif	/* __sparcv9 */

/*
 * Return the general registers.
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to get the registers of another lwp.
 */
#ifdef	__sparcv9
void
getgregs(klwp_t *lwp, gregset_t grp)
{
	struct regs *rp = lwptoregs(lwp);
	uint32_t fprs;

	kpreempt_disable();
	if (fpu_exists && ttolwp(curthread) == lwp) {
		fprs = _fp_read_fprs();
	} else {
		kfpu_t *fp = lwptofpu(lwp);
		fprs = fp->fpu_fprs;
	}
	kpreempt_enable();
	grp[REG_CCR] = (rp->r_tstate >> TSTATE_CCR_SHIFT) & TSTATE_CCR_MASK;
	grp[REG_PC] = rp->r_pc;
	grp[REG_nPC] = rp->r_npc;
	grp[REG_Y] = (uint32_t)rp->r_y;
	grp[REG_G1] = rp->r_g1;
	grp[REG_G2] = rp->r_g2;
	grp[REG_G3] = rp->r_g3;
	grp[REG_G4] = rp->r_g4;
	grp[REG_G5] = rp->r_g5;
	grp[REG_G6] = rp->r_g6;
	grp[REG_G7] = rp->r_g7;
	grp[REG_O0] = rp->r_o0;
	grp[REG_O1] = rp->r_o1;
	grp[REG_O2] = rp->r_o2;
	grp[REG_O3] = rp->r_o3;
	grp[REG_O4] = rp->r_o4;
	grp[REG_O5] = rp->r_o5;
	grp[REG_O6] = rp->r_o6;
	grp[REG_O7] = rp->r_o7;
	grp[REG_ASI] = (rp->r_tstate >> TSTATE_ASI_SHIFT) & TSTATE_ASI_MASK;
	grp[REG_FPRS] = fprs;
}
#endif	/* __sparcv9 */

#ifdef	__sparcv9
void
getgregs32(klwp_t *lwp, gregset32_t grp)
#else	/* __sparcv9 */
void
getgregs(klwp_t *lwp, gregset_t grp)
#endif	/* __sparcv9 */
{
	struct regs *rp = lwptoregs(lwp);
	uint32_t fprs;

	kpreempt_disable();
	if (fpu_exists && ttolwp(curthread) == lwp) {
		fprs = _fp_read_fprs();
	} else {
		kfpu_t *fp = lwptofpu(lwp);
		fprs = fp->fpu_fprs;
	}
	kpreempt_enable();
	grp[REG_PSR] = mkpsr(rp->r_tstate, fprs);
	grp[REG_PC] = rp->r_pc;
	grp[REG_nPC] = rp->r_npc;
	grp[REG_Y] = rp->r_y;
	grp[REG_G1] = rp->r_g1;
	grp[REG_G2] = rp->r_g2;
	grp[REG_G3] = rp->r_g3;
	grp[REG_G4] = rp->r_g4;
	grp[REG_G5] = rp->r_g5;
	grp[REG_G6] = rp->r_g6;
	grp[REG_G7] = rp->r_g7;
	grp[REG_O0] = rp->r_o0;
	grp[REG_O1] = rp->r_o1;
	grp[REG_O2] = rp->r_o2;
	grp[REG_O3] = rp->r_o3;
	grp[REG_O4] = rp->r_o4;
	grp[REG_O5] = rp->r_o5;
	grp[REG_O6] = rp->r_o6;
	grp[REG_O7] = rp->r_o7;
}

/*
 * Return the user-level PC.
 * If in a system call, return the address of the syscall trap.
 */
greg_t
getuserpc()
{
	return (lwptoregs(ttolwp(curthread))->r_pc);
}

/*
 * Set register windows.
 */
void
setgwins(klwp_t *lwp, gwindows_t *gwins)
{
	struct machpcb *mpcb = lwptompcb(lwp);
	int wbcnt = gwins->wbcnt;
	caddr_t sp;
	int i;
#ifdef	__sparcv9
	struct rwindow32 *rwp;
	int wbuf_rwindow_size;
	int is64;

	if (mpcb->mpcb_wstate == WSTATE_USER32) {
		wbuf_rwindow_size = WINDOWSIZE32;
		is64 = 0;
	} else {
		wbuf_rwindow_size = WINDOWSIZE64;
		is64 = 1;
	}
#endif
	ASSERT(wbcnt >= 0 && wbcnt <= SPARC_MAXREGWINDOW);
	mpcb->mpcb_wbcnt = 0;
	for (i = 0; i < wbcnt; i++) {
		sp = (caddr_t)gwins->spbuf[i];
		mpcb->mpcb_spbuf[i] = sp;
#ifdef	__sparcv9
		rwp = (struct rwindow32 *)
			(mpcb->mpcb_wbuf + (i * wbuf_rwindow_size));
		if (is64 && IS_V9STACK(sp))
			bcopy(&gwins->wbuf[i], rwp, sizeof (struct rwindow));
		else
			rwindow_nto32(&gwins->wbuf[i], rwp);
#else
		bcopy(&gwins->wbuf[i],
		    mpcb->mpcb_wbuf + (i * sizeof (struct rwindow)),
		    sizeof (struct rwindow));
#endif
		mpcb->mpcb_wbcnt++;
	}
}

#ifdef __sparcv9
void
setgwins32(klwp_t *lwp, gwindows32_t *gwins)
{
	struct machpcb *mpcb = lwptompcb(lwp);
	int wbcnt = gwins->wbcnt;
	caddr_t sp;
	int i;

	struct rwindow *rwp;
	int wbuf_rwindow_size;
	int is64;

	if (mpcb->mpcb_wstate == WSTATE_USER32) {
		wbuf_rwindow_size = WINDOWSIZE32;
		is64 = 0;
	} else {
		wbuf_rwindow_size = WINDOWSIZE64;
		is64 = 1;
	}

	ASSERT(wbcnt >= 0 && wbcnt <= SPARC_MAXREGWINDOW);
	mpcb->mpcb_wbcnt = 0;
	for (i = 0; i < wbcnt; i++) {
		sp = (caddr_t)gwins->spbuf[i];
		mpcb->mpcb_spbuf[i] = sp;
		rwp = (struct rwindow *)
			(mpcb->mpcb_wbuf + (i * wbuf_rwindow_size));
		if (is64 && IS_V9STACK(sp))
			rwindow_32ton(&gwins->wbuf[i], rwp);
		else
			bcopy(&gwins->wbuf[i], rwp, sizeof (struct rwindow32));
		mpcb->mpcb_wbcnt++;
	}
}
#endif	/* __sparcv9 */

/*
 * Get register windows.
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to set the registers of another lwp.
 */
void
getgwins(klwp_t *lwp, gwindows_t *gwp)
{
	struct machpcb *mpcb = lwptompcb(lwp);
	int wbcnt = mpcb->mpcb_wbcnt;
	caddr_t sp;
	int i;
#ifdef	__sparcv9
	struct rwindow32 *rwp;
	int wbuf_rwindow_size;
	int is64;

	if (mpcb->mpcb_wstate == WSTATE_USER32) {
		wbuf_rwindow_size = WINDOWSIZE32;
		is64 = 0;
	} else {
		wbuf_rwindow_size = WINDOWSIZE64;
		is64 = 1;
	}
#endif
	ASSERT(wbcnt >= 0 && wbcnt <= SPARC_MAXREGWINDOW);
	gwp->wbcnt = wbcnt;
	for (i = 0; i < wbcnt; i++) {
		sp = mpcb->mpcb_spbuf[i];
		gwp->spbuf[i] = (greg_t *)sp;
#ifdef	__sparcv9
		rwp = (struct rwindow32 *)
			(mpcb->mpcb_wbuf + (i * wbuf_rwindow_size));
		if (is64 && IS_V9STACK(sp))
			bcopy(rwp, &gwp->wbuf[i], sizeof (struct rwindow));
		else
			rwindow_32ton(rwp, &gwp->wbuf[i]);
#else
		bcopy(mpcb->mpcb_wbuf + (i * sizeof (struct rwindow)),
		    &gwp->wbuf[i], sizeof (struct rwindow));
#endif
	}
}

#ifdef	__sparcv9
void
getgwins32(klwp_t *lwp, gwindows32_t *gwp)
{
	struct machpcb *mpcb = lwptompcb(lwp);
	int wbcnt = mpcb->mpcb_wbcnt;
	int i;
	struct rwindow *rwp;
	int wbuf_rwindow_size;
	caddr_t sp;
	int is64;

	if (mpcb->mpcb_wstate == WSTATE_USER32) {
		wbuf_rwindow_size = WINDOWSIZE32;
		is64 = 0;
	} else {
		wbuf_rwindow_size = WINDOWSIZE64;
		is64 = 1;
	}

	ASSERT(wbcnt >= 0 && wbcnt <= SPARC_MAXREGWINDOW);
	gwp->wbcnt = wbcnt;
	for (i = 0; i < wbcnt; i++) {
		sp = mpcb->mpcb_spbuf[i];
		rwp = (struct rwindow *)
			(mpcb->mpcb_wbuf + (i * wbuf_rwindow_size));
		gwp->spbuf[i] = (caddr32_t)sp;
		if (is64 && IS_V9STACK(sp))
			rwindow_nto32(rwp, &gwp->wbuf[i]);
		else
			bcopy(rwp, &gwp->wbuf[i], sizeof (struct rwindow32));
	}
}
#endif	/* __sparcv9 */

/*
 * For things that depend on register state being on the stack,
 * copy any register windows that get saved into the window buffer
 * (in the pcb) onto the stack.  This normally gets fixed up
 * before returning to a user program.  Callers of this routine
 * require this to happen immediately because a later kernel
 * operation depends on window state (like instruction simulation).
 */
int
flush_user_windows_to_stack(caddr_t *psp)
{
	int j, k;
	caddr_t sp;
	proc_t *p = ttoproc(curthread);
	struct machpcb *mpcb = lwptompcb(ttolwp(curthread));
	int mapped;
	int err;
	int error = 0;
	int wbuf_rwindow_size;
	int rwindow_size;
	int stack_align;

	flush_user_windows();

	if (mpcb->mpcb_wstate != WSTATE_USER32)
		wbuf_rwindow_size = WINDOWSIZE64;
	else
		wbuf_rwindow_size = WINDOWSIZE32;

	j = mpcb->mpcb_wbcnt;
	while (j > 0) {
		sp = mpcb->mpcb_spbuf[--j];

		if ((mpcb->mpcb_wstate != WSTATE_USER32) &&
		    IS_V9STACK(sp)) {
			sp += V9BIAS64;
			stack_align = STACK_ALIGN64;
			rwindow_size = WINDOWSIZE64;
		} else {
			sp = (caddr_t)(uint32_t)sp;
			stack_align = STACK_ALIGN32;
			rwindow_size = WINDOWSIZE32;
		}
		if (((uintptr_t)sp & (stack_align - 1)) != 0)
			continue;
		mapped = 0;

		if (p->p_warea)	/* watchpoints in effect */
			mapped = pr_mappage(sp, rwindow_size, S_WRITE, 1);

		err = default_xcopyout(mpcb->mpcb_wbuf +
		    (j * wbuf_rwindow_size), sp, rwindow_size);
		if (err != 0) {
			if (psp != NULL) {
				/*
				 * Determine the offending address.
				 * It may not be the stack pointer itself.
				 */
				uint_t *kaddr = (uint_t *)(mpcb->mpcb_wbuf +
				    (j * wbuf_rwindow_size));
				uint_t *uaddr = (uint_t *)sp;

				for (k = 0;
				    k < rwindow_size / sizeof (int);
				    k++, kaddr++, uaddr++) {
					if (default_suword32(uaddr, *kaddr))
						break;
				}

				/* can't happen? */
				if (k == rwindow_size / sizeof (int))
					uaddr = (uint_t *)sp;

				*psp = (caddr_t)uaddr;
			}
			error = err;
		} else {
			/*
			 * stack was aligned and copyout succeeded;
			 * move other windows down.
			 */
			mpcb->mpcb_wbcnt--;
			for (k = j; k < mpcb->mpcb_wbcnt; k++) {
				mpcb->mpcb_spbuf[k] = mpcb->mpcb_spbuf[k+1];
				bcopy(
				    mpcb->mpcb_wbuf +
					((k+1) * wbuf_rwindow_size),
				    mpcb->mpcb_wbuf +
					(k * wbuf_rwindow_size),
				    wbuf_rwindow_size);
			}
		}
		if (mapped)
			pr_unmappage(sp, rwindow_size, S_WRITE, 1);
	} /* while there are windows in the wbuf */
	return (error);
}

#ifdef __sparcv9
static int
copy_return_window32(int dotwo)
{
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	struct machpcb *mpcb = lwptompcb(lwp);
	struct rwindow32 rwindow32;
	caddr_t sp1;
	int map1 = 0;
	caddr_t sp2;
	int map2 = 0;

	(void) flush_user_windows_to_stack(NULL);
	if (mpcb->mpcb_rsp[0] == NULL) {
		sp1 = (caddr_t)(uint32_t)lwptoregs(lwp)->r_sp;
		if (p->p_warea)		/* watchpoints in effect */
			map1 = pr_mappage(sp1, sizeof (struct rwindow32),
				S_READ, 1);
		if ((default_copyin(sp1, &rwindow32,
		    sizeof (struct rwindow32)) == 0))
			mpcb->mpcb_rsp[0] = sp1;
		rwindow_32ton(&rwindow32, &mpcb->mpcb_rwin[0]);
	}
	mpcb->mpcb_rsp[1] = NULL;
	if (dotwo && mpcb->mpcb_rsp[0] != NULL &&
	    (sp2 = (caddr_t)mpcb->mpcb_rwin[0].rw_fp) != NULL) {
		if (p->p_warea)		/* watchpoints in effect */
			map2 = pr_mappage(sp2, sizeof (struct rwindow32),
				S_READ, 1);
		if ((default_copyin(sp2, &rwindow32,
		    sizeof (struct rwindow32)) == 0))
			mpcb->mpcb_rsp[1] = sp2;
		rwindow_32ton(&rwindow32, &mpcb->mpcb_rwin[1]);
	}
	if (map2)
		pr_unmappage(sp2, sizeof (struct rwindow32), S_READ, 1);
	if (map1)
		pr_unmappage(sp1, sizeof (struct rwindow32), S_READ, 1);
	return (mpcb->mpcb_rsp[0] != NULL);
}
#endif

int
copy_return_window(int dotwo)
{
	proc_t *p = ttoproc(curthread);
	klwp_t *lwp;
	struct machpcb *mpcb;
	caddr_t sp1;
	int map1 = 0;
	caddr_t sp2;
	int map2 = 0;

#ifdef __sparcv9
	if (p->p_model == DATAMODEL_ILP32)
		return (copy_return_window32(dotwo));
#endif

	lwp = ttolwp(curthread);
	mpcb = lwptompcb(lwp);
	(void) flush_user_windows_to_stack(NULL);
	if (mpcb->mpcb_rsp[0] == NULL) {
		sp1 = (caddr_t)lwptoregs(lwp)->r_sp + STACK_BIAS;
		if (p->p_warea)		/* watchpoints in effect */
			map1 = pr_mappage(sp1, sizeof (struct rwindow),
				S_READ, 1);
		if ((default_copyin(sp1, &mpcb->mpcb_rwin[0],
		    sizeof (struct rwindow)) == 0))
			mpcb->mpcb_rsp[0] = sp1 - STACK_BIAS;
	}
	mpcb->mpcb_rsp[1] = NULL;
	if (dotwo && mpcb->mpcb_rsp[0] != NULL &&
	    (sp2 = (caddr_t)mpcb->mpcb_rwin[0].rw_fp) != NULL) {
		sp2 += STACK_BIAS;
		if (p->p_warea)		/* watchpoints in effect */
			map2 = pr_mappage(sp2, sizeof (struct rwindow),
				S_READ, 1);
		if ((default_copyin(sp2, &mpcb->mpcb_rwin[1],
		    sizeof (struct rwindow)) == 0))
			mpcb->mpcb_rsp[1] = sp2 - STACK_BIAS;
	}
	if (map2)
		pr_unmappage(sp2, sizeof (struct rwindow), S_READ, 1);
	if (map1)
		pr_unmappage(sp1, sizeof (struct rwindow), S_READ, 1);
	return (mpcb->mpcb_rsp[0] != NULL);
}

/*
 * Clear registers on exec(2).
 */
void
setregs(void)
{
	uintptr_t entry;
	struct regs *rp;
	klwp_t *lwp = ttolwp(curthread);
	kfpu_t *fpp = lwptofpu(lwp);
	struct machpcb *mpcb = lwptompcb(lwp);
	proc_t *p = ttoproc(curthread);

	entry = (uintptr_t)u.u_exdata.ux_entloc;

	/*
	 * Initialize user registers.
	 */
	(void) save_syscall_args();	/* copy args from registers first */
	rp = lwptoregs(lwp);
	rp->r_g1 = rp->r_g2 = rp->r_g3 = rp->r_g4 = rp->r_g5 =
	    rp->r_g6 = rp->r_g7 = rp->r_o0 = rp->r_o1 = rp->r_o2 =
	    rp->r_o3 = rp->r_o4 = rp->r_o5 = rp->r_o7 = 0;
	if (p->p_model == DATAMODEL_ILP32)
		rp->r_tstate = TSTATE_USER32;
	else
		rp->r_tstate = TSTATE_USER64;
#ifdef DEBUG
	if (!fpu_exists)
		rp->r_tstate &= ~TSTATE_PEF;
#endif
	rp->r_pc = entry;
	rp->r_npc = entry + 4;
	rp->r_y = 0;
	curthread->t_post_sys = 1;
	lwp->lwp_eosys = JUSTRETURN;
	lwp->lwp_pcb.pcb_trap0addr = NULL;	/* no trap 0 handler */
	/*
	 * Clear the fixalignment flag
	 */
	p->p_fixalignment = 0;

	/*
	 * Throw out old user windows, init window buf.
	 */
	trash_user_windows();

	if (p->p_model == DATAMODEL_LP64 &&
	    mpcb->mpcb_wstate != WSTATE_USER64) {
		ASSERT(mpcb->mpcb_wbcnt == 0);
		kmem_free(mpcb->mpcb_wbuf, MAXWIN * sizeof (struct rwindow32));
		mpcb->mpcb_wbuf = kmem_alloc(MAXWIN *
		    sizeof (struct rwindow64), KM_SLEEP);
		ASSERT(((uintptr_t)mpcb->mpcb_wbuf & 7) == 0);
		mpcb->mpcb_wstate = WSTATE_USER64;
	} else if (p->p_model == DATAMODEL_ILP32 &&
	    mpcb->mpcb_wstate != WSTATE_USER32) {
		ASSERT(mpcb->mpcb_wbcnt == 0);
		kmem_free(mpcb->mpcb_wbuf, MAXWIN * sizeof (struct rwindow64));
		mpcb->mpcb_wbuf = kmem_alloc(MAXWIN *
		    sizeof (struct rwindow32), KM_SLEEP);
		mpcb->mpcb_wstate = WSTATE_USER32;
	}

	/*
	 * Here we initialize minimal fpu state.
	 * The rest is done at the first floating
	 * point instruction that a process executes
	 * or by the lib_psr memcpy routines.
	 */
	fpp->fpu_en = 0;
	fpp->fpu_fprs = 0;
}

/*
 * Construct the execution environment for the user's signal
 * handler and arrange for control to be given to it on return
 * to userland.  The library code now calls setcontext() to
 * clean up after the signal handler, so sigret() is no longer
 * needed.
 */
int
sendsig(int sig, k_siginfo_t *sip, void (*hdlr)())
{
	/*
	 * 'volatile' is needed to ensure that values are
	 * correct on the error return from on_fault().
	 */
	volatile int minstacksz; /* min stack required to catch signal */
	int newstack = 0;	/* if true, switching to altstack */
	label_t ljb;
	caddr_t sp;
	struct regs *volatile rp;
	klwp_t *lwp = ttolwp(curthread);
	proc_t *volatile p = ttoproc(curthread);
	int fpq_size = 0;
	struct sigframe {
		struct frame frwin;
		ucontext_t uc;
	};
	siginfo_t *sip_addr;
	struct sigframe *volatile fp;
	ucontext_t *volatile tuc = NULL;
	char *volatile xregs = NULL;
	volatile size_t xregs_size = 0;
	gwindows_t *volatile gwp = NULL;
	volatile int gwin_size = 0;
	kfpu_t *fpp;
	struct machpcb *mpcb;
	volatile int mapped = 0;
	volatile int map2 = 0;
	caddr_t tos;

	/*
	 * Make sure the current last user window has been flushed to
	 * the stack save area before we change the sp.
	 * Restore register window if a debugger modified it.
	 */
	(void) flush_user_windows_to_stack(NULL);
	if (lwp->lwp_pcb.pcb_xregstat != XREGNONE)
		xregrestore(lwp, 0);

	mpcb = lwptompcb(lwp);
	rp = lwptoregs(lwp);

	/*
	 * Clear the watchpoint return stack pointers.
	 */
	mpcb->mpcb_rsp[0] = NULL;
	mpcb->mpcb_rsp[1] = NULL;

	minstacksz = sizeof (struct sigframe);

	/*
	 * We know that sizeof (siginfo_t) is stack-aligned:
	 * 128 bytes for ILP32, 256 bytes for LP64.
	 */
	if (sip != NULL)
		minstacksz += sizeof (siginfo_t);

	/*
	 * These two fields are pointed to by ABI structures and may
	 * be of arbitrary length. Size them now so we know how big
	 * the signal frame has to be.
	 */
	fpp = lwptofpu(lwp);
	fpp->fpu_fprs = _fp_read_fprs();
	if ((fpp->fpu_en) || (fpp->fpu_fprs & FPRS_FEF)) {
		fpq_size = fpp->fpu_q_entrysize * fpp->fpu_qcnt;
		minstacksz += SA(fpq_size);
	}

	mpcb = lwptompcb(lwp);
	if (mpcb->mpcb_wbcnt != 0) {
		gwin_size = (mpcb->mpcb_wbcnt * sizeof (struct rwindow)) +
		    (SPARC_MAXREGWINDOW * sizeof (caddr_t)) + sizeof (long);
		minstacksz += SA(gwin_size);
	}

	/*
	 * Extra registers, if support by this platform, may be of arbitrary
	 * length. Size them now so we know how big the signal frame has to be.
	 * For sparcv9 _LP64 user programs, use asrs instead of the xregs.
	 */
#ifndef __sparcv9
	xregs_size = xregs_getsize(p);
#endif
	minstacksz += SA(xregs_size);

	/*
	 * Figure out whether we will be handling this signal on
	 * an alternate stack specified by the user. Then allocate
	 * and validate the stack requirements for the signal handler
	 * context. on_fault will catch any faults.
	 */
	newstack = (sigismember(&u.u_sigonstack, sig) &&
	    !(lwp->lwp_sigaltstack.ss_flags & (SS_ONSTACK|SS_DISABLE)));

	tos = (caddr_t)rp->r_sp + STACK_BIAS;
	if (newstack != 0) {
		fp = (struct sigframe *)
		    (SA((uintptr_t)lwp->lwp_sigaltstack.ss_sp) +
			SA((int)lwp->lwp_sigaltstack.ss_size) - STACK_ALIGN -
			SA(minstacksz));
	} else {
		fp = (struct sigframe *)(tos - SA(minstacksz));
		/*
		 * Could call grow here, but stack growth now handled below
		 * in code protected by on_fault().
		 */
	}
	sp = (caddr_t)fp + sizeof (struct sigframe);

	/*
	 * Make sure process hasn't trashed its stack.
	 */
	if (((uintptr_t)fp & (STACK_ALIGN - 1)) != 0 ||
	    (caddr_t)fp >= p->p_usrstack ||
	    (caddr_t)fp + SA(minstacksz) >= p->p_usrstack) {
#ifdef DEBUG
		printf("sendsig: bad signal stack pid=%d, sig=%d\n",
		    p->p_pid, sig);
		printf("sigsp = %p, action = %p, upc = 0x%lx\n",
		    (void *)fp, (void *)hdlr, rp->r_pc);

		if (((uintptr_t)fp & (STACK_ALIGN - 1)) != 0)
			printf("bad stack alignment\n");
		else
			printf("fp above USRSTACK\n");
#endif
		return (0);
	}

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)fp, SA(minstacksz), S_WRITE, 1);
	if (on_fault(&ljb))
		goto badstack;

	tuc = kmem_alloc(sizeof (ucontext_t), KM_SLEEP);
	xregs_clrptr(lwp, tuc);
	savecontext(tuc, lwp->lwp_sigoldmask);

	/*
	 * save extra register state if it exists
	 */
	if (xregs_size != 0) {
		xregs_setptr(lwp, tuc, sp);
		xregs = kmem_alloc(xregs_size, KM_SLEEP);
		xregs_get(lwp, xregs);
		copyout_noerr(xregs, sp, xregs_size);
		kmem_free(xregs, xregs_size);
		xregs = NULL;
		sp += SA(xregs_size);
	}

	copyout_noerr(tuc, &fp->uc, sizeof (*tuc));
	kmem_free(tuc, sizeof (*tuc));
	tuc = NULL;

	if (sip != NULL) {
		uzero(sp, sizeof (siginfo_t));
		copyout_noerr(sip, sp, sizeof (*sip));
		sip_addr = (siginfo_t *)sp;
		sp += sizeof (siginfo_t);

		if (sig == SIGPROF &&
		    curthread->t_rprof != NULL &&
		    curthread->t_rprof->rp_anystate) {
			/*
			 * We stand on our head to deal with
			 * the real time profiling signal.
			 * Fill in the stuff that doesn't fit
			 * in a normal k_siginfo structure.
			 */
			int i = sip->si_nsysarg;
			while (--i >= 0) {
				sulword_noerr(
				    (ulong_t *)&sip_addr->si_sysarg[i],
				    (ulong_t)lwp->lwp_arg[i]);
			}
			copyout_noerr(curthread->t_rprof->rp_state,
			    sip_addr->si_mstate,
			    sizeof (curthread->t_rprof->rp_state));
		}
	} else {
		sip_addr = (siginfo_t *)NULL;
	}

	/*
	 * When flush_user_windows_to_stack() can't save all the
	 * windows to the stack, it puts them in the lwp's pcb.
	 */
	if (gwin_size != 0) {
		gwp = kmem_alloc(gwin_size, KM_SLEEP);
		getgwins(lwp, gwp);
		sulword_noerr(&fp->uc.uc_mcontext.gwins, (ulong_t)sp);
		copyout_noerr(gwp, sp, gwin_size);
		kmem_free(gwp, gwin_size);
		gwp = NULL;
		sp += SA(gwin_size);
	} else
		sulword_noerr(&fp->uc.uc_mcontext.gwins, (ulong_t)NULL);

	if (fpq_size != 0) {
		struct fq *fqp = (struct fq *)sp;
		sulword_noerr(&fp->uc.uc_mcontext.fpregs.fpu_q, (ulong_t)fqp);
		copyout_noerr(mpcb->mpcb_fpu_q, fqp, fpq_size);

		/*
		 * forget the fp queue so that the signal handler can run
		 * without being harrassed--it will do a setcontext that will
		 * re-establish the queue if there still is one
		 *
		 * NOTE: fp_runq() relies on the qcnt field being zeroed here
		 *	to terminate its processing of the queue after signal
		 *	delivery.
		 */
		mpcb->mpcb_fpu->fpu_qcnt = 0;
		sp += SA(fpq_size);

		/* Also, syscall needs to know about this */
		mpcb->mpcb_flags |= FP_TRAPPED;

	} else {
		sulword_noerr(&fp->uc.uc_mcontext.fpregs.fpu_q, (ulong_t)NULL);
		suword8_noerr(&fp->uc.uc_mcontext.fpregs.fpu_qcnt, 0);
	}


	/*
	 * Since we flushed the user's windows and we are changing his
	 * stack pointer, the window that the user will return to will
	 * be restored from the save area in the frame we are setting up.
	 * We copy in save area for old stack pointer so that debuggers
	 * can do a proper stack backtrace from the signal handler.
	 */
	if (mpcb->mpcb_wbcnt == 0) {
		if (p->p_warea)
			map2 = pr_mappage(tos, sizeof (struct rwindow),
			    S_READ, 1);
		ucopy(tos, &fp->frwin, sizeof (struct rwindow));
	}

	lwp->lwp_oldcontext = (uintptr_t)&fp->uc;

	if (newstack != 0)
		lwp->lwp_sigaltstack.ss_flags |= SS_ONSTACK;

	no_fault();
	mpcb->mpcb_wbcnt = 0;		/* let user go on */

	if (map2)
		pr_unmappage(tos, sizeof (struct rwindow),
		    S_READ, 1);
	if (mapped)
		pr_unmappage((caddr_t)fp, SA(minstacksz), S_WRITE, 1);

	/*
	 * Set up user registers for execution of signal handler.
	 */
	rp->r_sp = (uintptr_t)fp - STACK_BIAS;
	rp->r_pc = (uintptr_t)hdlr;
	rp->r_npc = (uintptr_t)hdlr + 4;
	/* make sure %asi is ASI_PNF */
	rp->r_tstate &= ~((uint64_t)TSTATE_ASI_MASK << TSTATE_ASI_SHIFT);
	rp->r_tstate |= ((uint64_t)ASI_PNF << TSTATE_ASI_SHIFT);
	rp->r_o0 = sig;
	rp->r_o1 = (uintptr_t)sip_addr;
	rp->r_o2 = (uintptr_t)&fp->uc;
	/*
	 * Don't set lwp_eosys here.  sendsig() is called via psig() after
	 * lwp_eosys is handled, so setting it here would affect the next
	 * system call.
	 */
	return (1);

badstack:
	no_fault();
	if (map2)
		pr_unmappage(tos, sizeof (struct rwindow), S_READ, 1);
	if (mapped)
		pr_unmappage((caddr_t)fp, SA(minstacksz), S_WRITE, 1);
	if (tuc)
		kmem_free(tuc, sizeof (ucontext_t));
	if (xregs)
		kmem_free(xregs, xregs_size);
	if (gwp)
		kmem_free(gwp, gwin_size);
#ifdef DEBUG
	printf("sendsig: bad signal stack pid=%d, sig=%d\n",
	    p->p_pid, sig);
	printf("on fault, sigsp = %p, action = %p, upc = 0x%lx\n",
	    (void *)fp, (void *)hdlr, rp->r_pc);
#endif
	return (0);
}


#ifdef _SYSCALL32_IMPL

/*
 * Construct the execution environment for the user's signal
 * handler and arrange for control to be given to it on return
 * to userland.  The library code now calls setcontext() to
 * clean up after the signal handler, so sigret() is no longer
 * needed.
 */
int
sendsig32(int sig, k_siginfo_t *sip, void (*hdlr)())
{
	/*
	 * 'volatile' is needed to ensure that values are
	 * correct on the error return from on_fault().
	 */
	volatile int minstacksz; /* min stack required to catch signal */
	int newstack = 0;	/* if true, switching to altstack */
	label_t ljb;
	caddr_t sp;
	struct regs *volatile rp;
	klwp_t *lwp = ttolwp(curthread);
	proc_t *volatile p = ttoproc(curthread);
	struct fq32 fpu_q[MAXFPQ]; /* to hold floating queue */
	struct fq32 *dfq = NULL;
	size_t fpq_size = 0;
	struct sigframe32 {
		struct frame32 frwin;
		ucontext32_t uc;
	};
	struct sigframe32 *volatile fp;
	siginfo32_t *sip_addr;
	ucontext_t *volatile tuc = NULL;
	ucontext32_t *volatile tuc32 = NULL;
	char *volatile xregs = NULL;
	volatile int xregs_size = 0;
	gwindows32_t *volatile gwp = NULL;
	volatile size_t gwin_size = 0;
	kfpu_t *fpp;
	struct machpcb *mpcb;
	volatile int mapped = 0;
	volatile int map2 = 0;
	caddr_t tos;

	/*
	 * Make sure the current last user window has been flushed to
	 * the stack save area before we change the sp.
	 * Restore register window if a debugger modified it.
	 */
	(void) flush_user_windows_to_stack(NULL);
	if (lwp->lwp_pcb.pcb_xregstat != XREGNONE)
		xregrestore(lwp, 0);

	mpcb = lwptompcb(lwp);
	rp = lwptoregs(lwp);

	/*
	 * Clear the watchpoint return stack pointers.
	 */
	mpcb->mpcb_rsp[0] = NULL;
	mpcb->mpcb_rsp[1] = NULL;

	minstacksz = sizeof (struct sigframe32);

	if (sip != NULL)
		minstacksz += sizeof (siginfo32_t);

	/*
	 * These two fields are pointed to by ABI structures and may
	 * be of arbitrary length. Size them now so we know how big
	 * the signal frame has to be.
	 */
	fpp = lwptofpu(lwp);
	fpp->fpu_fprs = _fp_read_fprs();
	if ((fpp->fpu_en) || (fpp->fpu_fprs & FPRS_FEF)) {
		fpq_size = sizeof (struct fpq32) * fpp->fpu_qcnt;
		minstacksz += fpq_size;
		dfq = fpu_q;
	}

	mpcb = lwptompcb(lwp);
	if (mpcb->mpcb_wbcnt != 0) {
		gwin_size = (mpcb->mpcb_wbcnt * sizeof (struct rwindow32)) +
		    (SPARC_MAXREGWINDOW * sizeof (caddr32_t)) +
		    sizeof (int32_t);
		minstacksz += gwin_size;
	}

	/*
	 * Extra registers, if supported by this platform, may be of arbitrary
	 * length. Size them now so we know how big the signal frame has to be.
	 */
	xregs_size = xregs_getsize(p);
	minstacksz += xregs_size;

	/*
	 * Figure out whether we will be handling this signal on
	 * an alternate stack specified by the user. Then allocate
	 * and validate the stack requirements for the signal handler
	 * context. on_fault will catch any faults.
	 */
	newstack = (sigismember(&u.u_sigonstack, sig) &&
	    !(lwp->lwp_sigaltstack.ss_flags & (SS_ONSTACK|SS_DISABLE)));

	tos = (void *)(uint32_t)rp->r_sp;
	if (newstack != 0) {
		fp = (struct sigframe32 *)
		    (SA32((uintptr_t)lwp->lwp_sigaltstack.ss_sp) +
			SA32((int)lwp->lwp_sigaltstack.ss_size) -
			STACK_ALIGN32 -
			SA32(minstacksz));
	} else {
		fp = (struct sigframe32 *)(tos - SA32(minstacksz));
		/*
		 * Could call grow here, but stack growth now handled below
		 * in code protected by on_fault().
		 */
	}
	sp = (caddr_t)fp + sizeof (struct sigframe32);

	/*
	 * Make sure process hasn't trashed its stack.
	 */
	if (((uintptr_t)fp & (STACK_ALIGN32 - 1)) != 0 ||
	    (caddr_t)fp >= p->p_usrstack ||
	    (caddr_t)fp + SA32(minstacksz) >= p->p_usrstack) {
#ifdef DEBUG
		printf("sendsig32: bad signal stack pid=%d, sig=%d\n",
		    p->p_pid, sig);
		printf("sigsp = 0x%p, action = 0x%p, upc = 0x%lx\n",
		    (void *)fp, (void *)hdlr, rp->r_pc);

		if (((uintptr_t)fp & (STACK_ALIGN32 - 1)) != 0)
			printf("bad stack alignment\n");
		else
			printf("fp above USRSTACK32\n");
#endif
		return (0);
	}

	if (p->p_warea != NULL)
		mapped = pr_mappage((caddr_t)fp, SA32(minstacksz), S_WRITE, 1);
	if (on_fault(&ljb))
		goto badstack;

	tuc = kmem_alloc(sizeof (*tuc), KM_SLEEP);
	tuc32 = kmem_alloc(sizeof (*tuc32), KM_SLEEP);
	xregs_clrptr(lwp, tuc);
	savecontext(tuc, lwp->lwp_sigoldmask);
	ucontext_nto32(tuc, tuc32, rp->r_tstate, dfq);

	/*
	 * save extra register state if it exists
	 */
	if (xregs_size == 0)
		xregs_clrptr32(lwp, tuc32);
	else {
		xregs_setptr32(lwp, tuc32, (caddr32_t)sp);
		xregs = kmem_alloc(xregs_size, KM_SLEEP);
		xregs_get(lwp, xregs);
		copyout_noerr(xregs, sp, xregs_size);
		kmem_free(xregs, xregs_size);
		xregs = NULL;
		sp += xregs_size;
	}

	copyout_noerr(tuc32, &fp->uc, sizeof (*tuc32));
	kmem_free(tuc, sizeof (*tuc));
	tuc = NULL;
	kmem_free(tuc32, sizeof (*tuc32));
	tuc32 = NULL;

	if (sip != NULL) {
		siginfo32_t si32;

		siginfo_kto32(sip, &si32);
		uzero(sp, sizeof (siginfo32_t));
		copyout_noerr(&si32, sp, sizeof (siginfo32_t));
		sip_addr = (siginfo32_t *)sp;
		sp += sizeof (siginfo32_t);

		if (sig == SIGPROF &&
		    curthread->t_rprof != NULL &&
		    curthread->t_rprof->rp_anystate) {
			/*
			 * We stand on our head to deal with
			 * the real time profiling signal.
			 * Fill in the stuff that doesn't fit
			 * in a normal k_siginfo structure.
			 */
			int i = sip->si_nsysarg;
			while (--i >= 0) {
				suword32_noerr(&sip_addr->si_sysarg[i],
				    (uint32_t)lwp->lwp_arg[i]);
			}
			copyout_noerr(curthread->t_rprof->rp_state,
			    sip_addr->si_mstate,
			    sizeof (curthread->t_rprof->rp_state));
		}
	} else {
		sip_addr = NULL;
	}

	/*
	 * When flush_user_windows_to_stack() can't save all the
	 * windows to the stack, it puts them in the lwp's pcb.
	 */
	if (gwin_size != 0) {
		gwp = kmem_alloc(gwin_size, KM_SLEEP);
		getgwins32(lwp, gwp);
		suword32_noerr(&fp->uc.uc_mcontext.gwins, (uint32_t)sp);
		copyout_noerr(gwp, sp, gwin_size);
		kmem_free(gwp, gwin_size);
		gwp = NULL;
		sp += gwin_size;
	} else {
		suword32_noerr(&fp->uc.uc_mcontext.gwins, (uint32_t)NULL);
	}

	if (fpq_size != 0) {
		/*
		 * Update the (already copied out) fpu32.fpu_q pointer
		 * from NULL to the 32-bit address on the user's stack
		 * where we then copyout the fq32 to.
		 */
		struct fq32 *fqp = (struct fq32 *)sp;
		suword32_noerr(&fp->uc.uc_mcontext.fpregs.fpu_q, (uint32_t)fqp);
		copyout_noerr(dfq, fqp, fpq_size);

		/*
		 * forget the fp queue so that the signal handler can run
		 * without being harrassed--it will do a setcontext that will
		 * re-establish the queue if there still is one
		 *
		 * NOTE: fp_runq() relies on the qcnt field being zeroed here
		 *	to terminate its processing of the queue after signal
		 *	delivery.
		 */
		mpcb->mpcb_fpu->fpu_qcnt = 0;
		sp += fpq_size;

		/* Also, syscall needs to know about this */
		mpcb->mpcb_flags |= FP_TRAPPED;

	} else {
		suword32_noerr(&fp->uc.uc_mcontext.fpregs.fpu_q,
		    (uint32_t)NULL);
		suword8_noerr(&fp->uc.uc_mcontext.fpregs.fpu_qcnt, 0);
	}


	/*
	 * Since we flushed the user's windows and we are changing his
	 * stack pointer, the window that the user will return to will
	 * be restored from the save area in the frame we are setting up.
	 * We copy in save area for old stack pointer so that debuggers
	 * can do a proper stack backtrace from the signal handler.
	 */
	if (mpcb->mpcb_wbcnt == 0) {
		if (p->p_warea != NULL) {
			map2 = pr_mappage(tos, sizeof (struct rwindow32),
			    S_READ, 1);
		}
		ucopy(tos, &fp->frwin, sizeof (struct rwindow32));
	}

	lwp->lwp_oldcontext = (uintptr_t)&fp->uc;

	if (newstack != 0)
		lwp->lwp_sigaltstack.ss_flags |= SS_ONSTACK;

	no_fault();
	mpcb->mpcb_wbcnt = 0;		/* let user go on */

	if (map2)
		pr_unmappage(tos, sizeof (struct rwindow32), S_READ, 1);
	if (mapped)
		pr_unmappage((caddr_t)fp, SA32(minstacksz), S_WRITE, 1);

	/*
	 * Set up user registers for execution of signal handler.
	 */
	rp->r_sp = (uintptr_t)fp;
	rp->r_pc = (uintptr_t)hdlr;
	rp->r_npc = (uintptr_t)hdlr + 4;
	/* make sure %asi is ASI_PNF */
	rp->r_tstate &= ~((uint64_t)TSTATE_ASI_MASK << TSTATE_ASI_SHIFT);
	rp->r_tstate |= ((uint64_t)ASI_PNF << TSTATE_ASI_SHIFT);
	rp->r_o0 = sig;
	rp->r_o1 = (uintptr_t)sip_addr;
	rp->r_o2 = (uintptr_t)&fp->uc;
	/*
	 * Don't set lwp_eosys here.  sendsig() is called via psig() after
	 * lwp_eosys is handled, so setting it here would affect the next
	 * system call.
	 */
	return (1);

badstack:
	no_fault();
	if (map2)
		pr_unmappage(tos, sizeof (struct rwindow32), S_READ, 1);
	if (mapped)
		pr_unmappage((caddr_t)fp, SA32(minstacksz), S_WRITE, 1);
	if (tuc)
		kmem_free(tuc, sizeof (*tuc));
	if (tuc32)
		kmem_free(tuc32, sizeof (*tuc32));
	if (xregs)
		kmem_free(xregs, xregs_size);
	if (gwp)
		kmem_free(gwp, gwin_size);
#ifdef DEBUG
	printf("sendsig32: bad signal stack pid=%d, sig=%d\n",
	    p->p_pid, sig);
	printf("on fault, sigsp = 0x%p, action = 0x%p, upc = 0x%lx\n",
	    (void *)fp, (void *)hdlr, rp->r_pc);
#endif
	return (0);
}

#endif /* _SYSCALL32_IMPL */


/*
 * load user registers into lwp
 */
void
lwp_load(klwp_t *lwp, gregset_t gregs)
{
	setgregs(lwp, gregs);
	if (lwptoproc(lwp)->p_model == DATAMODEL_ILP32)
		lwptoregs(lwp)->r_tstate = TSTATE_USER32;
	else
		lwptoregs(lwp)->r_tstate = TSTATE_USER64;
#ifdef DEBUG
	if (!fpu_exists)
		lwptoregs(lwp)->r_tstate &= ~TSTATE_PEF;
#endif
	lwp->lwp_eosys = JUSTRETURN;
	lwptot(lwp)->t_post_sys = 1;
}

/*
 * set syscall()'s return values for a lwp.
 */
void
lwp_setrval(klwp_t *lwp, int v1, int v2)
{
	struct regs *rp = lwptoregs(lwp);

	rp->r_tstate &= ~TSTATE_IC;
	rp->r_o0 = v1;
	rp->r_o1 = v2;
}

/*
 * set stack pointer for a lwp
 */
void
lwp_setsp(klwp_t *lwp, caddr_t sp)
{
	struct regs *rp = lwptoregs(lwp);
	rp->r_sp = (uintptr_t)sp;
}

/*
 * Invalidate the saved user register windows in the pcb struct
 * for the current thread. They will no longer be preserved.
 */
void
lwp_clear_uwin(void)
{
	struct machpcb *m = lwptompcb(ttolwp(curthread));

	/*
	 * This has the effect of invalidating all (any) of the
	 * user level windows that are currently sitting in the
	 * kernel buffer.
	 */
	m->mpcb_wbcnt = 0;
}

static uint_t
mkpsr(uint64_t tstate, uint_t fprs)
{
	uint_t psr, icc;

	psr = tstate & TSTATE_CWP_MASK;
	if (tstate & TSTATE_PRIV)
		psr |= PSR_PS;
	if (fprs & FPRS_FEF)
		psr |= PSR_EF;
	icc = (uint_t)(tstate >> PSR_TSTATE_CC_SHIFT) & PSR_ICC;
	psr |= icc;
	psr |= V9_PSR_IMPLVER;
	return (psr);
}

void
sync_icache(caddr_t va, uint_t len)
{
	caddr_t end;

	end = va + len;
	va = (caddr_t)((uintptr_t)va & -8l);	/* sparc needs 8-byte align */
	while (va < end) {
		doflush(va);
		va += 8;
	}
}

#ifdef _SYSCALL32_IMPL

/*
 * Copy the floating point queue if and only if there is a queue and a place
 * to copy it to. Let xregs take care of the other fp regs, for v8plus.
 * The issue is that while we are handling the fq32 in sendsig, we
 * still need a 64-bit pointer to it, and the caddr32_t in fpregset32_t
 * will not suffice, so we have the third parameter to this function.
 */
static void
fpuregset_nto32(const fpregset_t *src, fpregset32_t *dest, struct fq32 *dfq)
{
	int i;

	bzero(dest, sizeof (*dest));
	for (i = 0; i < 32; i++)
		dest->fpu_fr.fpu_regs[i] = src->fpu_fr.fpu_regs[i];
	dest->fpu_q = NULL;
	dest->fpu_fsr = (uint32_t)src->fpu_fsr;
	dest->fpu_qcnt = src->fpu_qcnt;
	dest->fpu_q_entrysize = sizeof (struct fpq32);
	dest->fpu_en = src->fpu_en;

	if ((src->fpu_qcnt) && (dfq != NULL)) {
		struct fq *sfq = src->fpu_q;
		for (i = 0; i < src->fpu_qcnt; i++, dfq++, sfq++) {
			dfq->FQu.fpq.fpq_addr =
			    (caddr32_t)sfq->FQu.fpq.fpq_addr;
			dfq->FQu.fpq.fpq_instr = sfq->FQu.fpq.fpq_instr;
		}
	}
}

/*
 * Copy the floating point queue if and only if there is a queue and a place
 * to copy it to. Let xregs take care of the other fp regs, for v8plus.
 * The *dfq is required to escape the bzero in both this function and in
 * ucontext_32ton. The *sfq is required because once the fq32 is copied
 * into the kernel, in setcontext, then we need a 64-bit pointer to it.
 */
static void
fpuregset_32ton(const fpregset32_t *src, fpregset_t *dest,
    const struct fq32 *sfq, struct fq *dfq)
{
	int i;

	bzero(dest, sizeof (*dest));
	for (i = 0; i < 32; i++)
		dest->fpu_fr.fpu_regs[i] = src->fpu_fr.fpu_regs[i];
	dest->fpu_q = dfq;
	dest->fpu_fsr = (uint64_t)src->fpu_fsr;
	if ((dest->fpu_qcnt = src->fpu_qcnt) > 0)
		dest->fpu_q_entrysize = sizeof (struct fpq);
	else
		dest->fpu_q_entrysize = 0;
	dest->fpu_en = src->fpu_en;

	if ((src->fpu_qcnt) && (sfq) && (dfq)) {
		for (i = 0; i < src->fpu_qcnt; i++, dfq++, sfq++) {
			dfq->FQu.fpq.fpq_addr =
			    (unsigned int *)sfq->FQu.fpq.fpq_addr;
			dfq->FQu.fpq.fpq_instr = sfq->FQu.fpq.fpq_instr;
		}
	}
}

void
ucontext_nto32(const ucontext_t *src, ucontext32_t *dest, uint64_t tstate,
    struct fq32 *dfq)
{
	int i;

	bzero(dest, sizeof (*dest));

	dest->uc_flags = src->uc_flags;
	dest->uc_link = (caddr32_t)src->uc_link;

	for (i = 0; i < 4; i++) {
		dest->uc_sigmask.__sigbits[i] = src->uc_sigmask.__sigbits[i];
	}

	dest->uc_stack.ss_sp = (caddr32_t)src->uc_stack.ss_sp;
	dest->uc_stack.ss_size = (size32_t)src->uc_stack.ss_size;
	dest->uc_stack.ss_flags = src->uc_stack.ss_flags;

	/* REG_PSR is 0, skip over it and handle it after this loop */
	for (i = 1; i < _NGREG32; i++)
		dest->uc_mcontext.gregs[i] =
		    (greg32_t)src->uc_mcontext.gregs[i];
	dest->uc_mcontext.gregs[REG_PSR] = mkpsr(tstate,
	    (uint32_t)src->uc_mcontext.gregs[REG_FPRS]);

	dest->uc_mcontext.gwins = (caddr32_t)src->uc_mcontext.gwins;
	fpuregset_nto32(&src->uc_mcontext.fpregs, &dest->uc_mcontext.fpregs,
	    dfq);
}

void
ucontext_32ton(const ucontext32_t *src, ucontext_t *dest,
    const struct fq32 *sfq, struct fq *dfq)
{
	int i;

	bzero(dest, sizeof (*dest));

	dest->uc_flags = src->uc_flags;
	dest->uc_link = (ucontext_t *)src->uc_link;

	for (i = 0; i < 4; i++) {
		dest->uc_sigmask.__sigbits[i] = src->uc_sigmask.__sigbits[i];
	}

	dest->uc_stack.ss_sp = (void *)src->uc_stack.ss_sp;
	dest->uc_stack.ss_size = (size_t)src->uc_stack.ss_size;
	dest->uc_stack.ss_flags = src->uc_stack.ss_flags;

	/* REG_CCR is 0, skip over it and handle it after this loop */
	for (i = 1; i < _NGREG32; i++)
		dest->uc_mcontext.gregs[i] =
		    (greg_t)(uint32_t)src->uc_mcontext.gregs[i];
	dest->uc_mcontext.gregs[REG_CCR] =
	    (src->uc_mcontext.gregs[REG_PSR] & PSR_ICC) >> PSR_ICC_SHIFT;
	dest->uc_mcontext.gregs[REG_ASI] = 0;
	/*
	 * A valid fpregs is only copied in if (uc.uc_flags & UC_FPU),
	 * otherwise there is no guarantee that anything in fpregs is valid.
	 */
	if (src->uc_flags & UC_FPU) {
		dest->uc_mcontext.gregs[REG_FPRS] =
		    ((src->uc_mcontext.fpregs.fpu_en) ?
		    (FPRS_DU|FPRS_DL|FPRS_FEF) : 0);
	} else {
		dest->uc_mcontext.gregs[REG_FPRS] = 0;
	}
	dest->uc_mcontext.gwins = (gwindows_t *)src->uc_mcontext.gwins;
	if (src->uc_flags & UC_FPU) {
		fpuregset_32ton(&src->uc_mcontext.fpregs,
		    &dest->uc_mcontext.fpregs, sfq, dfq);
	}
}

void
rwindow_nto32(struct rwindow *src, struct rwindow32 *dest)
{
	greg_t *s = (greg_t *)src;
	greg32_t *d = (greg32_t *)dest;
	int i;

	for (i = 0; i < 16; i++)
		*d++ = (greg32_t)*s++;
}

void
rwindow_32ton(struct rwindow32 *src, struct rwindow *dest)
{
	greg32_t *s = (greg32_t *)src;
	greg_t *d = (greg_t *)dest;
	int i;

	for (i = 0; i < 16; i++)
		*d++ = (uint32_t)*s++;
}

#endif /* _SYSCALL32_IMPL */

/*
 * The panic code invokes panic_saveregs() to record the contents of a
 * regs structure into the specified panic_data structure for debuggers.
 */
void
panic_saveregs(panic_data_t *pdp, struct regs *rp)
{
	panic_nv_t *pnv = PANICNVGET(pdp);

	PANICNVADD(pnv, "tstate", rp->r_tstate);
	PANICNVADD(pnv, "g1", rp->r_g1);
	PANICNVADD(pnv, "g2", rp->r_g2);
	PANICNVADD(pnv, "g3", rp->r_g3);
	PANICNVADD(pnv, "g4", rp->r_g4);
	PANICNVADD(pnv, "g5", rp->r_g5);
	PANICNVADD(pnv, "g6", rp->r_g6);
	PANICNVADD(pnv, "g7", rp->r_g7);
	PANICNVADD(pnv, "o0", rp->r_o0);
	PANICNVADD(pnv, "o1", rp->r_o1);
	PANICNVADD(pnv, "o2", rp->r_o2);
	PANICNVADD(pnv, "o3", rp->r_o3);
	PANICNVADD(pnv, "o4", rp->r_o4);
	PANICNVADD(pnv, "o5", rp->r_o5);
	PANICNVADD(pnv, "o6", rp->r_o6);
	PANICNVADD(pnv, "o7", rp->r_o7);
	PANICNVADD(pnv, "pc", (ulong_t)rp->r_pc);
	PANICNVADD(pnv, "npc", (ulong_t)rp->r_npc);
	PANICNVADD(pnv, "y", (uint32_t)rp->r_y);

	PANICNVSET(pdp, pnv);
}
