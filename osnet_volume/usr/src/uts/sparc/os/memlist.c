/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)memlist.c	1.7	98/10/25 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/class.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/archsystm.h>

#include <sys/reboot.h>
#include <sys/uadmin.h>

#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/session.h>
#include <sys/ucontext.h>

#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/debug/debug.h>
#include <sys/thread.h>
#include <sys/vtrace.h>
#include <sys/consdev.h>
#include <sys/frame.h>
#include <sys/stack.h>
#include <sys/swap.h>
#include <sys/vmparam.h>
#include <sys/cpuvar.h>

#include <sys/privregs.h>

#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>

#include <sys/exec.h>
#include <sys/acct.h>
#include <sys/modctl.h>
#include <sys/tuneable.h>

#include <c2/audit.h>

#include <sys/trap.h>
#include <sys/sunddi.h>
#include <sys/bootconf.h>
#include <sys/memlist.h>
#include <sys/dumphdr.h>
#include <sys/systeminfo.h>
#include <sys/promif.h>


/*
 * Count the number of available pages and the number of
 * chunks in the list of available memory.
 */
void
size_physavail(
	struct memlist	*physavail,
	pgcnt_t		*npages,
	int		*memblocks)
{
	*npages = 0;
	*memblocks = 0;

	for (; physavail; physavail = physavail->next) {
		*npages += (pgcnt_t)(physavail->size >> PAGESHIFT);
		(*memblocks)++;
	}
}

/*
 * Returns the max contiguous physical memory present in the
 * memlist "physavail".
 */
uint64_t
get_max_phys_size(
	struct memlist	*physavail)
{
	uint64_t	max_size = 0;

	for (; physavail; physavail = physavail->next) {
		if (physavail->size > max_size)
			max_size = physavail->size;
	}

	return (max_size);
}


/*
 * Copy boot's physavail list deducting memory at "start"
 * for "size" bytes.
 */
int
copy_physavail(
	struct memlist	*src,
	struct memlist	**dstp,
	u_int		start,
	u_int		size)
{
	struct memlist *dst, *prev;
	u_int end1;
	int deducted = 0;

	dst = *dstp;
	prev = dst;
	end1 = start + size;

	for (; src; src = src->next) {
		uint64_t addr, lsize, end2;

		addr = src->address;
		lsize = src->size;
		end2 = addr + lsize;

		if ((size != 0) && start >= addr && end1 <= end2) {
			/* deducted range in this chunk */
			deducted = 1;
			if (start == addr) {
				/* abuts start of chunk */
				if (end1 == end2)
					/* is equal to the chunk */
					continue;
				dst->address = end1;
				dst->size = lsize - size;
			} else if (end1 == end2) {
				/* abuts end of chunk */
				dst->address = addr;
				dst->size = lsize - size;
			} else {
				/* in the middle of the chunk */
				dst->address = addr;
				dst->size = start - addr;
				dst->next = 0;
				if (prev == dst) {
					dst->prev = 0;
					dst++;
				} else {
					dst->prev = prev;
					prev->next = dst;
					dst++;
					prev++;
				}
				dst->address = end1;
				dst->size = end2 - end1;
			}
			dst->next = 0;
			if (prev == dst) {
				dst->prev = 0;
				dst++;
			} else {
				dst->prev = prev;
				prev->next = dst;
				dst++;
				prev++;
			}
		} else {
			dst->address = src->address;
			dst->size = src->size;
			dst->next = 0;
			if (prev == dst) {
				dst->prev = 0;
				dst++;
			} else {
				dst->prev = prev;
				prev->next = dst;
				dst++;
				prev++;
			}
		}
	}

	*dstp = dst;
	return (deducted);
}

struct vnode prom_ppages;

/*
 * Find the pages allocated by the prom by diffing the original
 * phys_avail list and the current list.  In the difference, the
 * pages not locked belong to the PROM.  (The kernel has already locked
 * and removed all the pages it has allocated from the freelist, this
 * routine removes the remaining "free" pages that really belong to the
 * PROM and hashs them in on the 'prom_pages' vnode.)
 */
void
fix_prom_pages(struct memlist *orig, struct memlist *new)
{
	struct memlist *list, *nlist;

	nlist = new;
	for (list = orig; list; list = list->next) {

		uint64_t pa, end;
		pfn_t pfnum;
		page_t *pp;

		if (list->address == nlist->address &&
		    list->size == nlist->size) {
			nlist = nlist->next ? nlist->next : nlist;
			continue;
		}

		/*
		 * Loop through the old list looking to
		 * see if each page is still in the new one.
		 * If a page is not in the new list then we
		 * check to see if it locked permanently.
		 * If so, the kernel allocated and owns it.
		 * If not, then the prom must own it. We
		 * remove any pages found to owned by the prom
		 * from the freelist.
		 */
		end = list->address + list->size;
		for (pa = list->address; pa < end; pa += PAGESIZE) {

			if (address_in_memlist(new, pa, PAGESIZE))
				continue;

			pfnum = (pfn_t)(pa >> PAGESHIFT);
			if ((pp = page_numtopp_nolock(pfnum)) == NULL)
				cmn_err(CE_PANIC, "missing pfnum %lx", pfnum);

			if (!PAGE_LOCKED(pp)) {
				/*
				 * Ahhh yes, a prom page,
				 * suck it off the freelist,
				 * lock it, and hashin on prom_pages vp.
				 */
				if (page_trylock(pp, SE_EXCL) == 0)
					cmn_err(CE_PANIC, "prom page locked");

				(void) page_reclaim(pp, NULL);
				/*
				 * XXX	vnode offsets on the prom_ppages vnode
				 *	are page numbers (gack) for >32 bit
				 *	physical memory machines.
				 */
				(void) page_hashin(pp, &prom_ppages,
					(offset_t)pfnum, NULL);

				page_downgrade(pp);
			}
		}
		nlist = nlist->next ? nlist->next : nlist;
	}
}

/*
 * Find the page number of the highest installed physical
 * page and the number of pages installed (one cannot be
 * calculated from the other because memory isn't necessarily
 * contiguous).
 */
void
installed_top_size(
	struct memlist *list,	/* pointer to start of installed list */
	int *topp,		/* return ptr for top value */
	int *sumpagesp)		/* return prt for sum of installed pages */
{
	int top = 0;
	int sumpages = 0;
	int highp;		/* high page in a chunk */

	for (; list; list = list->next) {
		highp = (list->address + list->size - 1) >> PAGESHIFT;
		if (top < highp)
			top = highp;
		sumpages += (u_int)(list->size >> PAGESHIFT);
	}

	*topp = top;
	*sumpagesp = sumpages;
}

#ifdef _ILP32
/*
 * Check a memory list to see elements are
 * expansion memory.
 */
int
check_memexp(
	struct memlist	*list,
	u_int		memexp_start)
{
	int memexp_flag = 0;

	for (; list; list = list->next) {
		if (list->address >= memexp_start) {
			memexp_flag++;
			break;
		}
	}

	return (memexp_flag);
}
#endif

/*
 * Copy a memory list.  Used in startup() to copy boot's
 * memory lists to the kernel.
 */
void
copy_memlist(
	struct memlist	*src,
	struct memlist	**dstp)
{
	struct memlist *dst, *prev;

	dst = *dstp;
	prev = dst;

	for (; src; src = src->next) {
		dst->address = src->address;
		dst->size = src->size;
		dst->next = 0;
		if (prev == dst) {
			dst->prev = 0;
			dst++;
		} else {
			dst->prev = prev;
			prev->next = dst;
			dst++;
			prev++;
		}
	}

	*dstp = dst;
}
