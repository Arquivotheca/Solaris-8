/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)watchpoint.c	1.13	99/03/08 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/inline.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/regset.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/prsystm.h>
#include <sys/buf.h>
#include <sys/signal.h>
#include <sys/user.h>
#include <sys/cpuvar.h>

#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/cmn_err.h>
#include <sys/stack.h>
#include <sys/watchpoint.h>
#include <sys/copyops.h>

#include <sys/mman.h>
#include <vm/as.h>
#include <vm/seg.h>

/*
 * Copy ops vector for watchpoints.
 */
static int	watch_copyin(const void *, void *, size_t);
static int	watch_xcopyin(const void *, void *, size_t);
static int	watch_copyout(const void *, void *, size_t);
static int	watch_xcopyout(const void *, void *, size_t);
static int	watch_copyinstr(const char *, char *, size_t, size_t *);
static int	watch_copyoutstr(const char *, char *, size_t, size_t *);
static int	watch_fuword8(const void *, uint8_t *);
static int	watch_fuiword8(const void *, uint8_t *);
static int	watch_fuword16(const void *, uint16_t *);
static int	watch_fuword32(const void *, uint32_t *);
static int	watch_fuiword32(const void *, uint32_t *);
static int	watch_fuword64(const void *, uint64_t *);
static int	watch_suword8(void *, uint8_t);
static int	watch_suiword8(void *, uint8_t);
static int	watch_suword16(void *, uint16_t);
static int	watch_suword32(void *, uint32_t);
static int	watch_suiword32(void *, uint32_t);
static int	watch_suword64(void *, uint64_t);

struct copyops watch_copyops = {
	watch_copyin,
	watch_xcopyin,
	watch_copyout,
	watch_xcopyout,
	watch_copyinstr,
	watch_copyoutstr,
	watch_fuword8,
	watch_fuiword8,
	watch_fuword16,
	watch_fuword32,
	watch_fuiword32,
	watch_fuword64,
	watch_suword8,
	watch_suiword8,
	watch_suword16,
	watch_suword32,
	watch_suiword32,
	watch_suword64,
	default_physio
};

/*
 * Map the 'rw' argument to a protection flag.
 */
static int
rw_to_prot(enum seg_rw rw)
{
	switch (rw) {
	case S_EXEC:
		return (PROT_EXEC);
	case S_READ:
		return (PROT_READ);
	case S_WRITE:
		return (PROT_WRITE);
	}
	return (0);	/* can't happen */
}

/*
 * Map the 'rw' argument to an index into an array of exec/write/read things.
 * The index follows the precedence order:  exec .. write .. read
 */
static int
rw_to_index(enum seg_rw rw)
{
	switch (rw) {
	default:	/* default case "can't happen" */
	case S_EXEC:
		return (0);
	case S_WRITE:
		return (1);
	case S_READ:
		return (2);
	}
}

/*
 * Map an index back to a seg_rw.
 */
static enum seg_rw S_rw[4] = {
	S_EXEC,
	S_WRITE,
	S_READ,
	S_READ,
};

#define	X	0
#define	W	1
#define	R	2
#define	sum(a)	(a[X] + a[W] + a[R])

/*
 * Common code for pr_mappage() and pr_unmappage().
 */
static int
pr_do_mappage(caddr_t addr, size_t size, int mapin, enum seg_rw rw, int kernel)
{
	proc_t *p = curproc;
	struct as *as = p->p_as;
	char *eaddr = addr + size;
	int prot_rw = rw_to_prot(rw);
	int xrw = rw_to_index(rw);
	int rv = 0;
	struct watched_page *pwp;
	u_int prot;

	ASSERT(as != &kas);

startover:
	ASSERT(rv == 0);
	if ((pwp = as->a_wpage) == NULL)
		return (0);

	/*
	 * as->a_wpage and its linked list can only be changed while
	 * the process is totally stopped.  Don't grab p_lock here.
	 * Holding p_lock while grabbing the address space lock
	 * leads to deadlocks with the clock thread.
	 */
	do {
		if (eaddr <= pwp->wp_vaddr)
			break;
		if (addr >= pwp->wp_vaddr + PAGESIZE)
			continue;

		/*
		 * If the requested protection has not been
		 * removed, we need not remap this page.
		 */
		prot = pwp->wp_prot;
		if (kernel || (prot & PROT_USER))
			if (prot & prot_rw)
				continue;
		/*
		 * If the requested access does not exist in the page's
		 * original protections, we need not remap this page.
		 * If the page does not exist yet, we can't test it.
		 */
		if ((prot = pwp->wp_oprot) != 0) {
			if (!(kernel || (prot & PROT_USER)))
				continue;
			if (!(prot & prot_rw))
				continue;
		}

		if (mapin) {
			/*
			 * Before mapping the page in, ensure that
			 * all other lwps are held in the kernel.
			 */
			if (p->p_mapcnt == 0) {
				if (holdwatch() == 0) {
					/*
					 * We stopped in holdwatch().
					 * Start all over again because the
					 * watched page list may have changed.
					 */
					goto startover;
				}
				ASSERT(p->p_mapcnt == 0);
			}
			/* pr_do_mappage() is single-threaded now. */
			p->p_mapcnt++;
		}

		addr = pwp->wp_vaddr;
		rv++;

		prot = pwp->wp_prot;
		if (mapin) {
			if (kernel)
				pwp->wp_kmap[xrw]++;
			else
				pwp->wp_umap[xrw]++;
			pwp->wp_flags |= WP_NOWATCH;
			if (pwp->wp_kmap[X] + pwp->wp_umap[X])
				/* cannot have exec-only protection */
				prot |= PROT_READ|PROT_EXEC;
			if (pwp->wp_kmap[R] + pwp->wp_umap[R])
				prot |= PROT_READ;
			if (pwp->wp_kmap[W] + pwp->wp_umap[W])
				/* cannot have write-only protection */
				prot |= PROT_READ|PROT_WRITE;
#if 0	/* damned broken mmu feature! */
			if (sum(pwp->wp_umap) == 0)
				prot &= ~PROT_USER;
#endif
		} else {
			ASSERT(pwp->wp_flags & WP_NOWATCH);
			if (kernel) {
				ASSERT(pwp->wp_kmap[xrw] != 0);
				--pwp->wp_kmap[xrw];
			} else {
				ASSERT(pwp->wp_umap[xrw] != 0);
				--pwp->wp_umap[xrw];
			}
			if (sum(pwp->wp_kmap) + sum(pwp->wp_umap) == 0)
				pwp->wp_flags &= ~WP_NOWATCH;
			else {
				if (pwp->wp_kmap[X] + pwp->wp_umap[X])
					/* cannot have exec-only protection */
					prot |= PROT_READ|PROT_EXEC;
				if (pwp->wp_kmap[R] + pwp->wp_umap[R])
					prot |= PROT_READ;
				if (pwp->wp_kmap[W] + pwp->wp_umap[W])
					/* cannot have write-only protection */
					prot |= PROT_READ|PROT_WRITE;
#if 0	/* damned broken mmu feature! */
				if (sum(pwp->wp_umap) == 0)
					prot &= ~PROT_USER;
#endif
			}
		}

		if (pwp->wp_oprot != 0) {	/* if page exists */
			struct seg *seg;
			u_int oprot;

			AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
			seg = as_segat(as, addr);
			ASSERT(seg != NULL);
			SEGOP_GETPROT(seg, addr, 0, &oprot);
			if (prot != oprot)
				(void) SEGOP_SETPROT(seg, addr, PAGESIZE, prot);
			AS_LOCK_EXIT(as, &as->a_lock);
		}

		/*
		 * When all pages are mapped back to their normal state,
		 * continue the other lwps.
		 */
		if (!mapin) {
			ASSERT(p->p_mapcnt > 0);
			if (--p->p_mapcnt == 0) {
				mutex_enter(&p->p_lock);
				continuelwps(p);
				mutex_exit(&p->p_lock);
				/* pr_do_mappage() is multi-threaded now. */
			}
		}
	} while ((pwp = pwp->wp_forw) != as->a_wpage);

	return (rv);
}

/*
 * Restore the original page protections on an address range.
 * If 'kernel' is non-zero, just do it for the kernel.
 * pr_mappage() returns non-zero if it actually changed anything.
 *
 * pr_mappage() and pr_unmappage() must be executed in matched pairs,
 * but pairs may be nested within other pairs.  The reference counts
 * sort it all out.  See pr_do_mappage(), above.
 */
int
pr_mappage(const caddr_t addr, size_t size, enum seg_rw rw, int kernel)
{
	return (pr_do_mappage(addr, size, 1, rw, kernel));
}

/*
 * Set the modified page protections on a watched page.
 * Inverse of pr_mappage().
 * Needs to be called only if pr_mappage() returned non-zero.
 */
void
pr_unmappage(const caddr_t addr, size_t size, enum seg_rw rw, int kernel)
{
	(void) pr_do_mappage(addr, size, 0, rw, kernel);
}

/*
 * Function called by an lwp after it resumes from stop().
 */
void
setallwatch(void)
{
	struct as *as = curproc->p_as;
	struct watched_page *pwp;
	struct seg *seg;
	caddr_t vaddr;
	u_int prot;
	u_int npage;

	ASSERT(MUTEX_NOT_HELD(&curproc->p_lock));

	/*
	 * If there's no watchpoints, return without grabbing the as lock.
	 * This is the common case so we want to make it fast.
	 */
	if (as->a_wpage == NULL)
		return;

	AS_LOCK_ENTER(as, &as->a_lock, RW_WRITER);
	/* need to recheck in case watchpoints were just cleared */
	if ((pwp = as->a_wpage) == NULL) {
		AS_LOCK_EXIT(as, &as->a_lock);
		return;
	}

	for (npage = as->a_nwpage; npage; npage--) {
		vaddr = pwp->wp_vaddr;
		if ((pwp->wp_flags & WP_SETPROT) &&
		    (seg = as_segat(as, vaddr)) != NULL) {
			ASSERT(!(pwp->wp_flags & WP_NOWATCH));
			prot = pwp->wp_prot;
			pwp->wp_flags &= ~WP_SETPROT;
			(void) SEGOP_SETPROT(seg, vaddr, PAGESIZE, prot);
		}

		if (pwp->wp_read + pwp->wp_write + pwp->wp_exec != 0)
			pwp = pwp->wp_forw;
		else {
			/*
			 * No watched areas remain in this page.
			 * Free the watched_page structure.
			 */
			struct watched_page *next = pwp->wp_forw;
			as->a_nwpage--;
			if (as->a_wpage == pwp)
				as->a_wpage = next;
			if (as->a_wpage == pwp) {
				as->a_wpage = next = NULL;
				ASSERT(as->a_nwpage == 0);
			} else {
				remque(pwp);
				ASSERT(as->a_nwpage > 0);
			}
			kmem_free(pwp, sizeof (struct watched_page));
			pwp = next;
		}
	}

	AS_LOCK_EXIT(as, &as->a_lock);
}

/*
 * trap() calls here to determine if a fault is in a watched page.
 * We return nonzero if this is true and the load/store would fail.
 */
int
pr_is_watchpage(caddr_t addr, enum seg_rw rw)
{
	register struct as *as = curproc->p_as;
	register struct watched_page *pwp;
	u_int prot;
	int rv = 0;

	switch (rw) {
	case S_READ:
	case S_WRITE:
	case S_EXEC:
		break;
	default:
		return (0);
	}

	/*
	 * as->a_wpage and the linked list of pwp's can only
	 * be modified while the process is totally stopped.
	 * We need, and should use, no locks here.
	 */

	if (as != &kas && (pwp = as->a_wpage) != NULL) {
		do {
			if (addr < pwp->wp_vaddr)
				break;
			if (addr < pwp->wp_vaddr + PAGESIZE) {
				/*
				 * If page doesn't exist yet, forget it.
				 */
				if (pwp->wp_oprot == 0)
					break;
				prot = pwp->wp_prot;
				switch (rw) {
				case S_READ:
					rv = ((prot & (PROT_USER|PROT_READ))
						!= (PROT_USER|PROT_READ));
					break;
				case S_WRITE:
					rv = ((prot & (PROT_USER|PROT_WRITE))
						!= (PROT_USER|PROT_WRITE));
					break;
				case S_EXEC:
					rv = ((prot & (PROT_USER|PROT_EXEC))
						!= (PROT_USER|PROT_EXEC));
					break;
				}
				break;
			}
		} while ((pwp = pwp->wp_forw) != as->a_wpage);
	}

	return (rv);
}

/*
 * trap() calls here to determine if a fault is a watchpoint.
 */
int
pr_is_watchpoint(caddr_t *paddr, int *pta, size_t size, size_t *plen,
	enum seg_rw rw)
{
	proc_t *p = curproc;
	caddr_t addr = *paddr;
	caddr_t eaddr = addr + size;
	register struct watched_area *pwa;
	int rv = 0;
	int ta = 0;
	size_t len = 0;

	switch (rw) {
	case S_READ:
	case S_WRITE:
	case S_EXEC:
		break;
	default:
		*pta = 0;
		return (0);
	}

	/*
	 * p->p_warea and its linked list is protected by p->p_lock.
	 */
	mutex_enter(&p->p_lock);

	if ((pwa = p->p_warea) != NULL) {
		do {
			if (eaddr <= pwa->wa_vaddr)
				break;
			if (addr < pwa->wa_eaddr) {
				switch (rw) {
				case S_READ:
					if (pwa->wa_flags & WA_READ)
						rv = TRAP_RWATCH;
					break;
				case S_WRITE:
					if (pwa->wa_flags & WA_WRITE)
						rv = TRAP_WWATCH;
					break;
				case S_EXEC:
					if (pwa->wa_flags & WA_EXEC)
						rv = TRAP_XWATCH;
					break;
				}
				if (addr < pwa->wa_vaddr)
					addr = pwa->wa_vaddr;
				len = pwa->wa_eaddr - addr;
				if (pwa->wa_flags & WA_TRAPAFTER)
					ta = 1;
				break;
			}
		} while ((pwa = pwa->wa_forw) != p->p_warea);
	}

	mutex_exit(&p->p_lock);

	*paddr = addr;
	*pta = ta;
	if (plen != NULL)
		*plen = len;
	return (rv);
}


/*
 * Set up to perform a single-step at user level for the
 * case of a trapafter watchpoint.  Called from trap().
 */
void
do_watch_step(caddr_t vaddr, size_t sz, enum seg_rw rw,
	int watchcode, greg_t pc)
{
	register klwp_t *lwp = ttolwp(curthread);
	struct lwp_watch *pw = &lwp->lwp_watch[rw_to_index(rw)];

	/*
	 * Check to see if we are already performing this special
	 * watchpoint single-step.  We must not do pr_mappage() twice.
	 */

	/* special check for two read traps on the same instruction */
	if (rw == S_READ && pw->wpaddr != NULL &&
	    !(pw->wpaddr <= vaddr && vaddr < pw->wpaddr + pw->wpsize)) {
		ASSERT(lwp->lwp_watchtrap != 0);
		pw++;	/* use the extra S_READ struct */
	}

	if (pw->wpaddr != NULL) {
		ASSERT(lwp->lwp_watchtrap != 0);
		ASSERT(pw->wpaddr <= vaddr && vaddr < pw->wpaddr + pw->wpsize);
		if (pw->wpcode == 0) {
			pw->wpcode = watchcode;
			pw->wppc = pc;
		}
	} else {
		int mapped = pr_mappage(vaddr, sz, rw, 0);
		ASSERT(mapped != 0);
		prstep(lwp, 1);
		lwp->lwp_watchtrap = 1;
		pw->wpaddr = vaddr;
		pw->wpsize = sz;
		pw->wpcode = watchcode;
		pw->wppc = pc;
	}
}

/*
 * Undo the effects of do_watch_step().
 * Called from trap() after the single-step is finished.
 * Also called from issig_forreal() and stop() with a NULL
 * argument to avoid having these things set more than once.
 */
int
undo_watch_step(k_siginfo_t *sip)
{
	register klwp_t *lwp = ttolwp(curthread);
	int fault = 0;

	if (lwp->lwp_watchtrap) {
		struct lwp_watch *pw = lwp->lwp_watch;
		int i;

		for (i = 0; i < 4; i++, pw++) {
			if (pw->wpaddr == NULL)
				continue;
			pr_unmappage(pw->wpaddr, pw->wpsize, S_rw[i], 0);
			if (pw->wpcode != 0) {
				if (sip != NULL) {
					sip->si_signo = SIGTRAP;
					sip->si_code = pw->wpcode;
					sip->si_addr = pw->wpaddr;
					sip->si_trapafter = 1;
					sip->si_pc = (caddr_t)pw->wppc;
				}
				fault = FLTWATCH;
				pw->wpcode = 0;
			}
			pw->wpaddr = NULL;
			pw->wpsize = 0;
		}
		lwp->lwp_watchtrap = 0;
	}

	return (fault);
}

/*
 * Handle a watchpoint that occurs while doing copyin()
 * or copyout() in a system call.
 * Return non-zero if the fault or signal is cleared
 * by a debugger while the lwp is stopped.
 */
static int
sys_watchpoint(caddr_t addr, int watchcode, int ta)
{
	extern greg_t getuserpc(void);	/* XXX header file */
	k_sigset_t smask;
	register proc_t *p = ttoproc(curthread);
	register klwp_t *lwp = ttolwp(curthread);
	register sigqueue_t *sqp;
	int rval;

	/* assert no locks are held */
	/* ASSERT(curthread->t_nlocks == 0); */

	sqp = kmem_zalloc(sizeof (sigqueue_t), KM_SLEEP);
	sqp->sq_info.si_signo = SIGTRAP;
	sqp->sq_info.si_code = watchcode;
	sqp->sq_info.si_addr = addr;
	sqp->sq_info.si_trapafter = ta;
	sqp->sq_info.si_pc = (caddr_t)getuserpc();

	mutex_enter(&p->p_lock);

	/* this will be tested and cleared by the caller */
	lwp->lwp_sysabort = 0;

	if (prismember(&p->p_fltmask, FLTWATCH)) {
		lwp->lwp_curflt = (u_char)FLTWATCH;
		lwp->lwp_siginfo = sqp->sq_info;
		stop(PR_FAULTED, FLTWATCH);
		if (lwp->lwp_curflt == 0) {
			mutex_exit(&p->p_lock);
			kmem_free(sqp, sizeof (sigqueue_t));
			return (1);
		}
		lwp->lwp_curflt = 0;
	}

	/*
	 * post the SIGTRAP signal.
	 * Block all other signals so we only stop showing SIGTRAP.
	 */
	if (sigismember(&curthread->t_hold, SIGTRAP) ||
	    sigismember(&p->p_ignore, SIGTRAP)) {
		/* SIGTRAP is blocked or ignored, forget the rest. */
		mutex_exit(&p->p_lock);
		kmem_free(sqp, sizeof (sigqueue_t));
		return (0);
	}
	sigdelq(p, curthread, SIGTRAP);
	sigaddqa(p, curthread, sqp);
	smask = curthread->t_hold;
	sigfillset(&curthread->t_hold);
	sigdiffset(&curthread->t_hold, &cantmask);
	sigdelset(&curthread->t_hold, SIGTRAP);
	mutex_exit(&p->p_lock);

	rval = ((ISSIG_FAST(curthread, lwp, p, FORREAL))? 0 : 1);

	/* restore the original signal mask */
	mutex_enter(&p->p_lock);
	curthread->t_hold = smask;
	mutex_exit(&p->p_lock);

	return (rval);
}

/*
 * Wrappers for the copyin()/copyout() functions to deal
 * with watchpoints that fire while in system calls.
 */

static int
watch_copyin(const void *uaddr, void *kaddr, size_t count)
{
	return (watch_xcopyin(uaddr, kaddr, count)? -1 : 0);
}

static int
watch_xcopyin(const void *uaddr, void *kaddr, size_t count)
{
	klwp_t *lwp = ttolwp(curthread);
	caddr_t watch_uaddr = (caddr_t)uaddr;
	caddr_t watch_kaddr = (caddr_t)kaddr;
	int error = 0;

	while (count && error == 0) {
		int watchcode;
		caddr_t vaddr;
		size_t part;
		size_t len;
		int ta;
		int mapped;

		if ((part = PAGESIZE -
		    (((uintptr_t)uaddr) & PAGEOFFSET)) > count)
			part = count;

		if (!pr_is_watchpage(watch_uaddr, S_READ))
			watchcode = 0;
		else {
			vaddr = watch_uaddr;
			watchcode = pr_is_watchpoint(&vaddr, &ta,
			    part, &len, S_READ);
			if (watchcode && ta == 0)
				part = vaddr - watch_uaddr;
		}

		/*
		 * Copy the initial part, up to a watched address, if any.
		 */
		if (part != 0) {
			mapped = pr_mappage(watch_uaddr, part, S_READ, 1);
			error = default_xcopyin(watch_uaddr, watch_kaddr, part);
			if (mapped)
				pr_unmappage(watch_uaddr, part, S_READ, 1);
			watch_uaddr += part;
			watch_kaddr += part;
			count -= part;
		}
		/*
		 * If trapafter was specified, then copy through the
		 * watched area before taking the watchpoint trap.
		 */
		while (count && watchcode && ta && len > part && error == 0) {
			len -= part;
			if ((part = PAGESIZE) > count)
				part = count;
			if (part > len)
				part = len;
			mapped = pr_mappage(watch_uaddr, part, S_READ, 1);
			error = default_xcopyin(watch_uaddr, watch_kaddr, part);
			if (mapped)
				pr_unmappage(watch_uaddr, part, S_READ, 1);
			watch_uaddr += part;
			watch_kaddr += part;
			count -= part;
		}

		/* if we hit a watched address, do the watchpoint logic */
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (EFAULT);
		}
	}
	return (error);
}

static int
watch_copyout(const void *kaddr, void *uaddr, size_t count)
{
	return (watch_xcopyout(kaddr, uaddr, count)? -1 : 0);
}

static int
watch_xcopyout(const void *kaddr, void *uaddr, size_t count)
{
	klwp_t *lwp = ttolwp(curthread);
	caddr_t watch_uaddr = (caddr_t)uaddr;
	caddr_t watch_kaddr = (caddr_t)kaddr;
	int error = 0;

	while (count && error == 0) {
		int watchcode;
		caddr_t vaddr;
		size_t part;
		size_t len;
		int ta;
		int mapped;

		if ((part = PAGESIZE -
		    (((uintptr_t)uaddr) & PAGEOFFSET)) > count)
			part = count;

		if (!pr_is_watchpage(watch_uaddr, S_WRITE))
			watchcode = 0;
		else {
			vaddr = watch_uaddr;
			watchcode = pr_is_watchpoint(&vaddr, &ta,
			    part, &len, S_WRITE);
			if (watchcode) {
				if (ta == 0)
					part = vaddr - watch_uaddr;
				else {
					len += vaddr - watch_uaddr;
					if (part > len)
						part = len;
				}
			}
		}

		/*
		 * Copy the initial part, up to a watched address, if any.
		 */
		if (part != 0) {
			mapped = pr_mappage(watch_uaddr, part, S_WRITE, 1);
			error = default_xcopyout(watch_kaddr, watch_uaddr,
			    part);
			if (mapped)
				pr_unmappage(watch_uaddr, part, S_WRITE, 1);
			watch_uaddr += part;
			watch_kaddr += part;
			count -= part;
		}

		/*
		 * If trapafter was specified, then copy through the
		 * watched area before taking the watchpoint trap.
		 */
		while (count && watchcode && ta && len > part && error == 0) {
			len -= part;
			if ((part = PAGESIZE) > count)
				part = count;
			if (part > len)
				part = len;
			mapped = pr_mappage(watch_uaddr, part, S_WRITE, 1);
			error = default_xcopyout(watch_kaddr, watch_uaddr,
			    part);
			if (mapped)
				pr_unmappage(watch_uaddr, part, S_WRITE, 1);
			watch_uaddr += part;
			watch_kaddr += part;
			count -= part;
		}

		/* if we hit a watched address, do the watchpoint logic */
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (EFAULT);
		}
	}
	return (error);
}

static int
watch_copyinstr(
	const char *uaddr,
	char *kaddr,
	size_t maxlength,
	size_t *lencopied)
{
	klwp_t *lwp = ttolwp(curthread);
	size_t resid;
	int error = 0;

	if ((resid = maxlength) == 0)
		return (ENAMETOOLONG);

	while (resid && error == 0) {
		int watchcode;
		caddr_t vaddr;
		size_t part;
		size_t len;
		size_t size;
		int ta;
		int mapped;

		if ((part = PAGESIZE -
		    (((uintptr_t)uaddr) & PAGEOFFSET)) > resid)
			part = resid;

		if (!pr_is_watchpage((caddr_t)uaddr, S_READ))
			watchcode = 0;
		else {
			vaddr = (caddr_t)uaddr;
			watchcode = pr_is_watchpoint(&vaddr, &ta,
			    part, &len, S_READ);
			if (watchcode) {
				if (ta == 0)
					part = vaddr - uaddr;
				else {
					len += vaddr - uaddr;
					if (part > len)
						part = len;
				}
			}
		}

		/*
		 * Copy the initial part, up to a watched address, if any.
		 */
		if (part != 0) {
			mapped = pr_mappage((caddr_t)uaddr, part, S_READ, 1);
			error = default_copyinstr(uaddr, kaddr, part, &size);
			if (mapped)
				pr_unmappage((caddr_t)uaddr, part, S_READ, 1);
			if (error == EFAULT)
			    break;
			uaddr += size;
			kaddr += size;
			resid -= size;
			if (error == ENAMETOOLONG && resid > 0)
			    error = 0;
			if (watchcode &&
			    (uaddr < vaddr || kaddr[-1] == '\0'))
				break;	/* didn't reach the watched area */
		}

		/*
		 * If trapafter was specified, then copy through the
		 * watched area before taking the watchpoint trap.
		 */
		while (resid && watchcode && ta && len > part && error == 0 &&
		    size == part && kaddr[-1] != '\0') {
			len -= part;
			if ((part = PAGESIZE) > resid)
				part = resid;
			if (part > len)
				part = len;
			mapped = pr_mappage((caddr_t)uaddr, part, S_READ, 1);
			error = default_copyinstr(uaddr, kaddr, part, &size);
			if (mapped)
				pr_unmappage((caddr_t)uaddr, part, S_READ, 1);
			if (error == EFAULT)
			    break;
			uaddr += size;
			kaddr += size;
			resid -= size;
			if (error == ENAMETOOLONG && resid > 0)
			    error = 0;
		}

		/* if we hit a watched address, do the watchpoint logic */
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (EFAULT);
		}

		if (part != 0 && (size < part || kaddr[-1] == '\0'))
			break;
	}

	if (lencopied)
		*lencopied = maxlength - resid;
	return (error);
}

static int
watch_copyoutstr(
	const char *kaddr,
	char *uaddr,
	size_t maxlength,
	size_t *lencopied)
{
	klwp_t *lwp = ttolwp(curthread);
	size_t resid;
	int error = 0;

	if ((resid = maxlength) == 0)
		return (ENAMETOOLONG);

	while (resid && error == 0) {
		int watchcode;
		caddr_t vaddr;
		size_t part;
		size_t len;
		size_t size;
		int ta;
		int mapped;

		if ((part = PAGESIZE -
		    (((uintptr_t)uaddr) & PAGEOFFSET)) > resid)
			part = resid;

		if (!pr_is_watchpage(uaddr, S_WRITE))
			watchcode = 0;
		else {
			vaddr = uaddr;
			watchcode = pr_is_watchpoint(&vaddr, &ta,
			    part, &len, S_WRITE);
			if (watchcode && ta == 0)
				part = vaddr - uaddr;
		}

		/*
		 * Copy the initial part, up to a watched address, if any.
		 */
		if (part != 0) {
			mapped = pr_mappage(uaddr, part, S_WRITE, 1);
			error = default_copyoutstr(kaddr, uaddr, part, &size);
			if (mapped)
				pr_unmappage(uaddr, part, S_WRITE, 1);
			if (error == EFAULT)
			    break;
			uaddr += size;
			kaddr += size;
			resid -= size;
			if (error == ENAMETOOLONG && resid > 0)
			    error = 0;
			if (watchcode &&
			    (uaddr < vaddr || kaddr[-1] == '\0'))
				break;	/* didn't reach the watched area */
		}

		/*
		 * If trapafter was specified, then copy through the
		 * watched area before taking the watchpoint trap.
		 */
		while (resid && watchcode && ta && len > part && error == 0 &&
		    size == part && kaddr[-1] != '\0') {
			len -= part;
			if ((part = PAGESIZE) > resid)
				part = resid;
			if (part > len)
				part = len;
			mapped = pr_mappage(uaddr, part, S_WRITE, 1);
			error = default_copyoutstr(kaddr, uaddr, part, &size);
			if (mapped)
				pr_unmappage(uaddr, part, S_WRITE, 1);
			if (error == EFAULT)
			    break;
			uaddr += size;
			kaddr += size;
			resid -= size;
			if (error == ENAMETOOLONG && resid > 0)
			    error = 0;
		}

		/* if we hit a watched address, do the watchpoint logic */
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (EFAULT);
		}

		if (part != 0 && (size < part || kaddr[-1] == '\0'))
			break;
	}

	if (lencopied)
		*lencopied = maxlength - resid;
	return (error);
}

/*
 * Utility function for watch_*() functions below.
 * Return true if the object at [addr, addr+size) falls in a watched page.
 */
static int
is_watched(const void *addr, size_t size, enum seg_rw rw)
{
	caddr_t saddr = (caddr_t)addr;
	caddr_t eaddr = saddr + size - 1;

	return (pr_is_watchpage(saddr, rw) ||
	    (((uintptr_t)saddr & PAGEMASK) != ((uintptr_t)eaddr & PAGEMASK) &&
	    pr_is_watchpage(eaddr, rw)));
}

static int
watch_fuword8(const void *addr, uint8_t *dst)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint8_t), S_READ))
			return (default_fuword8(addr, dst));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (uint8_t), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (uint8_t),
			    S_READ, 1);
			rv = default_fuword8(addr, dst);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (uint8_t),
				    S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

static int
watch_fuiword8(const void *addr, uint8_t *dst)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint8_t), S_READ))
			return (default_fuiword8(addr, dst));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (uint8_t), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (uint8_t),
			    S_READ, 1);
			rv = default_fuiword8(addr, dst);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (uint8_t),
				    S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

static int
watch_fuword16(const void *addr, uint16_t *dst)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint16_t), S_READ))
			return (default_fuword16(addr, dst));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (uint16_t), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (uint16_t),
			    S_READ, 1);
			rv = default_fuword16(addr, dst);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (uint16_t),
				    S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

static int
watch_fuword32(const void *addr, uint32_t *dst)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint32_t), S_READ))
			return (default_fuword32(addr, dst));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (uint32_t), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (uint32_t),
			    S_READ, 1);
			rv = default_fuword32(addr, dst);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (uint32_t),
				    S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

static int
watch_fuiword32(const void *addr, uint32_t *dst)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint32_t), S_READ))
			return (default_fuiword32(addr, dst));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (uint32_t), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (uint32_t),
			    S_READ, 1);
			rv = default_fuiword32(addr, dst);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (uint32_t),
				    S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

static int
watch_suword8(void *addr, uint8_t value)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint8_t), S_WRITE))
			return (default_suword8(addr, value));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (uint8_t), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (uint8_t), S_WRITE, 1);
			rv = default_suword8(addr, value);
			if (mapped)
				pr_unmappage(addr, sizeof (uint8_t),
				    S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

static int
watch_suiword8(void *addr, uint8_t value)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint8_t), S_WRITE))
			return (default_suiword8(addr, value));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (uint8_t), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (uint8_t), S_WRITE, 1);
			rv = default_suiword8(addr, value);
			if (mapped)
				pr_unmappage(addr, sizeof (uint8_t),
				    S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

static int
watch_suword16(void *addr, uint16_t value)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint16_t), S_WRITE))
			return (default_suword16(addr, value));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
		    sizeof (uint16_t), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (uint16_t),
			    S_WRITE, 1);
			rv = default_suword16(addr, value);
			if (mapped)
				pr_unmappage(addr, sizeof (uint16_t),
				    S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

static int
watch_suword32(void *addr, uint32_t value)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint32_t), S_WRITE))
			return (default_suword32(addr, value));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
			sizeof (uint32_t), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (uint32_t),
				S_WRITE, 1);
			rv = default_suword32(addr, value);
			if (mapped)
				pr_unmappage(addr, sizeof (uint32_t),
					S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

static int
watch_suiword32(void *addr, uint32_t value)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint32_t), S_WRITE))
			return (default_suiword32(addr, value));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
		    sizeof (uint32_t), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (uint32_t),
			    S_WRITE, 1);
			rv = default_suiword32(addr, value);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (uint32_t),
				    S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

static int
watch_fuword64(const void *addr, uint64_t *dst)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint64_t), S_READ))
			return (default_fuword64(addr, dst));

		vaddr = (caddr_t)addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
		    sizeof (uint64_t), NULL, S_READ);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage((caddr_t)addr, sizeof (uint64_t),
			    S_READ, 1);
			rv = default_fuword64(addr, dst);
			if (mapped)
				pr_unmappage((caddr_t)addr, sizeof (uint64_t),
				    S_READ, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}

static int
watch_suword64(void *addr, uint64_t value)
{
	klwp_t *lwp = ttolwp(curthread);
	int watchcode;
	caddr_t vaddr;
	int mapped;
	int rv;
	int ta;

	for (;;) {
		if (!is_watched(addr, sizeof (uint64_t), S_WRITE))
			return (default_suword64(addr, value));

		vaddr = addr;
		watchcode = pr_is_watchpoint(&vaddr, &ta,
		    sizeof (uint64_t), NULL, S_WRITE);
		if (watchcode == 0 || ta != 0) {
			mapped = pr_mappage(addr, sizeof (uint64_t),
			    S_WRITE, 1);
			rv = default_suword64(addr, value);
			if (mapped)
				pr_unmappage(addr, sizeof (uint64_t),
				    S_WRITE, 1);
		}
		if (watchcode &&
		    (!sys_watchpoint(vaddr, watchcode, ta) ||
		    lwp->lwp_sysabort)) {
			lwp->lwp_sysabort = 0;
			return (-1);
		}
		if (watchcode == 0 || ta != 0)
			return (rv);
	}
}
