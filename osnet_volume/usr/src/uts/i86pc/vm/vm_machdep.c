/*
 * Copyright (c) 1987-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vm_machdep.c	1.70	99/11/08 SMI"

/*
 * UNIX machine dependent virtual memory support.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/vmem.h>
#include <sys/buf.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>
#include <sys/vm.h>
#include <sys/mman.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/debug.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kp.h>
#include <vm/seg_vn.h>
#include <vm/page.h>
#include <vm/seg_kmem.h>
#include <vm/mach_page.h>

#include <sys/cpu.h>
#include <sys/vm_machparam.h>
#include <sys/memlist.h>
#include <sys/bootconf.h> /* XXX the memlist stuff belongs in memlist_plat.h */

#include <vm/hat_i86.h>
#include <sys/x86_archext.h>
#include <sys/elf_386.h>
#include <sys/cmn_err.h>
#include <sys/machsystm.h>

#include <sys/vtrace.h>
#include <sys/ddidmareq.h>
#include <sys/promif.h>

int	largepagesupport = 0;
#ifdef	PTE36

#define	MMU_HIGHEST_PFN	0xffffff	/* 64 GB */
#define	MMU_NAME	"mmu36"
uint64_t	boot_pte0, boot_pte1;
uint64_t *percpu_pttbl;
uint_t	largepagesize = TWOMB_PAGESIZE;
uint_t	largepageshift = TWOMB_PAGESHIFT;

/* set restricted kmemalloc to 0 for testing drivers for > 4GB safety */
int	restricted_kmemalloc = 1;
page_t	*kvseg_pplist = NULL;
kmutex_t kvseg_pplist_lock;

#define	mach_pp	((machpage_t *)pp)
static page_t *page_get_kvseg(void);

#else

#define	MMU_HIGHEST_PFN	0xfffff
#define	MMU_NAME	"mmu32"

uint_t	largepagesize = FOURMB_PAGESIZE;
uint_t	largepageshift = FOURMB_PAGESHIFT;

#endif

extern pteval_t	 *pt_pdir;
extern uint_t page_create_new;
extern uint_t page_create_exists;
extern uint_t page_create_putbacks;
extern uint_t page_create_putbacks;
extern	uintptr_t eprom_kernelbase;
#if	!defined(lint)
extern uintptr_t _kernelbase, _userlimit, _userlimit32;
extern unsigned long _dsize_limit;
extern rlim64_t rlim_infinity_map[];
#endif

uint_t
page_num_pagesizes()
{
	return ((largepagesupport) ? 2 : 1);
}

size_t
page_get_pagesize(uint_t n)
{
	if (n > 1)
		cmn_err(CE_PANIC, "page_get_pagesize: out of range %d", n);
	return ((n == 0) ? PAGESIZE : largepagesize);
}

extern int hat_addrchk(kthread_t *, struct hat *, caddr_t, enum fault_type);
/*
 * Handle a pagefault.
 */
faultcode_t
pagefault(addr, type, rw, iskernel)
	register caddr_t addr;
	register enum fault_type type;
	register enum seg_rw rw;
	register int iskernel;
{
	struct as *as;
	struct hat *hat;
	struct proc *p;
	kthread_t *t;
	faultcode_t res;
	caddr_t base;
	size_t len;
	int err;
	int mapped_red;
#ifdef DEBUG
	int	lcr3;
#endif

	mapped_red = segkp_map_red();

	if (iskernel) {
		as = &kas;
		hat = as->a_hat;
	} else {
		t = curthread;
		p = ttoproc(t);
		as = p->p_as;
		hat = as->a_hat;
		/* check if fault is a result of hat_steal or hat_critical */
		if (hat_addrchk(t, hat, addr, type))
			return (0);		/* fault resolved */
	}



	/*
	 * Dispatch pagefault.
	 */
	res = as_fault(hat, as, addr, 1, type, rw);

	/*
	 * If this isn't a potential unmapped hole in the user's
	 * UNIX data or stack segments, just return status info.
	 */
	if (!(res == FC_NOMAP && iskernel == 0))
		goto out;

	/*
	 * Check to see if we happened to faulted on a currently unmapped
	 * part of the UNIX data or stack segments.  If so, create a zfod
	 * mapping there and then try calling the fault routine again.
	 */
	base = p->p_brkbase;
	len = p->p_brksize;

	if (addr < base || addr >= base + len) {		/* data seg? */
		base = (caddr_t)((caddr_t)USRSTACK - p->p_stksize);
		len = p->p_stksize;
		if (addr < base || addr >= (caddr_t)USRSTACK) {	/* stack seg? */
			/* not in either UNIX data or stack segments */
			res = FC_NOMAP;
			goto out;
		}
	}

	/* the rest of this function implements a 3.X 4.X 5.X compatibility */
	/* This code is probably not needed anymore */

	/* expand the gap to the page boundaries on each side */
	len = (((uint_t)base + len + PAGEOFFSET) & PAGEMASK) -
	    ((uint_t)base & PAGEMASK);
	base = (caddr_t)((uint_t)base & PAGEMASK);

	as_rangelock(as);
	if (as_gap(as, PAGESIZE, &base, &len, AH_CONTAIN, addr) != 0) {
		/*
		 * Since we already got an FC_NOMAP return code from
		 * as_fault, there must be a hole at `addr'.  Therefore,
		 * as_gap should never fail here.
		 */
		panic("pagefault as_gap");
	}

	err = as_map(as, base, len, segvn_create, zfod_argsp);
	as_rangeunlock(as);
	if (err) {
		res = FC_MAKE_ERR(err);
		goto out;
	}

	res = as_fault(hat, as, addr, 1, F_INVAL, rw);

out:
	if (mapped_red)
		segkp_unmap_red();
#if	DEBUG
	kpreempt_disable();
	lcr3 = cr3();
	ASSERT(lcr3 == kernel_only_cr3 || lcr3 == CPU->cpu_cr3 ||
		lcr3 == curproc->p_as->a_hat->hat_pdepfn);
	if (curthread->t_mmuctx) {
		ASSERT(lcr3 == ((cr3ctx_t *)curthread->t_mmuctx)->ct_cr3);
		ASSERT(CPU->cpu_curcr3 == curthread->t_mmuctx);
	}
	kpreempt_enable();
#endif	/* DEBUG */

	return (res);
}

/*
 * virtual address size for mapping the page tables and pte
 * For kernelbase == 0xc0000000, PTSIZE = 2MB (512 8 byte pte in page)
 *			addr		bytes		entries
 * - kernelbase		0xc0000000
 *	HWPPMAPSIZE 			6K 		1.5k
 *	page pad			2k
 *
 * - userhwppmap	0xbfffe000
 *	PTEMAPSIZE 			6MB		.75MB
 *
 * - userptemap		0xbfa00000
 * 	PTEMAPSIZE/PAGESIZE 		12k		1.5k
 *
 * - userptetable	0xbf9fb000
 *	NPDPERAS * PAGESIZE		16k		2048
 *
 * - userpagedir	0xbf9f7000
 *		(PTSIZE pad)
 *
 * - usermapend		0xbf800000
 *
 * For kernelbase == 0xe0000000, PTSIZE = 4MB_(1024 4 byte pte in page)
 *			addr		bytes		entries
 * - kernelbase		0xe0000000
 *	HWPPMAPSIZE 			3.5K 0xe00	896 0x380
 *	page pad			 .5k
 *
 * - userhwppmap	0xdffff000
 *	PTEMAPSIZE 			3.5MB 0x380000	.875MB 0xe0000
 *
 * - userptemap		0xdfc80000
 * 	PTEMAPSIZE/PAGESIZE 		3.5k 0xe00	896 0x380
 *	page pad			 .5k
 *
 * - userptetable	0xdfc7f000
 *	NPDPERAS * PAGESIZE		4k		1024
 *
 * - userpagedir	0xdfc7e000
 *		(PTSIZE pad)
 *
 * - usermapend		0xdfc00000
 *
 * Could save some space para the - 12 and 8 bytes - para HWPPMAPSIZE by
 * subtracting PTEMAPSIZE from kernelbase in the macro.
 */

#define	PTEMAPSZ	\
	(roundup(((uint_t)kernelbase/PAGESIZE)*(sizeof (pte_t)), PAGESIZE))

#define	HWPPMAPSZ	\
	(roundup(((uint_t)kernelbase/PTSIZE)*(sizeof (intptr_t)), PAGESIZE))

#define	PTETABLESZ	\
	((PTEMAPSZ/PAGESIZE)*(sizeof (pte_t)))


pte_t		*userptemap;		/* ptes for user address space */
hwptepage_t	**userhwppmap;		/* hwpp for pages for userptemap */

pte_t		*userptetable;		/* ptes 4 the pages used 4 userptemap */

/* ptes 4 pages used 4 userhwppmap, useptetable and itself */
pte_t		*userptetablemisc;

pte_t		*userpagedir;

uintptr_t	usermapend;		/* mod largepagesize */

/*ARGSUSED4*/
void
map_addr(caddr_t *addrp, size_t len, offset_t off, int align, uint_t flags)
{
	map_addr_proc(addrp, len, off, align, (caddr_t)usermapend, curproc);
}

/*
 * map_addr_proc() is the routine called when the system is to
 * choose an address for the user.  We will pick an address
 * range which is the highest available below kernelbase.
 *
 * addrp is a value/result parameter.
 *	On input it is a hint from the user to be used in a completely
 *	machine dependent fashion.  We decide to completely ignore this hint.
 *
 *	On output it is NULL if no address can be found in the current
 *	processes address space or else an address that is currently
 *	not mapped for len bytes with a page of red zone on either side.
 *	If align is true, then the selected address will obey the alignment
 *	constraints of a vac machine based on the given off value.
 */
/*ARGSUSED*/
void
map_addr_proc(caddr_t *addrp, size_t len, offset_t off, int align,
    caddr_t userlimit, struct proc *p)
{
	struct as *as = p->p_as;
	caddr_t addr;
	caddr_t base;
	size_t slen;
	size_t align_amount;

	ASSERT(userlimit == as->a_userlimit);

	base = p->p_brkbase;
	slen = userlimit - base;
	len = (len + PAGEOFFSET) & PAGEMASK;

	/*
	 * Redzone for each side of the request. This is done to leave
	 * one page unmapped between segments. This is not required, but
	 * it's useful for the user because if their program strays across
	 * a segment boundary, it will catch a fault immediately making
	 * debugging a little easier.
	 */
	len += 2 * PAGESIZE;

	if (len >= largepagesize) {
		/*
		 * We need to return 2MB or 4MB aligned address.
		 */
		align_amount = largepagesize;
	} else {
		/*
		 * Align virtual addresses on a 64K boundary to ensure
		 * that ELF shared libraries are mapped with the appropriate
		 * alignment constraints by the run-time linker.
		 */
		align_amount = ELF_386_MAXPGSZ;
	}

	len += align_amount;

	/*
	 * Look for a large enough hole starting below userlimit.
	 * After finding it, use the upper part.  Addition of PAGESIZE
	 * is for the redzone as described above.
	 */
	if (as_gap(as, len, &base, &slen, AH_HI, NULL) == 0) {
		caddr_t as_addr;

		addr = base + slen - len + PAGESIZE;
		as_addr = addr;
		/*
		 * Round address DOWN to the alignment amount,
		 * add the offset, and if this address is less
		 * than the original address, add alignment amount.
		 */
		addr = (caddr_t)((uintptr_t)addr & (~(align_amount - 1)));
		addr += (uintptr_t)(off & (align_amount - 1));
		if (addr < as_addr)
			addr += align_amount;

		ASSERT(addr <= (as_addr + align_amount));
		ASSERT(((uintptr_t)addr & (align_amount - 1)) ==
		    ((uintptr_t)(off & (align_amount - 1))));
		*addrp = addr;
	} else {
		*addrp = NULL;	/* no more virtual space */
	}
}

/*
 * Determine whether [base, base+len] contains a mapable range of
 * addresses at least minlen long. base and len are adjusted if
 * required to provide a mapable range.
 */
/*ARGSUSED3*/
int
valid_va_range(register caddr_t *basep, register size_t *lenp,
	register size_t minlen, register int dir)
{
	register caddr_t hi, lo;

	lo = *basep;
	hi = lo + *lenp;

	/*
	 * If hi rolled over the top, try cutting back.
	 */
	if (hi < lo) {
		if (0 - (uint_t)lo + (uint_t)hi < minlen)
			return (0);
		if (0 - (uint_t)lo < minlen)
			return (0);
		*lenp = 0 - (uint_t)lo;
	} else if (hi - lo < minlen)
		return (0);
	return (1);
}

/*
 * Determine whether [addr, addr+len] are valid user addresses.
 */
/*ARGSUSED*/
int
valid_usr_range(caddr_t addr, size_t len, uint_t prot, struct as *as,
    caddr_t userlimit)
{
	caddr_t eaddr = addr + len;

	if (eaddr <= addr || addr >= userlimit || eaddr > userlimit)
		return (RANGE_BADADDR);
	return (RANGE_OKAY);
}

/*
 * Return 1 if the page frame is onboard memory, else 0.
 */
int
pf_is_memory(pfn_t pf)
{
	return (address_in_memlist(phys_install,
		ptob((unsigned long long)pf), 1));
}


/*
 * initialized by page_coloring_init()
 */
static uint_t	page_colors = 1;
static uint_t	page_colors_mask;

/*
 * Within a memory range the page freelist and cachelist are hashed
 * into bins based on color. This makes it easiser to search for a page
 * within a specific memory range.
 */
#define	PAGE_COLORS_MAX	256
#define	MAX_MEM_RANGES	4

static	page_t *page_freelists[MAX_MEM_RANGES][PAGE_COLORS_MAX];
static	page_t *page_cachelists[MAX_MEM_RANGES][PAGE_COLORS_MAX];

static	struct	mem_range {
	ulong_t		pfn_lo;
	ulong_t		pfn_hi;
} memranges[] = {
	{0x100000, (ulong_t)0xFFFFFFFF},	/* pfn range for 4G and above */
	{0x80000, 0xFFFFF},	/* pfn range for 2G-4G */
	{0x01000, 0x7FFFF},	/* pfn range for 16M-2G */
	{0x00000, 0x00FFF},	/* pfn range for 0-16M */
};

/* number of memory ranges */
static int nranges = sizeof (memranges)/sizeof (struct mem_range);

/*
 * There are at most 256 colors/bins.  Spread them out under a
 * couple of locks.  There are mutexes for both the page freelist
 * and the page cachelist.
 */

#define	PC_SHIFT	(4)
#define	NPC_MUTEX	(PAGE_COLORS_MAX/(1 << PC_SHIFT))

static kmutex_t	fpc_mutex[NPC_MUTEX];
static kmutex_t	cpc_mutex[NPC_MUTEX];

#if	defined(COLOR_STATS) || defined(DEBUG)

#define	COLOR_STATS_INC(x) (x)++;
#define	COLOR_STATS_DEC(x) (x)--;

static	uint_t	pf_size[PAGE_COLORS_MAX];
static	uint_t	pc_size[PAGE_COLORS_MAX];

static	uint_t	sys_nak_bins[PAGE_COLORS_MAX];
static	uint_t	sys_req_bins[PAGE_COLORS_MAX];

#else	COLOR_STATS

#define	COLOR_STATS_INC(x)
#define	COLOR_STATS_DEC(x)

#endif	COLOR_STATS

/* ### in .h */
#define	mach_pp	((machpage_t *)pp)

#define	PP_2_BIN(pp) (((machpage_t *)(pp))->p_pagenum & page_colors_mask)

#define	PC_BIN_MUTEX(bin, list)	((list == PG_FREE_LIST)? \
	&fpc_mutex[(((bin)>>PC_SHIFT)&(NPC_MUTEX-1))] :	\
	&cpc_mutex[(((bin)>>PC_SHIFT)&(NPC_MUTEX-1))])

/*
 * hash `as' and `vaddr' to get a bin.
 * sizeof (struct as) is 60.
 * shifting down by 4 bits will cause consecutive as's to be offset by ~3.
 */
#define	AS_2_BIN(as, vaddr) \
	((((uint_t)(vaddr) >> PAGESHIFT) + ((uint_t)(as) >> 3)) \
	& page_colors_mask)


#define	VADDR_2_BIN(as, vaddr) AS_2_BIN(as, vaddr)

/* returns the number of the freelist to begin the search */
int
pagelist_num(ulong_t pfn)
{
	int n = 0;

	while (n < nranges) {
		if (pfn >= memranges[n].pfn_lo)
			return (n);
		n++;
	}

	return (0);
}

/*
 * is_contigpage_free:
 *	returns a page list of contiguous pages. It minimally has to return
 *	minctg pages. Caller determines minctg based on the scatter-gather
 *	list length.
 *
 *	pfnp is set to the next page frame to search on return.
 */
static page_t *
is_contigpage_free(uint_t *pfnp, pgcnt_t *pgcnt, pgcnt_t minctg,
	uint64_t pfnseg, int iolock)
{
	int	i = 0;
	uint_t	pfn = *pfnp;
	page_t	*pp;
	page_t	*plist = NULL;

	/*
	 * fail if pfn + minctg crosses a segment boundary.
	 * Adjust for next starting pfn to begin at segment boundary.
	 */

	if (((*pfnp + minctg - 1) & pfnseg) < (*pfnp & pfnseg)) {
		*pfnp = roundup(*pfnp, pfnseg + 1);
		return (NULL);
	}

	do {
retry:
		pp = page_numtopp_nolock(pfn + i);
		if ((pp == NULL) || (mach_pp->p_flags & P_KVSEGPAGE) ||
			(page_trylock(pp, SE_EXCL) == 0)) {
			break;
		}
		if (page_pptonum(pp) != pfn + i) {
			page_unlock(pp);
			goto retry;
		}

		if (!(PP_ISFREE(pp))) {
			page_unlock(pp);
			break;
		}

		if (!PP_ISAGED(pp)) {
			page_list_sub(PG_CACHE_LIST, pp);
			page_hashout(pp, (kmutex_t *)NULL);
		} else {
			page_list_sub(PG_FREE_LIST, pp);
		}

		PP_CLRFREE(pp);
		PP_CLRAGED(pp);
		PP_SETREF(pp);

		if (iolock)
			page_io_lock(pp);
		page_list_concat(&plist, &pp);

		/*
		 * exit loop when pgcnt satisfied or segment boundary reached.
		 */

	} while ((++i < *pgcnt) && ((pfn + i) & pfnseg));

	*pfnp += i + 1;		/* set to next pfn to search */

	if (i >= minctg) {
		*pgcnt -= i;
		return (plist);
	}

	/*
	 * failure: minctg not satisfied.
	 *
	 * if next request crosses segment boundary, set next pfn
	 * to search from the segment boundary.
	 */
	if (((*pfnp + minctg - 1) & pfnseg) < (*pfnp & pfnseg))
		*pfnp = roundup(*pfnp, pfnseg + 1);

	/* clean up any pages already allocated */

	while (plist) {
		pp = plist;
		page_sub(&plist, pp);
		PP_SETFREE(pp);
		PP_CLRALL(pp);
		PP_SETAGED(pp);
		page_list_add(PG_FREE_LIST, pp, PG_LIST_TAIL);
		if (iolock)
			page_io_unlock(pp);
		page_unlock(pp);
	}

	return (NULL);
}

kmutex_t	contig_lock;

#define	CONTIG_LOCK()	mutex_enter(&contig_lock);
#define	CONTIG_UNLOCK()	mutex_exit(&contig_lock);

static page_t *
page_get_contigpage(pgcnt_t *pgcnt, ddi_dma_attr_t *mattr, int iolock)
{
	uint_t		pfn;
	int		sgllen;
	uint64_t	pfnseg;
	pgcnt_t		minctg;
	page_t		*pplist = NULL, *plist;
	uint64_t	lo, hi;
	static uint_t	maxpfn,	startpfn, lastctgcnt;

	CONTIG_LOCK();

	if (maxpfn == 0) {
		struct memseg *tseg;

		for (tseg = memsegs; tseg; tseg = tseg->next)
			if (tseg->pages_end > maxpfn)
				maxpfn = tseg->pages_end;
		startpfn = 0;
	}

	if (mattr) {
		lo = (mattr->dma_attr_addr_lo + PAGEOFFSET) >> PAGESHIFT;
		hi = mattr->dma_attr_addr_hi >> PAGESHIFT;
		if (hi >= maxpfn)
			hi = maxpfn - 1;
		sgllen = mattr->dma_attr_sgllen;
		pfnseg = mattr->dma_attr_seg >> PAGESHIFT;

		/*
		 * in order to satisfy the request, must minimally
		 * acquire minctg contiguous pages
		 */
		minctg = howmany(*pgcnt, sgllen);

		ASSERT(hi >= lo);

		/*
		 * start from where last searched if the minctg >= lastctgcnt
		 */
		if (minctg < lastctgcnt || startpfn < lo || startpfn > hi)
			startpfn = lo;
	} else {
		hi = maxpfn - 1;
		lo = 0;
		sgllen = 1;
		pfnseg = 0xffffff;	/* 64 GB */
		minctg = *pgcnt;

		if (minctg < lastctgcnt)
			startpfn = lo;
	}
	lastctgcnt = minctg;

	ASSERT(pfnseg + 1 >= (uint64_t)minctg);

	pfn = startpfn;

	while (pfn + minctg - 1 <= hi) {

		if (plist = is_contigpage_free(&pfn, pgcnt, minctg, pfnseg,
						iolock)) {

			page_list_concat(&pplist, &plist);
			sgllen--;
			/*
			 * return when contig pages no longer needed
			 */
			if (!*pgcnt || *pgcnt <= sgllen) {
				startpfn = pfn;
				CONTIG_UNLOCK();
				return (pplist);
			}
			minctg = howmany(*pgcnt, sgllen);
		}
	}

	/* cannot find contig pages in specified range */
	if (startpfn == lo) {
		CONTIG_UNLOCK();
		return (NULL);
	}

	/* did not start with lo previously */
	pfn = lo;

	/* allow search to go above startpfn */
	while (pfn < startpfn) {

		if (plist = is_contigpage_free(&pfn, pgcnt, minctg, pfnseg,
						iolock)) {

			page_list_concat(&pplist, &plist);
			sgllen--;
			/*
			 * return when contig pages no longer needed
			 */
			if (!*pgcnt || *pgcnt <= sgllen) {
				startpfn = pfn;
				CONTIG_UNLOCK();
				return (pplist);
			}
			minctg = howmany(*pgcnt, sgllen);
		}
	}
	CONTIG_UNLOCK();
	return (NULL);
}

static ddi_dma_attr_t	largepage_attr = {
	DMA_ATTR_V0,
	LARGEPAGESIZE,				/* addr_lo */
	0xfffffffffULL,				/* addr_hi - 64 GB */
	LARGEPAGESIZE-1,			/* count_max */
	PAGESIZE,				/* align */
	1,					/* burstsizes */
	1,					/* minxfer */
	LARGEPAGESIZE,				/* maxxfer */
	LARGEPAGESIZE-1,			/* seg - segment boundary */
	1,					/* sgllen */
	1,					/* granular */
	0					/* flags */
};

page_t *
page_get_largepage()
{
	pgcnt_t	pgcnt = LARGEPAGESIZE / PAGESIZE;

	return (page_get_contigpage(&pgcnt, &largepage_attr, 0));
}

/*
 * Take a particular page off of whatever freelist the page is claimed to be on.
 */
void
page_list_sub(int list, page_t *pp)
{
	uint_t		bin;
	kmutex_t	*pcm;
	page_t		**ppp;
	int		n;

	ASSERT(PAGE_EXCL(pp));
	ASSERT(PP_ISFREE(pp));

	bin = PP_2_BIN(pp);
	pcm = PC_BIN_MUTEX(bin, list);

	n = pagelist_num(((machpage_t *)pp)->p_pagenum);

	if (list == PG_FREE_LIST) {
		ppp = &page_freelists[n][bin];
		COLOR_STATS_DEC(pf_size[bin]);
		ASSERT(PP_ISAGED(pp));

		ASSERT(page_pptonum(pp) <= physmax);
	} else {
		ppp = &page_cachelists[n][bin];
		COLOR_STATS_DEC(pc_size[bin]);
		ASSERT(PP_ISAGED(pp) == 0);
	}

	mutex_enter(pcm);
	page_sub(ppp, pp);
	mutex_exit(pcm);
}

void
page_list_add(int list, page_t *pp, int where)
{
	page_t		**ppp;
	kmutex_t	*pcm;
	uint_t		bin;
#ifdef	DEBUG
	uint_t		*pc_stats;
#endif
	int		n;

	ASSERT(PAGE_EXCL(pp));
	ASSERT(PP_ISFREE(pp));
	ASSERT(!hat_page_is_mapped(pp));

#ifdef	PTE36
	/*
	 * If this page was mapped by kvseg (kmem_alloc area)
	 * return this page to kvseg_pplist pool.
	 */
	if (mach_pp->p_flags & P_KVSEGPAGE) {
		mutex_enter(&kvseg_pplist_lock);
		page_add(&kvseg_pplist, pp);
		mutex_exit(&kvseg_pplist_lock);
		return;
	}
#endif

	bin = PP_2_BIN(pp);
	pcm = PC_BIN_MUTEX(bin, list);

	n = pagelist_num(((machpage_t *)pp)->p_pagenum);

	if (list == PG_FREE_LIST) {
		ASSERT(PP_ISAGED(pp));
		ASSERT((pc_stats = &pf_size[bin]) != NULL);
		ppp = &page_freelists[n][bin];

		ASSERT(page_pptonum(pp) <= physmax);
	} else {
		ASSERT(pp->p_vnode);
		ASSERT((pp->p_offset & 0xfff) == 0);
		ASSERT((pc_stats = &pc_size[bin]) != NULL);
		ppp = &page_cachelists[n][bin];
	}

	mutex_enter(pcm);
	COLOR_STATS_INC(*pc_stats);
	page_add(ppp, pp);

	if (where == PG_LIST_TAIL) {
		*ppp = (*ppp)->p_next;
	}
	mutex_exit(pcm);

	/*
	 * It is up to the caller to unlock the page!
	 */
	ASSERT(PAGE_EXCL(pp));
}


/*
 * When a bin is empty, and we can't satisfy a color request correctly,
 * we scan.  If we assume that the programs have reasonable spatial
 * behavior, then it will not be a good idea to use the adjacent color.
 * Using the adjacent color would result in virtually adjacent addresses
 * mapping into the same spot in the cache.  So, if we stumble across
 * an empty bin, skip a bunch before looking.  After the first skip,
 * then just look one bin at a time so we don't miss our cache on
 * every look. Be sure to check every bin.  Page_create() will panic
 * if we miss a page.
 *
 * This also explains the `<=' in the for loops in both page_get_freelist()
 * and page_get_cachelist().  Since we checked the target bin, skipped
 * a bunch, then continued one a time, we wind up checking the target bin
 * twice to make sure we get all of them bins.
 */
#define	BIN_STEP	19

/*
 * Find the `best' page on the freelist for this (vp,off) (as,vaddr) pair.
 *
 * Does its own locking and accounting.
 * If PG_MATCH_COLOR is set, then NULL will be returned if there are no
 * pages of the proper color even if there are pages of a different color.
 *
 * Finds a page, removes it, THEN locks it.
 */
/*ARGSUSED*/
page_t *
page_get_freelist(
	struct vnode *vp,
	u_offset_t off,
	struct seg *seg,
	caddr_t vaddr,
	size_t size,
	uint_t flags,
	void *resv)
{
	uint_t		bin;
	kmutex_t	*pcm;
	int		i;
	page_t		*pp, *first_pp;
	int		n;
	struct as 	*as = seg->s_as;

	if (size == largepagesize)
		return (page_get_largepage());
	else if (size != MMU_PAGESIZE)
		cmn_err(CE_PANIC,
		"page_get_freelist: illegal size request for i86 platform");

	/*
	 * In PAE mode, pages allocated to kmem_alloc come from
	 * kvseg_pplist pool. This is a pool of pages below 4Gb, big
	 * enough to satisfy kmem_alloc requests.
	 */
#ifdef	PTE36
	if ((vp == &kvp) && (vaddr >= kernelheap) &&
	    (vaddr < ekernelheap) && kvseg_pplist)
		return (page_get_kvseg());
#endif

	/*
	 * Only hold one freelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 *
	 * color = (vpage % cache_pages) + constant.
	 */
	bin = VADDR_2_BIN(as, vaddr);


	for (n = 0; n < nranges; n++) {
	    for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);
		if (page_freelists[n][bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_FREE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_freelists[n][bin]) {
				/*
				 * These were set before the page
				 * was put on the free list,
				 * they must still be set.
				 */
				ASSERT(PP_ISFREE(pp));
				ASSERT(PP_ISAGED(pp));
				ASSERT(pp->p_vnode == NULL);
				ASSERT(pp->p_hash == NULL);
				ASSERT(pp->p_offset == (u_offset_t)-1);
				first_pp = pp;

				/*
				 * Walk down the hash chain
				 */

				while (!page_trylock(pp, SE_EXCL)) {
					pp = pp->p_next;

					ASSERT(PP_ISFREE(pp));
					ASSERT(PP_ISAGED(pp));
					ASSERT(pp->p_vnode == NULL);
					ASSERT(pp->p_hash == NULL);
					ASSERT(pp->p_offset == -1);

					if (pp == first_pp) {
						pp = NULL;
						break;
					}
				}

				if (pp != NULL) {
				    COLOR_STATS_DEC(pf_size[bin]);
				    page_sub(&page_freelists[n][bin], pp);

				    ASSERT(page_pptonum(pp) <= physmax);

				    if ((PP_ISFREE(pp) == 0) ||
					(PP_ISAGED(pp) == 0)) {
					cmn_err(CE_PANIC,
					    "free page is not. pp %x", (int)pp);
				    }
				    mutex_exit(pcm);
				    return (pp);
				}
			}
			mutex_exit(pcm);
		}

		/*
		 * Wow! The bin was empty.
		 */
		COLOR_STATS_INC(sys_nak_bins[bin]);
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		bin += (i == 0) ? BIN_STEP : 1;
		bin &= page_colors_mask;
	    }

	    /* try the next memory range */
	}
	return (NULL);
}

/*
 * Find the `best' page on the cachelist for this (vp,off) (as,vaddr) pair.
 *
 * Does its own locking.
 * If PG_MATCH_COLOR is set, then NULL will be returned if there are no
 * pages of the proper color even if there are pages of a different color.
 * Otherwise, scan the bins for ones with pages.  For each bin with pages,
 * try to lock one of them.  If no page can be locked, try the
 * next bin.  Return NULL if a page can not be found and locked.
 *
 * Finds a pages, TRYs to lock it, then removes it.
 */
/*ARGSUSED*/
page_t *
page_get_cachelist(
	struct vnode *vp,
	u_offset_t off,
	struct seg *seg,
	caddr_t vaddr,
	uint_t flags,
	void *resv)
{
	kmutex_t	*pcm;
	int		i;
	page_t		*pp;
	page_t		*first_pp;
	int		bin;
	int		n;
	struct as	*as = seg->s_as;


#ifdef	PTE36
	if ((vp == &kvp) && (vaddr >= kernelheap) &&
	    (vaddr < ekernelheap) && kvseg_pplist)
		return (page_get_kvseg());
#endif

	/*
	 * Only hold one cachelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 *
	 * color = (vpage % cache_pages) + constant.
	 */
	bin = VADDR_2_BIN(as, vaddr);

	for (n = 0; n < nranges; n++) {
	    for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);
		if (page_cachelists[n][bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_CACHE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_cachelists[n][bin]) {
				first_pp = pp;
				ASSERT(pp->p_vnode);
				ASSERT(PP_ISAGED(pp) == 0);

				while (!page_trylock(pp, SE_EXCL)) {
					pp = pp->p_next;
					if (pp == first_pp) {
						/*
						 * We have searched the
						 * complete list!
						 * And all of them (might
						 * only be one) are locked.
						 * This can happen since
						 * these pages can also be
						 * found via the hash list.
						 * When found via the hash
						 * list, they are locked
						 * first, then removed.
						 * We give up to let the
						 * other thread run.
						 */
						pp = NULL;
						break;
					}
					ASSERT(pp->p_vnode);
					ASSERT(PP_ISFREE(pp));
					ASSERT(PP_ISAGED(pp) == 0);
				}

				if (pp) {
					/*
					 * Found and locked a page.
					 * Pull it off the list.
					 */
					COLOR_STATS_DEC(pc_size[bin]);
					page_sub(&page_cachelists[n][bin], pp);
					mutex_exit(pcm);
					ASSERT(pp->p_vnode);
					ASSERT(PP_ISAGED(pp) == 0);
					return (pp);
				}
			}
			mutex_exit(pcm);
		}
		COLOR_STATS_INC(sys_nak_bins[bin]);
		/*
		 * Wow! The bin was empty or no page could be locked.
		 * If only the proper bin is to be checked, get out
		 * now.
		 */
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		bin += (i == 0) ? BIN_STEP : 1;
		bin &= page_colors_mask;
	    }

	    /* try the next memory range */
	}


	return (NULL);
}

int	x86_l2cache = 512 * 1024;

/*
 * page_coloring_init()
 * called once at startup from kphysm_init() -- before memialloc()
 * is invoked to do the 1st page_free()/page_freelist_add().
 *
 * initializes page_colors and page_colors_mask
 */
void
page_coloring_init()
{
	uint_t colors;

	extern	void		i86_mlist_init();
	extern uintptr_t	kernelbase, _kernelbase, _userlimit,
				_userlimit32;
	extern unsigned long	_dsize_limit;
	extern rlim64_t		rlim_infinity_map[];

	colors = x86_l2cache/ PAGESIZE;	/* 256 */
	if (colors > PAGE_COLORS_MAX - 1)
		colors = PAGE_COLORS_MAX - 1;
	page_colors = colors;
	page_colors_mask = colors - 1;

/*
 * XXX Just a convenient place to calculate this after kernelbase has been
 * set from eeprom. Re evaluate if we need to move this into startup
 */

	i86_mlist_init();

	/*
	 *	Carve out a portion of the user address space for maintaining
	 *	pte and hwpp for the process. With large memory systems,
	 *	kernel address space may be quickly exhausted.
	 */
	if ((!eprom_kernelbase) && (kernelbase != 0xE0000000)) {
		kernelbase =  (uintptr_t)0xC0000000;
		_kernelbase = kernelbase;
	}
	userhwppmap = (hwptepage_t **)((uint_t)kernelbase - HWPPMAPSZ);
	userptemap = (pte_t *)((uintptr_t)kernelbase - PTEMAPSZ);
	/*
	 * Yes. I know we just subtracted again from kernelbase.
	 * But, this is safe as we are overcommitting PTEMAPSZ
	 * It is needed only till usermapend and the difference
	 * is guaranteed to be more than HWPPMAPSZ.
	 */

	userptetable = (pte_t *)((uintptr_t)userptemap - PTETABLESZ);

	userpagedir = (pte_t *)((uintptr_t)userptetable - NPDPERAS * PAGESIZE);
	/* round down to the next mod largepagesize */
	userpagedir = (pte_t *)(((uint_t)userpagedir / PAGESIZE) * PAGESIZE);

	/* round down to the next mod largepagesize */
	usermapend = ((uint_t)userpagedir / PTSIZE) * PTSIZE;

	ASSERT(HWPPMAPSZ <= (PTEMAPSZ -
		((uint_t)usermapend/PAGESIZE)*(sizeof (pte_t))));

	_userlimit = (uintptr_t)usermapend;
	_userlimit32 = _userlimit;
	_dsize_limit = _userlimit - USRTEXT;
	rlim_infinity_map[RLIMIT_DATA] = (rlim64_t)_dsize_limit;
}

/*ARGSUSED*/
int
bp_color(struct buf *bp)
{
	return (0);
}

/*
 * This function is similar to page_get_freelist()/page_get_cachelist()
 * but it searches both the lists to find a page with the specified
 * color (or no color) and DMA attributes. The search is done in the
 * freelist first and then in the cache list within the highest memory
 * range (based on DMA attributes) before searching in the lower
 * memory ranges.
 *
 * Note: This function is called only by page_create_io().
 */
/*ARGSUSED*/
page_t *
page_get_anylist(
	struct vnode *vp,
	u_offset_t off,
	struct as *as,
	caddr_t vaddr,
	size_t size,
	uint_t flags,
	ddi_dma_attr_t *dma_attr)
{
	uint_t		bin;
	kmutex_t	*pcm;
	int		i;
	page_t		*pp, *first_pp;
	int		n, m;
	uint64_t	pgaddr;

	/* currently we support only 4k pages */
	if (size != PAGESIZE)
		return (NULL);

	/*
	 * Only hold one freelist or cachelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 *
	 * color = (vpage % cache_pages) + constant.
	 */

	if (dma_attr == NULL) {
		n = 0;
		m = nranges;
	} else {
		/*
		 * We can gaurantee alignment only for page boundary.
		 */
		if (dma_attr->dma_attr_align > MMU_PAGESIZE)
			return (NULL);
		n = pagelist_num(btop(dma_attr->dma_attr_addr_hi));
		m = pagelist_num(btop(dma_attr->dma_attr_addr_lo)) + 1;
	}

	/*
	 * mem ranges go from high to low. m bounds the range for addr_lo
	 */
	while (n < m) {

	    /* start with the bin of matching color */
	    bin = VADDR_2_BIN(as, vaddr);

	    for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);

		/* try the freelist first */
		if (page_freelists[n][bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_FREE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_freelists[n][bin]) {
				/*
				 * These were set before the page
				 * was put on the free list,
				 * they must still be set.
				 */
				ASSERT(PP_ISFREE(pp));
				ASSERT(PP_ISAGED(pp));
				ASSERT(pp->p_vnode == NULL);
				ASSERT(pp->p_hash == NULL);
				ASSERT(pp->p_offset == (u_offset_t)-1);
				first_pp = pp;

				/*
				 * Walk down the hash chain
				 */

				while (pp) {
				    if (page_trylock(pp, SE_EXCL)) {
					if (dma_attr == NULL)
						break;
					/*
					 * Check for the page within the
					 * specified DMA attributes.
					 *
					 */
					pgaddr = ptob((unsigned long long)
						    (mach_pp->p_pagenum));
					if ((pgaddr >=
						dma_attr->dma_attr_addr_lo) &&
					    (pgaddr + PAGESIZE - 1 <=
						dma_attr->dma_attr_addr_hi))
						break;
					page_unlock(pp);
					/* continue looking */
				    }
				    pp = pp->p_next;

				    ASSERT(PP_ISFREE(pp));
				    ASSERT(PP_ISAGED(pp));
				    ASSERT(pp->p_vnode == NULL);
				    ASSERT(pp->p_hash == NULL);
				    ASSERT(pp->p_offset == -1);

				    if (pp == first_pp) {
					pp = NULL;
					break;
				    }
				}

				if (pp != NULL) {
				    COLOR_STATS_DEC(pf_size[bin]);
				    page_sub(&page_freelists[n][bin], pp);

				    ASSERT(page_pptonum(pp) <= physmax);

				    if ((PP_ISFREE(pp) == 0) ||
					(PP_ISAGED(pp) == 0)) {
					cmn_err(CE_PANIC,
					    "free page is not. pp %x", (int)pp);
				    }
				    mutex_exit(pcm);
				    return (pp);
				}
			}
			mutex_exit(pcm);
		}


		/*
		 * Wow! The bin was empty.
		 */
		COLOR_STATS_INC(sys_nak_bins[bin]);
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		bin += (i == 0) ? BIN_STEP : 1;
		bin &= page_colors_mask;
	    }

	    /* failed to find a page in the freelist; try it in the cachelist */

	    /* start with the bin of matching color */
	    bin = VADDR_2_BIN(as, vaddr);

	    for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);
		if (page_cachelists[n][bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_CACHE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_cachelists[n][bin]) {
				first_pp = pp;
				ASSERT(pp->p_vnode);
				ASSERT(PP_ISAGED(pp) == 0);
				while (pp) {
				    if (page_trylock(pp, SE_EXCL)) {
					if (dma_attr == NULL)
						break;
					/*
					 * Check for the page within the
					 * specified DMA attributes.
					 *
					 */
					pgaddr = ptob((unsigned long long)
						    (mach_pp->p_pagenum));
					if ((pgaddr >=
						dma_attr->dma_attr_addr_lo) &&
					    (pgaddr + PAGESIZE - 1 <=
						dma_attr->dma_attr_addr_hi))
						break;
					page_unlock(pp);
					/* continue looking */
				    }

				    pp = pp->p_next;
				    if (pp == first_pp) {
					/*
					 * We have searched the
					 * complete list!
					 * And all of them (might
					 * only be one) are locked.
					 * This can happen since
					 * these pages can also be
					 * found via the hash list.
					 * When found via the hash
					 * list, they are locked
					 * first, then removed.
					 * We give up to let the
					 * other thread run.
					 */
					pp = NULL;
					break;
				    }
				    ASSERT(pp->p_vnode);
				    ASSERT(PP_ISFREE(pp));
				    ASSERT(PP_ISAGED(pp) == 0);
				}

				if (pp) {
					/*
					 * Found and locked a page.
					 * Pull it off the list.
					 */
					COLOR_STATS_DEC(pc_size[bin]);
					page_sub(&page_cachelists[n][bin], pp);
					mutex_exit(pcm);
					ASSERT(pp->p_vnode);
					ASSERT(PP_ISAGED(pp) == 0);
					return (pp);
				}
			}
			mutex_exit(pcm);
		}
		COLOR_STATS_INC(sys_nak_bins[bin]);
		/*
		 * Wow! The bin was empty or no page could be locked.
		 * If only the proper bin is to be checked, get out
		 * now.
		 */
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		bin += (i == 0) ? BIN_STEP : 1;
		bin &= page_colors_mask;
	    }

	    n++; /* try the next freelist */
	}
	return (NULL);
}


/*
 * page_create_io()
 *
 * This function is a copy of page_create_va() with an additional
 * argument 'mattr' that specifies DMA memory requirements to
 * the page list functions. This function is used by the segkmem
 * allocator so it is only to create new pages (i.e PG_EXCL is
 * set).
 *
 * Note: This interface is currently used by x86 PSM only and is
 *	 not fully specified so the commitment level is only for
 *	 private interface specific to x86. This interface uses PSM
 *	 specific page_get_anylist() interface.
 */

extern kmutex_t	ph_mutex[];
extern uint_t	ph_mutex_shift;

#define	PAGE_HASH_MUTEX(index)	&ph_mutex[(index) >> ph_mutex_shift]

#define	PAGE_HASH_SEARCH(index, pp, vp, off) { \
	for ((pp) = page_hash[(index)]; (pp); (pp) = (pp)->p_hash) { \
		if ((pp)->p_vnode == (vp) && (pp)->p_offset == (off)) \
			break; \
	} \
}

page_t *
page_create_io(
	struct vnode	*vp,
	register u_offset_t off,
	uint_t		bytes,
	uint_t		flags,
	struct as	*as,
	caddr_t		vaddr,
	ddi_dma_attr_t	*mattr)	/* DMA memory attributes if any */
{
	page_t		*plist = NULL;
	uint_t		plist_len = 0;
	pgcnt_t		npages;
	page_t		*npp = NULL;
	uint_t		pages_req;

	TRACE_5(TR_FAC_VM, TR_PAGE_CREATE_START,
		"page_create_start:vp %x off %llx bytes %u flags %x freemem %d",
		vp, off, bytes, flags, freemem);

	ASSERT((flags & ~(PG_EXCL | PG_WAIT | PG_PHYSCONTIG)) == 0);

	pages_req = npages = btopr(bytes);

	/*
	 * Do the freemem and pcf accounting.
	 */
	if (!page_create_wait(npages, flags)) {
		return (NULL);
	}

	TRACE_3(TR_FAC_VM, TR_PAGE_CREATE_SUCCESS,
		"page_create_success:vp %x off %llx freemem %d",
		vp, off, freemem);

	/*
	 * If satisfying this request has left us with too little
	 * memory, start the wheels turning to get some back.  The
	 * first clause of the test prevents waking up the pageout
	 * daemon in situations where it would decide that there's
	 * nothing to do.
	 */
	if (nscan < desscan && freemem < minfree) {
		TRACE_1(TR_FAC_VM, TR_PAGEOUT_CV_SIGNAL,
			"pageout_cv_signal:freemem %ld", freemem);
		cv_signal(&proc_pageout->p_cv);
	}

	if (flags & PG_PHYSCONTIG) {
		page_t		*pp;

		if (!(plist = page_get_contigpage(&npages, mattr, 1))) {
			page_create_putback(npages);
			return (NULL);
		}

		pp = plist;

		do {
			if (!page_hashin(pp, vp, off, NULL)) {
				cmn_err(CE_PANIC,
				    "pg_creat_io: hashin failed %p %p %llx",
				    (void *)pp, (void *)vp, off);
			}
			VM_STAT_ADD(page_create_new);
			off += PAGESIZE;
			page_set_props(pp, P_REF);
			pp = pp->p_next;
		} while (pp != plist);

		if (!npages)
			return (plist);
		else
			vaddr += (pages_req - npages) * PAGESIZE;

		/*
		 * fall-thru:
		 *
		 * page_get_contigpage returns when npages <= sgllen.
		 * Grab the rest of the non-contig pages below from anylist.
		 */
	}

	/*
	 * Loop around collecting the requested number of pages.
	 * Most of the time, we have to `create' a new page. With
	 * this in mind, pull the page off the free list before
	 * getting the hash lock.  This will minimize the hash
	 * lock hold time, nesting, and the like.  If it turns
	 * out we don't need the page, we put it back at the end.
	 */
	while (npages--) {
		register page_t	*pp;
		kmutex_t	*phm = NULL;
		uint_t		index;

		index = PAGE_HASH_FUNC(vp, off);
top:
		ASSERT(phm == NULL);
		ASSERT(index == PAGE_HASH_FUNC(vp, off));
		ASSERT(MUTEX_NOT_HELD(page_vnode_mutex(vp)));

		if (npp == NULL) {
			/*
			 * Try to get the page of any color either from
			 * the freelist or from the cache list.
			 */
			npp = page_get_anylist(vp, off, as, vaddr,
				PAGESIZE, flags & ~PG_MATCH_COLOR, mattr);
			if (npp == NULL) {
				if (mattr == NULL) {
					/*
					 * Not looking for a special page;
					 * panic!
					 */
					cmn_err(CE_PANIC, "no page found %d",
						(int) npages);
				}
				/*
				 * No page found! This can happen
				 * if we are looking for a page
				 * within a specific memory range
				 * for DMA purposes. If PG_WAIT is
				 * specified then we wait for a
				 * while and then try again. The
				 * wait could be forever if we
				 * don't get the page(s) we need.
				 *
				 * Note: XXX We really need a mechanism
				 * to wait for pages in the desired
				 * range. For now, we wait for any
				 * pages and see if we can use it.
				 */

				if ((mattr != NULL) && (flags & PG_WAIT)) {
					delay(10);
					goto top;
				}

				goto fail; /* undo accounting stuff */
			}

			if (PP_ISAGED(npp) == 0) {
				/*
				 * Since this page came from the
				 * cachelist, we must destroy the
				 * old vnode association.
				 */
				page_hashout(npp, (kmutex_t *)NULL);
			}
		}

		/*
		 * We own this page!
		 */
		ASSERT(PAGE_EXCL(npp));
		ASSERT(npp->p_vnode == NULL);
		ASSERT(!hat_page_is_mapped(npp));
		PP_CLRFREE(npp);
		PP_CLRAGED(npp);

		/*
		 * Here we have a page in our hot little mits and are
		 * just waiting to stuff it on the appropriate lists.
		 * Get the mutex and check to see if it really does
		 * not exist.
		 */
		phm = PAGE_HASH_MUTEX(index);
		mutex_enter(phm);
		PAGE_HASH_SEARCH(index, pp, vp, off);
		if (pp == NULL) {
			VM_STAT_ADD(page_create_new);
			pp = npp;
			npp = NULL;
			if (!page_hashin(pp, vp, off, phm)) {
				/*
				 * Since we hold the page hash mutex and
				 * just searched for this page, page_hashin
				 * had better not fail.  If it does, that
				 * means somethread did not follow the
				 * page hash mutex rules.  Panic now and
				 * get it over with.  As usual, go down
				 * holding all the locks.
				 */
				ASSERT(MUTEX_NOT_HELD(phm));
				cmn_err(CE_PANIC,
				    "page_create: hashin failed %p %p %llx %p",
				    (void *)pp, (void *)vp, off, (void *)phm);

			}
			ASSERT(MUTEX_NOT_HELD(phm));	/* hashin dropped it */
			phm = NULL;

			/*
			 * Hat layer locking need not be done to set
			 * the following bits since the page is not hashed
			 * and was on the free list (i.e., had no mappings).
			 *
			 * Set the reference bit to protect
			 * against immediate pageout
			 *
			 * XXXmh modify freelist code to set reference
			 * bit so we don't have to do it here.
			 */
			page_set_props(pp, P_REF);
		} else {
			/*
			 * NOTE: This should not happen for pages associated
			 *	 with kernel vnode 'kvp'.
			 */
			if (vp == &kvp)
			    cmn_err(CE_NOTE, "page_create: page not expected "
				"in hash list for kernel vnode - pp 0x%p",
				(void *)pp);
			VM_STAT_ADD(page_create_exists);
			goto fail;
		}

		/*
		 * Got a page!  It is locked.  Acquire the i/o
		 * lock since we are going to use the p_next and
		 * p_prev fields to link the requested pages together.
		 */
		page_io_lock(pp);
		page_add(&plist, pp);
		plist = plist->p_next;
		off += PAGESIZE;
		vaddr += PAGESIZE;
	}

	return (plist);

fail:
	if (npp != NULL) {
		/*
		 * Did not need this page after all.
		 * Put it back on the free list.
		 */
		VM_STAT_ADD(page_create_putbacks);
		PP_SETFREE(npp);
		PP_SETAGED(npp);
		npp->p_offset = (u_offset_t)-1;
		page_list_add(PG_FREE_LIST, npp, PG_LIST_TAIL);
		page_unlock(npp);
	}

	/*
	 * Give up the pages we already got.
	 */
	while (plist != NULL) {
		register page_t	*pp;

		pp = plist;
		page_sub(&plist, pp);
		page_io_unlock(pp);
		plist_len++;
		/*LINTED: constant in conditional ctx*/
		VN_DISPOSE(pp, B_INVAL, 0, kcred);
	}

	/*
	 * VN_DISPOSE does freemem accounting for the pages in plist
	 * by calling page_free. So, we need to undo the pcf accounting
	 * for only the remaining pages.
	 */
	VM_STAT_ADD(page_create_putbacks);
	page_create_putback(pages_req - plist_len);

	return (NULL);
}

/*
 * Stub function until page_relocate() support required.
 */
/*ARGSUSED*/
page_t *
page_get_replacement_page(page_t *like_pp)
{
	return (NULL);
}

/*
 * Stub function until page_relocate() support required.
 */
/*ARGSUSED*/
int
platform_page_relocate(page_t **target, page_t **replacement)
{
	return (-1);
}

/*
 * Copy the data from the physical page represented by "frompp" to
 * that represented by "topp". ppcopy uses CPU->cpu_caddr1 and
 * CPU->cpu_caddr2.  It assumes that no one uses either map at interrupt
 * level and no one sleeps with an active mapping there.
 */
void
ppcopy(page_t *frompp, page_t *topp)
{
	caddr_t caddr1;
	caddr_t caddr2;
	kmutex_t *ppaddr_mutex;
	pteval_t	ptev;
	pteval_t	*caddr1_pte, *caddr2_pte;
	extern void 	hat_mempte();

	ASSERT(PAGE_LOCKED(frompp));
	ASSERT(PAGE_LOCKED(topp));

	kpreempt_disable(); /* can't preempt if holding caddr1, caddr2 */

	caddr1 = CPU->cpu_caddr1;
	caddr2 = CPU->cpu_caddr2;
	caddr1_pte = CPU->cpu_caddr1pte;
	caddr2_pte = CPU->cpu_caddr2pte;

	ppaddr_mutex = &CPU->cpu_ppaddr_mutex;

	mutex_enter(ppaddr_mutex);

	hat_mempte(frompp, PROT_READ, &ptev, caddr1);
	LOAD_PTE(caddr1_pte, ptev);
	hat_mempte(topp, PROT_READ|PROT_WRITE, &ptev, caddr2);
	LOAD_PTE(caddr2_pte, ptev);
	mmu_tlbflush_entry(caddr1);
	mmu_tlbflush_entry(caddr2);
	bcopy(caddr1, caddr2, PAGESIZE);


	mutex_exit(ppaddr_mutex);

	kpreempt_enable();
}

/*
 * Zero the physical page from off to off + len given by `pp'
 * without changing the reference and modified bits of page.
 * pagezero uses CPU->cpu_caddr2 and assumes that no one uses this
 * map at interrupt level and no one sleeps with an active mapping there.
 *
 * pagezero() must not be called at interrupt level.
 */
void
pagezero(page_t *pp, uint_t off, uint_t len)
{
	caddr_t		caddr2;
	pteval_t	*caddr2_pte;
	kmutex_t	*ppaddr_mutex;
	pteval_t	ptev;
	extern void 	hat_mempte();

	ASSERT((int)len > 0 && (int)off >= 0 && off + len <= PAGESIZE);
	ASSERT(PAGE_LOCKED(pp));

	kpreempt_disable(); /* can't preempt if holding caddr2 */

	caddr2 = CPU->cpu_caddr2;
	caddr2_pte = CPU->cpu_caddr2pte;

	ppaddr_mutex = &CPU->cpu_ppaddr_mutex;
	mutex_enter(ppaddr_mutex);

	hat_mempte(pp, PROT_READ|PROT_WRITE, &ptev, caddr2);
	LOAD_PTE(caddr2_pte, ptev);
	mmu_tlbflush_entry(caddr2);

	bzero(caddr2 + off, len);

	mutex_exit(ppaddr_mutex);
	kpreempt_enable();
}

void
setup_vaddr_for_ppcopy(struct cpu *cpup)
{
	void *addr;
	extern void		setup_pteasmap(void);
	extern	pteval_t	*Sysmap;

	addr = vmem_alloc(heap_arena, ptob(2), VM_SLEEP);

	cpup->cpu_caddr1 = addr;
	cpup->cpu_caddr2 = cpup->cpu_caddr1 + PAGESIZE;
	cpup->cpu_caddr1pte = (pteptr_t)
	    &Sysmap[mmu_btop((uintptr_t)cpup->cpu_caddr1 - kernelbase)];
	cpup->cpu_caddr2pte = (pteptr_t)
	    &Sysmap[mmu_btop((uintptr_t)cpup->cpu_caddr2 - kernelbase)];

	mutex_init(&cpup->cpu_ppaddr_mutex, NULL, MUTEX_DEFAULT, NULL);
	setup_pteasmap();
}

void
psm_pageinit(machpage_t *pp, uint32_t pnum)
{
	pp->p_pagenum = pnum;
	pp->p_share = 0;
	pp->p_deleted = 0;
	pp->p_flags = 0;
}

void
pageout_scanner_init(procedure)
	void (*procedure)();
{
	extern void set_pageout_scanner_context();

	set_pageout_scanner_context();
	procedure();
}

/*
 * Create & Initialise pageout scanner thread. The thread has to
 * start at procedure with process pp and priority pri.
 */
int
pageout_init(void (*procedure)(), proc_t *pp, pri_t pri)
{
	if (thread_create(NULL, PAGESIZE, pageout_scanner_init,
		(caddr_t)procedure, 0, pp, TS_RUN, pri) == NULL)
		return (0);
	return (1);
}

void
mmu_init()
{
	extern struct mmuinfo mmuinfo;

	if (largepagesize == TWOMB_PAGESIZE) {
	    if ((x86_feature & X86_PAE) == 0) {
		cmn_err(CE_PANIC, "Processor does not support"
		    "Physical Address Extension");
	    }
	    if ((x86_feature & X86_CXS) == 0) {
		cmn_err(CE_PANIC, "Processor does not support"
		    "cmpxchg8b instruction");
	    }
	}
	if (x86_feature & X86_LARGEPAGE)
		largepagesupport = 1;
	mmuinfo.mmu_highest_pfn = MMU_HIGHEST_PFN;
	mmuinfo.mmu_name = MMU_NAME;

}

/* ARGSUSED */
int
cpuid2nodeid(int cpun)
{
	return (0);
}

/* ARGSUSED */
void *
kmem_node_alloc(size_t size, int flags, int node)
{
	return (kmem_alloc(size, flags));
}


#ifdef	PTE36

void
clear_bootpde(struct cpu *cpup)
{
	boot_pte0 = cpup->cpu_pagedir[0];
	boot_pte1 = cpup->cpu_pagedir[1];
	cpup->cpu_pagedir[0] = MMU_STD_INVALIDPTE;
	cpup->cpu_pagedir[1] = MMU_STD_INVALIDPTE;
	pt_pdir[0] = MMU_STD_INVALIDPTE;
}

page_t *
page_get_kvseg(void)
{
	page_t	*pp, *first_pp;

	mutex_enter(&kvseg_pplist_lock);
	pp = kvseg_pplist;
	ASSERT(pp);
	ASSERT(PP_ISFREE(pp));
	first_pp = pp;

	/*
	 * Walk down the hash chain
	 */

	while (!page_trylock(pp, SE_EXCL)) {
		pp = pp->p_next;

		ASSERT(PP_ISFREE(pp));
		if (pp == first_pp) {
			pp = NULL;
			break;
		}
	}
	if (pp)
		page_sub(&kvseg_pplist, pp);
	mutex_exit(&kvseg_pplist_lock);
	return (pp);
}
void
mmu_setup_kvseg(pfn_t highest_pfn)
{
	int 		i;
	size_t		npages, pages_mapped;
	caddr_t		addr;
	page_t		*pp;
	ddi_dma_attr_t	attr;
	pfn_t		pfn;

	/*
	 * If the highest memory to be mapped is less than 4Gb,
	 * or user requested unrestricted kmem_allocs, just return
	 */
	if ((highest_pfn < 0x100000) || !restricted_kmemalloc)
		return;

	/*
	 * pre allocate physical pages for kvseg
	 */
	mutex_enter(&kvseg_pplist_lock);
	pages_mapped = 0;
	for (addr = kernelheap; addr < ekernelheap; addr += MMU_PAGESIZE) {
		if ((pfn = hat_getkpfnum(addr)) != PFN_INVALID) {
			pp = page_numtopp_nolock(pfn);
			if (pp)
				atomic_orb(&mach_pp->p_flags, P_KVSEGPAGE);
			pages_mapped++;
		}
	}

	npages = btop((uintptr_t)(ekernelheap - kernelheap)) - pages_mapped;

	if (!page_create_wait(npages, 0)) {
		cmn_err(CE_PANIC, "mmu_setup_kvseg: "
		    "Can not pre allocate pages for kvseg\n");
	}
	bzero((caddr_t)&attr, sizeof (attr));
	attr.dma_attr_addr_hi = 0x0FFFFFFFFULL;
	for (i = 0, addr = kernelheap; i < npages; i++, addr += MMU_PAGESIZE) {
		pp = page_get_anylist(&kvp, 0, &kas, addr,
			PAGESIZE, 0, &attr);
		if (pp == NULL)
			cmn_err(CE_PANIC, "mmu_setup_kvseg: "
			    "Can not pre allocate pages for kvseg\n");
		atomic_orb(&mach_pp->p_flags, P_KVSEGPAGE);

		page_add(&kvseg_pplist, pp);
		page_unlock(pp);
	}
	mutex_exit(&kvseg_pplist_lock);
}
#else	/* PTE36 */

void
clear_bootpde(struct cpu *cpup)
{
	pt_pdir[0] = MMU_STD_INVALIDPTE;
	*(uint32_t *)(cpup->cpu_pagedir) = 0;
}

/*ARGSUSED*/
void 	mmu_setup_kvseg(pfn_t pfn) {}
#endif
void post_startup_mmu_initialization(void) {}

/*
 * Function for flushing D-cache when performing module relocations
 * to an alternate mapping.  Stubbed out on all platforms except sun4u,
 * at least for now.
 */
void
dcache_flushall()
{
}
