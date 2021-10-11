/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vm_machdep.c	1.88	99/09/17 SMI"

/*
 * UNIX machine dependent virtual memory support.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
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
#include <vm/mach_page.h>

#include <sys/cpu.h>
#include <sys/vm_machparam.h>
#include <sys/atomic.h>
#include <sys/elf_SPARC.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>

#include <vm/hat_sfmmu.h>

#include <sys/memnode.h>

#include <sys/kmem.h>

#include <sys/mem_config.h>
#include <sys/mem_cage.h>
#include <vm/vm_machdep.h>

/*
 * These variables are set by module specific config routines.
 * They are only set by modules which will use physical cache page coloring
 * and/or virtual cache page coloring.
 */
int do_pg_coloring = 0;
int do_virtual_coloring = 0;

/*
 * These variables can be conveniently patched at kernel load time to
 * prevent do_pg_coloring or do_virtual_coloring from being enabled by
 * module specific config routines.
 */

int use_page_coloring = 1;
int use_virtual_coloring = 1;

/*
 * initialized by page_coloring_init()
 */
static uint_t page_colors = 0;
static uint_t page_colors_mask = 0;
static uint_t page_colors_max8k = 0;
static uint_t vac_colors = 0;
static uint_t vac_colors_mask = 0;
static uint_t page_coloring_shift = 0;
int consistent_coloring;

/*
 * This variable is set by the cpu module to contain the lowest
 * address not affected by the SF_ERRATA_57 workaround.  It should
 * remain 0 if the workaround is not needed.
 */
#if defined(__sparcv9) && defined(SF_ERRATA_57)
caddr_t errata57_limit;
#endif

extern int vac_size;
extern int vac_shift;

extern void page_relocate_hash(page_t *, page_t *);

/*
 * Convert page frame number to an OBMEM page frame number
 * (i.e. put in the type bits -- zero for this implementation)
 */
pfn_t
impl_obmem_pfnum(pfn_t pf)
{
	return (pf);
}

/*
 * Use physmax to determine the highest physical page of DRAM memory
 * It is assumed that any physical addresses above physmax is in IO space.
 * We don't bother checking the low end because we assume that memory space
 * begins at physical page frame 0.
 *
 * Return 1 if the page frame is onboard DRAM memory, else 0.
 * Returns 0 for nvram so it won't be cached.
 */
int
pf_is_memory(pfn_t pf)
{
	/* We must be IO space */
	if (pf > physmax)
		return (0);

	/* We must be memory space */
	return (1);
}

/*
 * Handle a pagefault.
 */
faultcode_t
pagefault(caddr_t addr, enum fault_type type, enum seg_rw rw, int iskernel)
{
	struct as *as;
	struct proc *p;
	faultcode_t res;
	caddr_t base;
	size_t len;
	int err;
#if !defined(__sparcv9)
	int mapped_red;
#endif

	if (INVALID_VADDR(addr))
		return (FC_NOMAP);

#if !defined(__sparcv9)
	/*
	 * On the 64-bit kernel, we don't even try
	 * to map in the redzone - partly because we
	 * have a ton of kernel address space, but
	 * mostly because we *want* the protection of the
	 * redzone at this point in the development of
	 * the system.
	 */
	mapped_red = segkp_map_red();
#endif

	if (iskernel) {
		as = &kas;
	} else {
		p = curproc;
		as = p->p_as;
#if defined(__sparcv9) && defined(SF_ERRATA_57)
		/*
		 * Prevent infinite loops due to a segment driver
		 * setting the execute permissions and the sfmmu hat
		 * silently ignoring them.
		 */
		if (rw == S_EXEC && AS_TYPE_64BIT(as) &&
		    addr < errata57_limit) {
			res = FC_NOMAP;
			goto out;
		}
#endif
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
		base = (caddr_t)(p->p_usrstack - p->p_stksize);
		len = p->p_stksize;
		if (addr < base || addr >= p->p_usrstack) {	/* stack seg? */
			/* not in either UNIX data or stack segments */
			res = FC_NOMAP;
			goto out;
		}
	}

	/* the rest of this function implements a 3.X 4.X 5.X compatibility */
	/* This code is probably not needed anymore */

	/* expand the gap to the page boundaries on each side */
	len = (((uintptr_t)base + len + PAGEOFFSET) & PAGEMASK) -
	    ((uintptr_t)base & PAGEMASK);
	base = (caddr_t)((uintptr_t)base & PAGEMASK);

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
#if !defined(__sparcv9)
	if (mapped_red)
		segkp_unmap_red();
#endif

	return (res);
}

/*
 * This is the routine which defines the address limit implied
 * by the flag '_MAP_LOW32'.  USERLIMIT32 matches the highest
 * mappable address in a 32-bit process on this platform (though
 * perhaps we should make it be UINT32_MAX here?)
 */
void
map_addr(caddr_t *addrp, size_t len, offset_t off, int align, uint_t flags)
{
	struct proc *p = curproc;
	caddr_t userlimit = flags & _MAP_LOW32 ?
		(caddr_t)USERLIMIT32 : p->p_as->a_userlimit;
	map_addr_proc(addrp, len, off, align, userlimit, p);
}

/*
 * map_addr_proc() is the routine called when the system is to
 * choose an address for the user.  We will pick an address
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
/*ARGSUSED4*/
void
map_addr_proc(caddr_t *addrp, size_t len, offset_t off, int align,
    caddr_t userlimit, struct proc *p)
{
	struct as *as = p->p_as;
	caddr_t addr;
	caddr_t base;
	size_t slen;
	uintptr_t align_amount;

	base = p->p_brkbase;
	if (userlimit < as->a_userlimit) {
		/*
		 * This happens when a program wants to map something in
		 * a range that's accessible to a program in a smaller
		 * address space.  For example, a 64-bit program might
		 * be calling mmap32(2) to guarantee that the returned
		 * address is below 4Gbytes.
		 */
		ASSERT(userlimit > base);
		slen = userlimit - base;
	} else
		slen = p->p_usrstack - base
		    - (((rlim_t)P_CURLIMIT(p, RLIMIT_STACK) + PAGEOFFSET) &
		    PAGEMASK);
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
	if (len >= MMU_PAGESIZE4M) {  /* 4MB mappings */
		align_amount = MMU_PAGESIZE4M;
	} else if (len >= MMU_PAGESIZE512K) { /* 512KB mappings */
		align_amount = MMU_PAGESIZE512K;
	} else if (len >= MMU_PAGESIZE64K) { /* 64KB mappings */
		align_amount = MMU_PAGESIZE64K;
	} else  {
		/*
		 * Align virtual addresses on a 64K boundary to ensure
		 * that ELF shared libraries are mapped with the appropriate
		 * alignment constraints by the run-time linker.
		 */
		align_amount = ELF_SPARC_MAXPGSZ;
	}

	/*
	 * 64-bit processes require 1024K alignment of ELF shared libraries.
	 */
	if (p->p_model == DATAMODEL_LP64)
		align_amount = MAX(align_amount, ELF_SPARCV9_MAXPGSZ);

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
	if (as_gap(as, len, &base, &slen, AH_HI, NULL) == 0) {
		caddr_t as_addr;

		addr = base + slen - len + PAGESIZE;
		as_addr = addr;
		/*
		 * Round address DOWN to the alignment amount,
		 * add the offset, and if this address is less
		 * than the original address, add alignment amount.
		 */
		addr = (caddr_t)((uintptr_t)addr & (~(align_amount - 1l)));
		addr += (long)(off & (align_amount - 1l));
		if (addr < as_addr)
			addr += align_amount;

		ASSERT(addr <= (as_addr + align_amount));
		ASSERT(((uintptr_t)addr & (align_amount - 1l)) ==
		    ((uintptr_t)(off & (align_amount - 1l))));
		*addrp = addr;

#if defined(__sparcv9) && defined(SF_ERRATA_57)
		if (AS_TYPE_64BIT(as) && addr < errata57_limit)
			*addrp = NULL;
#endif
	} else {
		*addrp = NULL;	/* no more virtual space */
	}
}

#ifdef __sparcv9
/*
 * Some V9 CPUs have holes in the middle of the 64-bit virtual address range.
 */
caddr_t	hole_start, hole_end;
#endif

/*
 * Determine whether [base, base+len] contains a mapable range of
 * addresses at least minlen long. base and len are adjusted if
 * required to provide a mapable range.
 */
/* ARGSUSED */
int
valid_va_range(caddr_t *basep, size_t *lenp, size_t minlen, int dir)
{
	caddr_t hi, lo;

	lo = *basep;
	hi = lo + *lenp;

	/*
	 * If hi rolled over the top, try cutting back.
	 */
	if (hi < lo) {
		size_t newlen = 0 - (uintptr_t)lo - 1l;

		if (newlen + (uintptr_t)hi < minlen)
			return (0);
		if (newlen < minlen)
			return (0);
		*lenp = newlen;
	} else if (hi - lo < minlen)
		return (0);

#ifdef __sparcv9

	/*
	 * Deal with a possible hole in the address range between
	 * hole_start and hole_end that should never be mapped by the MMU.
	 */
	hi = lo + *lenp;

	if (lo < hole_start) {
		if (hi > hole_start)
			if (hi < hole_end)
				hi = hole_start;
			else
				/* lo < hole_start && hi >= hole_end */
				if (dir == AH_LO) {
					/*
					 * prefer lowest range
					 */
					if (hole_start - lo >= minlen)
						hi = hole_start;
					else if (hi - hole_end >= minlen)
						lo = hole_end;
					else
						return (0);
				} else {
					/*
					 * prefer highest range
					 */
					if (hi - hole_end >= minlen)
						lo = hole_end;
					else if (hole_start - lo >= minlen)
						hi = hole_start;
					else
						return (0);
				}
	} else {
		/* lo >= hole_start */
		if (hi < hole_end)
			return (0);
		if (lo < hole_end)
			lo = hole_end;
	}

	if (hi - lo < minlen)
		return (0);

	*basep = lo;
	*lenp = hi - lo;
#endif

	return (1);
}

/*
 * Determine whether [addr, addr+len] with protections `prot' are valid
 * for a user address space.
 */
/*ARGSUSED*/
int
valid_usr_range(caddr_t addr, size_t len, uint_t prot, struct as *as,
    caddr_t userlimit)
{
	caddr_t eaddr = addr + len;

	if (eaddr <= addr || addr >= userlimit || eaddr > userlimit)
		return (RANGE_BADADDR);

#ifdef __sparcv9
	/*
	 * Determine if the address range falls within an illegal
	 * range of the MMU.
	 */
	if (eaddr > hole_start && addr < hole_end)
		return (RANGE_BADADDR);

#if defined(SF_ERRATA_57)
	/*
	 * Make sure USERLIMIT isn't raised too high
	 */
	ASSERT64(addr <= (caddr_t)0xffffffff80000000ul ||
	    errata57_limit == 0);

	if (AS_TYPE_64BIT(as) &&
	    (addr < errata57_limit) &&
	    (prot & PROT_EXEC))
		return (RANGE_BADPROT);
#endif /* SF_ERRATA57 */
#endif /* __sparcv9 */
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
	 * DATA_ALIGN == 2 * PAGESIZE
	 */
	if (exp->a_text)
		return ((caddr_t)(roundup(USRTEXT + exp->a_text, DATA_ALIGN)));
	else
		return ((caddr_t)USRTEXT);
}

/*
 * Return the starting disk address for the data segment.
 */
ulong_t
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
uint_t
gettfile(struct exec *exp)
{
	if (exp->a_magic == ZMAGIC)
		return (0);
	else
		return (sizeof (struct exec));
}

void
getexinfo(
	struct exdata *edp_in,
	struct exdata *edp_out,
	int *pagetext,
	int *pagedata)
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
 * Array of page sizes.
 */
typedef struct hw_pagesize {
	size_t	size;
	size_t	shift;
} hw_pagesize_t;

static hw_pagesize_t hw_page_array[] = {
	{MMU_PAGESIZE,		MMU_PAGESHIFT},
	{MMU_PAGESIZE64K,	MMU_PAGESHIFT64K},
	{MMU_PAGESIZE512K,	MMU_PAGESHIFT512K},
	{MMU_PAGESIZE4M,	MMU_PAGESHIFT4M},
	{0, 0}
};

uint_t
page_num_pagesizes(void)
{
	return (MMU_PAGE_SIZES);
}

size_t
page_get_pagesize(uint_t n)
{
	if (n >= MMU_PAGE_SIZES)
		cmn_err(CE_PANIC, "page_get_pagesize: out of range %d", n);
	return (hw_page_array[n].size);
}

static size_t
page_get_shift(uint_t n)
{
	if (n >= MMU_PAGE_SIZES)
		cmn_err(CE_PANIC, "page_get_pagesize: out of range %d", n);
	return (hw_page_array[n].shift);
}

uint_t
page_get_pagecolors(uint_t szc) {

	ASSERT(page_colors != 0);
	return (page_colors >> (3 * szc));
}

#define	PNUM_SIZE(size_code)						\
	(hw_page_array[size_code].size >> hw_page_array[0].shift)

/*
 * Anchored in the table below are counters used to keep track
 * of free contiguous physical memory. Each element of the table contains
 * the array of counters, the size of array which is allocated during
 * startup based on physmax and a shift value used to convert a pagenum
 * into a counter array index or vice versa. The table has page size
 * for rows and region size for columns:
 *
 *	page_counters[page_size][region_size]
 *
 *	page_size: 	TTE size code of pages on page_size freelist.
 *
 *	region_size:	TTE size code of a candidate larger page made up
 *			made up of contiguous free page_size pages.
 *
 * As you go across a page_size row increasing region_size each
 * element keeps track of how many (region_size - 1) size groups
 * made up of page_size free pages can be coalesced into a
 * regsion_size page. Yuck! Lets try an example:
 *
 * 	page_counters[1][3] is the table element used for identifying
 *	candidate 4M pages from contiguous pages off the 64K free list.
 *	Each index in the page_counters[1][3].array spans 4M. Its the
 *	number of free 512K size (regsion_size - 1) groups of contiguous
 *	64K free pages.	So when page_counters[1][3].counters[n] == 8
 *	we know we have a candidate 4M page made up of 512K size groups
 *	of 64K free pages.
 *
 * NOTE:It may seem that basing the size of the array on physmax would
 *	waste memory depending how the memory banks are populated.
 *	On our large server machines this is not true.
 *	At power on physical memory is moved around such that it starts
 *	at page zero and there are no holes.
 */
typedef struct HW_PAGE_MAP {
	char	*counters;	/* counter array */
	size_t	size;		/* size of array above */
	int	shift;		/* shift for pnum/array index conversion */
} hw_page_map_t;

static hw_page_map_t
page_counters[MAX_MEM_NODES][MMU_PAGE_SIZES][MMU_PAGE_SIZES];

#define	PAGE_COUNTERS(mnode, pg_sz, rg_sz, idx)				\
	(page_counters[mnode][pg_sz][rg_sz].counters[idx])

#define	PAGE_COUNTERS_SHIFT(mnode, pg_sz, rg_sz) 			\
	(page_counters[mnode][pg_sz][rg_sz].shift)

#define	PAGE_COUNTERS_SIZE(mnode, pg_sz, rg_sz) 			\
	(page_counters[mnode][pg_sz][rg_sz].size)

#define	PNUM_TO_IDX(p_sz, region_sz, pnum)				\
	(PFN_2_MEM_NODE_OFF(pnum) >>					\
		PAGE_COUNTERS_SHIFT(PFN_2_MEM_NODE(pnum),		\
		p_sz, region_sz))

#define	IDX_TO_PNUM(mnode, p_sz, region_sz, index) 			\
	MEM_NODE_2_PFN(mnode, (index <<					\
		PAGE_COUNTERS_SHIFT(mnode, p_sz, region_sz)))

/*
 * For sfmmu each larger page is 8 times the size of the previous
 * size page.
 */
#define	FULL_REGION_CNT(p_sz, r_sz)	(8)


/*
 * Per page size free lists. Allocated dynamically.
 */
page_t **page_freelists[MAX_MEM_NODES][MMU_PAGE_SIZES][MAX_MEM_TYPES];

/*
 * Counters for number of free pages for each size
 * free list.
 */
#ifdef DEBUG

uint_t	page_freemem[MAX_MEM_NODES][MMU_PAGE_SIZES];

#define	FREEMEM_INC(mnode, sz)						\
	(atomic_add_32(&page_freemem[mnode][sz], 1))
#define	FREEMEM_DEC(mnode, sz)						\
	(atomic_add_32(&page_freemem[mnode][sz], -1))

#else

#define	FREEMEM_INC(mnode, sz)	{}
#define	FREEMEM_DEC(mnode, sz)	{}

#endif /* DEBUG */

/*
 * For now there is only a single size cache list.
 * Allocated dynamically.
 */
page_t **page_cachelists[MAX_MEM_NODES][MAX_MEM_TYPES];

kmutex_t fpc_mutex[MAX_MEM_NODES][NPC_MUTEX];
kmutex_t cpc_mutex[MAX_MEM_NODES][NPC_MUTEX];


/*
 * We can only allow a single thread to update a counter within
 * a 4M region of physical memory. That is the finest granularity
 * possible since the counter values are dependent on each other
 * as you move accross region sizes.
 */
static kmutex_t	ctr_mutex[MAX_MEM_NODES][NPC_MUTEX];

#define	PP_CTR_MUTEX(pp)						\
	&ctr_mutex[PP_2_MEM_NODE(pp)]					\
		[((pp->p_pagenum >> (MMU_PAGESHIFT4M - MMU_PAGESHIFT))	\
			& (NPC_MUTEX-1))]

/*
 * Called by startup().
 * Size up the per page size free list counters based on physmax
 * of each node.
 */
size_t
page_ctrs_sz(void)
{
	pgcnt_t npgs;
	int	p, r;
	int	mnode;
	uint_t	ctrs_sz = 0;

	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++) {
		npgs = mem_node_config[mnode].physmax;

		for (p = 0; p < MMU_PAGE_SIZES; p++) {
			for (r = p + 1; r < MMU_PAGE_SIZES; r++) {
				if (mem_node_config[mnode].exists) {
					PAGE_COUNTERS_SHIFT(mnode, p, r) =
					    TTE_BSZS_SHIFT(r);
					PAGE_COUNTERS_SIZE(mnode, p, r) =
					    (npgs >> PAGE_COUNTERS_SHIFT(
					    mnode, p, r)) + 1;
					ctrs_sz +=
					    PAGE_COUNTERS_SIZE(mnode, p, r);
				} else {
					page_counters[mnode][p][r].counters =
					    NULL;
					/*
					 * Paranoid. If some mem_node does not
					 * exist but there's a bug that
					 * references its page we want to help
					 * panic right away. Shift of pfn by
					 * 31 right should give an index of 0
					 * and crash on NULL pointer.
					 */
					/* TODO: 64bit: (sizeof(x)*NBBY)-1 */
					PAGE_COUNTERS_SHIFT(mnode, p, r) = 31;
					PAGE_COUNTERS_SIZE(mnode, p, r) = 0;
				}
			}
		}
	}

	/*
	 * add some slop for roundups. page_ctrs_alloc will roundup the start
	 * address of the counters to ecache_linesize boundary for every
	 * memory node.
	 */
	return (ctrs_sz + MAX_MEM_NODES * ecache_linesize);
}

caddr_t
page_ctrs_alloc(caddr_t alloc_base)
{

	int	mnode;
	int	p, r;

	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++) {

		if (mem_node_config[mnode].exists == 0)
			continue;

		for (p = 0; p < MMU_PAGE_SIZES; p++) {
			for (r = p + 1; r < MMU_PAGE_SIZES; r++) {
				page_counters[mnode][p][r].counters =
					(caddr_t)alloc_base;
				alloc_base += page_counters[mnode][p][r].size;
			}
		}

		alloc_base = (caddr_t)roundup((uintptr_t)alloc_base,
			ecache_linesize);
	}

	return (alloc_base);
}

caddr_t
ndata_alloc_page_freelists(caddr_t alloc_base)
{
	int	mnode, mtype;
	int	psz;

	/*
	 * Note that adding CPUs may change the ecache_size so
	 * we must not be reliant on ecache_size not changing.
	 */
	page_colors_max8k = MAX(MMU_PAGESIZE4M, ecache_size) / MMU_PAGESIZE;

	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++) {
		if (mem_node_config[mnode].exists == 0) {
			for (mtype = 0; mtype < MAX_MEM_TYPES; mtype++) {
				page_cachelists[mnode][mtype] = NULL;
				for (psz  = 0; psz < MMU_PAGE_SIZES; psz++)
					page_freelists[mnode][psz][mtype] =
									NULL;
			}
		} else {
			/*
			 * We only support small pages in the cachelist.
			 */
			for (mtype = 0; mtype < MAX_MEM_TYPES; mtype++) {
				page_cachelists[mnode][mtype] =
							(page_t **)alloc_base;
				alloc_base += sizeof (machpage_t *) *
						page_colors_max8k;
				/*
				 * Allocate freelists bins for all
				 * supported page sizes.
				 */
				for (psz  = 0; psz < MMU_PAGE_SIZES; psz++) {
					page_freelists[mnode][psz][mtype] =
							(page_t **)alloc_base;
					alloc_base += sizeof (machpage_t *) *
							(page_colors_max8k >>
								(3 * psz));
				}
			}
			alloc_base = (caddr_t)roundup((uintptr_t)alloc_base,
				ecache_linesize);
		}
	}
	return (alloc_base);
}

/*
 * Local functions prototypes.
 */

static	void page_ctr_add(machpage_t *);
static	void page_ctr_sub(machpage_t *);
static	uint_t page_convert_color(uchar_t, uchar_t, uint_t);
#ifdef DEBUG
static void CHK_LPG(machpage_t *, uchar_t);
#endif
static void page_freelist_lock(int);
static void page_freelist_unlock(int);
static machpage_t *page_cons_create(machpage_t *);
static int page_promote(pfn_t, uchar_t, uchar_t);
static void page_demote(pfn_t, uchar_t, uchar_t);
static int page_freelist_fill(uchar_t, int, int, int);
static page_t *page_get_mnode_freelist(uint_t, uchar_t, uint_t, int);
static page_t *page_get_mnode_cachelist(uint_t, uint_t, int);


/*
 * Functions to adjust region counters for each size free list.
 */
static void
page_ctr_add(machpage_t *pp)
{
	kmutex_t	*lock = PP_CTR_MUTEX(pp);
	ssize_t		p, r, idx;
	pfn_t		pfnum;
	int		mnode = PP_2_MEM_NODE(pp);

	ASSERT(pp->p_cons >= 0 && pp->p_cons < MMU_PAGE_SIZES);

	/*
	 * p is the current page size.
	 * r is the region size.
	 */
	p = pp->p_cons;
	r = p + 1;
	pfnum = pp->p_pagenum;

	FREEMEM_INC(mnode, p);

	/*
	 * Increment the count of free pages for the current
	 * region. Continue looping up in region size incrementing
	 * count if the preceeding region is full.
	 */
	mutex_enter(lock);
	while (r < MMU_PAGE_SIZES) {
		idx = PNUM_TO_IDX(p, r, pfnum);

		ASSERT(PAGE_COUNTERS(mnode, p, r, idx) < FULL_REGION_CNT(p, r));

		if (++PAGE_COUNTERS(mnode, p, r, idx) !=
			FULL_REGION_CNT(p, r)) {
			break;
		}
		r++;
	}
	mutex_exit(lock);
}

static void
page_ctr_sub(machpage_t *pp)
{
	kmutex_t	*lock = PP_CTR_MUTEX(pp);
	ssize_t		p, r, idx;
	pfn_t		pfnum;
	int		mnode = PP_2_MEM_NODE(pp);

	ASSERT(pp->p_cons >= 0 && pp->p_cons < MMU_PAGE_SIZES);

	/*
	 * p is the current page size.
	 * r is the region size.
	 */
	p = pp->p_cons;
	r = p + 1;
	pfnum = pp->p_pagenum;

	FREEMEM_DEC(mnode, p);

	/*
	 * Decrement the count of free pages for the current
	 * region. Continue looping up in region size decrementing
	 * count if the preceeding region was full.
	 */
	mutex_enter(lock);
	while (r < MMU_PAGE_SIZES) {
		idx = PNUM_TO_IDX(p, r, pfnum);

		ASSERT(PAGE_COUNTERS(mnode, p, r, idx) > 0);

		if (--PAGE_COUNTERS(mnode, p, r, idx) !=
			FULL_REGION_CNT(p, r) - 1) {
			break;
		}
		r++;
	}
	mutex_exit(lock);
}

/*
 * Adjust page counters if memory should suddenly be
 * plugged in.
 */
uint_t
page_ctrs_adjust(int mnode)
{
	pgcnt_t npgs;
	int	p, r;
	size_t	pcsz, old_csz;
	caddr_t	new_ctr, old_ctr;

	npgs = roundup(mem_node_config[mnode].physmax, 8);

	for (p = 0; p < MMU_PAGE_SIZES; p++) {
		for (r = p + 1; r < MMU_PAGE_SIZES; r++) {
			PAGE_COUNTERS_SHIFT(mnode, p, r) = TTE_BSZS_SHIFT(r);
			pcsz = (npgs >> PAGE_COUNTERS_SHIFT(mnode, p, r)) + 1;

			if (pcsz > PAGE_COUNTERS_SIZE(mnode, p, r)) {
				old_ctr = page_counters[mnode][p][r].counters;
				old_csz = PAGE_COUNTERS_SIZE(mnode, p, r);

				new_ctr = kmem_zalloc(pcsz, KM_NOSLEEP);
				if (new_ctr == NULL)
					return (ENOMEM);
				page_freelist_lock(mnode);
				bcopy(page_counters[mnode][p][r].counters,
				    new_ctr, PAGE_COUNTERS_SIZE(mnode, p, r));
				page_counters[mnode][p][r].counters = new_ctr;
				PAGE_COUNTERS_SIZE(mnode, p, r) = pcsz;
				page_freelist_unlock(mnode);

				if (old_ctr > kernelheap)
					kmem_free(old_ctr, old_csz);
			}
		}
	}

	return (0);
}

/*
 * Function to get an ecache color bin: F(as, cnt, vcolor).
 * the goal of this function is to:
 * - to spread a processes' physical pages across the entire ecache to
 *	maximize its use.
 * - to minimize vac flushes caused when we reuse a physical page on a
 *	different vac color than it was previously used.
 * - to prevent all processes to use the same exact colors and trash each
 *	other.
 *
 * cnt is a bin ptr kept on a per as basis.  As we page_create we increment
 * the ptr so we spread out the physical pages to cover the entire ecache.
 * The virtual color is made a subset of the physical color in order to
 * in minimize virtual cache flushing.
 * We add in the as to spread out different as.  This happens when we
 * initialize the start count value.
 * sizeof(struct as) is 60 so we shift by 3 to get into the bit range
 * that will tend to change.  For example, on spitfire based machines
 * (vcshft == 1) contigous as are spread bu ~6 bins.
 * vcshft provides for proper virtual color alignment.
 * In theory cnt should be updated using cas only but if we are off by one
 * or 2 it is no big deal.
 * We also keep a start value which is used to randomize on what bin we
 * start counting when it is time to start another loop. This avoids
 * contigous allocations of ecache size to point to the same bin.
 * Why 3? Seems work ok. Better than 7 or anything larger.
 */
#define	PGCLR_LOOPFACTOR 3


/*
 * AS_2_BIN macro controls the page coloring policy.
 * 0 (default) uses various vaddr bits
 * 1 virtual=paddr
 * 2 bin hopping
 */
#define	AS_2_BIN(as, cnt, addr, bin)					\
	switch (consistent_coloring) {					\
	default:							\
		cmn_err(CE_WARN,					\
			"AS_2_BIN: bad consistent coloring value");	\
		/* assume default algorithm -> continue */		\
	case 0:								\
		bin = (((uintptr_t)addr >> MMU_PAGESHIFT) +		\
			(((uintptr_t)addr >> page_coloring_shift) <<	\
			(vac_shift - MMU_PAGESHIFT))) 			\
			& page_colors_mask;				\
		break;							\
	case 1:								\
		bin = ((uintptr_t)addr >> MMU_PAGESHIFT) &		\
			page_colors_mask;				\
		break;							\
	case 2: 							\
		cnt = as_color_bin(as);					\
		/* make sure physical color aligns with vac color */	\
		while ((cnt & vac_colors_mask) !=			\
		    addr_to_vcolor(addr)) {				\
			cnt++;						\
		}							\
		bin = cnt = cnt & page_colors_mask;			\
		/* update per as page coloring fields */		\
		cnt = (cnt + 1) & page_colors_mask;			\
		if (cnt == (as_color_start(as) & page_colors_mask)) {	\
			cnt = as_color_start(as) = as_color_start(as) +	\
				PGCLR_LOOPFACTOR; 			\
		}							\
		as_color_bin(as) = cnt & page_colors_mask;		\
		break;							\
	}								\
	ASSERT(bin <= page_colors_mask);

/* ARGSUSED */
uint_t
get_color_start(struct as *as)
{
	/* approximates a random number by reading tick register */
	/* LINTED */
	return ((uint_t)(((gettick()) << (vac_shift - MMU_PAGESHIFT)) &
	    page_colors_mask));
}

static uint_t
page_convert_color(uchar_t cur_szc, uchar_t new_szc, uint_t color)
{
	size_t shift;

	if (cur_szc > new_szc) {
		shift = page_get_shift(cur_szc) - page_get_shift(new_szc);
		return (color << shift);
	} else if (cur_szc < new_szc) {
		shift = page_get_shift(new_szc) - page_get_shift(cur_szc);
		return (color >> shift);
	}
	return (color);
}

#ifdef DEBUG

static void
CHK_LPG(machpage_t *pp, uchar_t szc)
{
	spgcnt_t npgs = page_get_pagesize(pp->p_cons) >> MMU_PAGESHIFT;
	uint_t noreloc;

	if (npgs == 1) {
		ASSERT(pp->p_cons == 0);
		ASSERT(PP2MACHPP(pp->genp_next) == pp);
		ASSERT(PP2MACHPP(pp->genp_prev) == pp);
		return;
	}

	ASSERT(PP2MACHPP(pp->genp_vpnext) == pp || pp->genp_vpnext == NULL);
	ASSERT(PP2MACHPP(pp->genp_vpprev) == pp || pp->genp_vpprev == NULL);

	ASSERT(IS_P2ALIGNED((pp->p_pagenum << MMU_PAGESHIFT),
	    page_get_pagesize(pp->p_cons)));
	ASSERT(pp->p_pagenum == (PP2MACHPP(pp->genp_next)->p_pagenum - 1));
	ASSERT(PP2MACHPP(pp->genp_prev)->p_pagenum ==
	    (pp->p_pagenum + (npgs - 1)));

	/*
	 * Check list of pages.
	 */
	noreloc = PP_ISNORELOC(MACHPP2PP(pp));
	while (npgs--) {
		if (npgs != 0) {
			ASSERT(pp->p_pagenum ==
			    PP2MACHPP(pp->genp_next)->p_pagenum - 1);
		}
		ASSERT(pp->p_cons == szc);
		ASSERT(PP_ISFREE(MACHPP2PP(pp)));
		ASSERT(PP_ISAGED(MACHPP2PP(pp)));
		ASSERT(PP2MACHPP(pp->genp_vpnext) == pp ||
		    pp->genp_vpnext == NULL);
		ASSERT(PP2MACHPP(pp->genp_vpprev) == pp ||
		    pp->genp_vpprev == NULL);
		ASSERT(pp->genp_vnode  == NULL);
		ASSERT(PP_ISNORELOC(MACHPP2PP(pp)) == noreloc);

		pp = PP2MACHPP(pp->genp_next);
	}
}

#else /* !DEBUG */

#define	CHK_LPG(pp, szc)	{}

#endif /* DEBUG */

static void
page_freelist_lock(int mnode)
{
	int i;
	for (i = 0; i < NPC_MUTEX; i++) {
		mutex_enter(&fpc_mutex[mnode][i]);
		mutex_enter(&cpc_mutex[mnode][i]);
	}
}

static void
page_freelist_unlock(int mnode)
{
	int i;
	for (i = 0; i < NPC_MUTEX; i++) {
		mutex_exit(&fpc_mutex[mnode][i]);
		mutex_exit(&cpc_mutex[mnode][i]);
	}
}

/*
 * Manages freed up constituent pages. The upper layers
 * are going to free the pages up one PAGESIZE page
 * at a time. We wait until we see all the constituent
 * pages for the large page then gather them up and free
 * the entire large page at once.
 *
 * RFE: When we have the new page_create_ru and truly
 *	big pages we can delete this code.
 */

static 	kmutex_t conslist_mutex;
static 	page_t *page_conslists[MMU_PAGE_SIZES]; /* staging list */
int	cons_sema;	/* counting semaphore to trigger */
			/* calls to page_cons_create 	*/

#define	PAGE_CONLISTS(szc) (page_conslists[szc])
#define	PFN_BASE(pfnum, szc)  (pfnum & ~((1 << (szc * 3)) - 1))
#define	CONS_LPFN(pfnum, szc)					\
	(PFN_BASE(pfnum, szc) != PFN_BASE((pfnum + 1), szc))

#ifdef DEBUG
static uint_t page_cons_creates[MAX_MEM_NODES][MMU_PAGE_SIZES];
#endif

/*
 * Called with last_pp when caller believes that all the constituent
 * pages of a large page are free on the constituent page list and
 * can be grouped back toghther and placed on the large page free list.
 *
 * Caller must protect constituent list. Returns large page
 * on success or NULL on failure.
 */
machpage_t *
page_cons_create(machpage_t *last_pp)
{
	page_t	*plist = NULL, *pp;
	size_t  npgs;
	pfn_t   pfnum;
	uint_t	szc;
	int	found_noreloc = 0;

	szc	= last_pp->p_cons;
	pfnum	= PFN_BASE(last_pp->p_pagenum, szc);
	npgs 	= page_get_pagesize(last_pp->p_cons) >> MMU_PAGESHIFT;

#ifdef DEBUG
	atomic_add_32(&page_cons_creates[PFN_2_MEM_NODE(pfnum)][szc], 1);
#endif
	/*
	 * Gather up all the constituent pages to be linked
	 * together and placed back on the large page freelist.
	 * The common case is they are all here.
	 * But while gathering them if we find that one is missing
	 * then stop and place them all back on the constituent
	 * list to try again later.
	 */
	while (npgs--) {

		pp = page_numtopp_nolock(pfnum++);
		ASSERT(pp != NULL);

		if (PP_ISNORELOC(pp))
			found_noreloc = 1;

		if (PP2MACHPP(pp)->p_conslist == 0) {

			/*
			 * There're not all free.
			 * Put em back and try again
			 * later.
			 */
			while (plist) {
				pp = plist;
				mach_page_sub(&plist, pp);
				mach_page_add(
				    &PAGE_CONLISTS(PP2MACHPP(pp)->p_cons), pp);
			}
			return (NULL);
		}

		ASSERT(PP_ISFREE(pp));
		ASSERT(PP2MACHPP(pp)->p_mapping == NULL);
		ASSERT(PP2MACHPP(pp)->genp_vnode == NULL);
		ASSERT(PP2MACHPP(pp)->p_cons == szc);

		mach_page_sub(&PAGE_CONLISTS(PP2MACHPP(pp)->p_cons), pp);
		page_list_concat(&plist, &pp);
	}

	/*
	 * If we found at least one NORELOC page, then
	 * we have to set the NORELOC bits for them all.
	 */
	if (found_noreloc && ((pp = plist) != NULL)) {
		do {
			PP_SETNORELOC(pp);
		} while ((pp = page_list_next(pp)) != plist);
	}

	return (PP2MACHPP(plist));
}

static int lgpg_reclaims;	/* protected by conslist_mutex */

void
page_list_add(int list, page_t *gen_pp, int where)
{
	machpage_t	*our_pp = PP2MACHPP(gen_pp), *pplist = NULL;
	page_t		**ppp;
	kmutex_t	*pcm;
	machpage_t	*tpp;
	size_t		npgs;
	uint_t		bin, mnode, mtype;

	ASSERT(PAGE_EXCL(&our_pp->p_paget));
	ASSERT(PP_ISFREE(MACHPP2PP(our_pp)));
	ASSERT(our_pp->p_mapping == NULL);

	/*
	 * PAGESIZE case.
	 *
	 * Don't need to lock the freelist first here
	 * because the page isn't on the freelist yet.
	 * This means p_cons can't change on us.
	 */
	if (our_pp->p_cons == 0) {

		bin = PP_2_BIN(our_pp);
		mnode = PP_2_MEM_NODE(our_pp);
		pcm = PC_BIN_MUTEX(mnode, bin, list);

		mtype = PP_2_MTYPE(our_pp);

		if (list == PG_FREE_LIST) {
			ASSERT(PP_ISAGED(MACHPP2PP(our_pp)));
			ppp = &PAGE_FREELISTS(mnode, 0, bin, mtype);
		} else {
			ASSERT(our_pp->genp_vnode);
			ASSERT((our_pp->genp_offset & PAGEOFFSET) == 0);
			ppp = &PAGE_CACHELISTS(mnode, bin, mtype);
		}

		mutex_enter(pcm);
		page_add(ppp, MACHPP2PP(our_pp));

		if (where == PG_LIST_TAIL) {
			*ppp = (*ppp)->p_next;
		}

		/*
		 * Add counters before releasing
		 * pcm mutex to avoid a race with
		 * page_freelist_coalesce and
		 * page_freelist_fill.
		 */
		page_ctr_add(our_pp);
		mutex_exit(pcm);
		if (PP_ISNORELOC(MACHPP2PP(our_pp)))
			kcage_freemem_add(1);

		/*
		 * It is up to the caller to unlock the page!
		 */
		ASSERT(PAGE_EXCL(&our_pp->p_paget));
		return;
	}

	/*
	 * Large page case.
	 *
	 * This is a constituent page of a larger page so
	 * we place it on a temporary staging list until we
	 * can free them all up in one chunk. Hopefully this
	 * will go away someday when we have true big pages.
	 *
	 * Most of the time the pages will be freed in
	 * order and all I have to do is try a coalesce
	 * them when I see the last constituent page of the
	 * large page. The problem is that it's possible the
	 * pager can get ahead of me and trigger the freeing
	 * of the last constituent page before all others have
	 * been freed. For this reason I have a counting
	 * semaphore to indicate I now have or have seen a last
	 * constituent page. So I only have to check when
	 * semaphore > 0. A counting semaphore also handles the
	 * case where two or more threads are freeing a large
	 * page and the pager gets ahead of both of them. This
	 * shouldn't be expensive since its rare the pager will
	 * get ahead of us.
	 */
	mutex_enter(&conslist_mutex);
	our_pp->p_conslist = 1;
	page_add(&PAGE_CONLISTS(our_pp->p_cons), MACHPP2PP(our_pp));
	if (CONS_LPFN(our_pp->p_pagenum, our_pp->p_cons)) {
		cons_sema++;
	}

	if (cons_sema == 0) {
		mutex_exit(&conslist_mutex);
		return;
	}

	pplist = page_cons_create(our_pp);
	if (pplist) {
		ssize_t	npgs_saved;
		/*
		 * The big page is finally free.
		 */
		cons_sema--;

		tpp = pplist;
		npgs = page_get_pagesize(our_pp->p_cons) >> MMU_PAGESHIFT;
		npgs_saved = (ssize_t)npgs;
		while (npgs--) {
			tpp->p_conslist = 0;
			tpp = PP2MACHPP(MACHPP2PP(tpp)->p_next);
		}

		CHK_LPG(pplist, our_pp->p_cons);

		bin = PP_2_BIN(pplist);
		mnode = PP_2_MEM_NODE(our_pp);
		pcm = PC_BIN_MUTEX(mnode, bin, PG_FREE_LIST);
		mutex_enter(pcm);
		mtype = PP_2_MTYPE(our_pp);
		page_vpadd(&PAGE_FREELISTS(mnode, our_pp->p_cons, bin, mtype),
			MACHPP2PP(pplist));

		/*
		 * Add counters before releasing
		 * pcm mutex to avoid a race with
		 * page_freelist_coalesce and
		 * page_freelist_fill.
		 */
		page_ctr_add(pplist);
		mutex_exit(pcm);
		if (mtype == MTYPE_NORELOC)
			kcage_freemem_add(npgs_saved);

		/*
		 * If there were 1 or more PAGESIZE pages
		 * waiting to be reclaimed. Wake up
		 * any thread blocked waiting for the reclaim
		 * in page_list_sub. These threads would
		 * be sleeping on the base constituent page
		 * of the large page.
		 */
		if (lgpg_reclaims) {
			cv_broadcast(&MACHPP2PP(pplist)->p_cv);
		}
	}
	mutex_exit(&conslist_mutex);
}

/*
 * Take a particular page off of whatever freelist the page
 * is claimed to be on.
 *
 * NOTE: Only used for PAGESIZE pages.
 */
void
page_list_sub(int list, page_t *gen_pp)
{
	machpage_t	*pp = PP2MACHPP(gen_pp);
	int		bin;
	uint_t		mnode, mtype;
	kmutex_t	*pcm;
	page_t		**ppp;

	ASSERT(PAGE_EXCL(&pp->p_paget));
	ASSERT(PP_ISFREE(MACHPP2PP(pp)));

	/*
	 * The p_cons field can only be changed by page_promote()
	 * and page_demote(). Only free pages can be promoted and
	 * demoted and the free list MUST be locked during these
	 * operations. So to prevent a race in page_list_sub()
	 * between computing which bin of the freelist lock to
	 * grab and actually grabing the lock we check again that
	 * the bin we locked is still the correct one. Notice that
	 * the p_cons field could have actually changed on us but
	 * if the bin happens to still be the same we are safe.
	 */
try_again:
	bin = PP_2_BIN(pp);
	mnode = PP_2_MEM_NODE(pp);
	pcm = PC_BIN_MUTEX(mnode, bin, list);
	mutex_enter(pcm);
	if (PP_2_BIN(pp) != bin) {
		mutex_exit(pcm);
		goto	try_again;
	}
	mtype = PP_2_MTYPE(pp);

	if (list == PG_FREE_LIST) {
		ASSERT(PP_ISAGED(MACHPP2PP(pp)));
		ppp = &PAGE_FREELISTS(mnode, pp->p_cons, bin, mtype);
	} else {
		ASSERT(!PP_ISAGED(MACHPP2PP(pp)));
		ppp = &PAGE_CACHELISTS(mnode, bin, mtype);
		ASSERT(PP_ISAGED(MACHPP2PP(pp)) == 0);
	}

	/*
	 * Common PAGESIZE case.
	 *
	 * Note that we locked the freelist. This prevents
	 * any page promotion/demotion operations. Therefore
	 * the p_cons will not change until we drop pcm mutex.
	 */
	if (pp->p_cons == 0) {

		/*
		 * Don't need to grab to conslist_mutex since this
		 * is not a constituent page.
		 */
		ASSERT(pp->p_conslist == 0);
		page_sub(ppp, MACHPP2PP(pp));
		/*
		 * Subtract counters before releasing pcm mutex
		 * to avoid race with page_freelist_coalesce.
		 */
		page_ctr_sub(pp);
		mutex_exit(pcm);
		if (PP_ISNORELOC(MACHPP2PP(pp)))
			kcage_freemem_sub(1);
		return;
	}

	/*
	 * Large pages on the cache list are not supported.
	 */
	if (list != PG_FREE_LIST) {
		cmn_err(CE_PANIC, "page_list_sub: large page on cachelist");
	}

	/*
	 * Slow but rare.
	 *
	 * Somebody wants this paticular page which is part
	 * of a large page. Probably page_numtopp(), Grrrh!!
	 * In this case we just demote the page if its on the
	 * freelist. If its in the process of still being freed
	 * then life gets messy. We'll just leave the page locked
	 * SE_EXCL and block until the entire large page has
	 * been freed and we have been unblocked by page_list_add().
	 *
	 * We have to drop pcm before locking the entire freelist.
	 * Once we have re-locked the freelist check to make sure
	 * the page hasn't already been demoted or completely
	 * freed.
	 */
	mutex_exit(pcm);
	mutex_enter(&conslist_mutex);
	page_freelist_lock(mnode);
	if (pp->p_cons != 0) {
		if (pp->p_conslist == 0) {

			/*
			 * Large page is on freelist.
			 */
			page_demote(PFN_BASE(pp->p_pagenum, pp->p_cons),
				pp->p_cons, 0);
		} else {

			/*
			 * Page is not completely freed yet.  We'll
			 * block on the base constituent page of the
			 * large page until the entire large page has
			 * been freed. When this happens we'll be
			 * unblocked in page_list_add().
			 */
			page_t	*base_pp;
			page_freelist_unlock(mnode);

			base_pp = page_numtopp_nolock(
			    PFN_BASE(pp->p_pagenum, pp->p_cons));

			lgpg_reclaims++;
			cv_wait(&base_pp->p_cv, &conslist_mutex);
			lgpg_reclaims--;
			mutex_exit(&conslist_mutex);

			ASSERT(PAGE_EXCL(&pp->p_paget));
			ASSERT(PP_ISFREE(MACHPP2PP(pp)));
			ASSERT(PP_ISAGED(MACHPP2PP(pp)));
			goto try_again;
		}
	}
	mutex_exit(&conslist_mutex);

	ASSERT(PP_ISFREE(MACHPP2PP(pp)));
	ASSERT(PP_ISAGED(MACHPP2PP(pp)));
	ASSERT(pp->p_cons == 0);
	ASSERT(pp->p_conslist == 0);

	/*
	 * Subtract counters before releasing pcm mutex
	 * to avoid race with page_freelist_coalesce.
	 */
	bin = PP_2_BIN(pp);
	mtype = PP_2_MTYPE(pp);
	ppp = &PAGE_FREELISTS(mnode, pp->p_cons, bin, mtype);

	page_sub(ppp, MACHPP2PP(pp));
	page_ctr_sub(pp);
	page_freelist_unlock(mnode);

	if (mtype == MTYPE_NORELOC)
		kcage_freemem_sub(1);

}

/*
 * Add the page to the front of a linked list of pages
 * using the p_next & p_prev pointers for the list.
 * The caller is responsible for protecting the list pointers.
 */
void
mach_page_add(page_t **ppp, page_t *pp)
{
	if (*ppp == NULL) {
		pp->p_next = pp->p_prev = pp;
	} else {
		pp->p_next = *ppp;
		pp->p_prev = (*ppp)->p_prev;
		(*ppp)->p_prev = pp;
		pp->p_prev->p_next = pp;
	}
	*ppp = pp;
}

/*
 * Remove this page from a linked list of pages
 * using the p_next & p_prev pointers for the list.
 *
 * The caller is responsible for protecting the list pointers.
 */
void
mach_page_sub(page_t **ppp, page_t *pp)
{
	ASSERT(PP_ISFREE(pp));

	if (*ppp == NULL || pp == NULL)
		cmn_err(CE_PANIC, "mach_page_sub");

	if (*ppp == pp)
		*ppp = pp->p_next;		/* go to next page */

	if (*ppp == pp)
		*ppp = NULL;			/* page list is gone */
	else {
		pp->p_prev->p_next = pp->p_next;
		pp->p_next->p_prev = pp->p_prev;
	}
	pp->p_prev = pp->p_next = pp;		/* make pp a list of one */
}


static uint_t page_promote_err;
static uint_t page_promote_noreloc_err;

/*
 * Create a single larger page from smaller contiguous pages.
 * Pages involved are on the freelist before and after operation.
 * The caller is responsible for locking the freelist.
 * Returns 1 on success, 0 on failure.
 *
 * RFE: For performance pass in pp instead of pfnum so
 * 	we can avoid excessive calls to page_numtopp_nolock().
 *	This would depend on an assumption that all contiguous
 *	pages are in the same memseg so we can just add/dec
 *	our pp.
 *
 * Lock ordering:
 *
 *	There is a potential but rare deadlock situation
 *	for page promotion and demotion operations. The problem
 *	is there are two paths into the freelist manager and
 *	they have different lock orders:
 *
 *	page_create()
 *		lock freelist
 *		page_lock(EXCL)
 *		unlock freelist
 *		return
 *		caller drops page_lock
 *
 *	page_free() and page_reclaim()
 *		caller grabs page_lock(EXCL)
 *
 *		lock freelist
 *		unlock freelist
 *		drop page_lock
 *
 *	What prevents a thread in page_creat() from deadlocking
 *	with a thread freeing or reclaiming the same page is the
 *	page_trylock() in page_get_freelist(). If the trylock fails
 *	it skips the page.
 *
 *	The lock ordering for promotion and demotion is the same as
 *	for page_create(). Since the same deadlock could occur during
 *	page promotion and freeing or reclaiming of a page on the
 *	cache list we might have to fail the operation and undo what
 *	have done so far. Again this is rare.
 */
static int
page_promote(pfn_t pfnum, uchar_t cur_szc, uchar_t new_szc)
{
	machpage_t	*pp, *pplist, *tpp;
	size_t		cur_npgs, new_npgs, npgs, coalesces, bin;
	int		mnode, mtype;
	ulong_t		index;
	kmutex_t	*phm;
	pfn_t		save_pfnum;
	uint_t		noreloc;

	ASSERT(new_szc > cur_szc);

	/*
	 * Number of pages per constituent page.
	 */
	cur_npgs = btop(page_get_pagesize(cur_szc));
	new_npgs = btop(page_get_pagesize(new_szc));

	/*
	 * Loop through smaller pages to confirm that all pages
	 * give the same result for PP_ISNORELOC().
	 * We can check this reliably here as the protocol for setting
	 * P_NORELOC requires pages to be taken off the free list first.
	 */
	coalesces = new_npgs / cur_npgs;
	save_pfnum = pfnum;
	while (coalesces--) {

		pp = PP2MACHPP(page_numtopp_nolock(pfnum));
		pfnum += PNUM_SIZE(cur_szc);
		ASSERT(pp != NULL);
		ASSERT(PP_ISFREE(MACHPP2PP(pp)));

		if (pp->p_pagenum == save_pfnum) {
			/* First page, set requirement. */
			noreloc = PP_ISNORELOC(MACHPP2PP(pp));
		} else {
			if (noreloc != PP_ISNORELOC(MACHPP2PP(pp))) {
				page_promote_noreloc_err++;
				page_promote_err++;
				return (0);
			}
		}
	}
	pfnum = save_pfnum;

	/*
	 * Loop around coalescing the smaller pages
	 * into a big page.
	 */
	coalesces = new_npgs / cur_npgs;
	pplist = NULL;
	while (coalesces--) {

		pp = PP2MACHPP(page_numtopp_nolock(pfnum));
		pfnum += PNUM_SIZE(cur_szc);
		ASSERT(pp != NULL);
		ASSERT(PP_ISFREE(MACHPP2PP(pp)));

		/*
		 * Remove from the freelist.
		 */
		bin = PP_2_BIN(pp);
		mnode = PP_2_MEM_NODE(pp);
		mtype = PP_2_MTYPE(pp);
		if (PP_ISAGED(MACHPP2PP(pp))) {

			/*
			 * PG_FREE_LIST
			 */
			if (pp->p_cons) {
				page_vpsub(&PAGE_FREELISTS(mnode, cur_szc,
								bin, mtype),
					MACHPP2PP(pp));
			} else {
				ASSERT(cur_szc == 0);
				mach_page_sub(&PAGE_FREELISTS(mnode, 0,
								bin, mtype),
					MACHPP2PP(pp));
			}
		} else {
			ASSERT(pp->p_cons == 0);
			ASSERT(cur_szc == 0);

			/*
			 * PG_CACHE_LIST
			 *
			 * Since this page comes from the
			 * cachelist, we must destory the
			 * vnode association.
			 */
			if (!page_trylock(MACHPP2PP(pp), SE_EXCL)) {
				goto fail_promote;
			}

			/*
			 * We need to be careful not to deadlock
			 * with another thread in page_lookup().
			 * The page_lookup() thread could be holding
			 * the same phm that we need if the two
			 * pages happen to hash to the same phm lock.
			 * At this point we have locked the entire
			 * freelist and page_lookup() could be trying
			 * to grab a freelist lock.
			 */
			index = PAGE_HASH_FUNC(MACHPP2PP(pp)->p_vnode,
			    MACHPP2PP(pp)->p_offset);
			phm = PAGE_HASH_MUTEX(index);
			if (!mutex_tryenter(phm)) {
				page_unlock(MACHPP2PP(pp));
				goto fail_promote;
			}

			mach_page_sub(&PAGE_CACHELISTS(mnode, bin, mtype),
					MACHPP2PP(pp));
			page_hashout(MACHPP2PP(pp), phm);
			mutex_exit(phm);
			PP_SETAGED(MACHPP2PP(pp));
			page_unlock(MACHPP2PP(pp));
		}
		CHK_LPG(pp, cur_szc);
		page_ctr_sub(pp);

		ASSERT(pp->p_cons == cur_szc);

		/*
		 * Concatenate the smaller page(s) onto
		 * the large page list.
		 */
		npgs = cur_npgs;
		tpp = pp;
		while (npgs--) {
			ASSERT(tpp->p_cons == cur_szc);
			tpp->p_cons = new_szc;
			tpp = PP2MACHPP(tpp->genp_next);
		}
		page_list_concat((page_t **)&pplist, (page_t **)&pp);
	}

	/*
	 * Now place the new large page on the freelist.
	 */
	CHK_LPG(pplist, new_szc);

	bin = PP_2_BIN(pplist);
	mnode = PP_2_MEM_NODE(pplist);
	mtype = PP_2_MTYPE(pplist);
	page_vpadd(&PAGE_FREELISTS(mnode, new_szc, bin, mtype),
			MACHPP2PP(pplist));

	page_ctr_add(pplist);
	return (1);

fail_promote:
	/*
	 * A thread must have still been freeing or
	 * reclaiming the page on the cachelist.
	 * To prevent a deadlock undo what we have
	 * done sofar and return failure. This
	 * situation can only happen while promoting
	 * PAGESIZE pages.
	 */
	page_promote_err++;
	ASSERT(cur_szc == 0);
	while (pplist) {
		pp = pplist;
		mach_page_sub((page_t **)&pplist, MACHPP2PP(pp));
		pp->p_cons = 0;
		bin = PP_2_BIN(pp);
		mtype = PP_2_MTYPE(pp);
		mach_page_add(&PAGE_FREELISTS(mnode, 0, bin, mtype),
				MACHPP2PP(pp));
		page_ctr_add(pp);
	}
	return (0);
}

/*
 * Break up a large page into smaller size pages.
 * Pages involved are on the freelist before and after operation.
 * The caller is responsible for locking.
 */
static void
page_demote(pfn_t pfnum, uchar_t cur_szc, uchar_t new_szc)
{
	machpage_t	*pp, *pplist, *npplist;
	size_t	npgs, bin, n;
	int	mnode, mtype;

	ASSERT(cur_szc != 0);
	ASSERT(new_szc < cur_szc);

	pplist = PP2MACHPP(page_numtopp_nolock(pfnum));
	ASSERT(pplist != NULL);

	ASSERT(pplist->p_cons == cur_szc);

	bin = PP_2_BIN(pplist);
	mnode = PP_2_MEM_NODE(pplist);
	mtype = PP_2_MTYPE(pplist);
	page_vpsub(&PAGE_FREELISTS(mnode, cur_szc, bin, mtype),
			MACHPP2PP(pplist));

	CHK_LPG(pplist, cur_szc);
	page_ctr_sub(pplist);

	/*
	 * Number of PAGESIZE pages for smaller new_szc
	 * page.
	 */
	npgs = btop(page_get_pagesize(new_szc));

	while (pplist) {
		pp = pplist;

		ASSERT(pp->p_cons == cur_szc);

		/*
		 * We either break it up into PAGESIZE pages or larger.
		 */
		if (npgs == 1) {	/* PAGESIZE case */
			mach_page_sub((page_t **)&pplist, MACHPP2PP(pp));
			ASSERT(pp->p_cons == cur_szc);
			ASSERT(new_szc == 0);
			pp->p_cons = new_szc;
			bin = PP_2_BIN(pp);
			mnode = PP_2_MEM_NODE(pp);
			mtype = PP_2_MTYPE(pp);
			mach_page_add(&PAGE_FREELISTS(mnode, 0, bin, mtype),
				MACHPP2PP(pp));
			page_ctr_add(pp);
		} else {

			/*
			 * Break down into smaller lists of pages.
			 */
			page_list_break((page_t **)&pplist, (page_t **)&npplist,
				npgs);

			pp = pplist;
			n = npgs;
			while (n--) {
				ASSERT(pp->p_cons == cur_szc);
				pp->p_cons = new_szc;
				pp = PP2MACHPP(pp->genp_next);
			}

			CHK_LPG(pplist, new_szc);

			bin = PP_2_BIN(pplist);
			mnode = PP_2_MEM_NODE(pp);
			mtype = PP_2_MTYPE(pp);
			page_vpadd(&PAGE_FREELISTS(mnode, new_szc, bin, mtype),
				MACHPP2PP(pplist));

			page_ctr_add(pplist);
			pplist = npplist;
		}
	}
}

/*
 * Coalesce as many contiguous pages into the largest
 * possible supported page sizes.
 */
void
page_freelist_coalesce(int mnode)
{
	int 	p, r, idx, full;
	pfn_t pfnum;
	size_t len;

	/*
	 * Lock the entire freelist and coalesce what we can.
	 *
	 * I choose to always promote to the largest page possible
	 * first to reduce the number of page promotions.
	 *
	 * RFE: For performance maybe we can do something less
	 *	brutal than locking the entire freelist. So far
	 * 	this doesn't seem to be a performance problem?
	 */
	page_freelist_lock(mnode);
	for (p = 0; p < MMU_PAGE_SIZES; p++) {
		for (r = MMU_PAGE_SIZES - 1; r > p; r--) {

			full = FULL_REGION_CNT(p, r);
			len  = PAGE_COUNTERS_SIZE(mnode, p, r);

			for (idx = 0; idx < len; idx++) {
				if (PAGE_COUNTERS(mnode, p, r, idx) == full) {
					pfnum = IDX_TO_PNUM(mnode, p, r, idx);
					(void) page_promote(pfnum, p, r);
				}
			}
		}
	}
	page_freelist_unlock(mnode);
}

/*
 * This is where all polices for moving pages around
 * to different page size free lists is implemented.
 * Returns 1 on success, 0 on failure.
 *
 * So far these are the priorities for this algorithm in descending
 * order:
 *
 *	1) Minimize fragmentation
 *		 At startup place as many pages on the
 *		 4M free list as possible. (done in startup)
 *
 *	2) When servicing a request try to do so with a free page
 *	   from next size up. Helps defer fragmentation as long
 *	   as possible.
 *
 *	3) Page coalesce on demand. Only when a freelist
 *	   larger than PAGESIZE is empty and step 2
 *	   will not work since all larger size lists are
 *	   also empty.
 */
static int
page_freelist_fill(uchar_t size, int color, int mnode, int flags)
{
	uchar_t next_size = size + 1;
	int 	bin;
	int	mtype;
	machpage_t *pp;

	ASSERT(size < MMU_PAGE_SIZES);

	mtype = (flags & PG_NORELOC) ? MTYPE_NORELOC : MTYPE_RELOC;
	/*
	 * First try to break up a larger page to fill
	 * current size freelist.
	 */
	while (next_size < MMU_PAGE_SIZES) {
		/*
		 * If page found then demote it.
		 */
		bin = page_convert_color(size, next_size, color);
		if (PP2MACHPP(PAGE_FREELISTS(mnode, next_size, bin, mtype))) {
			page_freelist_lock(mnode);
			pp = PP2MACHPP(PAGE_FREELISTS(mnode, next_size,
					bin, mtype));
			if (pp) {
				ASSERT(pp->p_cons == next_size);
				page_demote(pp->p_pagenum, pp->p_cons, size);
				page_freelist_unlock(mnode);
				return (1);
			}
			page_freelist_unlock(mnode);
		}
		next_size++;
	}

	/*
	 * Ok that didn't work. Time to coalesce.
	 */
	if (size != 0) {
		page_freelist_coalesce(mnode);
	}

	if (PAGE_FREELISTS(mnode, size, color, mtype)) {
		return (1);
	} else {
		return (0);
	}
}

/*
 * Internal PG_ flags.
 * PGI_RELOCONLY acts in the opposite sense to PG_NORELOC.
 * PGI_NOCAGE indicates Cage is disabled.
 */
#define	PGI_RELOCONLY	0x10000
#define	PGI_NOCAGE	0x20000

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
#define	BIN_STEP	20

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
	struct as *as = seg->s_as;
	page_t	*pp;
	ulong_t		bin;
	uchar_t		szc;
	int	cpu_mnode = CPUID_2_MEM_NODE(CPU->cpu_id);
	int	mnode;
	int		colorcnt;

	if (!kcage_on) {
		flags &= ~PG_NORELOC;
		flags |= PGI_NOCAGE;
	}

	/*
	 * Convert size to page size code. For now
	 * we only allow two sizes to be used.
	 */
	switch (size) {
	case MMU_PAGESIZE:
		szc = 0;
		break;

	case MMU_PAGESIZE64K:
		return (NULL);

	case MMU_PAGESIZE512K:
		return (NULL);

	case MMU_PAGESIZE4M:
		szc = 3;
		break;

	default:
		cmn_err(CE_PANIC,
		    "page_get_freelist: illegal page size request");
	}

	/* LINTED */
	AS_2_BIN(as, colorcnt, vaddr, bin);

	/*
	 * AS_2_BIN() gave us an 8k color. Might need to convert it.
	 */
	if (szc) {
		bin = page_convert_color(0, szc, bin);
	}

	/*
	 * Try local memory node first.
	 */
	ASSERT(mem_node_config[cpu_mnode].exists == 1);
	pp = page_get_mnode_freelist(bin, szc, flags, cpu_mnode);
	/* LINTED */
	if ((MAX_MEM_NODES == 1) || pp || (flags & PG_MATCH_COLOR))
		return (pp);

	/*
	 * Try local cachelist before remote freelist for small pages.
	 * Don't need to do it for larger ones cause page_freelist_coalesce()
	 * already failed there anyway.
	 */
	if (size == MMU_PAGESIZE) {
		pp = page_get_mnode_cachelist(bin, flags, cpu_mnode);
		if (pp) {
			page_hashout(pp, NULL);
			PP_SETAGED(pp);
			return (pp);
		}
	}

	/* Now try remote freelists */
	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++) {
		if ((mnode == cpu_mnode) ||
			(mem_node_config[mnode].exists == 0))
			continue;

		pp = page_get_mnode_freelist(bin, szc, flags, mnode);
		if (pp)
			return (pp);
	}

	return (NULL);
}

/*
 * Helper routine used only by the freelist code to lock
 * a page. If the page is a large page then it succeeds in
 * locking all the constituent pages or none at all.
 * Returns 1 on sucess, 0 on failure.
 */
static int
mach_page_trylock(page_t *pp, se_t se) {

	page_t	*tpp, *first_pp = pp;

	/*
	 * Fail if can't lock first or only page.
	 */
	if (!page_trylock(pp, se)) {
		return (0);
	}

	/*
	 * PAGESIZE: common case.
	 */
	if (PP2MACHPP(pp)->p_cons == 0) {
		return (1);
	}

	/*
	 * Large page case.
	 */
	tpp = pp->p_next;
	while (tpp != pp) {
		if (!page_trylock(tpp, se)) {
			/*
			 * On failure unlock what we
			 * have locked so far.
			 */
			while (first_pp != tpp) {
				page_unlock(first_pp);
				first_pp = first_pp->p_next;
			}
			return (0);
		}
		tpp = tpp->p_next;
	}
	return (1);
}

/*ARGSUSED*/
static page_t *
page_get_mnode_freelist(
	uint_t bin,
	uchar_t szc,
	uint_t flags,
	int mnode)
{
	kmutex_t	*pcm;
	int		i, fill_tried, fill_marker;
	int		mtype;
	page_t		*pp, *first_pp;
	uint_t		bin_marker;
	int		colors;
	uchar_t		nszc;
	size_t		nszc_color_shift;

	/*
	 * Set how many physical colors for this page size.
	 */
	colors = page_convert_color(0, szc, page_colors - 1) + 1;
	nszc = MIN(szc + 1, MMU_PAGE_SIZES - 1);
	nszc_color_shift = hw_page_array[nszc].shift - hw_page_array[szc].shift;

	ASSERT(colors <= page_colors);
	ASSERT(colors);
	ASSERT((colors & (colors - 1)) == 0);

	ASSERT(bin < colors);

	mtype = (flags & PG_NORELOC) ? MTYPE_NORELOC : MTYPE_RELOC;

	/*
	 * Only hold one freelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 */
big_try_again:
	fill_tried = 0;
	for (i = 0; i <= colors; i++) {
try_again:
		ASSERT(bin < colors);
		if (PAGE_FREELISTS(mnode, szc, bin, mtype)) {
			pcm = PC_BIN_MUTEX(mnode, bin, PG_FREE_LIST);
			mutex_enter(pcm);
			pp = PAGE_FREELISTS(mnode, szc, bin, mtype);
			if (pp != NULL) {
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
				ASSERT(PP2MACHPP(pp)->p_cons == szc);
				ASSERT(PFN_2_MEM_NODE(
					PP2MACHPP(pp)->p_pagenum) == mnode);

				/*
				 * Walk down the hash chain.
				 * 8k pages are linked on p_next
				 * and p_prev fields. Large pages
				 * are a contiguous group of
				 * constituent pages linked together
				 * on their p_next and p_prev fields.
				 * The large pages are linked together
				 * on the hash chain using p_vpnext
				 * p_vpprev of the base constituent
				 * page of each large page.
				 */
				first_pp = pp;
				while (!mach_page_trylock(pp, SE_EXCL)) {
					if (szc == 0) {
						pp = pp->p_next;
					} else {
						pp = pp->p_vpnext;
					}

					ASSERT(PP_ISFREE(pp));
					ASSERT(PP_ISAGED(pp));
					ASSERT(pp->p_vnode == NULL);
					ASSERT(pp->p_hash == NULL);
					ASSERT(pp->p_offset == (u_offset_t)-1);
					ASSERT(PP2MACHPP(pp)->p_cons == szc);
					ASSERT(PFN_2_MEM_NODE(
						PP2MACHPP(pp)->p_pagenum) ==
							mnode);

					if (pp == first_pp) {
						pp = NULL;
						break;
					}
				}

				if (pp) {
					mtype = PP_2_MTYPE(PP2MACHPP(pp));
					ASSERT(PP2MACHPP(pp)->p_cons == szc);
					if (szc == 0) {
						page_sub(&PAGE_FREELISTS(mnode,
							szc, bin, mtype), pp);
					} else {
						page_vpsub(&PAGE_FREELISTS(
							mnode, szc, bin,
							mtype), pp);
						CHK_LPG(PP2MACHPP(pp), szc);
					}
					page_ctr_sub(PP2MACHPP(pp));

					if ((PP_ISFREE(pp) == 0) ||
					    (PP_ISAGED(pp) == 0)) {
						cmn_err(CE_PANIC,
						    "free page is not. pp %p",
						    (void *)pp);
					}
					mutex_exit(pcm);
					ASSERT(!kcage_on ||
					    (flags & PG_NORELOC) == 0 ||
					    PP_ISNORELOC(pp));
					if (PP_ISNORELOC(pp)) {
						ssize_t	npgs;

						npgs = (ssize_t)
							page_get_pagesize(szc)
							>> MMU_PAGESHIFT;
						kcage_freemem_sub(npgs);
					}
					return (pp);
				}
			}
			mutex_exit(pcm);
		}

		/*
		 * Wow! The bin was empty. First try and satisfy
		 * the request by breaking up or coalescing pages from
		 * a different size freelist of the correct color that
		 * satisfies the ORIGINAL color requested. If that
		 * fails then try pages of the same size but different
		 * colors assuming we are not called with
		 * PG_MATCH_COLOR.
		 */
		if (!fill_tried) {
			fill_tried = 1;
			fill_marker = bin >> nszc_color_shift;
			if (page_freelist_fill(szc, bin, mnode, flags)) {
				goto try_again;
			}
		}

		if (flags & PG_MATCH_COLOR) {
			break;
		}

		/*
		 * Select next color bin to try.
		 */
		if (szc == 0) {
			/*
			 * PAGESIZE page case.
			 */
			if (i == 0) {
				bin = (bin + BIN_STEP) & page_colors_mask;
				bin_marker = bin;
			} else {
				bin = (bin +  vac_colors) & page_colors_mask;
				if (bin == bin_marker) {
					bin = (bin + 1) & page_colors_mask;
					bin_marker = bin;
				}
			}
		} else {
			/*
			 * Large page case.
			 */
			bin = (bin + 1) & (colors - 1);
		}
		/*
		 * If bin advanced to the next color bin of the
		 * next larger pagesize, there is a chance the fill
		 * could succeed.
		 */
		if (fill_marker != (bin >> nszc_color_shift))
			fill_tried = 0;
	}

	if (!(flags & (PG_NORELOC | PGI_NOCAGE | PGI_RELOCONLY)) &&
		(kcage_freemem >= kcage_lotsfree)) {
		/*
		 * The Cage is ON and with plenty of free mem, and
		 * we're willing to check for a NORELOC page if we
		 * couldn't find a RELOC page, so spin again.
		 */
		flags |= PG_NORELOC;
		mtype = MTYPE_NORELOC;
		goto big_try_again;
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
 * Finds a pages, trys to lock it, then removes it.
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
	page_t	*pp;
	struct as *as = seg->s_as;
	ulong_t	bin;
	int	cpu_mnode = CPUID_2_MEM_NODE(CPU->cpu_id);
	int	mnode;
	int	colorcnt;

	/* LINTED */
	AS_2_BIN(as, colorcnt, vaddr, bin);
	ASSERT(bin <= page_colors_mask);

	if (!kcage_on) {
		flags &= ~PG_NORELOC;
		flags |= PGI_NOCAGE;
	}

	/*
	 * Try local memory node first.
	 */
	ASSERT(mem_node_config[cpu_mnode].exists == 1);
	pp = page_get_mnode_cachelist(bin, flags, cpu_mnode);
	/* LINTED */
	if ((MAX_MEM_NODES == 1) || pp || (flags & PG_MATCH_COLOR))
		return (pp);

	/* Now try remote cachelists */
	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++) {
		if ((mnode == cpu_mnode) ||
			(mem_node_config[mnode].exists == 0))
			continue;

		pp = page_get_mnode_cachelist(bin, flags, mnode);
		if (pp)
			return (pp);
	}

	return (NULL);

}

/*ARGSUSED*/
static page_t *
page_get_mnode_cachelist(
	uint_t bin,
	uint_t flags,
	int mnode)
{
	kmutex_t	*pcm;
	int		i;
	int		mtype;
	page_t		*pp;
	page_t		*first_pp;
	uint_t		bin_marker;

	/*
	 * Only hold one cachelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 */

	mtype = (flags & PG_NORELOC) ? MTYPE_NORELOC : MTYPE_RELOC;

big_try_again:
	for (i = 0; i <= page_colors; i++) {
		if (PAGE_CACHELISTS(mnode, bin, mtype)) {
			pcm = PC_BIN_MUTEX(mnode, bin, PG_CACHE_LIST);
			mutex_enter(pcm);
			pp = PAGE_CACHELISTS(mnode, bin, mtype);
			if (pp != NULL) {
				first_pp = pp;
				ASSERT(pp->p_vnode);
				ASSERT(PP_ISAGED(pp) == 0);
				ASSERT(PP2MACHPP(pp)->p_cons == 0);
				ASSERT(PFN_2_MEM_NODE(
					PP2MACHPP(pp)->p_pagenum) == mnode);
				while (!page_trylock(pp, SE_EXCL)) {
					pp = pp->p_next;
					ASSERT(PP2MACHPP(pp)->p_cons == 0);
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
					ASSERT(PFN_2_MEM_NODE(
						PP2MACHPP(pp)->p_pagenum) ==
							mnode);
				}

				if (pp) {
					page_t	**ppp;
					/*
					 * Found and locked a page.
					 * Pull it off the list.
					 */
					mtype = PP_2_MTYPE(PP2MACHPP(pp));
					ppp = &PAGE_CACHELISTS(mnode, bin,
								mtype);
					page_sub(ppp, pp);
					/*
					 * Subtract counters before releasing
					 * pcm mutex to avoid a race with
					 * page_freelist_coalesce and
					 * page_freelist_fill.
					 */
					page_ctr_sub(PP2MACHPP(pp));
					mutex_exit(pcm);
					ASSERT(pp->p_vnode);
					ASSERT(PP_ISAGED(pp) == 0);
					ASSERT(!kcage_on ||
					    (flags & PG_NORELOC) == 0 ||
					    PP_ISNORELOC(pp));
					if (PP_ISNORELOC(pp))
						kcage_freemem_sub(1);
					return (pp);
				}
			}
			mutex_exit(pcm);
		}
		/*
		 * Wow! The bin was empty or no page could be locked.
		 * If only the proper bin is to be checked, get out
		 * now.
		 */
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		if (i == 0) {
			bin = (bin + BIN_STEP) & page_colors_mask;
			bin_marker = bin;
		} else {
			bin = (bin +  vac_colors) & page_colors_mask;
			if (bin == bin_marker) {
				bin = (bin + 1) & page_colors_mask;
				bin_marker = bin;
			}
		}
	}

	if (!(flags & (PG_NORELOC | PGI_NOCAGE | PGI_RELOCONLY)) &&
		(kcage_freemem >= kcage_lotsfree)) {
		/*
		 * The Cage is ON and with plenty of free mem, and
		 * we're willing to check for a NORELOC page if we
		 * couldn't find a RELOC page, so spin again.
		 */
		flags |= PG_NORELOC;
		mtype = MTYPE_NORELOC;
		goto big_try_again;
	}

	return (NULL);
}

#ifdef DEBUG
#define	REPL_PAGE_STATS
#endif /* DEBUG */

#ifdef REPL_PAGE_STATS
struct repl_page_stats {
	uint_t	ngets;
	uint_t	ngets_noreloc;
	uint_t	nnopage_first;
	uint_t	nnopage;
	uint_t	nhashout;
	uint_t	nnofree;
	uint_t	nnext_machpp;
} repl_page_stats;
#define	REPL_STAT_INCR(v)	atomic_add_32(&repl_page_stats.v, 1)
#else /* REPL_PAGE_STATS */
#define	REPL_STAT_INCR(v)
#endif /* REPL_PAGE_STATS */

/*
 * The freemem accounting must be done by the caller.
 * First we try to get a replacement page of the same size as like_pp,
 * if that is not possible, then we just get a set of discontiguous
 * PAGESIZE pages.
 */
page_t *
page_get_replacement_page(page_t *orig_like_pp)
{
#define	mach_like_pp	PP2MACHPP(like_pp)
	page_t		*like_pp;
	page_t		*pp, *pplist;
	page_t		*pl = NULL;
	ulong_t		bin;
	uint_t		page_mnode, mnode;
	int		l_szc, szc;
	int		flags;
	int		first, found;
	spgcnt_t	npgs, pg_cnt;
	size_t		pgsz;
	pfn_t		pfnum;

	REPL_STAT_INCR(ngets);
	like_pp = orig_like_pp;
#ifdef REPL_PAGE_STATS
	if (PP_ISNORELOC(like_pp))
		REPL_STAT_INCR(ngets_noreloc);
#endif /* REPL_PAGE_STATS */

	ASSERT(PAGE_EXCL(like_pp));

	flags = PGI_RELOCONLY;
	first = 1;

	page_mnode = PP_2_MEM_NODE(mach_like_pp);
	l_szc = mach_like_pp->p_cons;
	pgsz = page_get_pagesize(l_szc);
	npgs = pgsz >> MMU_PAGESHIFT;
	/*
	 * Now we reset mach_like_pp to the base page_t.
	 * That way, we won't walk past then end of this 'szc' page.
	 */
	pfnum = PFN_BASE(mach_like_pp->p_pagenum, l_szc);
	like_pp = page_numtopp_nolock(pfnum);

	while (npgs) {
		szc = l_szc;
		pplist = NULL;
		pg_cnt = 0;
		while (szc >= 0) {
			pgsz = page_get_pagesize(szc);
			pg_cnt = pgsz >> MMU_PAGESHIFT;
			bin = mach_like_pp->p_pagenum & page_colors_mask;
			/*
			 * We may need to convert from an 8k color
			 * to a bin that corresponds to "szc".
			 */
			bin = page_convert_color(0, szc, bin);
			ASSERT(mach_like_pp->p_cons ==
				PP2MACHPP(orig_like_pp)->p_cons);
			if (pg_cnt <= npgs) {
				/*
				 * First we try the same mnode as
				 * like_pp.
				 */
				pplist = page_get_mnode_freelist(bin, szc,
					flags, page_mnode);
				if (pplist != NULL)
					break;
				REPL_STAT_INCR(nnofree);

				/*
				 * Try local cachelist before remote freelist
				 * for small pages. Don't need to do it for
				 * larger ones cause page_freelist_coalesce()
				 * already failed there anyway.
				 */
				if (pgsz == MMU_PAGESIZE) {
					pplist = page_get_mnode_cachelist(bin,
						flags, page_mnode);
					if (pplist) {
						page_hashout(pplist,
							(kmutex_t *)NULL);
						PP_SETAGED(pplist);
						REPL_STAT_INCR(nhashout);
						break;
					}
				}

				/* Now try remote freelists */
				for (mnode = 0; mnode < MAX_MEM_NODES;
					mnode++) {
					if ((mnode == page_mnode) ||
					(mem_node_config[mnode].exists == 0))
						continue;
					pplist = page_get_mnode_freelist(bin,
						szc, flags, mnode);
					if (pplist)
						break;
				}

				/*
				 * If a request for a page of size
				 * larger than PAGESIZE failed
				 * then don't try that size anymore.
				 */
				if (pplist == NULL) {
					l_szc = szc - 1;
				} else {
					break;
				}
			}
			szc--;
		}

		if (pplist != NULL) {
			found = 1;
		} else {
			found = 0;
		}
		/*
		 * pplist is a list of pg_cnt PAGESIZE pages.
		 * These pages are locked SE_EXCL since they
		 * came directly off the free list.
		 */
		while ((pplist != NULL) && (pg_cnt--)) {

			ASSERT(pplist != NULL);
			pp = pplist;
			page_sub(&pplist, pp);
			PP_CLRFREE(pp);
			PP_CLRAGED(pp);

			pagezero(pp, 0, PAGESIZE);
			hat_setrefmod(pp);
			/*
			 * We want to add these pages to the end
			 * of pl, therefore we have to adjust
			 * the value of pl on each iteration.
			 */
			page_add(&pl, pp);
			pl = pl->p_next;

			npgs--;
			/*
			 * Since the SE_EXCK lock is held on
			 * the the original like_pp, we don't have
			 * to worry about the "size" of the page
			 * changing, and we can assume that the
			 * constituent machpage_t's are virtually
			 * contiguous.
			 * Increment mach_like_pp so that we can have
			 * the correct bin on each iteration above.
			 */
			like_pp = MACHPP2PP(mach_like_pp + 1);
			REPL_STAT_INCR(nnext_machpp);
		}

		/*
		 * If we're making progress, keep trying.
		 */
		if (found)
			continue;

		if (first && npgs) {
			REPL_STAT_INCR(nnopage_first);
			first = 0;
			/*
			 * If not relocating out of the non-relocatable
			 * area (cage), try for anything.
			 */
			if (!PP_ISNORELOC(like_pp)) {
				flags &= ~PGI_RELOCONLY;
			}
		} else {
			break;
		}
	}

	if (npgs) {
		/*
		 * We were unable to allocate the necessary number
		 * of pages.
		 * We need to free up any pl.
		 */
		REPL_STAT_INCR(nnopage);
		while (pl != NULL) {
			/*
			* pp_targ is a linked list.
			*/
			pp = pl;
			page_sub(&pl, pp);

			PP_SETFREE(pp);
			PP_SETAGED(pp);
			hat_clrrefmod(pp);
			pp->p_offset = (u_offset_t)-1;
			page_list_add(PG_FREE_LIST, pp, PG_LIST_TAIL);
			page_unlock(pp);
		}
		return (NULL);
	} else {
		return (pl);
	}
}

/*
 * Helper routined used to lock all remaining members of a
 * large page. The caller is responsible for passing in a locked
 * pp. If pp is a large page, then it succeeds in locking all the
 * remaining constituent pages or it returns with only the
 * original page locked.
 * Returns 1 on success, 0 on failure.
 */
static int
group_page_trylock(page_t *pp, se_t se)
{
	page_t	*tpp;
	pgcnt_t	npgs;
	uint_t	szc;
	pfn_t	base_pfn, pfn;

	szc = PP2MACHPP(pp)->p_cons;
	npgs = page_get_pagesize(szc) >> MMU_PAGESHIFT;
	pfn = PP2MACHPP(pp)->p_pagenum;
	base_pfn = PFN_BASE(pfn, szc);
	pfn = base_pfn;

	while (npgs) {
		tpp = page_numtopp_nolock(pfn);
		if ((tpp != pp) && (!page_trylock(tpp, se))) {
			/* We failed to lock pfn so decrement by one. */
			pfn--;

			/*
			 * On failure, unlock what we have locked
			 * so far.
			 */
			while (pfn >= base_pfn) {
				tpp = page_numtopp_nolock(pfn);
				if (tpp != pp)
					page_unlock(tpp);
				pfn--;
			}
			return (0);
		}
		pfn++;
		npgs--;
	}
	return (1);
}

static void
group_page_unlock(page_t *pp)
{
	page_t	*tpp;
	pfn_t   pfn, base_pfn;
	uint_t	szc;
	pgcnt_t	npgs;

	szc = PP2MACHPP(pp)->p_cons;
	npgs = page_get_pagesize(szc) / PAGESIZE;
	pfn = PP2MACHPP(pp)->p_pagenum;
	base_pfn = PFN_BASE(pfn, szc);
	tpp = page_numtopp_nolock(base_pfn);

	while (npgs--) {
		if (tpp != pp)
			page_unlock(tpp);
		tpp = page_next(tpp);
	}
}
/*
 * return -1 if this routine is not supported.
 * or return the number of PAGESIZE pages that were relocated.
 * i.e. if it is supported, but fails, return zero.
 *
 * Return with all constituent members of target and replacement
 * SE_EXCL locked. It is the callers responsibility to drop the
 * locks.
 */
int
platform_page_relocate(
	page_t **target,
	page_t **replacement)
{
#ifdef DEBUG
	page_t *first_repl;
#endif /* DEBUG */
page_t *repl;
	page_t *targ;
	page_t *pl = NULL;
	uint_t ppattr;
	pfn_t   pfn;
	uint_t	szc;
	int	npgs, i;

	/*
	 * If this is not a base page,
	 * just return with 0x0 pages relocated.
	 */
	targ = *target;
	ASSERT(PAGE_EXCL(targ));
	szc = PP2MACHPP(targ)->p_cons;
	pfn = PP2MACHPP(targ)->p_pagenum;
	if (pfn != PFN_BASE(pfn, szc))
		return (0);

	if (!group_page_trylock(targ, SE_EXCL)) {
		return (0);
	}
	npgs = page_get_pagesize(PP2MACHPP(targ)->p_cons) >> MMU_PAGESHIFT;

	/*
	 * We must lock all members of this large page or we cannot
	 * relocate any part of it.
	 */
	if ((repl = *replacement) == NULL) {
		pgcnt_t dofree;

		dofree = npgs;		/* Size of target page in MMU pages */
		if (!page_create_wait(dofree, 0)) {
			group_page_unlock(targ);
			return (0);
		}
		repl = page_get_replacement_page(targ);
		if (repl == NULL) {
			group_page_unlock(targ);
			page_create_putback(dofree);
			return (0);
		}
	}
#ifdef DEBUG
	else {
		ASSERT(PAGE_LOCKED(repl));
	}
#endif /* DEBUG */

#ifdef DEBUG
	first_repl = repl;
#endif /* DEBUG */
	for (i = 0; i < npgs; i++) {
		ASSERT(PAGE_EXCL(targ));

		(void) hat_pageunload(targ, HAT_FORCE_PGUNLOAD);

		ASSERT(hat_page_getshare(targ) == 0);
		ASSERT(!PP_ISFREE(targ));
		ASSERT(PP2MACHPP(targ)->p_pagenum == (pfn + i));
		/*
		 * We can use the props routines here
		 * as we have the SE_EXCL lock on the page and
		 * no mappings. This avoids the HAT layer lock.
		 * There is no generic props routine for getting the props.
		 */
		/* Save props. */
		ppattr = hat_page_getattr(targ, (P_MOD | P_REF | P_RO));
		/* Copy contents. */
		ppcopy(targ, repl);
		/* Move the page's identity. */
		page_relocate_hash(repl, targ);
		/* Restore props. */
		page_clr_all_props(repl);
		page_set_props(repl, ppattr);

		ASSERT(hat_page_getshare(targ) == 0);
		ASSERT(hat_page_getshare(repl) == 0);
		/*
		 * Now clear the props on targ, after the
		 * page_relocate_hash(), they no longer
		 * have any meaning.
		 */
		page_clr_all_props(targ);
		page_add(&pl, targ);
		/*
		 * repl is a circular linked list of pages.
		 */
		repl = repl->p_next;
		targ = page_next(targ);
	}
	/* assert that we have come full circle with repl */
	ASSERT(first_repl == repl);

	*target = pl;
	if (*replacement == NULL)
		*replacement = repl;
	return (npgs);
}

/*
 * Called once at startup from kphysm_init() -- before memialloc()
 * is invoked to do the 1st page_free()/page_freelist_add().
 *
 * initializes page_colors and page_colors_mask
 * based on the cache size of the boot CPU.
 *
 * Also initializes the counter locks.
 */
void
page_coloring_init()
{
	int a;

	if (do_pg_coloring == 0) {
		page_colors = 1;
		return;
	}

	/*
	 * Note that adding CPUs may change the ecache_size so
	 * we must not be reliant on ecache_size not changing.
	 */
	page_colors = ecache_size / MMU_PAGESIZE;
	page_colors_mask = page_colors - 1;

	vac_colors = vac_size / MMU_PAGESIZE;
	vac_colors_mask = vac_colors -1;

	page_coloring_shift = 0;
	a = ecache_size;
	while (a >>= 1) {
		page_coloring_shift++;
	}
}

int
bp_color(struct buf *bp)
{
	int color = -1;

	if (vac) {
		if ((bp->b_flags & B_PAGEIO) != 0) {
			color = sfmmu_get_ppvcolor(PP2MACHPP(bp->b_pages));
		} else if (bp->b_un.b_addr != NULL) {
			color = sfmmu_get_addrvcolor(bp->b_un.b_addr);
		}
	}
	return (color < 0 ? 0 : ptob(color));
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
	sfmmu_cache_flushall();
}
