/*
 * Copyright (c) 1991-1995 Sun Microsystems, Inc.
 */

#ident	"@(#)standalloc.c	1.44	97/06/30 SMI"

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

extern	caddr_t		resalloc(enum RESOURCES type,
				unsigned bytes, caddr_t virthint, int align);

caddr_t		memlistpage;
extern int	pagesize;

/*
 *  This routine should be called get_a_page().
 *  It allocates from the appropriate entity one or
 *  more pages and maps them in.
 *  Note that there is no resfree() function.
 *  This then assumes that the standalones cannot free
 *  any pages which have been allocated and mapped.
 */

caddr_t
kern_resalloc(caddr_t virthint, size_t size, int align)
{
	if (virthint != 0)
		return (resalloc(RES_CHILDVIRT, size, virthint, align));
	else {
		return (resalloc(RES_BOOTSCRATCH, size, NULL, NULL));
	}
}

int
get_progmemory(caddr_t vaddr, size_t size, int align)
{
	u_int n;

	/*
	 * if the vaddr given is not a mult of PAGESIZE,
	 * then we rounddown to a page, but keep the same
	 * ending addr.
	 */
	n = (u_int)vaddr & (pagesize - 1);
	if (n) {
		vaddr -= n;
		size += n;
	}

	dprintf("get_progmem: requesting %x bytes at %x\n", size, vaddr);
	if (resalloc(RES_CHILDVIRT, size, vaddr, align) != vaddr)
		return (-1);
	return (0);
}
