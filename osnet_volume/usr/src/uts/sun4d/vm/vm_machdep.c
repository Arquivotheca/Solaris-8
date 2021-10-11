/*
 * Copyright (c) 1987-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vm_machdep.c	1.76	99/09/17 SMI"

/*
 * UNIX machine dependent virtual memory support.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>
#include <sys/vm.h>
#include <sys/mman.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/cmn_err.h>
#include <sys/vmsystm.h>
#include <sys/bitmap.h>
#include <sys/debug.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kp.h>
#include <vm/seg_vn.h>
#include <vm/page.h>
#include <vm/seg_kmem.h>

#include <sys/mmu.h>
#include <sys/pte.h>
#include <sys/cpu.h>
#include <sys/vm_machparam.h>
#include <sys/elf_SPARC.h>

#include <vm/hat_srmmu.h>
#include <vm/mach_page.h>

#ifdef NOT_SUPPORTED
/*
 * Assuming that maximum memory will be 512MB (this assumption can be voided
 * and the following tables can be sized according to the installed memory
 * early during startup) we logically partition memory to either belong to
 * a 256K chunk or a 16MB chunk. The tables l1_freecnt and l2_freecnt
 * serve as maps for free memory not being used by the system.
 * The free memory is incremented every time a page is added to
 * the freelist and decremented every time the page is deleted from the
 * page freelist. There will be some (although minimal) cost increase to
 * the page_add and page_sub operations. The tables are protected by the
 * page_free_lock.
 */

static u_int	still_counting = 1;
static kmutex_t	cp_lock;

u_short		*l1_freecnt;
u_char		*l2_freecnt;
int		l1_free_tblsz, l2_free_tblsz;
#endif /* NOT_SUPPORTED */

#ifdef DEBUG
#define	COLOR_STATS
#endif DEBUG

/*
 * Array of page sizes.
 */

#define	SRMMU_PAGE_SIZES	3

u_int hw_pgz_array[] = {
	PAGESIZE,
	L3PTSIZE,
	L2PTSIZE,
	0
};

u_int
page_num_pagesizes()
{
	return (SRMMU_PAGE_SIZES);
}

size_t
page_get_pagesize(u_int n)
{
	if (n >= SRMMU_PAGE_SIZES)
		cmn_err(CE_PANIC, "page_get_pagesize: out of range %d", n);
	return (hw_pgz_array[n]);
}

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
	register struct as *as;
	register struct proc *p;
	register faultcode_t res;
	caddr_t base;
	size_t len;
	int err;
	int mapped_red;

	mapped_red = segkp_map_red();

	if (iskernel) {
		as = &kas;
	} else {
		p = curproc;
		as = p->p_as;
	}

	/*
	 * Dispatch pagefault.
	 */
	res = as_fault(as->a_hat, as, addr, 1, type, rw);

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
	len = (((u_int)base + len + PAGEOFFSET) & PAGEMASK) -
	    ((u_int)base & PAGEMASK);
	base = (caddr_t)((u_int)base & PAGEMASK);

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

	res = as_fault(as->a_hat, as, addr, 1, F_INVAL, rw);

out:
	if (mapped_red)
		segkp_unmap_red();

	return (res);
}

#ifdef DEBUG
static u_int map_l1, map_l2;
#endif DEBUG

/*ARGSUSED*/
void
map_addr(caddr_t *addrp, size_t len, offset_t off, int align, u_int flags)
{
	ASSERT(curproc->p_as->a_userlimit == (caddr_t)USERLIMIT);
	map_addr_proc(addrp, len, off, align, (caddr_t)USERLIMIT, curproc);
}

/*
 * map_addr_proc() is the routine called when the system is to
 * chose an address for the user.  We will pick an address
 * range which is just below the current stack limit.  The
 * algorithm used for cache consistency on machines with virtual
 * address caches is such that offset 0 in the vnode is always
 * on a shm_alignment'ed aligned address.  Unfortunately, this
 * means that vnodes which are demand paged will not be mapped
 * cache consistently with the executable images.  When the
 * cache alignment for a given object is inconsistent, the
 * lower level code must manage the translations so that this
 * is not seen here (at the cost of efficiency, of course).
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
map_addr_proc(
	caddr_t		*addrp,
	size_t		len,
	offset_t	off,
	int		align,
	caddr_t		userlimit,
	struct proc	*p)
{
	struct as *as = p->p_as;
	caddr_t addr;
	caddr_t base;
	size_t slen;
	u_int align_amount;

	ASSERT(userlimit == as->a_userlimit);

	base = p->p_brkbase;
	slen = (caddr_t)USRSTACK - base
	    - (((rlim_t)P_CURLIMIT(p, RLIMIT_STACK) + PAGEOFFSET)
	    & PAGEMASK);
	len = (len + PAGEOFFSET) & PAGEMASK;

	/*
	 * Redzone for each side of the request. This is done to leave
	 * one page unmapped between segments. This is not required, but
	 * it's useful for the user because if their program strays across
	 * a segment boundary, it will catch a fault immediately making
	 * debugging a little easier.
	 */
	len += (2 * PAGESIZE);

	/*
	 *  If the request is larger than the size of a particular
	 *  mmu level, then we use that level to map the request.
	 *  But this requires that both the virtual and the physical
	 *  addresses be aligned with respect to that level, so we
	 *  do the virtual bit of nastiness here.
	 */
	if (len >= MMU_STD_REGIONSIZE) {  /* level 1 mappings */
#ifdef DEBUG
		map_l1++;
#endif
		align_amount = MMU_STD_REGIONSIZE;
	} else if (len >= MMU_STD_SEGMENTSIZE) { /* level 2 mappings */
#ifdef DEBUG
		map_l2++;
#endif
		align_amount = MMU_STD_SEGMENTSIZE;
	} else {
		/*
		 * Align virtual addresses on a 64K boundary to ensure
		 * that ELF shared libraries are mapped with the appropriate
		 * alignment constraints by the run-time linker.
		 */
		align_amount = ELF_SPARC_MAXPGSZ;
	}

#ifdef VAC
	if (vac && align)
		if (align_amount < shm_alignment)
			align_amount = shm_alignment;
#endif

	len += align_amount;

	/*
	 * Look for a large enough hole starting below the stack limit.
	 * After finding it, use the upper part.  Addition of PAGESIZE is
	 * for the redzone as described above.
	 */
	if (as_gap(as, len, &base, &slen, AH_HI, (caddr_t)NULL) == 0) {
		caddr_t as_addr;

		addr = base + slen - len + PAGESIZE;
		as_addr = addr;
		/*
		 * Round address DOWN to the alignment amount,
		 * add the offset, and if this address is less
		 * than the original address, add alignment amount.
		 */
		addr = (caddr_t)((u_int)addr & (~(align_amount - 1)));
		addr += (int)(off & (align_amount - 1));
		if (addr < as_addr)
			addr += align_amount;

		ASSERT(addr <= (as_addr + align_amount));
		ASSERT(((u_int)addr & (align_amount - 1)) ==
			((u_int)(off & (align_amount - 1))));
		*addrp = addr;
	} else {
		*addrp = ((caddr_t)NULL);	/* no more virtual space */
	}
}

/*
 * Determine whether [base, base+len] contains a mapable range of
 * addresses at least minlen long. base and len are adjusted if
 * required to provide a mapable range.
 */
/* ARGSUSED */
int
valid_va_range(caddr_t *basep, size_t *lenp, size_t minlen, int dir)
{
	register caddr_t hi, lo;

	lo = *basep;
	hi = lo + *lenp;

	/*
	 * If hi rolled over the top, try cutting back.
	 */
	if (hi < lo) {
		if (0 - (u_int)lo + (u_int)hi < minlen)
			return (0);
		if (0 - (u_int)lo < minlen)
			return (0);
		*lenp = 0 - (u_int)lo;
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
 * Routine used to check to see if an a.out can be executed
 * by the current machine/architecture.
 */
int
chkaout(struct exdata *exp)
{
	if (exp->ux_mach == M_SPARC)
		return (0);
	else
		return (ENOEXEC);
}

/*
 * The following functions return information about an a.out
 * which is used when a program is executed.
 */

/*
 * Return the load memory address for the data segment.
 */
caddr_t
getdmem(struct exec *exp)
{
	/*
	 * XXX - Sparc Reference Hack approaching
	 * Remember that we are loading
	 * 8k executables into a 4k machine
	 * DATA_ALIGN == 2 * NBPG
	 */
	if (exp->a_text)
		return ((caddr_t)(roundup(USRTEXT + exp->a_text, DATA_ALIGN)));
	else
		return ((caddr_t)USRTEXT);
}

/*
 * Return the starting disk address for the data segment.
 */
u_long
getdfile(struct exec *exp)
{
	if (exp->a_magic == ZMAGIC)
		return (exp->a_text);
	else
		return (sizeof (struct exec) + exp->a_text);
}

/*
 * Return the load memory address for the text segment.
 */
/*ARGSUSED*/
caddr_t
gettmem(struct exec *exp)
{
	return ((caddr_t)USRTEXT);
}

/*
 * Return the file byte offset for the text segment.
 */
u_int
gettfile(struct exec *exp)
{
	if (exp->a_magic == ZMAGIC)
		return (0);
	else
		return (sizeof (struct exec));
}

#ifdef XXX

/*
 * XXX - The support for this stuff has not been implemented.
 *
 * Give up this page, which contains the address that caused the
 * parity error.  While we are at it, see if the parity error is permanent
 * If it is, then lock the page so that it will be never be used again (at
 * least until we reboot).
 */
page_giveup(addr, ppte, pp)
	caddr_t		 addr;
	struct pte	*ppte;
	struct page	*pp;
{
	int remove_page;

	ASSERT(PAGE_LOCKED(pp));

	/*
	 * Check to see if we get another parity error when we access
	 * this address again.
	 */
	remove_page = parerr_reset(addr, ppte);

	hat_pageunload(pp, HAT_FORCE_PGUNLOAD);

	/*
	 * Destroy the association of this page with its data.
	 * If retry failed (returned -1), then tell VN_DISPOSE to not
	 * free or unlock the page to keep it from being used again.
	 * This will let the machine stay up for now, without the
	 * parity error affecting future processes.
	 */
	VN_DISPOSE(pp, B_INVAL, (remove_page == -1), kcred);

	/* Tell what we did. */
	printf("page %x %s service.\n", ptob(ppte->PhysicalPageNumber),
	    remove_page == -1 ? "marked out of" : "back in");
}
#endif	XXX

void
getexinfo(edp_in, edp_out, pagetext, pagedata)
	struct exdata *edp_in, *edp_out;
	int *pagetext;
	int *pagedata;
{
	*edp_out = *edp_in;	/* structure copy */

	if ((edp_in->ux_mag == ZMAGIC) &&
	    ((edp_in->vp->v_flag & VNOMAP) == 0)) {
		*pagetext = 1;
		*pagedata = 1;
	} else {
		*pagetext = 0;
		*pagedata = 0;
	}
}

/*
 * Return 1 if the page frame is onboard memory, else 0.
 */
int
pf_is_memory(pfn_t pf)
{
	/*
	 * If the upper four bits of the 36-bit physical address
	 * are less than 8, then it's memory (see sec. 3.2 of
	 * the sun4d architecture).
	 */
	return ((pf >> (32 - PAGESHIFT)) < 8);
}


/*
 * initialized by page_coloring_init()
 */
static u_int page_colors = 1;
static u_int page_colors_mask;

/*
 * The hat_data[] area is used to keep state about a particular address space.
 */
#define	astocolor_flags(as)	((as)->a_hat->hat_data[HAT_DATA_COLOR_FLAGS])

/*
 * we always use page coloring on sun4d, so do_pg_coloring
 * exists only for tests, and in case we have a bug.
 */
extern int do_pg_coloring;

/*
 * Both the page freelist and cachelist are hashed into bins
 * based on their physical page frame numbers.
 */
#define	PAGE_COLORS_MAX	512	/* sun4d specific limit */
static	page_t *page_freelists[PAGE_COLORS_MAX];
static	page_t *page_cachelists[PAGE_COLORS_MAX];

/*
 * There are at most 512 colors/bins.  Spread them out under a
 * couple of locks.  There are mutexes for both the page freelist
 * and the page cachelist.
 */
#define	PC_SHIFT	(4)
#define	NPC_MUTEX	(PAGE_COLORS_MAX/(1 << PC_SHIFT))

static kmutex_t	fpc_mutex[NPC_MUTEX];
static kmutex_t	cpc_mutex[NPC_MUTEX];

#ifdef	COLOR_STATS

#define	COLOR_STATS_INC(x) (x)++;
#define	COLOR_STATS_DEC(x) (x)--;

static	u_int	pf_size[PAGE_COLORS_MAX];
static	u_int	pc_size[PAGE_COLORS_MAX];

static	u_int	color_flags[2];

static	u_int	sys_nak_bins[PAGE_COLORS_MAX];
static	u_int	sys_req_bins[PAGE_COLORS_MAX];

#else	COLOR_STATS

#define	COLOR_STATS_INC(x)
#define	COLOR_STATS_DEC(x)

#endif	COLOR_STATS


#define	PP_2_BIN(pp)		(((pp)->p_pagenum) & page_colors_mask)

#define	PC_BIN_MUTEX(bin, list)	((list == PG_FREE_LIST)? \
	&fpc_mutex[(((bin)>>PC_SHIFT)&(NPC_MUTEX-1))] :	\
	&cpc_mutex[(((bin)>>PC_SHIFT)&(NPC_MUTEX-1))])

/*
 * hash `as' and `vaddr' to get a bin.
 * sizeof(struct as) is 60.
 * shifting down by 4 bits will cause consecutive as's to be offset by ~3.
 */
#define	AS_2_BIN(as, vaddr, hash) \
	(((hash) ? \
	(((u_int)(vaddr) >> PAGESHIFT) ^ ((u_int)(as) >> 4)) : \
	((u_int)(vaddr) >> PAGESHIFT) + ((u_int)(as) >> 3)) \
	& page_colors_mask)

/*
 * return per address space page coloring flags
 * according to how busy the system is at the time
 * the address space is created.  This allows the page coloring
 * policy to change depending on system load.
 *
 * currently, we return 1 if 2 * the one-minute load average >= the
 * number of cpus on-line.  A better alternative may be to base this on
 * the instantaneous nrunnable + nrunning.
 */

#define	INFLATE_LOAD_SHIFT	1

u_int
get_color_flags(struct as *as)
{
	u_int load_1m;
	u_int busy_flag = 0;

	if (as == &kas)
		return (0);

	load_1m = (avenrun[0] >> (FSHIFT - INFLATE_LOAD_SHIFT));

	if (load_1m >= ncpus_online)
		busy_flag = 1;

	COLOR_STATS_INC(color_flags[busy_flag]);

	return (busy_flag);
}

/*
 * Take a particular page off of whatever freelist the page is claimed to be on.
 */
void
page_list_sub(int list, page_t *pp)
{
	u_int		bin;
	kmutex_t	*pcm;
	page_t		**ppp;

	ASSERT(PAGE_EXCL(pp));
	ASSERT(PP_ISFREE(pp));

	bin = PP_2_BIN((machpage_t *)pp);
	pcm = PC_BIN_MUTEX(bin, list);

	if (list == PG_FREE_LIST) {
		ppp = &page_freelists[bin];
		COLOR_STATS_DEC(pf_size[bin]);
		ASSERT(PP_ISAGED(pp));

#ifdef NOT_SUPPORTED
		if (still_counting) {
			u_int		pf;

			pf = page_pptonum(pp);
			ASSERT(pf <= physmax);
			mutex_enter(&cp_lock);
			l1_freecnt[pf >> MMU_STD_FIRSTSHIFT]--;
			l2_freecnt[pf >> MMU_STD_SECONDSHIFT]--;
			mutex_exit(&cp_lock);
		}
#endif /* NOT_SUPPORTED */

	} else {
		ppp = &page_cachelists[bin];
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
	u_int		bin;
	u_int		*pc_stats;

	ASSERT(PAGE_EXCL(pp));
	ASSERT(PP_ISFREE(pp));
	ASSERT(!hat_page_is_mapped(pp));

	bin = PP_2_BIN((machpage_t *)pp);
	pcm = PC_BIN_MUTEX(bin, list);

	if (list == PG_FREE_LIST) {
		ASSERT(PP_ISAGED(pp));
		/* LINTED */
		ASSERT(pc_stats = &pf_size[bin]);  /* the `=' is correct */
		ppp = &page_freelists[bin];

#ifdef NOT_SUPPORTED
		if (still_counting) {
			u_int		pf;

			pf = page_pptonum(pp);
			ASSERT(pf <= physmax);
			mutex_enter(&cp_lock);
			l1_freecnt[pf >> MMU_STD_FIRSTSHIFT]++;
			l2_freecnt[pf >> MMU_STD_SECONDSHIFT]++;
			mutex_exit(&cp_lock);
		}
#endif /* NOT_SUPPORTED */

	} else {
		ASSERT(pp->p_vnode);
		ASSERT((pp->p_offset & 0xfff) == 0);
		/* LINTED */
		ASSERT(pc_stats = &pc_size[bin]);  /* the `=' is correct */
		ppp = &page_cachelists[bin];
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
	u_int flags,
	void *resv)
{
	u_int		bin;
	kmutex_t	*pcm;
	int		i;
	page_t		*pp, *first_pp;
	struct as 	*as = seg->s_as;


	/*
	 * Only support one page size.
	 */
	if (size != MMU_PAGESIZE)
		return (NULL);

	/*
	 * Only hold one freelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 *
	 * color = (vpage % cache_pages) + constant.
	 */
	bin = AS_2_BIN(as, vaddr, astocolor_flags(as));

	for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);
		if (page_freelists[bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_FREE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_freelists[bin]) {
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
					ASSERT(pp->p_offset == (u_offset_t)-1);

					if (pp == first_pp) {
						pp = NULL;
						break;
					}
				}

				if (pp != NULL) {
					COLOR_STATS_DEC(pf_size[bin]);
					page_sub(&page_freelists[bin], pp);

					if ((PP_ISFREE(pp) == 0) ||
					    (PP_ISAGED(pp) == 0)) {
						cmn_err(CE_PANIC,
						    "free page is not. pp %p",
						    (void *)pp);
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
	u_int flags,
	void *resv)
{
	kmutex_t	*pcm;
	int		i;
	page_t		*pp;
	page_t		*first_pp;
	int		bin;
	struct as	*as = seg->s_as;

	/*
	 * Only hold one cachelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 *
	 * color = (vpage % cache_pages) + constant.
	 */
	bin = AS_2_BIN(as, vaddr, astocolor_flags(as));

	for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);
		if (page_cachelists[bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_CACHE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_cachelists[bin]) {
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
					page_sub(&page_cachelists[bin], pp);
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

	return (NULL);
}

/*
 * page_coloring_init()
 * called once at startup from kphysm_init() -- before memialloc()
 * is invoked to do the 1st page_free()/page_freelist_add().
 *
 * initializes page_colors and page_colors_mask and
 * based on the cache size of the boot CPU.
 *
 * On SC2000, while unsupported, somebody could mix 1MB and 2MB ecache modules.
 * Effect will be sub-optimal, but still functional:
 *
 * if (cpu0 is 2MB): 512 colors.
 *		No effect on V2P running on 1MB ecache.
 *		BEST_BIN on 1MB ecache, however, will more frequently
 *		choose "false cool" pages which conflict with hot
 *		application pages.
 *
 * if (cpu0 is 1MB): 256 colors.
 *		effect on applications and the kernel running on 2MB ecache
 *		is entirely dependent on the ordering of the freelists.
 */
void
page_coloring_init()
{
	u_int		mxcc_ctl;
	u_int		colors;
	extern u_int	intr_mxcc_cntrl_get();

	if (do_pg_coloring == 0)
		return;

	/*
	 * yes, we could ask OBP how big the ecache is,
	 * but OBP tends to lie about this fact.
	 */
	mxcc_ctl = intr_mxcc_cntrl_get();

	if (mxcc_ctl & MXCC_CTL_CS) {
		colors = (2 * 1024 * 1024) / PAGESIZE;	/* 512 */
	} else {
		colors = (1 * 1024 * 1024) / PAGESIZE;	/* 256 */
	}

	if (mxcc_ctl & MXCC_CTL_HC)
		colors = colors >> 1;

	page_colors = colors;
	page_colors_mask = colors - 1;
}

/*ARGSUSED*/
int
bp_color(struct buf *bp)
{
	return (0);
}

#ifdef NOT_SUPPORTED

/*
 * This routine will search the L1 L2 page free count tables to find
 * contiguous physical memory pages big enough to fit user's request.
 *
 * Input:	u_int	req_bytes  (number of contig bytes)
 *		int	addr_align	(e.g 0x40000 = 256Kb align)
 *
 * Output:	page_t	*pp	Pointing to the first contiguous page
 *		int	NULL	NO such contig chunk is available
 */
page_t *
page_get_contig(u_int req_bytes, int addr_align)
{
	machpage_t	*first_pp, *last_pp, *tmp_pp, *prev_pp;
	u_int  		req_pages, first_pfnum;
	int		req_chunks = 0;
	int		i, start;

	/* Default to level 2 when no alignment requested */
	if (addr_align == 0) {
		addr_align = MMU_L2_SIZE;
	}

	/* Must be mutiples of the alignment size */
	if ((req_bytes % addr_align) != 0) {
		return (NULL);
	}
	req_pages = mmu_btop(req_bytes);

	/*
	 * Ask the vm system if there are enough pages
	 * to satisfy the request.
	 */
	if (!page_create_wait(req_pages, PG_EXCL)) {
		return (NULL);
	}

	/*
	 * lock the freelist
	 */
	for (i = 0; i < NPC_MUTEX; i++) {
		mutex_enter(&fpc_mutex[i]);
	}

	/*
	 * Search for a big enough & correctly aligned block.
	 *
	 * start should always point to the last 16MB chunk before
	 * chunk i which is not completely free. The number of
	 * contiguous completely free chunks currently seen
	 * is thus i - start.
	 */
	if (addr_align == MMU_L1_SIZE) {
		req_chunks = req_pages / mmu_btop(MMU_L1_SIZE);
		start = -1;
		for (i = 0; i < l1_free_tblsz; i++) {
			if (l1_freecnt[i] != mmu_btop(MMU_L1_SIZE))
				start = i;
			else if ((i - start) >= req_chunks)
				break;
		}
		if (i == l1_free_tblsz)
			goto contig_fail;
		first_pfnum = (start + 1) * mmu_btop(MMU_L1_SIZE);
	} else if (addr_align == MMU_L2_SIZE) {
		req_chunks = req_pages / mmu_btop(MMU_L2_SIZE);
		start = -1;
		for (i = 0; i < l2_free_tblsz; i++) {
			if (l2_freecnt[i] != mmu_btop(MMU_L2_SIZE))
				start = i;
			else if ((i - start) >= req_chunks)
				break;
		}
		if (i == l2_free_tblsz)
			goto contig_fail;
		first_pfnum = (start + 1) * mmu_btop(MMU_L2_SIZE);
	} else {
		goto contig_fail;
	}

	/*
	 * There are enough contiguous pages.
	 *
	 * Pages on the page free list are pulled from the
	 * list then locked.  We hold the freelist (fpc_mutex)
	 * locks at the moment, so start pulling.
	 */
	first_pp = (machpage_t *)page_numtopp_nolock(first_pfnum);
	/*
	 * No need to use the page_next mechanism here
	 * as we're using machpage_t's and we know the memory is
	 * contiguous so the machpage_t's will be also.
	 */
	last_pp = first_pp + (req_pages - 1);
	for (tmp_pp = first_pp; tmp_pp <=  last_pp; tmp_pp++) {
		u_int		bin;

		bin = PP_2_BIN(tmp_pp);
		ASSERT(PP_ISFREE((page_t *)tmp_pp));
		ASSERT(PP_ISAGED((page_t *)tmp_pp));
		ASSERT(tmp_pp->p_paget.p_vnode == NULL);
		page_sub(&page_freelists[bin], (page_t *)tmp_pp);
	}

	/*
	 * Unlock the freelist
	 */
	for (i = 0; i < NPC_MUTEX; i++) {
		mutex_exit(&fpc_mutex[i]);
	}

	for (tmp_pp = first_pp; tmp_pp <=  last_pp; tmp_pp++) {
		/*
		 * Do we have to worry about collisions with
		 * page_numtopp_nowait() and the like?
		 *
		 * These routines do not search the freelists
		 * for pages, but rather find a page based on
		 * its pfn, just like we did.  Subsequently,
		 * they can still find these pages even though
		 * the pages are not on the freelist (or any list
		 * for that matter) right now.
		 */
		ASSERT(PP_ISFREE((page_t *)tmp_pp));
		ASSERT(PP_ISAGED((page_t *)tmp_pp));
		ASSERT(tmp_pp->p_paget.p_vnode == NULL);

		while (!page_lock((page_t *)tmp_pp, SE_EXCL,
		    NULL, P_NO_RECLAIM)) {
			if ((PP_ISFREE((page_t *)tmp_pp) == 0) ||
			    (PP_ISAGED((page_t *)tmp_pp) == 0)) {
				cmn_err(CE_PANIC,
				    "contig free page is not, %x",
				    tmp_pp);
			}
		}

		/*
		 * It is too bad we can not give these pages
		 * a real identity, but we did not get a vnode/offset
		 * to start with.
		 */
		PP_CLRFREE((page_t *)tmp_pp);
		PP_CLRAGED((page_t *)tmp_pp);
	}

	/*
	 * Make the contiguous page list doubly linked
	 * Then fix up the ends so the list is not circular.
	 */
	for (tmp_pp = first_pp, prev_pp = (machpage_t *)0;
	    tmp_pp <= last_pp;
	    prev_pp = tmp_pp, tmp_pp = (machpage_t *)tmp_pp->p_paget.p_next) {
		tmp_pp->p_paget.p_next = (page_t *)(tmp_pp + 1);
		tmp_pp->p_paget.p_prev = (page_t *)prev_pp;
	}

	last_pp->p_paget.p_next = (page_t *)0;

	/*
	 * Since we did not bother to fix up the l1 & l2 counts,
	 * don't do this any more.
	 */
	still_counting = 0;

	mutex_enter(&freemem_lock);
	availrmem -= req_pages;
	mutex_exit(&freemem_lock);

	return ((page_t *)first_pp);

contig_fail:
	/*
	 * unlock the freelist, account for our failure.
	 */
	for (i = 0; i < NPC_MUTEX; i++) {
		mutex_exit(&fpc_mutex[i]);
	}

	page_create_putback(req_pages);

	return (NULL);
}


/*
 * This routine will return the reserved pages back to page_freelist
 *
 * Input:	page_t	*pp	The address of first page-struct
 *
 * Output:	1		succeeded
 *
 */
int
page_free_contig(page_t *pp)
{
	page_t		*o_pp;
	page_t		*n_pp;

	ASSERT(pp->p_prev == NULL);

	o_pp = NULL;
	while (pp) {
		ASSERT(PAGE_EXCL(pp));
		ASSERT(PP_ISFREE(pp) == 0);
		ASSERT(!hat_page_is_mapped(pp));

		if ((o_pp) && (pp->p_prev != o_pp)) {
			cmn_err(CE_PANIC,
			    "page_free_contig: bad list, %x", pp);
		}
		n_pp = pp->p_next;

		/*
		 * Call page_free directly since there is not really
		 * an associated vnode that needs to be informed.
		 */
		page_free(pp, 1);

		mutex_enter(&freemem_lock);
		availrmem++;
		mutex_exit(&freemem_lock);

		o_pp = pp;
		pp = n_pp;
	}
	return (1);
}

#endif /* NOT_SUPPORTED */

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
 * Create & Initialise pageout scanner thread. The thread has to
 * start at procedure with process pp and priority pri.
 */
int
pageout_init(void (*procedure)(), proc_t *pp, pri_t pri)
{
	if (thread_create(NULL, PAGESIZE, procedure,
		0, 0, pp, TS_RUN, (int)pri) == NULL)
		return (0);
	return (1);
}

/*
 * Function for flushing D-cache when performing module relocations
 * to an alternate mapping.  Stubbed out on all platforms except sun4u,
 * at least for now.
 */
void
dcache_flushall()
{
}
