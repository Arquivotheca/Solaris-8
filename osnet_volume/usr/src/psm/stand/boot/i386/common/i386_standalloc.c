/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)i386_standalloc.c	1.13	99/10/07 SMI"

#include <sys/types.h>
#include <sys/saio.h>
#include <sys/sysmacros.h>
#include <sys/promif.h>
#include <sys/bootconf.h>
#include <sys/bootdef.h>
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
extern	void		reset_alloc(void);
extern	caddr_t		get_low_vpage(size_t n);
extern	void		alloc_segment(caddr_t v);
extern	void 		map_child(caddr_t v, caddr_t p);
extern	void		print_memlist(struct memlist *av);
extern	caddr_t		top_bootmem;
extern	caddr_t		magic_phys;

caddr_t		memlistpage;
caddr_t 	scratchmemp;
/*
 * max_bootaddr is the most we let the arena grow. It should go up
 * to magic_phys which is 4Mb. It is limited due to losing memory
 * on pre-2.6 kernels and wanting install on 16Mb to work. This
 * should be changed to 4Mb when this problem is worked around.
 */
caddr_t 	max_bootaddr = (caddr_t)0x3e0000;
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

		printf("Alloc of 0x%x bytes at 0x%x refused.\n",
			bytes, virthint);
		return ((caddr_t)0);

		/*NOTREACHED*/

	default:
		printf("Bad resurce type\n");
		return ((caddr_t)0);
	}
}

void
reset_alloc()
{
	extern char _endkeep[];

	/* Cannot be called multiple times */
	if (memlistpage != (caddr_t)0)
		return;

	/*
	 *  Due to kernel history and ease of programming, we
	 *  want to keep everything private to /boot BELOW MAGIC_PHYS.
	 *  In this way, the kernel can just snarf it all when
	 *  it is ready, and not worry about snarfing lists.
	 */
	memlistpage = (caddr_t)roundup((u_int)_endkeep, pagesize);

	/*
	 *  This next is for scratch memory only
	 *  We only need 1 page in memlistpage for now
	 */
	scratchmemp = (caddr_t)(memlistpage + pagesize);

	bzero(memlistpage, pagesize);
	bzero(scratchmemp, pagesize);
	dprintf("memlistpage = %x\n", memlistpage);
}

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

	/*
	 * keep things from getting out of hand
	 * XXX - perhaps the alloc should just fail ?
	 */
	if (scratchmemp >= max_bootaddr)
		prom_panic("Boot:  scratch memory overflow.\n");

	return (v);
}
