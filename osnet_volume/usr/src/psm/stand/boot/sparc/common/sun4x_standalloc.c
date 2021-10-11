/*
 * Copyright (c) 1991-1995 Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)sun4x_standalloc.c	1.6	99/10/04 SMI"

#include <sys/types.h>
#include <sys/saio.h>
#include <sys/sysmacros.h>
#include <sys/promif.h>
#include <sys/bootconf.h>
#include <sys/salib.h>

#define	NIL		0

#ifdef DEBUG
static int	resalloc_debug = 1;
#else DEBUG
static int	resalloc_debug = 0;
#endif DEBUG
#define	dprintf	if (resalloc_debug) printf

extern struct memlist	*vfreelistp, *pfreelistp;
extern int 		insert_node(struct memlist **list,
					struct memlist *request);
extern	struct memlist	*get_min_node(struct memlist *l, u_longlong_t lo,
				u_int size);
extern	void		reset_alloc(void);
extern	caddr_t		get_low_vpage(size_t n);
extern	void		alloc_segment(caddr_t v);
extern	caddr_t		get_hi_ppage(void);
extern	caddr_t		resalloc(enum RESOURCES type,
				size_t bytes, caddr_t virthint, int align);
extern	void		print_memlist(struct memlist *av);

caddr_t		memlistpage;
caddr_t		le_page;
caddr_t		ie_page;
caddr_t 	scratchmemp;
extern int	pagesize;

caddr_t
resalloc(enum RESOURCES type, size_t bytes, caddr_t virthint, int align)
{
	caddr_t	vaddr;
	long pmap = 0;

	if (memlistpage == (caddr_t)0)
		reset_alloc();

	if (bytes == 0)
		return ((caddr_t)0);

	/* extend request to fill a page */
	bytes = roundup(bytes, pagesize);

	dprintf("resalloc:  bytes = %x\n", bytes);

	switch (type) {

	/*
	 * even V2 PROMs never bother to indicate whether the
	 * first MAGIC_PHYS is taken or not.  So we do it all here.
	 * Smart PROM or no smart PROM.
	 */
	case RES_BOOTSCRATCH:
		vaddr = get_low_vpage(bytes/pagesize);

		if (resalloc_debug) {
			dprintf("vaddr = %x, paddr = %x\n", vaddr, ptob(pmap));
			print_memlist(vfreelistp);
			print_memlist(pfreelistp);
		}
		return (vaddr);
		/*NOTREACHED*/

	case RES_CHILDVIRT:
		vaddr = (caddr_t)prom_alloc(virthint, bytes, align);

		if (vaddr == (caddr_t)virthint)
			return (vaddr);
		if (prom_getversion() <= 0) {
			struct memlist a;
			caddr_t	v;
			/*
			 * First we need to find, allocate, and map
			 * the segments corresponding to the
			 * virthint+size.  We also map the
			 * pages within that range.
			 * We don't really care if the pages
			 * were previously mapped or not because
			 * we rely upon the integrity of our own
			 * memlists.
			 */
			for (v = virthint; v < virthint + bytes;
					v += pagesize) {
				caddr_t p;

				/*
				 *  we put the virtual
				 *  addr on the list
				 */
				a.address = (uint64_t)v;
				a.size = pagesize;
				a.next = a.prev = NIL;
				if (insert_node(&vfreelistp, &a) == 0)
					prom_panic("Cannot fill request.\n");

				/*
				 *  ...and the physical
				 */
				p = get_hi_ppage();
				a.address = (uint64_t)p;
				a.size = pagesize;
				a.next = a.prev = NIL;
				if (insert_node(&pfreelistp, &a) == 0)
					prom_panic("Cannot fill request.\n");
			}
			if (resalloc_debug) {
				dprintf("virthint = %x\n", virthint);
				print_memlist(vfreelistp);
				print_memlist(pfreelistp);
			}
			return (virthint);
		} else {
			printf("Alloc of 0x%x bytes at 0x%x refused.\n",
				bytes, virthint);
			return ((caddr_t)0);
		}
		/*NOTREACHED*/

	default:
		printf("Bad resurce type\n");
		return ((caddr_t)0);
	}
}

#ifdef	lint
static char _end[1];	/* defined by the linker! */
#endif	lint

void
reset_alloc()
{
	extern char _end[];

	/* Cannot be called multiple times */
	if (memlistpage != (caddr_t)0)
		return;

	/*
	 *  Due to kernel history and ease of programming, we
	 *  want to keep everything private to /boot BELOW MAGIC_PHYS.
	 *  In this way, the kernel can just snarf it all when
	 *  when it is ready, and not worry about snarfing lists.
	 */
	memlistpage = (caddr_t)roundup((uintptr_t)_end, pagesize);

	/*
	 *  This next is for scratch memory only
	 *  We only need 1 page in memlistpage for now
	 */
	scratchmemp = (caddr_t)(memlistpage + pagesize);
	le_page = (caddr_t)(scratchmemp + pagesize);
	ie_page = (caddr_t)(le_page + pagesize);

	bzero(memlistpage, pagesize);
	bzero(scratchmemp, pagesize);
	dprintf("memlistpage = %x\n", memlistpage);
	dprintf("le_page = %x\n", le_page);
}


/*
 * For sparc/sun platforms, the top_bootmem is the same as MAGIC_PHYS
 * and is not changed.
 */
#define	MAGIC_PHYS	(4*1024*1024)
caddr_t magic_phys = (caddr_t)MAGIC_PHYS;
caddr_t top_bootmem = (caddr_t)MAGIC_PHYS;

/*
 *	This routine will find the next PAGESIZE chunk in the
 *	low MAGIC_PHYS.  It is analogous to valloc(). It is only for boot
 *	scratch memory, because child scratch memory goes up in
 *	the the high memory.  We just need to verify that the
 *	pages are on the list.  The calling routine will actually
 *	remove them.
 */
caddr_t
get_low_vpage(size_t numpages)
{
	caddr_t v;

	v = scratchmemp;

	if (!numpages)
		return (0);

	/* We know the page is mapped because the 1st MAGIC_PHYS is 1:1 */
	scratchmemp += (numpages * pagesize);

	/* keep things from getting out of hand */
	if (scratchmemp >= top_bootmem)
		prom_panic("Boot:  scratch memory overflow.\n");

	return (v);
}

/*
 *  This routine will allocate a phys page from our
 *  list of ones known to be unallocated.  Death and
 *  destruction shall befall those programs which do not
 *  register their memory usage.  They will most likely
 *  obtain double mappings.
 */
caddr_t
get_hi_ppage(void)
{
	struct memlist *node;

	node = NIL;

	if (pfreelistp == NIL)
		prom_panic("No physmemory list.\n");

/*
 *	Since physmem gets crowded below MAGIC_PHYS, we start
 *	searching above that magic spot all the time for
 *	phys pages for a child.
 */
	node = get_min_node(pfreelistp, (u_longlong_t)magic_phys, pagesize);
	if (node == NIL)
		prom_panic("get_hi_ppage():  Cannot find link in range.\n");
	/*
	 * It is best to let the calling routine take
	 * the node off the freelist.
	 */

	return ((caddr_t)MAX(node->address, (uintptr_t)magic_phys));
}
