/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.144	99/11/20 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stack.h>
#include <sys/regset.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/procfs_isa.h>
#include <sys/kmem.h>
#include <sys/cpuvar.h>
#include <sys/systm.h>
#include <sys/machpcb.h>
#include <sys/spitasi.h>
#include <sys/vis.h>
#include <sys/fpu/fpusystm.h>

int maxphys = MMU_PAGESIZE * 16;	/* 128k */
int klustsize = MMU_PAGESIZE * 16;	/* 128k */

#if !defined(__sparcv9)
/*
 * Satisfy C compiler reference for -xcg92 option, used by older drivers.
 * DO NOT REMOVE! (It is not in sun4c and sun4m because the directive
 * means that the kernel can do hardware multiply and divide.)
 */
int __cg92_used;
#endif

/*
 * Initialize kernel thread's stack.
 */
caddr_t
thread_stk_init(caddr_t stk)
{
	struct v9_fpu *fp;
	ulong_t align;

	/* allocate extra space for floating point state */
	stk -= SA(sizeof (struct v9_fpu) + GSR_SIZE);
	align = (uintptr_t)stk & 0x3f;
	stk -= align;		/* force v9_fpu to be 16 byte aligned */
	fp = (struct v9_fpu *)stk;
	fp->fpu_fprs = 0;

	stk -= SA(MINFRAME);
	return (stk);
}

/*
 * Initialize lwp's kernel stack.
 * Note that now that the floating point register save area (struct v9_fpu)
 * has been broken out from machpcb and aligned on a 64 byte boundary so that
 * we can do block load/stores to/from it, there are a couple of potential
 * optimizations to save stack space. 1. The floating point register save
 * area could be aligned on a 16 byte boundary, and the floating point code
 * changed to (a) check the alignment and (b) use different save/restore
 * macros depending upon the alignment. 2. The lwp_stk_init code below
 * could be changed to calculate if less space would be wasted if machpcb
 * was first instead of second. However there is a REGOFF macro used in
 * locore, syscall_trap, machdep and mlsetup that assumes that the saved
 * register area is a fixed distance from the %sp, and would have to be
 * changed to a pointer or something...JJ said later.
 */
caddr_t
lwp_stk_init(klwp_t *lwp, caddr_t stk)
{
	struct machpcb *mpcb;
	struct v9_fpu *fp;
	uintptr_t aln;

	stk -= SA(sizeof (struct v9_fpu) + GSR_SIZE);
	aln = (uintptr_t)stk & 0x3F;
	stk -= aln;
	fp = (struct v9_fpu *)stk;
	stk -= SA(sizeof (struct machpcb));
	mpcb = (struct machpcb *)stk;
	bzero(mpcb, sizeof (struct machpcb));
	bzero(fp, sizeof (struct v9_fpu) + GSR_SIZE);
	lwp->lwp_regs = (void *)&mpcb->mpcb_regs;
	lwp->lwp_fpu = (void *)fp;
	mpcb->mpcb_fpu = fp;
	mpcb->mpcb_fpu->fpu_q = mpcb->mpcb_fpu_q;
	mpcb->mpcb_thread = lwp->lwp_thread;
	mpcb->mpcb_wbcnt = 0;
	if (lwp->lwp_procp->p_model == DATAMODEL_ILP32) {
		mpcb->mpcb_wstate = WSTATE_USER32;
		mpcb->mpcb_wbuf = kmem_alloc(MAXWIN * sizeof (struct rwindow32),
		    KM_SLEEP);
	} else {
		mpcb->mpcb_wstate = WSTATE_USER64;
		mpcb->mpcb_wbuf = kmem_alloc(MAXWIN * sizeof (struct rwindow64),
		    KM_SLEEP);
	}
	ASSERT(((uintptr_t)mpcb->mpcb_wbuf & 7) == 0);
	return (stk);
}

void
lwp_stk_fini(klwp_t *lwp)
{
	struct machpcb *mpcb = lwptompcb(lwp);

	/*
	 * there might be windows still in the wbuf due to unmapped
	 * stack, misaligned stack pointer, etc.  We just free it.
	 */
	mpcb->mpcb_wbcnt = 0;
	if (mpcb->mpcb_wstate == WSTATE_USER32)
		kmem_free(mpcb->mpcb_wbuf, MAXWIN * sizeof (struct rwindow32));
	else
		kmem_free(mpcb->mpcb_wbuf, MAXWIN * sizeof (struct rwindow64));
	mpcb->mpcb_wbuf = NULL;
}


/*
 * Copy regs from parent to child.
 */
void
lwp_forkregs(klwp_t *lwp, klwp_t *clwp)
{
	kthread_t *t, *pt = lwptot(lwp);
	struct machpcb *mpcb = lwptompcb(clwp);
	struct machpcb *pmpcb = lwptompcb(lwp);
	struct v9_fpu *fp, *pfp = lwptofpu(lwp);
	caddr_t wbuf;
	uint_t wstate;

	t = mpcb->mpcb_thread;
	/*
	 * remember child's fp and wbuf since they will get erased during
	 * the bcopy.
	 */
	fp = mpcb->mpcb_fpu;
	wbuf = mpcb->mpcb_wbuf;
	wstate = mpcb->mpcb_wstate;
	/*
	 * Don't copy mpcb_frame since we hand-crafted it
	 * in thread_load().
	 */
	bcopy(lwp->lwp_regs, clwp->lwp_regs, sizeof (struct machpcb) - REGOFF);
	mpcb->mpcb_thread = t;
	mpcb->mpcb_fpu = fp;
	fp->fpu_q = mpcb->mpcb_fpu_q;

	/*
	 * It is theoretically possibly for the lwp's wstate to
	 * be different from its value assigned in lwp_stk_init,
	 * since lwp_stk_init assumed the data model of the process.
	 * Here, we took on the data model of the cloned lwp.
	 */
	if (mpcb->mpcb_wstate != wstate) {
		size_t osize, size;

		if (wstate == WSTATE_USER32) {
			osize = MAXWIN * sizeof (struct rwindow32);
			size = MAXWIN * sizeof (struct rwindow64);
			wstate = WSTATE_USER64;
		} else {
			osize = MAXWIN * sizeof (struct rwindow64);
			size = MAXWIN * sizeof (struct rwindow32);
			wstate = WSTATE_USER32;
		}
		kmem_free(wbuf, osize);
		wbuf = kmem_alloc(size, KM_SLEEP);
	}

	mpcb->mpcb_wbuf = wbuf;
	ASSERT(mpcb->mpcb_wstate == wstate);

	if (mpcb->mpcb_wbcnt != 0) {
		bcopy(pmpcb->mpcb_wbuf, mpcb->mpcb_wbuf,
		    mpcb->mpcb_wbcnt * ((mpcb->mpcb_wstate == WSTATE_USER32) ?
		    sizeof (struct rwindow32) : sizeof (struct rwindow64)));
	}

	if (pt == curthread)
		pfp->fpu_fprs = _fp_read_fprs();
	if ((pfp->fpu_en) || (pfp->fpu_fprs & FPRS_FEF)) {
		if (pt == curthread && fpu_exists) {
			save_gsr(clwp->lwp_fpu);
		} else {
			uint64_t gsr;
			gsr = get_gsr(lwp->lwp_fpu);
			set_gsr(gsr, clwp->lwp_fpu);
		}
		fp_fork(lwp, clwp);
	}
}

/*
 * Free lwp fpu regs.
 */
void
lwp_freeregs(klwp_t *lwp, int isexec)
{
	struct v9_fpu *fp = lwptofpu(lwp);

	if (lwptot(lwp) == curthread)
		fp->fpu_fprs = _fp_read_fprs();
	if ((fp->fpu_en) || (fp->fpu_fprs & FPRS_FEF))
		fp_free(fp, isexec);
}

/*
 * fill in the extra register state area specified with the
 * specified lwp's platform-dependent non-floating-point extra
 * register state information
 */
/* ARGSUSED */
void
xregs_getgfiller(klwp_id_t lwp, caddr_t xrp)
{
	/* for sun4u nothing to do here, added for symmetry */
}

/*
 * fill in the extra register state area specified with the specified lwp's
 * platform-dependent floating-point extra register state information.
 * NOTE:  'lwp' might not correspond to 'curthread' since this is
 * called from code in /proc to get the registers of another lwp.
 */
void
xregs_getfpfiller(klwp_id_t lwp, caddr_t xrp)
{
	prxregset_t *xregs = (prxregset_t *)xrp;
	kfpu_t *fp = lwptofpu(lwp);
	uint32_t fprs = (FPRS_FEF|FPRS_DU|FPRS_DL);
	uint64_t gsr;

	/*
	 * fp_fksave() does not flush the GSR register into
	 * the lwp area, so do it now
	 */
	kpreempt_disable();
	if (ttolwp(curthread) == lwp && fpu_exists) {
		fp->fpu_fprs = _fp_read_fprs();
		if ((fp->fpu_fprs & FPRS_FEF) != FPRS_FEF) {
			_fp_write_fprs(fprs);
			fp->fpu_fprs = (V9_FPU_FPRS_TYPE)fprs;
		}
		save_gsr(fp);
	}
	gsr = get_gsr(fp);
	kpreempt_enable();
	PRXREG_GSR(xregs) = gsr;
}

/*
 * set the specified lwp's platform-dependent non-floating-point
 * extra register state based on the specified input
 */
/* ARGSUSED */
void
xregs_setgfiller(klwp_id_t lwp, caddr_t xrp)
{
	/* for sun4u nothing to do here, added for symmetry */
}

/*
 * set the specified lwp's platform-dependent floating-point
 * extra register state based on the specified input
 */
void
xregs_setfpfiller(klwp_id_t lwp, caddr_t xrp)
{
	prxregset_t *xregs = (prxregset_t *)xrp;
	kfpu_t *fp = lwptofpu(lwp);
	uint32_t fprs = (FPRS_FEF|FPRS_DU|FPRS_DL);
	uint64_t gsr = PRXREG_GSR(xregs);

	kpreempt_disable();
	set_gsr(gsr, lwptofpu(lwp));

	if ((lwp == ttolwp(curthread)) && fpu_exists) {
		fp->fpu_fprs = _fp_read_fprs();
		if ((fp->fpu_fprs & FPRS_FEF) != FPRS_FEF) {
			_fp_write_fprs(fprs);
			fp->fpu_fprs = (V9_FPU_FPRS_TYPE)fprs;
		}
		restore_gsr(lwptofpu(lwp));
	}
	kpreempt_enable();
}

#ifdef __sparcv9
/*
 * fill in the sun4u asrs, ie, the lwp's platform-dependent
 * non-floating-point extra register state information
 */
/* ARGSUSED */
void
getasrs(klwp_t *lwp, asrset_t asr)
{
	/* for sun4u nothing to do here, added for symmetry */
}

/*
 * fill in the sun4u asrs, ie, the lwp's platform-dependent
 * floating-point extra register state information
 */
void
getfpasrs(klwp_t *lwp, asrset_t asr)
{
	kfpu_t *fp = lwptofpu(lwp);
	uint32_t fprs = (FPRS_FEF|FPRS_DU|FPRS_DL);

	kpreempt_disable();
	if (ttolwp(curthread) == lwp)
		fp->fpu_fprs = _fp_read_fprs();
	if ((fp->fpu_en) || (fp->fpu_fprs & FPRS_FEF)) {
		if (fpu_exists && ttolwp(curthread) == lwp) {
			if ((fp->fpu_fprs & FPRS_FEF) != FPRS_FEF) {
				_fp_write_fprs(fprs);
				fp->fpu_fprs = (V9_FPU_FPRS_TYPE)fprs;
			}
			save_gsr(fp);
		}
		asr[ASR_GSR] = (int64_t)get_gsr(fp);
	}
	kpreempt_enable();
}

/*
 * set the sun4u asrs, ie, the lwp's platform-dependent
 * non-floating-point extra register state information
 */
/* ARGSUSED */
void
setasrs(klwp_t *lwp, asrset_t asr)
{
	/* for sun4u nothing to do here, added for symmetry */
}

void
setfpasrs(klwp_t *lwp, asrset_t asr)
{
	kfpu_t *fp = lwptofpu(lwp);
	uint32_t fprs = (FPRS_FEF|FPRS_DU|FPRS_DL);

	kpreempt_disable();
	if (ttolwp(curthread) == lwp)
		fp->fpu_fprs = _fp_read_fprs();
	if ((fp->fpu_en) || (fp->fpu_fprs & FPRS_FEF)) {
		set_gsr(asr[ASR_GSR], fp);
		if (fpu_exists && ttolwp(curthread) == lwp) {
			if ((fp->fpu_fprs & FPRS_FEF) != FPRS_FEF) {
				_fp_write_fprs(fprs);
				fp->fpu_fprs = (V9_FPU_FPRS_TYPE)fprs;
			}
			restore_gsr(fp);
		}
	}
	kpreempt_enable();
}
#endif /* __sparcv9 */
