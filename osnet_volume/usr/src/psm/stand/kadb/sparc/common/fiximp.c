/*
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fiximp.c	1.25	99/10/22 SMI"

#include <sys/types.h>
#include <sys/machparam.h>
#include <sys/mmu.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>

uint_t npmgrps;
uint_t segmask;
int vac;
int vac_size;
int vac_nlines;
int vac_linesize;
int pagesize;
int use_align;
int icache_flush;
#ifdef __sparcv9cpu
int v9flag = 1;
#else
int v9flag = 0;
#endif

extern int nwindows;
extern void mach_fiximp(void);

/*
 * Look up a property by name, and fetch its value.
 * This version of getprop fetches an int-length property
 * into the space at *ip. It returns the length of the
 * requested property; the caller should test that the
 * return value == sizeof(int) for success.
 * A non-int-length property can be fetched by getlongprop(), q.v.
 */
int
getprop(dnode_t id, char *name, caddr_t ip)
{
	int len;

	len = prom_getproplen((dnode_t)id, (caddr_t)name);
	if (len != (int)OBP_BADNODE && len != (int)OBP_NONODE)
		prom_getprop((dnode_t)id, (caddr_t)name, (caddr_t)ip);
	return (len);
}

void
fiximp(void)
{
	pstack_t *stk;
	dnode_t node;
	dnode_t sp[OBP_STACKDEPTH];

	mach_fiximp();

	/*
	 * What's our pagesize?
	 */
	pagesize = PAGESIZE;	/* default */
	if (prom_is_openprom()) {
		stk = prom_stack_init(sp, sizeof (sp));
		node = prom_findnode_bydevtype(prom_rootnode(), OBP_CPU, stk);
		if (node != OBP_NONODE && node != OBP_BADNODE)
			(void) getprop(node, "page-size", (caddr_t)&pagesize);
		prom_stack_fini(stk);
	}

	/*
	 * Can we make aligned memory requests?
	 */
	use_align = 0;
	if (prom_is_openprom()) {
		stk = prom_stack_init(sp, sizeof (sp));
		node = prom_findnode_byname(prom_rootnode(), "openprom", stk);
		if (node != OBP_NONODE && node != OBP_BADNODE) {
			if (prom_getproplen(node, "aligned-allocator") == 0)
				use_align = 1;
		}
		prom_stack_fini(stk);
	}
}
