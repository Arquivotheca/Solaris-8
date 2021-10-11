/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)prmachdep.c	1.43	99/05/04 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/psw.h>
#include <sys/pcb.h>
#include <sys/buf.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/cpuvar.h>

#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/cmn_err.h>
#include <sys/stack.h>
#include <sys/debugreg.h>
#include <sys/copyops.h>

#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/vmparam.h>
#include <sys/fp.h>
#include <sys/archsystm.h>
#include <sys/vmsystm.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_kp.h>
#include <vm/page.h>

#include <sys/sysi86.h>

#include <fs/proc/prdata.h>

int	prnwatch = 10000;	/* maximum number of watched areas */

/*
 * Force a thread into the kernel if it is not already there.
 * This is a no-op on uniprocessors.
 */
/* ARGSUSED */
void
prpokethread(kthread_t *t)
{
	if (t->t_state == TS_ONPROC && t->t_cpu != CPU)
		poke_cpu(t->t_cpu->cpu_id);
}

/*
 * Map a target process's u-block in and out.  prumap() makes it addressable
 * (if necessary) and returns a pointer to it.
 */
struct user *
prumap(proc_t *p)
{
	return (PTOU(p));
}

/* ARGSUSED */
void
prunmap(proc_t *p)
{
	/*
	 * With paged u-blocks, there's nothing to do in order to unmap.
	 */
}

/*
 * Return general registers.
 */
void
prgetprregs(klwp_t *lwp, prgregset_t prp)
{
	struct regs *r = lwptoregs(lwp);

	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	bcopy((caddr_t)r, (caddr_t)prp, sizeof (prgregset_t));
}

/*
 * Set general registers.
 * (Note: This can be an alias to setgregs().)
 */
void
prsetprregs(klwp_t *lwp, prgregset_t prp, int initial)
{
	if (initial)		/* set initial values */
		lwptoregs(lwp)->r_efl = PSL_USER;
	(void) setgregs(lwp, prp);
}

/*
 * Get the syscall return values for the lwp.
 */
int
prgetrvals(klwp_t *lwp, long *rval1, long *rval2)
{
	struct regs *r = lwptoregs(lwp);

	if (r->r_efl & PS_C)
		return (r->r_eax);
	if (lwp->lwp_eosys == JUSTRETURN) {
		*rval1 = 0;
		*rval2 = 0;
	} else {
		*rval1 = r->r_eax;
		*rval2 = r->r_edx;
	}
	return (0);
}

/*
 * Does the system support floating-point, either through hardware
 * or by trapping and emulating floating-point machine instructions?
 */
int
prhasfp(void)
{
	extern int fp_kind;

	return (fp_kind != FP_NO);
}

/*
 * Get floating-point registers.
 */
void
prgetprfpregs(klwp_t *lwp, prfpregset_t *pfp)
{
	bzero(pfp, sizeof (prfpregset_t));
	(void) getfpregs(lwp, pfp);
}

/*
 * Set floating-point registers.
 * (Note: This can be an alias to setfpregs().)
 */
void
prsetprfpregs(klwp_t *lwp, prfpregset_t *pfp)
{
	(void) setfpregs(lwp, pfp);
}

/*
 * Does the system support extra register state?
 */
/* ARGSUSED */
int
prhasx(proc_t *p)
{
	return (0);
}

/*
 * Get the size of the extra registers.
 */
/* ARGSUSED */
int
prgetprxregsize(proc_t *p)
{
	return (0);
}

/*
 * Get extra registers.
 */
/*ARGSUSED*/
void
prgetprxregs(klwp_t *lwp, caddr_t prx)
{
	/* no extra registers */
}

/*
 * Set extra registers.
 */
/*ARGSUSED*/
void
prsetprxregs(klwp_t *lwp, caddr_t prx)
{
	/* no extra registers */
}

#if 0	/* USL, not Solaris */
/*
 * Get debug registers.
 * XXX - NEEDS REVIEW w.r.t save/restore debug registers. For now, we assume
 *	 that the pcb already has the saved debug registers.
 */
void
prgetdbregs(klwp_t *lwp, dbregset_t *db)
{
	struct pcb *pcb = &lwp->lwp_pcb;

	bcopy((caddr_t)&pcb->pcb_dregs, (caddr_t)db, sizeof (dbregset_t));
}
#endif

#if 0	/* USL, not Solaris */
/*
 * Set debug registers.
 * XXX - NEEDS REVIEW.
 * (Notes: switch code needs to save/restore the debug registers. Also it
 *  is assumed that the trap/syscall handling saves/restores the debug
 *  registers in locore.s. The DEBUG_ON flag indicates if the process is
 *  using debug registers.
 *  Question: How do we handle if kadb (i.e kernel debugging) needs to
 *  use debug registers?)
 */
void
prsetdbregs(klwp_t *lwp, dbregset_t *db)
{
	struct pcb *pcb = &lwp->lwp_pcb;

	db->debugreg[DR_CONTROL] &= ~(DR_GLOBAL_SLOWDOWN |
					DR_CONTROL_RESERVED |
					DR_GLOBAL_ENABLE_MASK);

	bcopy((caddr_t)db, (caddr_t)&pcb->pcb_dregs, sizeof (dbregset_t));
	if (db->debugreg[DR_CONTROL] & (DR_LOCAL_SLOWDOWN|DR_LOCAL_ENABLE_MASK))
		pcb->pcb_flags |= DEBUG_ON;
	else
		pcb->pcb_flags &= ~DEBUG_ON;
}
#endif

/*
 * Return the base (lower limit) of the process stack.
 */
caddr_t
prgetstackbase(proc_t *p)
{
	return ((caddr_t)USRSTACK - p->p_stksize);
}

/*
 * Return the "addr" field for pr_addr in prpsinfo_t.
 * This is a vestige of the past, so whatever we return is OK.
 */
caddr_t
prgetpsaddr(proc_t *p)
{
	return ((caddr_t)p);
}

/*
 * Arrange to single-step the lwp.
 */
void
prstep(klwp_t *lwp, int watchstep)
{
	struct regs *r = lwptoregs(lwp);

	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	if (watchstep)
		lwp->lwp_pcb.pcb_flags |= WATCH_STEP;
	else
		lwp->lwp_pcb.pcb_flags |= NORMAL_STEP;

	r->r_efl |= PS_T;	/* set the trace flag in PSW */
}

/*
 * Undo prstep().
 */
void
prnostep(klwp_t *lwp)
{
	struct regs *r = lwptoregs(lwp);

	ASSERT(ttolwp(curthread) == lwp ||
	    MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	r->r_efl &= ~PS_T;	/* turn off trace flag in PSW */
	lwp->lwp_pcb.pcb_flags &= ~(NORMAL_STEP|WATCH_STEP|DEBUG_PENDING);
}

/*
 * Return non-zero if a single-step is in effect.
 */
int
prisstep(klwp_t *lwp)
{
	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	return ((lwp->lwp_pcb.pcb_flags &
		(NORMAL_STEP|WATCH_STEP|DEBUG_PENDING)) != 0);
}

/*
 * Set the PC to the specified virtual address.
 */
void
prsvaddr(klwp_t *lwp, caddr_t vaddr)
{
	struct regs *r = lwptoregs(lwp);

	ASSERT(MUTEX_NOT_HELD(&lwptoproc(lwp)->p_lock));

	r->r_eip = (int)vaddr;
}

/*
 * Map address "addr" in address space "as" into a kernel virtual address.
 * The memory is guaranteed to be resident and locked down.
 */
caddr_t
prmapin(struct as *as, caddr_t addr, int writing)
{
	page_t *pp;
	caddr_t kaddr;
	pfn_t pfnum;

	/*
	 * XXX - Because of past mistakes, we have bits being returned
	 * by getpfnum that are actually the page type bits of the pte.
	 * When the object we are trying to map is a memory page with
	 * a page structure everything is ok and we can use the optimal
	 * method, ppmapin.  Otherwise, we have to do something special.
	 */
	pfnum = hat_getpfnum(as->a_hat, addr);
	if (pf_is_memory(pfnum)) {
		pp = page_numtopp_nolock(pfnum);
		if (pp != NULL) {
			ASSERT(PAGE_LOCKED(pp));
			kaddr = ppmapin(pp, writing ?
				(PROT_READ | PROT_WRITE) : PROT_READ,
				(caddr_t)-1);
			return (kaddr + ((int)addr & PAGEOFFSET));
		}
	}

	/*
	 * Oh well, we didn't have a page struct for the object we were
	 * trying to map in; ppmapin doesn't handle devices, but allocating a
	 * heap address allows ppmapout to free virutal space when done.
	 */
	kaddr = vmem_alloc(heap_arena, PAGESIZE, VM_SLEEP);

	hat_devload(kas.a_hat, kaddr, MMU_PAGESIZE,  pfnum,
		writing ? (PROT_READ | PROT_WRITE) : PROT_READ, 0);

	return (kaddr + ((int)addr & PAGEOFFSET));
}

/*
 * Unmap address "addr" in address space "as"; inverse of prmapin().
 */
/* ARGSUSED */
void
prmapout(struct as *as, caddr_t addr, caddr_t vaddr, int writing)
{
	extern void ppmapout(caddr_t);

	vaddr = (caddr_t)((long)vaddr & PAGEMASK);
	ppmapout(vaddr);
}

/*
 * Make sure the lwp is in an orderly state
 * for inspection by a debugger through /proc.
 * Called from stop() and from syslwp_create().
 */
/* ARGSUSED */
void
prstop(int why, int what)
{
	klwp_t *lwp = ttolwp(curthread);
	proc_t *p = lwptoproc(lwp);
	struct regs *r = lwptoregs(lwp);
	int mapped;

	/*
	 * Make sure we don't deadlock on a recursive call
	 * to prstop().  stop() tests the lwp_nostop flag.
	 */
	ASSERT(lwp->lwp_nostop == 0);
	lwp->lwp_nostop = 1;

	mapped = 0;
	if (p->p_warea)		/* watchpoints in effect */
		mapped = pr_mappage((caddr_t)r->r_eip,
			sizeof (lwp->lwp_pcb.pcb_instr), S_READ, 1);
	if (default_copyin((caddr_t)r->r_eip, &lwp->lwp_pcb.pcb_instr,
	    sizeof (lwp->lwp_pcb.pcb_instr)) == 0)
		lwp->lwp_pcb.pcb_flags |= INSTR_VALID;
	else {
		lwp->lwp_pcb.pcb_flags &= ~INSTR_VALID;
		lwp->lwp_pcb.pcb_instr = 0;
	}
	if (mapped)
		pr_unmappage((caddr_t)r->r_eip,
			sizeof (lwp->lwp_pcb.pcb_instr), S_READ, 1);

	(void) save_syscall_args();
	ASSERT(lwp->lwp_nostop == 1);
	lwp->lwp_nostop = 0;
}

/*
 * Fetch the user-level instruction on which the lwp is stopped.
 * It was saved by the lwp itself, in prstop().
 * Return non-zero if the instruction is valid.
 */
int
prfetchinstr(klwp_t *lwp, ulong_t *ip)
{
	*ip = (ulong_t)(instr_t)lwp->lwp_pcb.pcb_instr;
	return (lwp->lwp_pcb.pcb_flags & INSTR_VALID);
}

/*
 * Called from trap() when a load or store instruction
 * falls in a watched page but is not a watchpoint.
 * We emulate the instruction in the kernel.
 */
/* ARGSUSED */
int
pr_watch_emul(struct regs *rp, caddr_t addr, enum seg_rw rw)
{
#ifdef SOMEDAY
	int res;
	proc_t *p = curproc;
	char *badaddr = (caddr_t)(-1);
	int mapped;

	/* prevent recursive calls to pr_watch_emul() */
	ASSERT(!(curthread->t_flag & T_WATCHPT));
	curthread->t_flag |= T_WATCHPT;

	mapped = pr_mappage(addr, 8, rw, 1);
	res = do_unaligned(rp, &badaddr);
	if (mapped)
		pr_unmappage(addr, 8, rw, 1);

	curthread->t_flag &= ~T_WATCHPT;
	if (res == SIMU_SUCCESS) {
		/* adjust the pc */
		return (1);
	}
#endif
	return (0);
}

/*
 * Return the number of active entries in the local descriptor table.
 */
int
prnldt(proc_t *p)
{
	int n;
	int i;
	int limit;
	struct dscr *dscrp;

	ASSERT(MUTEX_HELD(&p->p_ldtlock));

	if (p->p_ldt == NULL)
		return (0);

	n = 0;
	limit = p->p_ldtlimit;
	ASSERT(limit >= 0 && limit < MAXLDTSZ);

	for (i = seltoi(USER_DS) + 1, dscrp = (struct dscr *)p->p_ldt + i;
	    i <= limit; i++, dscrp++) {
		if (getdscracc1(dscrp))
			n++;
	}

	return (n);
}

/*
 * Fetch the active entries from the local descriptor table.
 */
void
prgetldt(proc_t *p, struct ssd *ssd)
{
	int i;
	int limit;
	struct dscr *dscrp;

	ASSERT(MUTEX_HELD(&p->p_ldtlock));

	if (p->p_ldt == NULL)
		return;

	limit = p->p_ldtlimit;
	ASSERT(limit >= 0 && limit < MAXLDTSZ);

	for (i = seltoi(USER_DS) + 1, dscrp = (struct dscr *)p->p_ldt + i;
	    i <= limit; i++, dscrp++) {
		if (getdscracc1(dscrp)) {
			ssd->sel = itosel(i);
			ssd->bo = getdscrbase(dscrp);
			ssd->ls = getdscrlim(dscrp);
			ssd->acc1 = getdscracc1(dscrp);
			ssd->acc2 = getdscracc2(dscrp);
			ssd++;
		}
	}
}
